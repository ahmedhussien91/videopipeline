#pragma once

/**
 * @file video_pipeline.h
 * @brief Main header file for the Video Pipeline Framework
 * 
 * This header provides access to all the core components of the video pipeline framework:
 * - Buffer and video frame interfaces
 * - Block interfaces (IBlock, IVideoSource, IVideoSink)
 * - Pipeline management (PipelineManager, BlockRegistry)
 * - Configuration parsing
 * - Logging and timing utilities
 */

// Core interfaces
#include "buffer.h"
#include "block.h"
#include "video_source.h"
#include "video_sink.h"

// Framework management
#include "pipeline_manager.h"
#include "block_registry.h"
#include "config_parser.h"

// Utilities
#include "logger.h"
#include "timer.h"

namespace video_pipeline {

/**
 * @brief Framework version information
 */
struct FrameworkVersion {
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;
    
    static std::string GetVersionString() {
        return std::to_string(MAJOR) + "." + 
               std::to_string(MINOR) + "." + 
               std::to_string(PATCH);
    }
};

/**
 * @brief Framework initialization
 */
class Framework {
public:
    // Initialize the framework
    static bool Initialize();
    
    // Shutdown the framework
    static void Shutdown();
    
    // Check if framework is initialized
    static bool IsInitialized();
    
    // Get framework version
    static std::string GetVersion() { return FrameworkVersion::GetVersionString(); }
    
    // Set global log level
    static void SetLogLevel(LogLevel level);
    
    // Register platform-specific blocks
    static void RegisterPlatformBlocks();

private:
    static bool initialized_;
};

} // namespace video_pipeline