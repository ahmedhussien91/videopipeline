#include "video_pipeline/blocks/console_sink.h"
#include "video_pipeline/logger.h"
#include "video_pipeline/timer.h"
#include <iostream>
#include <iomanip>

namespace video_pipeline {

ConsoleSink::ConsoleSink()
    : BaseVideoSink("ConsoleSink", "ConsoleSink") {
}

bool ConsoleSink::SupportsFormat(PixelFormat format) const {
    // ConsoleSink can log info for any format
    return true;
}

std::vector<PixelFormat> ConsoleSink::GetSupportedFormats() const {
    return {
        PixelFormat::RGB24,
        PixelFormat::BGR24,
        PixelFormat::RGBA32,
        PixelFormat::BGRA32,
        PixelFormat::YUV420P,
        PixelFormat::NV12,
        PixelFormat::NV21,
        PixelFormat::YUYV,
        PixelFormat::UYVY
    };
}

bool ConsoleSink::Initialize(const BlockParams& params) {
    if (!BaseVideoSink::Initialize(params)) {
        return false;
    }
    
    // Parse console sink specific parameters
    auto verbose_str = BaseBlock::GetParameter("verbose");
    if (!verbose_str.empty()) {
        verbose_ = (verbose_str == "true" || verbose_str == "1");
    }
    
    auto show_pixels_str = BaseBlock::GetParameter("show_pixels");
    if (!show_pixels_str.empty()) {
        show_pixel_data_ = (show_pixels_str == "true" || show_pixels_str == "1");
    }
    
    auto max_pixels_str = BaseBlock::GetParameter("max_pixels");
    if (!max_pixels_str.empty()) {
        max_pixels_ = std::stoul(max_pixels_str);
    }
    
    VP_LOG_INFO_F("ConsoleSink initialized: verbose={}, show_pixels={}, max_pixels={}",
                  verbose_, show_pixel_data_, max_pixels_);
    return true;
}

bool ConsoleSink::ProcessFrameImpl(VideoFramePtr frame) {
    if (!frame || !frame->IsValid()) {
        std::cout << "[" << BaseBlock::GetName() << "] ERROR: Invalid frame received" << std::endl;
        return false;
    }
    
    LogFrameInfo(frame);
    
    if (show_pixel_data_) {
        LogPixelData(frame);
    }
    
    return true;
}

void ConsoleSink::LogFrameInfo(VideoFramePtr frame) {
    const auto& info = frame->GetFrameInfo();
    const auto& stats = BaseBlock::GetStats();
    
    static uint64_t last_log_time = 0;
    uint64_t current_time = Timer::GetCurrentTimestampMs();
    
    // Log frame info every second or if verbose
    bool should_log = verbose_ || (current_time - last_log_time) > 1000;
    
    if (should_log) {
        std::cout << "[" << BaseBlock::GetName() << "] ";
        std::cout << "Frame " << std::setw(8) << info.sequence_number;
        std::cout << " | " << info.ToString();
        std::cout << " | Size: " << std::setw(8) << frame->GetSize() << " bytes";
        
        if (info.timestamp_us > 0) {
            double age_ms = (Timer::GetCurrentTimestampUs() - info.timestamp_us) / 1000.0;
            std::cout << " | Age: " << std::fixed << std::setprecision(1) << age_ms << "ms";
        }
        
        std::cout << " | FPS: " << std::fixed << std::setprecision(1) << stats.avg_fps;
        std::cout << " | Queue: " << GetQueueDepth() << "/" << GetMaxQueueDepth();
        std::cout << std::endl;
        
        last_log_time = current_time;
    } else if (stats.frames_processed % 30 == 0) {
        // Brief update every 30 frames
        std::cout << "[" << BaseBlock::GetName() << "] Frames: " << stats.frames_processed 
                  << ", FPS: " << std::fixed << std::setprecision(1) << stats.avg_fps << "\r" << std::flush;
    }
}

void ConsoleSink::LogPixelData(VideoFramePtr frame) {
    const auto& info = frame->GetFrameInfo();
    const uint8_t* data = static_cast<const uint8_t*>(frame->GetData());
    
    std::cout << "[" << BaseBlock::GetName() << "] Pixel data (first " << max_pixels_ << " pixels):" << std::endl;
    
    size_t pixels_to_show = std::min(static_cast<size_t>(max_pixels_), 
                                   static_cast<size_t>(info.width * info.height));
    
    for (size_t i = 0; i < pixels_to_show; ++i) {
        if (i > 0 && i % 8 == 0) {
            std::cout << std::endl;
        }
        
        std::cout << "  " << std::setw(2) << i << ": " 
                  << FormatPixelValue(data, info.pixel_format, i);
    }
    
    std::cout << std::endl;
}

std::string ConsoleSink::FormatPixelValue(const uint8_t* data, PixelFormat format, size_t pixel_index) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    
    switch (format) {
        case PixelFormat::RGB24:
        case PixelFormat::BGR24:
            oss << "(" << std::setw(2) << static_cast<int>(data[pixel_index * 3 + 0])
                << "," << std::setw(2) << static_cast<int>(data[pixel_index * 3 + 1])
                << "," << std::setw(2) << static_cast<int>(data[pixel_index * 3 + 2]) << ")";
            break;
            
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32:
            oss << "(" << std::setw(2) << static_cast<int>(data[pixel_index * 4 + 0])
                << "," << std::setw(2) << static_cast<int>(data[pixel_index * 4 + 1])
                << "," << std::setw(2) << static_cast<int>(data[pixel_index * 4 + 2])
                << "," << std::setw(2) << static_cast<int>(data[pixel_index * 4 + 3]) << ")";
            break;
            
        case PixelFormat::YUYV:
        case PixelFormat::UYVY:
            oss << "(" << std::setw(2) << static_cast<int>(data[pixel_index * 2 + 0])
                << "," << std::setw(2) << static_cast<int>(data[pixel_index * 2 + 1]) << ")";
            break;
            
        default:
            oss << std::setw(2) << static_cast<int>(data[pixel_index]);
            break;
    }
    
    return oss.str();
}

} // namespace video_pipeline