#include "video_pipeline/video_pipeline.h"
#include "video_pipeline/logger.h"

namespace video_pipeline {

bool Framework::initialized_ = false;

bool Framework::Initialize() {
    if (initialized_) {
        return true;
    }
    
    // Initialize global logger
    Logger::SetLogger(std::make_shared<ConsoleLogger>(LogLevel::INFO));
    
    VP_LOG_INFO_F("Initializing Video Pipeline Framework v{}", GetVersion());
    
    // Register common blocks
    auto& registry = BlockRegistry::Instance();
    registry.RegisterCommonBlocks();
    
    // Register platform-specific blocks
    RegisterPlatformBlocks();
    
    initialized_ = true;
    VP_LOG_INFO("Framework initialization complete");
    return true;
}

void Framework::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    VP_LOG_INFO("Shutting down Video Pipeline Framework");
    
    // Clear block registry
    BlockRegistry::Instance().Clear();
    
    initialized_ = false;
    VP_LOG_DEBUG("Framework shutdown complete");
}

bool Framework::IsInitialized() {
    return initialized_;
}

void Framework::SetLogLevel(LogLevel level) {
    auto logger = Logger::GetLogger();
    if (logger) {
        logger->SetLevel(level);
    }
}

void Framework::RegisterPlatformBlocks() {
    auto& registry = BlockRegistry::Instance();
    registry.RegisterPlatformBlocks();
}

} // namespace video_pipeline