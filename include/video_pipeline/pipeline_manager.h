#pragma once

#include "block.h"
#include "video_source.h"
#include "video_sink.h"
#include <vector>
#include <memory>
#include <map>

namespace video_pipeline {

/**
 * @brief Connection between blocks
 */
struct Connection {
    std::string source_block;
    std::string source_output{"output"};
    std::string sink_block;
    std::string sink_input{"input"};
    
    std::string ToString() const;
};

/**
 * @brief Pipeline configuration
 */
struct PipelineConfig {
    std::string name;
    std::string platform{"generic"};
    
    // Block definitions
    struct BlockDef {
        std::string name;
        std::string type;
        BlockParams parameters;
    };
    std::vector<BlockDef> blocks;
    
    // Connections between blocks
    std::vector<Connection> connections;
    
    // Global pipeline settings
    std::map<std::string, std::string> settings;
};

/**
 * @brief Pipeline interface
 */
class IPipeline {
public:
    virtual ~IPipeline() = default;
    
    // Pipeline lifecycle
    virtual bool Initialize(const PipelineConfig& config) = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual bool Shutdown() = 0;
    
    // Pipeline state
    virtual bool IsRunning() const = 0;
    virtual std::string GetStatus() const = 0;
    
    // Block access
    virtual BlockPtr GetBlock(const std::string& name) const = 0;
    virtual std::vector<BlockPtr> GetBlocks() const = 0;
    virtual std::vector<std::string> GetBlockNames() const = 0;
    
    // Statistics
    virtual std::map<std::string, BlockStats> GetAllStats() const = 0;
    virtual void ResetAllStats() = 0;
};

/**
 * @brief Pipeline manager - main entry point for pipeline operations
 */
class PipelineManager : public IPipeline {
public:
    PipelineManager();
    virtual ~PipelineManager();
    
    // IPipeline implementation
    bool Initialize(const PipelineConfig& config) override;
    bool Start() override;
    bool Stop() override;
    bool Shutdown() override;
    
    bool IsRunning() const override;
    std::string GetStatus() const override;
    
    BlockPtr GetBlock(const std::string& name) const override;
    std::vector<BlockPtr> GetBlocks() const override;
    std::vector<std::string> GetBlockNames() const override;
    
    std::map<std::string, BlockStats> GetAllStats() const override;
    void ResetAllStats() override;
    
    // Configuration management
    bool LoadConfiguration(const std::string& config_file);
    bool LoadConfigurationFromString(const std::string& config_content, 
                                   const std::string& format = "yaml");
    PipelineConfig GetConfiguration() const { return config_; }
    
    // Error handling
    void SetErrorCallback(ErrorCallback callback);
    std::string GetLastError() const;

private:
    // Internal methods
    bool CreateBlocks();
    bool ConfigureBlocks();
    bool ConnectBlocks();
    void OnBlockError(IBlock* block, const std::string& error);
    
    // Pipeline state
    PipelineConfig config_;
    std::map<std::string, BlockPtr> blocks_;
    std::atomic<bool> is_running_{false};
    
    // Error handling
    ErrorCallback error_callback_;
    std::string last_error_;
    
    // Thread safety
    mutable std::mutex mutex_;
};

} // namespace video_pipeline