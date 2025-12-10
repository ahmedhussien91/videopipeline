#pragma once

#include "block.h"
#include "video_source.h"
#include "video_sink.h"
#include <functional>
#include <map>
#include <vector>

namespace video_pipeline {

/**
 * @brief Block factory function type
 */
using BlockFactory = std::function<BlockPtr()>;

/**
 * @brief Block registry for managing block types and their factories
 */
class BlockRegistry {
public:
    static BlockRegistry& Instance();
    
    // Block registration
    bool RegisterBlock(const std::string& type, BlockFactory factory);
    bool UnregisterBlock(const std::string& type);
    
    // Block creation
    BlockPtr CreateBlock(const std::string& type) const;
    BlockPtr CreateBlock(const std::string& type, const std::string& name) const;
    
    // Registry queries
    bool IsRegistered(const std::string& type) const;
    std::vector<std::string> GetRegisteredTypes() const;
    size_t GetRegisteredCount() const;
    
    // Platform-specific registration
    void RegisterPlatformBlocks();
    void RegisterCommonBlocks();
    
    // Clear registry (mainly for testing)
    void Clear();

private:
    BlockRegistry() = default;
    ~BlockRegistry() = default;
    BlockRegistry(const BlockRegistry&) = delete;
    BlockRegistry& operator=(const BlockRegistry&) = delete;
    
    std::map<std::string, BlockFactory> factories_;
    mutable std::mutex mutex_;
};

/**
 * @brief Helper template for easy block registration
 */
template<typename BlockType>
class BlockRegistrar {
public:
    BlockRegistrar(const std::string& type) {
        BlockRegistry::Instance().RegisterBlock(type, []() -> BlockPtr {
            return std::make_shared<BlockType>();
        });
    }
};

/**
 * @brief Macro for convenient block registration
 */
#define REGISTER_BLOCK(BlockType, type_name) \
    static video_pipeline::BlockRegistrar<BlockType> g_##BlockType##_registrar(type_name)

} // namespace video_pipeline