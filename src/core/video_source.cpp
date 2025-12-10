#include "video_pipeline/video_source.h"
#include "video_pipeline/logger.h"
#include "video_pipeline/timer.h"

namespace video_pipeline {

BaseVideoSource::BaseVideoSource(const std::string& name, const std::string& type)
    : BaseBlock(name, type) {
    // Initialize default output format
    output_format_.width = 640;
    output_format_.height = 480;
    output_format_.pixel_format = PixelFormat::RGB24;
    output_format_.stride = output_format_.width * 3;
}

bool BaseVideoSource::SetFrameCallback(FrameCallback callback) {
    frame_callback_ = callback;
    return true;
}

bool BaseVideoSource::SetFrameRate(double fps) {
    if (fps <= 0 || fps > 1000) {
        SetError("Invalid frame rate: " + std::to_string(fps));
        return false;
    }
    
    frame_rate_ = fps;
    UpdateFrameInterval();
    return true;
}

bool BaseVideoSource::SetBufferCount(size_t count) {
    if (count == 0 || count > 100) {
        SetError("Invalid buffer count: " + std::to_string(count));
        return false;
    }
    
    buffer_count_ = count;
    return true;
}

bool BaseVideoSource::Initialize(const BlockParams& params) {
    // Parse common parameters
    auto width_str = BaseBlock::GetParameter("width");
    auto height_str = BaseBlock::GetParameter("height");
    auto fps_str = BaseBlock::GetParameter("fps");
    auto format_str = BaseBlock::GetParameter("format");
    
    if (!width_str.empty()) {
        output_format_.width = std::stoul(width_str);
    }
    
    if (!height_str.empty()) {
        output_format_.height = std::stoul(height_str);
    }
    
    if (!fps_str.empty()) {
        SetFrameRate(std::stod(fps_str));
    }
    
    if (!format_str.empty()) {
        // Parse pixel format string
        if (format_str == "RGB24") output_format_.pixel_format = PixelFormat::RGB24;
        else if (format_str == "BGR24") output_format_.pixel_format = PixelFormat::BGR24;
        else if (format_str == "RGBA32") output_format_.pixel_format = PixelFormat::RGBA32;
        else if (format_str == "YUV420P") output_format_.pixel_format = PixelFormat::YUV420P;
        else if (format_str == "YUYV") output_format_.pixel_format = PixelFormat::YUYV;
        // Add more formats as needed
    }
    
    // Update stride based on pixel format
    switch (output_format_.pixel_format) {
        case PixelFormat::RGB24:
        case PixelFormat::BGR24:
            output_format_.stride = output_format_.width * 3;
            break;
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32:
            output_format_.stride = output_format_.width * 4;
            break;
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            output_format_.stride = output_format_.width * 2;
            break;
        default:
            output_format_.stride = output_format_.width;
            break;
    }
    
    SetState(BlockState::INITIALIZED);
    VP_LOG_INFO_F("VideoSource {} initialized: {}", BaseBlock::GetName(), output_format_.ToString());
    return true;
}

bool BaseVideoSource::Stop() {
    SetState(BlockState::STOPPING);
    // Derived classes should override to stop their specific source
    SetState(BlockState::STOPPED);
    return true;
}

void BaseVideoSource::EmitFrame(VideoFramePtr frame) {
    if (!frame || !frame_callback_) {
        return;
    }
    
    // Check frame rate limiting
    if (!ShouldEmitFrame()) {
        UpdateStats(false, 0, true);  // Count as dropped frame
        return;
    }
    
    // Update timestamp and sequence number
    auto& info = const_cast<FrameInfo&>(frame->GetFrameInfo());
    info.timestamp_us = Timer::GetCurrentTimestampUs();
    info.sequence_number = BaseBlock::GetStats().frames_processed + 1;
    
    // Emit the frame
    frame_callback_(frame);
    
    // Update statistics
    UpdateStats(true, frame->GetSize(), false);
    last_frame_time_ = std::chrono::steady_clock::now();
}

bool BaseVideoSource::ShouldEmitFrame() const {
    if (frame_rate_ <= 0) return true;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame_time_);
    
    return elapsed >= frame_interval_;
}

void BaseVideoSource::UpdateFrameInterval() {
    if (frame_rate_ > 0) {
        frame_interval_ = std::chrono::microseconds(static_cast<uint64_t>(1000000.0 / frame_rate_));
    }
}

} // namespace video_pipeline