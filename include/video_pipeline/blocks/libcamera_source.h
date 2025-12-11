#pragma once

#include "video_pipeline/video_source.h"
#include <libcamera/libcamera.h>
#include <atomic>
#include <thread>
#include <unordered_map>

namespace video_pipeline {

struct LibcameraBuffer {
    libcamera::FrameBuffer* buffer{nullptr};
    void* mapped{nullptr};
    size_t length{0};
};

/**
 * @brief Camera source using libcamera (e.g., Raspberry Pi camera stack)
 */
class LibcameraSource : public BaseVideoSource {
public:
    LibcameraSource(const std::string& name = "libcamera_source");
    ~LibcameraSource() override;

    FrameInfo GetOutputFormat() const override { return output_format_; }
    bool SetOutputFormat(const FrameInfo& format) override;

    bool Initialize(const BlockParams& params) override;
    bool Start() override;
    bool Stop() override;
    bool Shutdown() override;

    bool SupportsFormat(PixelFormat format) const override;
    std::vector<PixelFormat> GetSupportedFormats() const override;
    std::vector<std::pair<uint32_t, uint32_t>> GetSupportedResolutions() const override;

    // Exposed for zero-copy frame wrapper
    void RecycleRequest(libcamera::Request* request);

private:
    bool ConfigureCamera();
    bool SetupBuffers();
    void TeardownBuffers();
    void OnRequestComplete(libcamera::Request* request);
    libcamera::PixelFormat ToLibcameraFormat(PixelFormat fmt) const;

    // Libcamera objects
    std::unique_ptr<libcamera::CameraManager> camera_manager_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    libcamera::Stream* stream_{nullptr};
    std::vector<std::unique_ptr<libcamera::Request>> requests_;

    std::unordered_map<const libcamera::FrameBuffer*, LibcameraBuffer> buffer_map_;

    // Config
    std::string camera_id_;
    PixelFormat configured_format_{PixelFormat::YUYV};

    std::atomic<bool> running_{false};
};

} // namespace video_pipeline
