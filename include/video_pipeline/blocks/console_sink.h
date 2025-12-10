#pragma once

#include "video_pipeline/video_sink.h"

namespace video_pipeline {

/**
 * @brief Video sink that logs frame information to console
 */
class ConsoleSink : public BaseVideoSink {
public:
    ConsoleSink();
    virtual ~ConsoleSink() = default;
    
    // IVideoSink implementation
    bool SupportsFormat(PixelFormat format) const override;
    std::vector<PixelFormat> GetSupportedFormats() const override;
    
    // IBlock implementation
    bool Initialize(const BlockParams& params) override;
    
    // Console sink specific
    void SetVerbose(bool verbose) { verbose_ = verbose; }
    bool IsVerbose() const { return verbose_; }
    
    void SetShowPixelData(bool show_data) { show_pixel_data_ = show_data; }
    bool ShouldShowPixelData() const { return show_pixel_data_; }
    
    void SetMaxPixels(size_t max_pixels) { max_pixels_ = max_pixels; }
    size_t GetMaxPixels() const { return max_pixels_; }
    
protected:
    bool ProcessFrameImpl(VideoFramePtr frame) override;
    
private:
    void LogFrameInfo(VideoFramePtr frame);
    void LogPixelData(VideoFramePtr frame);
    std::string FormatPixelValue(const uint8_t* data, PixelFormat format, size_t pixel_index);
    
    bool verbose_{false};
    bool show_pixel_data_{false};
    size_t max_pixels_{16};  // Maximum number of pixels to show
};

} // namespace video_pipeline