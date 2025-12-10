#include "video_pipeline/video_pipeline.h"
#include "video_pipeline/blocks/test_pattern_source.h"
#include "video_pipeline/blocks/console_sink.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace video_pipeline;

int main() {
    std::cout << "Simple Test Pattern Example\n";
    std::cout << "Video Pipeline Framework v" << Framework::GetVersion() << "\n\n";
    
    // Initialize framework
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
    
    try {
        // Create blocks manually (without configuration file)
        auto source = std::make_shared<TestPatternSource>();
        auto sink = std::make_shared<ConsoleSink>();
        
        // Configure source
        BlockParams source_params;
        source_params["width"] = "320";
        source_params["height"] = "240";
        source_params["fps"] = "10";
        source_params["pattern"] = "checkerboard";
        
        if (!source->Initialize(source_params)) {
            std::cerr << "Failed to initialize source: " << source->GetLastError() << "\n";
            return 1;
        }
        
        // Configure sink
        BlockParams sink_params;
        sink_params["verbose"] = "true";
        sink_params["show_pixels"] = "true";
        sink_params["max_pixels"] = "4";
        
        if (!sink->Initialize(sink_params)) {
            std::cerr << "Failed to initialize sink: " << sink->GetLastError() << "\n";
            return 1;
        }
        
        // Connect source to sink
        source->SetFrameCallback([sink](VideoFramePtr frame) {
            sink->ProcessFrame(frame);
        });
        
        // Start blocks
        std::cout << "Starting blocks...\n";
        if (!sink->Start()) {
            std::cerr << "Failed to start sink\n";
            return 1;
        }
        
        if (!source->Start()) {
            std::cerr << "Failed to start source\n";
            return 1;
        }
        
        std::cout << "Running for 5 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Stop blocks
        std::cout << "\nStopping blocks...\n";
        source->Stop();
        sink->Stop();
        
        // Print final statistics
        auto source_stats = source->GetStats();
        auto sink_stats = sink->GetStats();
        
        std::cout << "\nFinal Statistics:\n";
        std::cout << "Source: " << source_stats.frames_processed << " frames, "
                  << std::fixed << std::setprecision(1) << source_stats.avg_fps << " FPS\n";
        std::cout << "Sink: " << sink_stats.frames_processed << " frames, "
                  << sink_stats.frames_dropped << " dropped\n";
        
        source->Shutdown();
        sink->Shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        Framework::Shutdown();
        return 1;
    }
    
    Framework::Shutdown();
    std::cout << "\nExample completed successfully!\n";
    return 0;
}