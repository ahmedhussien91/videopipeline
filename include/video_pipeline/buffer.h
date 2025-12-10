#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <atomic>

namespace video_pipeline {

/**
 * @brief Pixel format enumeration
 */
enum class PixelFormat {
    UNKNOWN = 0,
    RGB24,      // 24-bit RGB
    BGR24,      // 24-bit BGR
    RGBA32,     // 32-bit RGBA
    BGRA32,     // 32-bit BGRA
    YUV420P,    // Planar YUV 4:2:0
    NV12,       // Semi-planar YUV 4:2:0
    NV21,       // Semi-planar YUV 4:2:0 (VU)
    YUYV,       // Packed YUV 4:2:2
    UYVY        // Packed YUV 4:2:2
};

/**
 * @brief Video frame metadata
 */
struct FrameInfo {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t stride{0};              // Bytes per row
    PixelFormat pixel_format{PixelFormat::UNKNOWN};
    uint64_t timestamp_us{0};        // Timestamp in microseconds (PTS)
    uint64_t sequence_number{0};     // Frame sequence number
    
    // For hardware-accelerated buffers
    bool is_hardware_buffer{false};
    void* hw_handle{nullptr};        // Platform-specific handle (e.g., dmabuf fd)
    
    size_t GetFrameSize() const;
    std::string ToString() const;
};

/**
 * @brief Generic buffer interface for video data
 */
class IBuffer {
public:
    virtual ~IBuffer() = default;
    
    // Buffer data access
    virtual void* GetData() = 0;
    virtual const void* GetData() const = 0;
    virtual size_t GetSize() const = 0;
    virtual size_t GetCapacity() const = 0;
    
    // Frame metadata
    virtual const FrameInfo& GetFrameInfo() const = 0;
    virtual void SetFrameInfo(const FrameInfo& info) = 0;
    
    // Buffer management
    virtual bool IsValid() const = 0;
    virtual void Reset() = 0;
    
    // Reference counting for zero-copy
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual uint32_t GetRefCount() const = 0;
};

using BufferPtr = std::shared_ptr<IBuffer>;

/**
 * @brief Video frame - specialized buffer for video data
 */
class IVideoFrame : public IBuffer {
public:
    virtual ~IVideoFrame() = default;
    
    // Plane access for planar formats
    virtual void* GetPlaneData(int plane) = 0;
    virtual const void* GetPlaneData(int plane) const = 0;
    virtual size_t GetPlaneSize(int plane) const = 0;
    virtual uint32_t GetPlaneStride(int plane) const = 0;
    virtual int GetPlaneCount() const = 0;
    
    // Convenience methods
    virtual bool CopyFrom(const IVideoFrame& other) = 0;
    virtual BufferPtr Clone() const = 0;
};

using VideoFramePtr = std::shared_ptr<IVideoFrame>;

// Factory functions
BufferPtr CreateBuffer(size_t capacity);
VideoFramePtr CreateVideoFrame(const FrameInfo& info);

} // namespace video_pipeline