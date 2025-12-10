#pragma once

#include "block.h"
#include "buffer.h"
#include <functional>
#include <queue>
#include <condition_variable>

namespace video_pipeline {

/**
 * @brief Output callback for video sources
 */
using FrameCallback = std::function<void(VideoFramePtr frame)>;

/**
 * @brief Video source interface
 */
class IVideoSource : public virtual IBlock {
public:
    virtual ~IVideoSource() = default;
    
    // Source-specific methods
    virtual bool SetFrameCallback(FrameCallback callback) = 0;
    virtual FrameInfo GetOutputFormat() const = 0;
    virtual bool SetOutputFormat(const FrameInfo& format) = 0;
    
    // Frame rate control
    virtual double GetFrameRate() const = 0;
    virtual bool SetFrameRate(double fps) = 0;
    
    // Buffer management
    virtual size_t GetBufferCount() const = 0;
    virtual bool SetBufferCount(size_t count) = 0;
    
    // Source-specific capabilities
    virtual bool SupportsFormat(PixelFormat format) const = 0;
    virtual std::vector<PixelFormat> GetSupportedFormats() const = 0;
    virtual std::vector<std::pair<uint32_t, uint32_t>> GetSupportedResolutions() const = 0;
};

/**
 * @brief Base video source implementation
 */
class BaseVideoSource : public BaseBlock, public IVideoSource {
public:
    BaseVideoSource(const std::string& name, const std::string& type);
    virtual ~BaseVideoSource() = default;
    
    // IVideoSource implementation
    bool SetFrameCallback(FrameCallback callback) override;
    double GetFrameRate() const override { return frame_rate_; }
    bool SetFrameRate(double fps) override;
    size_t GetBufferCount() const override { return buffer_count_; }
    bool SetBufferCount(size_t count) override;
    
    // Common functionality
    bool Initialize(const BlockParams& params) override;
    bool Stop() override;
    
protected:
    // Helper methods for derived classes
    void EmitFrame(VideoFramePtr frame);
    bool ShouldEmitFrame() const;  // For frame rate limiting
    
    // Configuration
    FrameInfo output_format_;
    double frame_rate_{30.0};
    size_t buffer_count_{3};
    
    // Frame emission
    FrameCallback frame_callback_;
    std::chrono::steady_clock::time_point last_frame_time_;
    std::chrono::microseconds frame_interval_{33333}; // ~30 FPS
    
private:
    void UpdateFrameInterval();
};

} // namespace video_pipeline