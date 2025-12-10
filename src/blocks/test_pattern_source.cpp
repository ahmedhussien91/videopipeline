#include "video_pipeline/blocks/test_pattern_source.h"
#include "video_pipeline/logger.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

namespace video_pipeline {

// Forward declaration - these functions are implemented in buffer.cpp
BufferPtr CreateBuffer(size_t capacity);
VideoFramePtr CreateVideoFrame(const FrameInfo& info);

TestPatternSource::TestPatternSource() 
    : BaseVideoSource("TestPatternSource", "TestPatternSource") {
    // Set default format
    output_format_.width = 640;
    output_format_.height = 480;
    output_format_.pixel_format = PixelFormat::RGB24;
    output_format_.stride = output_format_.width * 3;
}

TestPatternSource::~TestPatternSource() {
    Stop();
    Shutdown();
}

bool TestPatternSource::SetOutputFormat(const FrameInfo& format) {
    if (BaseBlock::GetState() == BlockState::RUNNING) {
        SetError("Cannot change output format while running");
        return false;
    }
    
    if (!SupportsFormat(format.pixel_format)) {
        SetError("Unsupported pixel format");
        return false;
    }
    
    output_format_ = format;
    
    // Update stride based on pixel format if not set
    if (output_format_.stride == 0) {
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
    }
    
    VP_LOG_INFO_F("TestPatternSource output format set to: {}", output_format_.ToString());
    return true;
}

bool TestPatternSource::SupportsFormat(PixelFormat format) const {
    switch (format) {
        case PixelFormat::RGB24:
        case PixelFormat::BGR24:
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32:
        case PixelFormat::YUV420P:
        case PixelFormat::YUYV:
            return true;
        default:
            return false;
    }
}

std::vector<PixelFormat> TestPatternSource::GetSupportedFormats() const {
    return {
        PixelFormat::RGB24,
        PixelFormat::BGR24,
        PixelFormat::RGBA32,
        PixelFormat::BGRA32,
        PixelFormat::YUV420P,
        PixelFormat::YUYV
    };
}

std::vector<std::pair<uint32_t, uint32_t>> TestPatternSource::GetSupportedResolutions() const {
    return {
        {160, 120},   // QQVGA
        {320, 240},   // QVGA
        {640, 480},   // VGA
        {800, 600},   // SVGA
        {1024, 768},  // XGA
        {1280, 720},  // HD
        {1920, 1080}  // Full HD
    };
}

bool TestPatternSource::Initialize(const BlockParams& params) {
    if (!BaseVideoSource::Initialize(params)) {
        return false;
    }
    
    // Parse test pattern specific parameters
    auto pattern_str = BaseBlock::GetParameter("pattern");
    if (!pattern_str.empty()) {
        if (pattern_str == "solid") test_pattern_ = TestPattern::SOLID_COLOR;
        else if (pattern_str == "bars") test_pattern_ = TestPattern::COLOR_BARS;
        else if (pattern_str == "checkerboard") test_pattern_ = TestPattern::CHECKERBOARD;
        else if (pattern_str == "gradient") test_pattern_ = TestPattern::GRADIENT;
        else if (pattern_str == "noise") test_pattern_ = TestPattern::NOISE;
        else if (pattern_str == "moving_box") test_pattern_ = TestPattern::MOVING_BOX;
    }
    
    auto color_str = BaseBlock::GetParameter("color");
    if (!color_str.empty()) {
        // Parse color in format "r,g,b" or "#rrggbb"
        if (color_str[0] == '#' && color_str.length() == 7) {
            // Hex format
            std::string r_str = color_str.substr(1, 2);
            std::string g_str = color_str.substr(3, 2);
            std::string b_str = color_str.substr(5, 2);
            color_r_ = static_cast<uint8_t>(std::stoul(r_str, nullptr, 16));
            color_g_ = static_cast<uint8_t>(std::stoul(g_str, nullptr, 16));
            color_b_ = static_cast<uint8_t>(std::stoul(b_str, nullptr, 16));
        } else {
            // CSV format
            size_t pos1 = color_str.find(',');
            size_t pos2 = color_str.find(',', pos1 + 1);
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                color_r_ = static_cast<uint8_t>(std::stoul(color_str.substr(0, pos1)));
                color_g_ = static_cast<uint8_t>(std::stoul(color_str.substr(pos1 + 1, pos2 - pos1 - 1)));
                color_b_ = static_cast<uint8_t>(std::stoul(color_str.substr(pos2 + 1)));
            }
        }
    }
    
    VP_LOG_INFO_F("TestPatternSource initialized: pattern={}, color=({},{},{})", 
                  static_cast<int>(test_pattern_), color_r_, color_g_, color_b_);
    return true;
}

bool TestPatternSource::Start() {
    if (BaseBlock::GetState() == BlockState::RUNNING) {
        return true;
    }
    
    if (BaseBlock::GetState() != BlockState::INITIALIZED && BaseBlock::GetState() != BlockState::STOPPED) {
        SetError("Cannot start from state: " + BaseBlock::GetStateString());
        return false;
    }
    
    SetState(BlockState::STARTING);
    
    // Start generator thread
    stop_generator_.store(false);
    frame_counter_ = 0;
    generator_thread_ = std::thread(&TestPatternSource::GeneratorThread, this);
    
    SetState(BlockState::RUNNING);
    VP_LOG_INFO_F("TestPatternSource '{}' started", BaseBlock::GetName());
    return true;
}

bool TestPatternSource::Stop() {
    if (BaseBlock::GetState() != BlockState::RUNNING) {
        return true;
    }
    
    SetState(BlockState::STOPPING);
    
    // Stop generator thread
    stop_generator_.store(true);
    if (generator_thread_.joinable()) {
        generator_thread_.join();
    }
    
    SetState(BlockState::STOPPED);
    VP_LOG_INFO_F("TestPatternSource '{}' stopped", BaseBlock::GetName());
    return true;
}

bool TestPatternSource::Shutdown() {
    Stop();
    return true;
}

bool TestPatternSource::SetTestPattern(TestPattern pattern) {
    test_pattern_ = pattern;
    VP_LOG_DEBUG_F("TestPatternSource test pattern set to: {}", static_cast<int>(pattern));
    return true;
}

void TestPatternSource::SetColor(uint8_t r, uint8_t g, uint8_t b) {
    color_r_ = r;
    color_g_ = g;
    color_b_ = b;
}

void TestPatternSource::GetColor(uint8_t& r, uint8_t& g, uint8_t& b) const {
    r = color_r_;
    g = color_g_;
    b = color_b_;
}

void TestPatternSource::GeneratorThread() {
    VP_LOG_DEBUG_F("TestPatternSource '{}' generator thread started", BaseBlock::GetName());
    
    while (!stop_generator_.load()) {
        if (!ShouldEmitFrame()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        
        // Create frame buffer
        auto frame = CreateVideoFrame(output_format_);
        if (!frame) {
            VP_LOG_ERROR("Failed to create video frame");
            break;
        }
        
        // Generate test pattern
        GenerateFrame(frame);
        
        // Emit frame
        EmitFrame(frame);
        
        frame_counter_++;
    }
    
    VP_LOG_DEBUG_F("TestPatternSource '{}' generator thread stopped", BaseBlock::GetName());
}

void TestPatternSource::GenerateFrame(VideoFramePtr frame) {
    switch (test_pattern_) {
        case TestPattern::SOLID_COLOR:
            GenerateSolidColor(frame);
            break;
        case TestPattern::COLOR_BARS:
            GenerateColorBars(frame);
            break;
        case TestPattern::CHECKERBOARD:
            GenerateCheckerboard(frame);
            break;
        case TestPattern::GRADIENT:
            GenerateGradient(frame);
            break;
        case TestPattern::NOISE:
            GenerateNoise(frame);
            break;
        case TestPattern::MOVING_BOX:
            GenerateMovingBox(frame);
            break;
    }
}

void TestPatternSource::GenerateSolidColor(VideoFramePtr frame) {
    uint8_t* data = static_cast<uint8_t*>(frame->GetData());
    size_t pixel_count = output_format_.width * output_format_.height;
    
    switch (output_format_.pixel_format) {
        case PixelFormat::RGB24:
            for (size_t i = 0; i < pixel_count; ++i) {
                data[i * 3 + 0] = color_r_;
                data[i * 3 + 1] = color_g_;
                data[i * 3 + 2] = color_b_;
            }
            break;
            
        case PixelFormat::BGR24:
            for (size_t i = 0; i < pixel_count; ++i) {
                data[i * 3 + 0] = color_b_;
                data[i * 3 + 1] = color_g_;
                data[i * 3 + 2] = color_r_;
            }
            break;
            
        case PixelFormat::RGBA32:
            for (size_t i = 0; i < pixel_count; ++i) {
                data[i * 4 + 0] = color_r_;
                data[i * 4 + 1] = color_g_;
                data[i * 4 + 2] = color_b_;
                data[i * 4 + 3] = 255;  // Alpha
            }
            break;
            
        default:
            // Fill with grayscale equivalent
            uint8_t gray = static_cast<uint8_t>(0.299 * color_r_ + 0.587 * color_g_ + 0.114 * color_b_);
            std::memset(data, gray, frame->GetSize());
            break;
    }
}

void TestPatternSource::GenerateColorBars(VideoFramePtr frame) {
    uint8_t* data = static_cast<uint8_t*>(frame->GetData());
    const uint32_t width = output_format_.width;
    const uint32_t height = output_format_.height;
    
    // Standard color bar colors (RGB)
    const uint8_t colors[8][3] = {
        {255, 255, 255}, // White
        {255, 255, 0},   // Yellow
        {0, 255, 255},   // Cyan
        {0, 255, 0},     // Green
        {255, 0, 255},   // Magenta
        {255, 0, 0},     // Red
        {0, 0, 255},     // Blue
        {0, 0, 0}        // Black
    };
    
    uint32_t bar_width = width / 8;
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t bar_index = std::min(7u, x / bar_width);
            const uint8_t* color = colors[bar_index];
            
            switch (output_format_.pixel_format) {
                case PixelFormat::RGB24:
                    data[(y * width + x) * 3 + 0] = color[0];
                    data[(y * width + x) * 3 + 1] = color[1];
                    data[(y * width + x) * 3 + 2] = color[2];
                    break;
                    
                case PixelFormat::BGR24:
                    data[(y * width + x) * 3 + 0] = color[2];
                    data[(y * width + x) * 3 + 1] = color[1];
                    data[(y * width + x) * 3 + 2] = color[0];
                    break;
                    
                case PixelFormat::RGBA32:
                    data[(y * width + x) * 4 + 0] = color[0];
                    data[(y * width + x) * 4 + 1] = color[1];
                    data[(y * width + x) * 4 + 2] = color[2];
                    data[(y * width + x) * 4 + 3] = 255;
                    break;
                    
                default:
                    // Grayscale
                    uint8_t gray = static_cast<uint8_t>(0.299 * color[0] + 0.587 * color[1] + 0.114 * color[2]);
                    data[y * width + x] = gray;
                    break;
            }
        }
    }
}

void TestPatternSource::GenerateCheckerboard(VideoFramePtr frame) {
    uint8_t* data = static_cast<uint8_t*>(frame->GetData());
    const uint32_t width = output_format_.width;
    const uint32_t height = output_format_.height;
    const uint32_t check_size = 32;
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            bool is_white = ((x / check_size) + (y / check_size)) % 2 == 0;
            uint8_t value = is_white ? 255 : 0;
            
            switch (output_format_.pixel_format) {
                case PixelFormat::RGB24:
                case PixelFormat::BGR24:
                    data[(y * width + x) * 3 + 0] = value;
                    data[(y * width + x) * 3 + 1] = value;
                    data[(y * width + x) * 3 + 2] = value;
                    break;
                    
                case PixelFormat::RGBA32:
                    data[(y * width + x) * 4 + 0] = value;
                    data[(y * width + x) * 4 + 1] = value;
                    data[(y * width + x) * 4 + 2] = value;
                    data[(y * width + x) * 4 + 3] = 255;
                    break;
                    
                default:
                    data[y * width + x] = value;
                    break;
            }
        }
    }
}

void TestPatternSource::GenerateGradient(VideoFramePtr frame) {
    uint8_t* data = static_cast<uint8_t*>(frame->GetData());
    const uint32_t width = output_format_.width;
    const uint32_t height = output_format_.height;
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t r = static_cast<uint8_t>((x * 255) / width);
            uint8_t g = static_cast<uint8_t>((y * 255) / height);
            uint8_t b = static_cast<uint8_t>(((x + y) * 255) / (width + height));
            
            switch (output_format_.pixel_format) {
                case PixelFormat::RGB24:
                    data[(y * width + x) * 3 + 0] = r;
                    data[(y * width + x) * 3 + 1] = g;
                    data[(y * width + x) * 3 + 2] = b;
                    break;
                    
                case PixelFormat::BGR24:
                    data[(y * width + x) * 3 + 0] = b;
                    data[(y * width + x) * 3 + 1] = g;
                    data[(y * width + x) * 3 + 2] = r;
                    break;
                    
                case PixelFormat::RGBA32:
                    data[(y * width + x) * 4 + 0] = r;
                    data[(y * width + x) * 4 + 1] = g;
                    data[(y * width + x) * 4 + 2] = b;
                    data[(y * width + x) * 4 + 3] = 255;
                    break;
                    
                default:
                    uint8_t gray = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
                    data[y * width + x] = gray;
                    break;
            }
        }
    }
}

void TestPatternSource::GenerateNoise(VideoFramePtr frame) {
    uint8_t* data = static_cast<uint8_t*>(frame->GetData());
    size_t data_size = frame->GetSize();
    
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    for (size_t i = 0; i < data_size; ++i) {
        data[i] = dist(rng_);
    }
}

void TestPatternSource::GenerateMovingBox(VideoFramePtr frame) {
    uint8_t* data = static_cast<uint8_t*>(frame->GetData());
    const uint32_t width = output_format_.width;
    const uint32_t height = output_format_.height;
    
    // Clear background to black
    std::memset(data, 0, frame->GetSize());
    
    // Calculate moving box position
    const uint32_t box_size = 64;
    uint32_t period = width + height;  // Total distance to travel
    uint32_t pos = frame_counter_ % period;
    
    uint32_t box_x, box_y;
    if (pos < width) {
        // Moving right along top
        box_x = pos;
        box_y = 0;
    } else {
        // Moving down along right edge
        box_x = width - box_size;
        box_y = pos - width;
    }
    
    // Ensure box stays within frame
    box_x = std::min(box_x, width - box_size);
    box_y = std::min(box_y, height - box_size);
    
    // Draw the box
    for (uint32_t y = box_y; y < box_y + box_size && y < height; ++y) {
        for (uint32_t x = box_x; x < box_x + box_size && x < width; ++x) {
            switch (output_format_.pixel_format) {
                case PixelFormat::RGB24:
                    data[(y * width + x) * 3 + 0] = color_r_;
                    data[(y * width + x) * 3 + 1] = color_g_;
                    data[(y * width + x) * 3 + 2] = color_b_;
                    break;
                    
                case PixelFormat::BGR24:
                    data[(y * width + x) * 3 + 0] = color_b_;
                    data[(y * width + x) * 3 + 1] = color_g_;
                    data[(y * width + x) * 3 + 2] = color_r_;
                    break;
                    
                case PixelFormat::RGBA32:
                    data[(y * width + x) * 4 + 0] = color_r_;
                    data[(y * width + x) * 4 + 1] = color_g_;
                    data[(y * width + x) * 4 + 2] = color_b_;
                    data[(y * width + x) * 4 + 3] = 255;
                    break;
                    
                default:
                    uint8_t gray = static_cast<uint8_t>(0.299 * color_r_ + 0.587 * color_g_ + 0.114 * color_b_);
                    data[y * width + x] = gray;
                    break;
            }
        }
    }
}

} // namespace video_pipeline