#pragma once

#include "block.h"
#include "buffer.h"
#include <queue>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace video_pipeline {

/**
 * @brief Video sink interface
 */
class IVideoSink : public virtual IBlock {
public:
    virtual ~IVideoSink() = default;
    
    // Sink-specific methods
    virtual bool ProcessFrame(VideoFramePtr frame) = 0;
    virtual FrameInfo GetInputFormat() const = 0;
    virtual bool SetInputFormat(const FrameInfo& format) = 0;
    
    // Buffer management
    virtual size_t GetQueueDepth() const = 0;
    virtual size_t GetMaxQueueDepth() const = 0;
    virtual bool SetMaxQueueDepth(size_t depth) = 0;
    
    // Synchronization
    virtual bool IsBlocking() const = 0;
    virtual void SetBlocking(bool blocking) = 0;
    
    // Sink-specific capabilities
    virtual bool SupportsFormat(PixelFormat format) const = 0;
    virtual std::vector<PixelFormat> GetSupportedFormats() const = 0;
};

/**
 * @brief Base video sink implementation with queue management
 */
class BaseVideoSink : public BaseBlock, public IVideoSink {
public:
    BaseVideoSink(const std::string& name, const std::string& type);
    virtual ~BaseVideoSink() = default;
    
    // IVideoSink implementation
    bool ProcessFrame(VideoFramePtr frame) override;
    FrameInfo GetInputFormat() const override { return input_format_; }
    bool SetInputFormat(const FrameInfo& format) override;
    
    size_t GetQueueDepth() const override;
    size_t GetMaxQueueDepth() const override { return max_queue_depth_; }
    bool SetMaxQueueDepth(size_t depth) override;
    
    bool IsBlocking() const override { return is_blocking_; }
    void SetBlocking(bool blocking) override { is_blocking_ = blocking; }
    
    // Common functionality
    bool Initialize(const BlockParams& params) override;
    bool Start() override;
    bool Stop() override;
    bool Shutdown() override;
    
protected:
    // Pure virtual method for derived classes to implement
    virtual bool ProcessFrameImpl(VideoFramePtr frame) = 0;
    
    // Configuration
    FrameInfo input_format_;
    size_t max_queue_depth_{10};
    bool is_blocking_{true};
    
private:
    // Worker thread for processing frames
    void WorkerThread();
    
    // Frame queue and synchronization
    std::queue<VideoFramePtr> frame_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::condition_variable queue_not_full_condition_;
    
    // Thread management
    std::thread worker_thread_;
    std::atomic<bool> stop_worker_{false};
};

} // namespace video_pipeline