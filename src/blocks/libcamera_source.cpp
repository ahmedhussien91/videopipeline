#include "video_pipeline/blocks/libcamera_source.h"
#include "video_pipeline/logger.h"
#include "video_pipeline/timer.h"
#include <sys/mman.h>
#include <algorithm>
#include <cstring>
#include <atomic>

namespace video_pipeline {

namespace {

class LibcameraFrame : public IVideoFrame {
public:
    LibcameraFrame(void* data, size_t length, const FrameInfo& info, libcamera::Request* request, LibcameraSource* owner)
        : data_(data)
        , length_(length)
        , frame_info_(info)
        , request_(request)
        , owner_(owner) {}

    ~LibcameraFrame() override = default;

    // IBuffer
    void* GetData() override { return data_; }
    const void* GetData() const override { return data_; }
    size_t GetSize() const override { return frame_info_.GetFrameSize(); }
    size_t GetCapacity() const override { return length_; }

    const FrameInfo& GetFrameInfo() const override { return frame_info_; }
    void SetFrameInfo(const FrameInfo& info) override { frame_info_ = info; }

    bool IsValid() const override { return data_ != nullptr && GetFrameSize() <= length_; }
    void Reset() override { frame_info_ = FrameInfo{}; }

    void AddRef() override { ref_count_.fetch_add(1, std::memory_order_relaxed); }
    void Release() override {
        if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (owner_) {
                owner_->RecycleRequest(request_);
            }
            delete this;
        }
    }
    uint32_t GetRefCount() const override { return ref_count_.load(std::memory_order_relaxed); }

    // IVideoFrame
    void* GetPlaneData(int plane) override {
        return (plane == 0) ? data_ : nullptr;
    }
    const void* GetPlaneData(int plane) const override {
        return (plane == 0) ? data_ : nullptr;
    }
    size_t GetPlaneSize(int plane) const override {
        return (plane == 0) ? frame_info_.GetFrameSize() : 0;
    }
    uint32_t GetPlaneStride(int plane) const override {
        return (plane == 0) ? frame_info_.stride : 0;
    }
    int GetPlaneCount() const override { return 1; }

    bool CopyFrom(const IVideoFrame& other) override {
        if (other.GetFrameInfo().GetFrameSize() > length_) return false;
        std::memcpy(data_, other.GetData(), other.GetFrameInfo().GetFrameSize());
        frame_info_ = other.GetFrameInfo();
        return true;
    }

    BufferPtr Clone() const override {
        // Zero-copy frames cannot clone without allocation; fall back to copy.
        auto clone_info = frame_info_;
        auto clone = CreateVideoFrame(clone_info);
        if (clone) {
            clone->CopyFrom(*this);
        }
        return clone;
    }

private:
    size_t GetFrameSize() const { return frame_info_.GetFrameSize(); }

    void* data_{nullptr};
    size_t length_{0};
    FrameInfo frame_info_{};
    libcamera::Request* request_{nullptr};
    LibcameraSource* owner_{nullptr};
    std::atomic<uint32_t> ref_count_{1};
};

} // namespace

LibcameraSource::LibcameraSource(const std::string& name)
    : BaseVideoSource(name, "LibcameraSource") {}

LibcameraSource::~LibcameraSource() {
    Shutdown();
}

bool LibcameraSource::SetOutputFormat(const FrameInfo& format) {
    if (GetState() == BlockState::RUNNING) {
        SetError("Cannot change format while running");
        return false;
    }

    if (!SupportsFormat(format.pixel_format)) {
        SetError("Unsupported pixel format");
        return false;
    }

    output_format_ = format;
    configured_format_ = format.pixel_format;

    if (output_format_.stride == 0) {
        switch (output_format_.pixel_format) {
            case PixelFormat::RGB24:
            case PixelFormat::BGR24:
                output_format_.stride = output_format_.width * 3;
                break;
            case PixelFormat::NV12:
            case PixelFormat::NV21:
                output_format_.stride = output_format_.width;
                break;
            case PixelFormat::YUYV:
            case PixelFormat::UYVY:
                output_format_.stride = output_format_.width * 2;
                break;
            default:
                break;
        }
    }

    return true;
}

bool LibcameraSource::Initialize(const BlockParams& params) {
    if (!BaseVideoSource::Initialize(params)) {
        return false;
    }

    camera_id_ = GetParameter("camera_id");
    auto buffer_count_param = GetParameter("buffer_count");
    if (!buffer_count_param.empty()) {
        SetBufferCount(std::stoul(buffer_count_param));
    }
    configured_format_ = output_format_.pixel_format;

    camera_manager_ = std::make_unique<libcamera::CameraManager>();
    if (camera_manager_->start()) {
        SetError("Failed to start libcamera manager");
        return false;
    }

    if (!ConfigureCamera()) {
        return false;
    }

    if (!SetupBuffers()) {
        return false;
    }

    SetState(BlockState::INITIALIZED);
    VP_LOG_INFO_F("LibcameraSource {} initialized: {}", GetName(), output_format_.ToString());
    return true;
}

bool LibcameraSource::ConfigureCamera() {
    if (camera_manager_->cameras().empty()) {
        SetError("No cameras found via libcamera");
        return false;
    }

    if (camera_id_.empty()) {
        camera_ = camera_manager_->get(camera_manager_->cameras().front()->id());
    } else {
        // Allow numeric index or exact id
        camera_ = camera_manager_->get(camera_id_);
        if (!camera_) {
            try {
                size_t idx = std::stoul(camera_id_);
                if (idx < camera_manager_->cameras().size()) {
                    camera_ = camera_manager_->get(camera_manager_->cameras()[idx]->id());
                }
            } catch (...) {
                // Ignore parse errors
            }
        }
    }

    if (!camera_) {
        SetError("Requested camera not found: " + camera_id_);
        return false;
    }

    if (camera_->acquire()) {
        SetError("Failed to acquire camera");
        return false;
    }

    auto config = camera_->generateConfiguration({libcamera::StreamRole::VideoRecording});
    if (!config || config->empty()) {
        SetError("Failed to generate camera configuration");
        return false;
    }

    auto& cfg = config->at(0);
    cfg.size.width = output_format_.width;
    cfg.size.height = output_format_.height;
    cfg.pixelFormat = ToLibcameraFormat(configured_format_);
    cfg.bufferCount = buffer_count_;

    if (config->validate() == libcamera::CameraConfiguration::Invalid) {
        SetError("Camera configuration invalid");
        return false;
    }

    if (camera_->configure(config.get())) {
        SetError("Failed to configure camera");
        return false;
    }

    stream_ = cfg.stream();
    if (!stream_) {
        SetError("Camera stream unavailable after configuration");
        return false;
    }

    // Update output format with the validated configuration
    output_format_.width = cfg.size.width;
    output_format_.height = cfg.size.height;

    switch (cfg.pixelFormat) {
        case libcamera::formats::RGB888:
            output_format_.pixel_format = PixelFormat::RGB24;
            output_format_.stride = output_format_.width * 3;
            break;
        case libcamera::formats::BGR888:
            output_format_.pixel_format = PixelFormat::BGR24;
            output_format_.stride = output_format_.width * 3;
            break;
        case libcamera::formats::NV12:
            output_format_.pixel_format = PixelFormat::NV12;
            output_format_.stride = output_format_.width;
            break;
        case libcamera::formats::NV21:
            output_format_.pixel_format = PixelFormat::NV21;
            output_format_.stride = output_format_.width;
            break;
        case libcamera::formats::UYVY:
            output_format_.pixel_format = PixelFormat::UYVY;
            output_format_.stride = output_format_.width * 2;
            break;
        case libcamera::formats::YUYV:
        default:
            output_format_.pixel_format = PixelFormat::YUYV;
            output_format_.stride = output_format_.width * 2;
            break;
    }

    return true;
}

bool LibcameraSource::SetupBuffers() {
    allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
    if (allocator_->allocate(stream_) < 0) {
        SetError("Failed to allocate camera buffers");
        return false;
    }

    const auto& buffers = allocator_->buffers(stream_);
    if (buffers.empty()) {
        SetError("No buffers allocated for camera stream");
        return false;
    }

    for (const auto& buffer : buffers) {
        const libcamera::FrameBuffer* fb = buffer.get();
        if (fb->planes().empty()) {
            SetError("Camera buffer has no planes");
            return false;
        }

        const auto& plane = fb->planes().front();
        int fd = plane.fd.get();
        void* map = mmap(nullptr, plane.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED) {
            SetError("Failed to mmap camera buffer");
            return false;
        }

        buffer_map_[fb] = LibcameraBuffer{const_cast<libcamera::FrameBuffer*>(fb), map, plane.length};

        auto request = camera_->createRequest();
        if (!request) {
            SetError("Failed to create capture request");
            return false;
        }

        if (request->addBuffer(stream_, buffer.get()) < 0) {
            SetError("Failed to add buffer to request");
            return false;
        }

        requests_.push_back(std::move(request));
    }

    return true;
}

void LibcameraSource::TeardownBuffers() {
    for (auto& entry : buffer_map_) {
        if (entry.second.mapped && entry.second.length > 0) {
            munmap(entry.second.mapped, entry.second.length);
        }
    }
    buffer_map_.clear();

    requests_.clear();
    allocator_.reset();
}

bool LibcameraSource::Start() {
    if (!camera_ || !stream_) {
        SetError("Camera not initialized");
        return false;
    }

    SetState(BlockState::STARTING);
    running_.store(true);

    camera_->requestCompleted.connect(this, &LibcameraSource::OnRequestComplete);

    if (camera_->start() < 0) {
        SetError("Failed to start camera");
        running_.store(false);
        return false;
    }

    for (auto& request : requests_) {
        camera_->queueRequest(request.get());
    }

    SetState(BlockState::RUNNING);
    VP_LOG_INFO_F("LibcameraSource '{}' started", GetName());
    return true;
}

bool LibcameraSource::Stop() {
    running_.store(false);

    if (camera_) {
        camera_->stop();
        camera_->requestCompleted.disconnect(this);
    }

    SetState(BlockState::STOPPED);
    VP_LOG_INFO_F("LibcameraSource '{}' stopped", GetName());
    return true;
}

bool LibcameraSource::Shutdown() {
    Stop();

    TeardownBuffers();

    if (camera_) {
        camera_->release();
        camera_.reset();
    }

    if (camera_manager_) {
        camera_manager_->stop();
        camera_manager_.reset();
    }

    return true;
}

void LibcameraSource::RecycleRequest(libcamera::Request* request) {
    if (!running_.load() || !camera_ || !request) {
        return;
    }
    request->reuse(libcamera::Request::ReuseBuffers);
    camera_->queueRequest(request);
}

void LibcameraSource::OnRequestComplete(libcamera::Request* request) {
    if (!request || !running_.load()) {
        return;
    }

    if (request->status() == libcamera::Request::RequestCancelled) {
        return;
    }

    if (request->buffers().empty()) {
        return;
    }

    const libcamera::FrameBuffer* fb = request->buffers().begin()->second;
    auto it = buffer_map_.find(fb);
    if (it == buffer_map_.end()) {
        VP_LOG_WARNING("Received buffer not in map");
        return;
    }

    const auto& mapped = it->second;
    FrameInfo info = output_format_;
    info.timestamp_us = Timer::GetCurrentTimestampUs();
    info.is_hardware_buffer = true;
    info.hw_handle = const_cast<libcamera::FrameBuffer*>(fb);

    // Zero-copy: wrap libcamera buffer and requeue when the last reference is released.
    auto frame = VideoFramePtr(new LibcameraFrame(mapped.mapped, mapped.length, info, request, this),
                               [](IVideoFrame* f) { f->Release(); });
    EmitFrame(frame);
}

bool LibcameraSource::SupportsFormat(PixelFormat format) const {
    switch (format) {
        case PixelFormat::RGB24:
        case PixelFormat::BGR24:
        case PixelFormat::NV12:
        case PixelFormat::NV21:
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            return true;
        default:
            return false;
    }
}

std::vector<PixelFormat> LibcameraSource::GetSupportedFormats() const {
    return {PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::NV12, PixelFormat::NV21, PixelFormat::YUYV, PixelFormat::UYVY};
}

std::vector<std::pair<uint32_t, uint32_t>> LibcameraSource::GetSupportedResolutions() const {
    return {{640, 480}, {1280, 720}, {1920, 1080}};
}

libcamera::PixelFormat LibcameraSource::ToLibcameraFormat(PixelFormat fmt) const {
    switch (fmt) {
        case PixelFormat::RGB24: return libcamera::formats::RGB888;
        case PixelFormat::BGR24: return libcamera::formats::BGR888;
        case PixelFormat::NV12: return libcamera::formats::NV12;
        case PixelFormat::NV21: return libcamera::formats::NV21;
        case PixelFormat::UYVY: return libcamera::formats::UYVY;
        case PixelFormat::YUYV:
        default:
            return libcamera::formats::YUYV;
    }
}

} // namespace video_pipeline
