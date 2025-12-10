#pragma once

#include "video_pipeline/video_source.h"
#include <thread>
#include <atomic>
#include <random>

namespace video_pipeline {

/**
 * @brief Test pattern types
 */
enum class TestPattern {
    SOLID_COLOR = 0,
    COLOR_BARS,
    CHECKERBOARD,
    GRADIENT,
    NOISE,
    MOVING_BOX
};

/**
 * @brief Test pattern video source for testing and debugging
 */
class TestPatternSource : public BaseVideoSource {
public:
    TestPatternSource();
    virtual ~TestPatternSource();
    
    // IVideoSource implementation
    FrameInfo GetOutputFormat() const override { return output_format_; }
    bool SetOutputFormat(const FrameInfo& format) override;
    
    bool SupportsFormat(PixelFormat format) const override;
    std::vector<PixelFormat> GetSupportedFormats() const override;
    std::vector<std::pair<uint32_t, uint32_t>> GetSupportedResolutions() const override;
    
    // IBlock implementation
    bool Initialize(const BlockParams& params) override;
    bool Start() override;
    bool Stop() override;
    bool Shutdown() override;
    
    // Test pattern specific
    bool SetTestPattern(TestPattern pattern);
    TestPattern GetTestPattern() const { return test_pattern_; }
    
    void SetColor(uint8_t r, uint8_t g, uint8_t b);
    void GetColor(uint8_t& r, uint8_t& g, uint8_t& b) const;
    
private:
    void GeneratorThread();
    void GenerateFrame(VideoFramePtr frame);
    void GenerateSolidColor(VideoFramePtr frame);
    void GenerateColorBars(VideoFramePtr frame);
    void GenerateCheckerboard(VideoFramePtr frame);
    void GenerateGradient(VideoFramePtr frame);
    void GenerateNoise(VideoFramePtr frame);
    void GenerateMovingBox(VideoFramePtr frame);
    
    TestPattern test_pattern_{TestPattern::COLOR_BARS};
    uint8_t color_r_{255}, color_g_{255}, color_b_{255};
    
    // Threading
    std::thread generator_thread_;
    std::atomic<bool> stop_generator_{false};
    
    // Animation state
    uint32_t frame_counter_{0};
    std::mt19937 rng_{std::random_device{}()};
};

} // namespace video_pipeline