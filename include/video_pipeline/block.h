#pragma once

#include "buffer.h"
#include <functional>
#include <memory>
#include <string>
#include <map>
#include <atomic>
#include <mutex>

namespace video_pipeline {

// Forward declarations
class IBlock;
class IPipeline;

/**
 * @brief Block states
 */
enum class BlockState {
    UNINITIALIZED = 0,
    INITIALIZED,
    STARTING,
    RUNNING,
    STOPPING,
    STOPPED,
    ERROR
};

/**
 * @brief Block configuration parameters
 */
using BlockParams = std::map<std::string, std::string>;

/**
 * @brief Block statistics
 */
struct BlockStats {
    uint64_t frames_processed{0};
    uint64_t frames_dropped{0};
    uint64_t bytes_processed{0};
    double avg_fps{0.0};
    double avg_latency_ms{0.0};
    uint32_t queue_depth{0};
    std::chrono::steady_clock::time_point last_frame_time;
};

/**
 * @brief Block error callback
 */
using ErrorCallback = std::function<void(IBlock*, const std::string& error)>;

/**
 * @brief Base interface for all pipeline blocks
 */
class IBlock {
public:
    virtual ~IBlock() = default;
    
    // Block identification
    virtual std::string GetName() const = 0;
    virtual std::string GetType() const = 0;
    virtual void SetName(const std::string& name) = 0;
    
    // Lifecycle management
    virtual bool Initialize(const BlockParams& params) = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual bool Shutdown() = 0;
    
    // State management
    virtual BlockState GetState() const = 0;
    virtual std::string GetStateString() const;
    
    // Statistics and monitoring
    virtual BlockStats GetStats() const = 0;
    virtual void ResetStats() = 0;
    
    // Error handling
    virtual void SetErrorCallback(ErrorCallback callback) = 0;
    virtual std::string GetLastError() const = 0;
    
    // Configuration
    virtual BlockParams GetConfiguration() const = 0;
    virtual bool SetParameter(const std::string& key, const std::string& value) = 0;
    virtual std::string GetParameter(const std::string& key) const = 0;
};

using BlockPtr = std::shared_ptr<IBlock>;

/**
 * @brief Base implementation of IBlock with common functionality
 */
class BaseBlock : public virtual IBlock {
public:
    BaseBlock(const std::string& name, const std::string& type);
    virtual ~BaseBlock() = default;
    
    // IBlock implementation
    std::string GetName() const override { return name_; }
    std::string GetType() const override { return type_; }
    void SetName(const std::string& name) override { name_ = name; }
    
    BlockState GetState() const override { return state_.load(); }
    std::string GetStateString() const override;
    
    BlockStats GetStats() const override;
    void ResetStats() override;
    
    void SetErrorCallback(ErrorCallback callback) override { error_callback_ = callback; }
    std::string GetLastError() const override;
    
    BlockParams GetConfiguration() const override;
    bool SetParameter(const std::string& key, const std::string& value) override;
    std::string GetParameter(const std::string& key) const override;

protected:
    // Helper methods for derived classes
    void SetState(BlockState state);
    void SetError(const std::string& error);
    void UpdateStats(bool frame_processed = true, size_t bytes = 0, bool dropped = false);
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Make stats accessible to derived classes
    BlockStats stats_;
    
private:
    std::string name_;
    std::string type_;
    std::atomic<BlockState> state_{BlockState::UNINITIALIZED};
    
    std::string last_error_;
    ErrorCallback error_callback_;
    BlockParams params_;
};

} // namespace video_pipeline