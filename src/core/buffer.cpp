#include "video_pipeline/buffer.h"
#include "video_pipeline/logger.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <memory>
#include <cstdlib>
#include <stdexcept>

namespace video_pipeline {

size_t FrameInfo::GetFrameSize() const {
    switch (pixel_format) {
        case PixelFormat::RGB24:
        case PixelFormat::BGR24:
            return width * height * 3;
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32:
            return width * height * 4;
        case PixelFormat::YUV420P:
            return width * height * 3 / 2;  // Y + U/V planes
        case PixelFormat::NV12:
        case PixelFormat::NV21:
            return width * height * 3 / 2;  // Y + UV plane
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            return width * height * 2;
        default:
            return 0;
    }
}

std::string FrameInfo::ToString() const {
    std::ostringstream oss;
    oss << width << "x" << height;
    
    // Add pixel format
    switch (pixel_format) {
        case PixelFormat::RGB24: oss << " RGB24"; break;
        case PixelFormat::BGR24: oss << " BGR24"; break;
        case PixelFormat::RGBA32: oss << " RGBA32"; break;
        case PixelFormat::BGRA32: oss << " BGRA32"; break;
        case PixelFormat::YUV420P: oss << " YUV420P"; break;
        case PixelFormat::NV12: oss << " NV12"; break;
        case PixelFormat::NV21: oss << " NV21"; break;
        case PixelFormat::YUYV: oss << " YUYV"; break;
        case PixelFormat::UYVY: oss << " UYVY"; break;
        default: oss << " UNKNOWN"; break;
    }
    
    if (stride > 0 && stride != width * 3) {  // Assuming RGB for default
        oss << " stride=" << stride;
    }
    
    if (timestamp_us > 0) {
        oss << " ts=" << timestamp_us << "us";
    }
    
    if (sequence_number > 0) {
        oss << " seq=" << sequence_number;
    }
    
    return oss.str();
}

/**
 * @brief Simple buffer implementation
 */
class SimpleBuffer : public IVideoFrame {
public:
    SimpleBuffer(size_t capacity) 
        : capacity_(capacity)
        , size_(0)
        , ref_count_(1) {
        data_ = std::aligned_alloc(32, capacity);  // 32-byte alignment for SIMD
        if (!data_) {
            throw std::bad_alloc();
        }
    }
    
    ~SimpleBuffer() {
        if (data_) {
            std::free(data_);
        }
    }
    
    // IBuffer implementation
    void* GetData() override { return data_; }
    const void* GetData() const override { return data_; }
    size_t GetSize() const override { return size_; }
    size_t GetCapacity() const override { return capacity_; }
    
    const FrameInfo& GetFrameInfo() const override { return frame_info_; }
    void SetFrameInfo(const FrameInfo& info) override { 
        frame_info_ = info;
        size_ = info.GetFrameSize();
    }
    
    bool IsValid() const override { return data_ != nullptr && size_ <= capacity_; }
    void Reset() override { 
        size_ = 0;
        frame_info_ = FrameInfo{};
    }
    
    void AddRef() override { ref_count_.fetch_add(1); }
    void Release() override { 
        if (ref_count_.fetch_sub(1) == 1) {
            delete this;
        }
    }
    uint32_t GetRefCount() const override { return ref_count_.load(); }
    
    // IVideoFrame implementation
    void* GetPlaneData(int plane) override {
        if (!data_ || plane < 0) return nullptr;
        
        switch (frame_info_.pixel_format) {
            case PixelFormat::RGB24:
            case PixelFormat::BGR24:
            case PixelFormat::RGBA32:
            case PixelFormat::BGRA32:
            case PixelFormat::YUYV:
            case PixelFormat::UYVY:
                // Packed formats have only one plane
                return (plane == 0) ? data_ : nullptr;
                
            case PixelFormat::YUV420P:
                // Y, U, V planes
                if (plane == 0) return data_;
                if (plane == 1) return static_cast<uint8_t*>(data_) + frame_info_.width * frame_info_.height;
                if (plane == 2) return static_cast<uint8_t*>(data_) + frame_info_.width * frame_info_.height * 5 / 4;
                return nullptr;
                
            case PixelFormat::NV12:
            case PixelFormat::NV21:
                // Y plane and UV plane
                if (plane == 0) return data_;
                if (plane == 1) return static_cast<uint8_t*>(data_) + frame_info_.width * frame_info_.height;
                return nullptr;
                
            default:
                return nullptr;
        }
    }
    
    const void* GetPlaneData(int plane) const override {
        return const_cast<SimpleBuffer*>(this)->GetPlaneData(plane);
    }
    
    size_t GetPlaneSize(int plane) const override {
        switch (frame_info_.pixel_format) {
            case PixelFormat::YUV420P:
                if (plane == 0) return frame_info_.width * frame_info_.height;
                if (plane == 1 || plane == 2) return frame_info_.width * frame_info_.height / 4;
                return 0;
                
            case PixelFormat::NV12:
            case PixelFormat::NV21:
                if (plane == 0) return frame_info_.width * frame_info_.height;
                if (plane == 1) return frame_info_.width * frame_info_.height / 2;
                return 0;
                
            default:
                return (plane == 0) ? GetSize() : 0;
        }
    }
    
    uint32_t GetPlaneStride(int plane) const override {
        switch (frame_info_.pixel_format) {
            case PixelFormat::RGB24:
            case PixelFormat::BGR24:
                return (plane == 0) ? frame_info_.width * 3 : 0;
            case PixelFormat::RGBA32:
            case PixelFormat::BGRA32:
                return (plane == 0) ? frame_info_.width * 4 : 0;
            case PixelFormat::YUV420P:
                if (plane == 0) return frame_info_.width;
                if (plane == 1 || plane == 2) return frame_info_.width / 2;
                return 0;
            case PixelFormat::NV12:
            case PixelFormat::NV21:
                if (plane == 0) return frame_info_.width;
                if (plane == 1) return frame_info_.width;
                return 0;
            case PixelFormat::YUYV:
            case PixelFormat::UYVY:
                return (plane == 0) ? frame_info_.width * 2 : 0;
            default:
                return 0;
        }
    }
    
    int GetPlaneCount() const override {
        switch (frame_info_.pixel_format) {
            case PixelFormat::YUV420P:
                return 3;  // Y, U, V
            case PixelFormat::NV12:
            case PixelFormat::NV21:
                return 2;  // Y, UV
            default:
                return 1;  // Packed formats
        }
    }
    
    bool CopyFrom(const IVideoFrame& other) override {
        const auto& other_info = other.GetFrameInfo();
        if (other_info.GetFrameSize() > capacity_) {
            return false;
        }
        
        SetFrameInfo(other_info);
        
        // Copy plane by plane for better performance
        int plane_count = std::min(GetPlaneCount(), other.GetPlaneCount());
        for (int i = 0; i < plane_count; ++i) {
            const void* src_data = other.GetPlaneData(i);
            void* dst_data = GetPlaneData(i);
            size_t plane_size = other.GetPlaneSize(i);
            
            if (src_data && dst_data && plane_size > 0) {
                std::memcpy(dst_data, src_data, plane_size);
            }
        }
        
        return true;
    }
    
    BufferPtr Clone() const override {
        auto clone = std::make_shared<SimpleBuffer>(capacity_);
        clone->CopyFrom(*this);
        return clone;
    }

private:
    void* data_;
    size_t capacity_;
    size_t size_;
    FrameInfo frame_info_;
    std::atomic<uint32_t> ref_count_;
};

// Factory function to create buffers
BufferPtr CreateBuffer(size_t capacity) {
    try {
        return std::make_shared<SimpleBuffer>(capacity);
    } catch (const std::exception& e) {
        VP_LOG_ERROR_F("Failed to create buffer: {}", e.what());
        return nullptr;
    }
}

VideoFramePtr CreateVideoFrame(const FrameInfo& info) {
    size_t required_size = info.GetFrameSize();
    if (required_size == 0) {
        VP_LOG_ERROR("Cannot create video frame: invalid frame info");
        return nullptr;
    }
    
    try {
        auto buffer = std::make_shared<SimpleBuffer>(required_size);
        buffer->SetFrameInfo(info);
        return buffer;
    } catch (const std::exception& e) {
        VP_LOG_ERROR_F("Failed to create video frame: {}", e.what());
        return nullptr;
    }
}

} // namespace video_pipeline