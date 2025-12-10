#include "video_pipeline/block_registry.h"
#include "video_pipeline/logger.h"

namespace video_pipeline {

BlockRegistry& BlockRegistry::Instance() {
    static BlockRegistry instance;
    return instance;
}

bool BlockRegistry::RegisterBlock(const std::string& type, BlockFactory factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (factories_.find(type) != factories_.end()) {
        VP_LOG_WARNING_F("Block type '{}' already registered, overwriting", type);
    }
    
    factories_[type] = factory;
    VP_LOG_DEBUG_F("Registered block type: {}", type);
    return true;
}

bool BlockRegistry::UnregisterBlock(const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = factories_.find(type);
    if (it == factories_.end()) {
        VP_LOG_WARNING_F("Block type '{}' not found for unregistration", type);
        return false;
    }
    
    factories_.erase(it);
    VP_LOG_DEBUG_F("Unregistered block type: {}", type);
    return true;
}

BlockPtr BlockRegistry::CreateBlock(const std::string& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = factories_.find(type);
    if (it == factories_.end()) {
        VP_LOG_ERROR_F("Block type '{}' not registered", type);
        return nullptr;
    }
    
    try {
        auto block = it->second();
        if (!block) {
            VP_LOG_ERROR_F("Factory for block type '{}' returned null", type);
            return nullptr;
        }
        
        VP_LOG_DEBUG_F("Created block of type: {}", type);
        return block;
    } catch (const std::exception& e) {
        VP_LOG_ERROR_F("Exception creating block type '{}': {}", type, e.what());
        return nullptr;
    }
}

BlockPtr BlockRegistry::CreateBlock(const std::string& type, const std::string& name) const {
    auto block = CreateBlock(type);
    if (block) {
        block->SetName(name);
    }
    return block;
}

bool BlockRegistry::IsRegistered(const std::string& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return factories_.find(type) != factories_.end();
}

std::vector<std::string> BlockRegistry::GetRegisteredTypes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> types;
    types.reserve(factories_.size());
    
    for (const auto& pair : factories_) {
        types.push_back(pair.first);
    }
    
    return types;
}

size_t BlockRegistry::GetRegisteredCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return factories_.size();
}

void BlockRegistry::RegisterCommonBlocks() {
    // Forward declarations - these will be implemented in separate files
    // RegisterBlock("TestPatternSource", []() -> BlockPtr { return std::make_shared<TestPatternSource>(); });
    // RegisterBlock("FileSink", []() -> BlockPtr { return std::make_shared<FileSink>(); });
    // RegisterBlock("ConsoleSink", []() -> BlockPtr { return std::make_shared<ConsoleSink>(); });
    
    VP_LOG_INFO("Common blocks registered");
}

void BlockRegistry::RegisterPlatformBlocks() {
#ifdef __linux__
    // Register Linux-specific blocks
    // RegisterBlock("V4L2Source", []() -> BlockPtr { return std::make_shared<V4L2Source>(); });
    // RegisterBlock("FramebufferSink", []() -> BlockPtr { return std::make_shared<FramebufferSink>(); });
    VP_LOG_INFO("Linux platform blocks registered");
#endif
    
    // Add other platforms as needed
}

void BlockRegistry::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    factories_.clear();
    VP_LOG_DEBUG("Block registry cleared");
}

} // namespace video_pipeline