#include "video_pipeline/video_pipeline.h"
#include "video_pipeline/blocks/test_pattern_source.h"
#include "video_pipeline/blocks/console_sink.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>

using namespace video_pipeline;

struct TestConfig {
    uint32_t width;
    uint32_t height;
    double fps;
    TestPattern pattern;
    std::string description;
};

int main() {
    std::cout << "Video Pipeline Framework Performance Test\n";
    std::cout << "Version: " << Framework::GetVersion() << "\n\n";
    
    if (!Framework::Initialize()) {
        std::cerr << "Failed to initialize framework\n";
        return 1;
    }
    
    // Register blocks
    auto& registry = BlockRegistry::Instance();
    registry.RegisterBlock("TestPatternSource", []() -> BlockPtr {
        return std::make_shared<TestPatternSource>();
    });
    registry.RegisterBlock("ConsoleSink", []() -> BlockPtr {
        return std::make_shared<ConsoleSink>();
    });
    
    // Test configurations
    std::vector<TestConfig> test_configs = {
        {320, 240, 30, TestPattern::SOLID_COLOR, "QVGA 30fps solid"},
        {640, 480, 30, TestPattern::COLOR_BARS, "VGA 30fps bars"},
        {800, 600, 25, TestPattern::CHECKERBOARD, "SVGA 25fps checkerboard"},
        {1280, 720, 15, TestPattern::GRADIENT, "HD 15fps gradient"},
        {320, 240, 60, TestPattern::NOISE, "QVGA 60fps noise"},
        {640, 480, 60, TestPattern::MOVING_BOX, "VGA 60fps moving box"}
    };
    
    std::cout << "Running " << test_configs.size() << " performance tests...\n\n";
    
    for (size_t i = 0; i < test_configs.size(); ++i) {
        const auto& config = test_configs[i];
        
        std::cout << "Test " << (i + 1) << "/" << test_configs.size() 
                  << ": " << config.description << "\n";
        
        try {
            auto source = std::make_shared<TestPatternSource>();
            auto sink = std::make_shared<ConsoleSink>();
            
            // Configure blocks
            BlockParams source_params;
            source_params["width"] = std::to_string(config.width);
            source_params["height"] = std::to_string(config.height);
            source_params["fps"] = std::to_string(config.fps);
            source_params["pattern"] = std::to_string(static_cast<int>(config.pattern));
            
            BlockParams sink_params;
            sink_params["verbose"] = "false";  // Minimal output for performance testing
            sink_params["queue_depth"] = "30";
            
            if (!source->Initialize(source_params) || !sink->Initialize(sink_params)) {
                std::cout << "  FAILED: Initialization failed\n\n";
                continue;
            }
            
            // Connect and start
            source->SetFrameCallback([sink](VideoFramePtr frame) {
                sink->ProcessFrame(frame);
            });
            
            sink->Start();
            source->Start();
            
            // Run test for 3 seconds
            Timer test_timer;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            double test_duration = test_timer.GetElapsedSeconds();
            
            // Stop and collect stats
            source->Stop();
            sink->Stop();
            
            auto source_stats = source->GetStats();
            auto sink_stats = sink->GetStats();
            
            // Calculate performance metrics
            double actual_fps = source_stats.frames_processed / test_duration;
            double throughput_mbps = (source_stats.bytes_processed / (1024.0 * 1024.0)) / test_duration;
            double cpu_efficiency = (actual_fps / config.fps) * 100.0;
            
            std::cout << std::fixed << std::setprecision(1);
            std::cout << "  Generated: " << source_stats.frames_processed << " frames\n";
            std::cout << "  Processed: " << sink_stats.frames_processed << " frames\n";
            std::cout << "  Dropped: " << sink_stats.frames_dropped << " frames\n";
            std::cout << "  Actual FPS: " << actual_fps << " (target: " << config.fps << ")\n";
            std::cout << "  Throughput: " << throughput_mbps << " MB/s\n";
            std::cout << "  Efficiency: " << cpu_efficiency << "%\n";
            std::cout << "  Avg Latency: " << std::setprecision(2) << source_stats.avg_latency_ms << "ms\n";
            
            // Performance verdict
            if (cpu_efficiency >= 90.0 && sink_stats.frames_dropped == 0) {
                std::cout << "  RESULT: EXCELLENT\n";
            } else if (cpu_efficiency >= 75.0 && sink_stats.frames_dropped < 5) {
                std::cout << "  RESULT: GOOD\n";
            } else if (cpu_efficiency >= 50.0) {
                std::cout << "  RESULT: ACCEPTABLE\n";
            } else {
                std::cout << "  RESULT: POOR\n";
            }
            
            source->Shutdown();
            sink->Shutdown();
            
        } catch (const std::exception& e) {
            std::cout << "  FAILED: Exception - " << e.what() << "\n";
        }
        
        std::cout << "\n";
        
        // Small delay between tests
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    Framework::Shutdown();
    std::cout << "Performance testing completed.\n";
    return 0;
}