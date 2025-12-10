#include "video_pipeline/video_sink.h"
#include "video_pipeline/logger.h"
#include <thread>

namespace video_pipeline {

BaseVideoSink::BaseVideoSink(const std::string& name, const std::string& type)
    : BaseBlock(name, type) {
    // Initialize default input format
    input_format_.width = 640;
    input_format_.height = 480;
    input_format_.pixel_format = PixelFormat::RGB24;
    input_format_.stride = input_format_.width * 3;
}

bool BaseVideoSink::ProcessFrame(VideoFramePtr frame) {
    if (!frame) {
        VP_LOG_WARNING_F("VideoSink {} received null frame", BaseBlock::GetName());
        return false;
    }
    
    if (BaseBlock::GetState() != BlockState::RUNNING) {
        VP_LOG_WARNING_F("VideoSink {} not running, dropping frame", BaseBlock::GetName());
        return false;
    }
    
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // Check queue depth
    if (frame_queue_.size() >= max_queue_depth_) {
        if (!is_blocking_) {
            // Drop oldest frame
            frame_queue_.pop();
            UpdateStats(false, 0, true);  // Count as dropped
            VP_LOG_DEBUG_F("VideoSink {} queue full, dropping oldest frame", BaseBlock::GetName());
        } else {
            // Wait for space in queue
            queue_not_full_condition_.wait(lock, [this] {
                return frame_queue_.size() < max_queue_depth_ || stop_worker_.load();
            });
            
            if (stop_worker_.load()) {
                return false;
            }
        }
    }
    
    // Add frame to queue
    frame_queue_.push(frame);
    stats_.queue_depth = frame_queue_.size();
    
    // Notify worker thread
    queue_condition_.notify_one();
    
    return true;
}

bool BaseVideoSink::SetInputFormat(const FrameInfo& format) {
    if (BaseBlock::GetState() == BlockState::RUNNING) {
        SetError("Cannot change input format while running");
        return false;
    }
    
    input_format_ = format;
    VP_LOG_INFO_F("VideoSink {} input format set to: {}", BaseBlock::GetName(), format.ToString());
    return true;
}

size_t BaseVideoSink::GetQueueDepth() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return frame_queue_.size();
}

bool BaseVideoSink::SetMaxQueueDepth(size_t depth) {
    if (depth == 0 || depth > 1000) {
        SetError("Invalid queue depth: " + std::to_string(depth));
        return false;
    }
    
    max_queue_depth_ = depth;
    return true;
}

bool BaseVideoSink::Initialize(const BlockParams& params) {
    // Parse common parameters
    auto queue_depth_str = BaseBlock::GetParameter("queue_depth");
    auto blocking_str = BaseBlock::GetParameter("blocking");
    
    if (!queue_depth_str.empty()) {
        SetMaxQueueDepth(std::stoul(queue_depth_str));
    }
    
    if (!blocking_str.empty()) {
        SetBlocking(blocking_str == "true" || blocking_str == "1");
    }
    
    SetState(BlockState::INITIALIZED);
    VP_LOG_INFO_F("VideoSink {} initialized, queue_depth={}, blocking={}", 
                  BaseBlock::GetName(), max_queue_depth_, is_blocking_);
    return true;
}

bool BaseVideoSink::Start() {
    if (BaseBlock::GetState() != BlockState::INITIALIZED && BaseBlock::GetState() != BlockState::STOPPED) {
        SetError("Cannot start VideoSink from state: " + BaseBlock::GetStateString());
        return false;
    }
    
    SetState(BlockState::STARTING);
    
    // Start worker thread
    stop_worker_.store(false);
    worker_thread_ = std::thread(&BaseVideoSink::WorkerThread, this);
    
    SetState(BlockState::RUNNING);
    VP_LOG_INFO_F("VideoSink {} started", BaseBlock::GetName());
    return true;
}

bool BaseVideoSink::Stop() {
    if (BaseBlock::GetState() != BlockState::RUNNING) {
        return true;
    }
    
    SetState(BlockState::STOPPING);
    VP_LOG_INFO_F("VideoSink {} stopping", BaseBlock::GetName());
    
    // Signal worker thread to stop
    stop_worker_.store(true);
    queue_condition_.notify_all();
    queue_not_full_condition_.notify_all();
    
    // Wait for worker thread to finish
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    // Clear remaining frames
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!frame_queue_.empty()) {
        frame_queue_.pop();
    }
    
    SetState(BlockState::STOPPED);
    VP_LOG_INFO_F("VideoSink {} stopped", BaseBlock::GetName());
    return true;
}

bool BaseVideoSink::Shutdown() {
    Stop();
    return true;
}

void BaseVideoSink::WorkerThread() {
    VP_LOG_DEBUG_F("VideoSink {} worker thread started", BaseBlock::GetName());
    
    while (!stop_worker_.load()) {
        VideoFramePtr frame;
        
        // Get frame from queue
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this] {
                return !frame_queue_.empty() || stop_worker_.load();
            });
            
            if (stop_worker_.load() && frame_queue_.empty()) {
                break;
            }
            
            if (!frame_queue_.empty()) {
                frame = frame_queue_.front();
                frame_queue_.pop();
                stats_.queue_depth = frame_queue_.size();
                
                // Notify that queue has space
                queue_not_full_condition_.notify_one();
            }
        }
        
        // Process frame
        if (frame) {
            try {
                bool success = ProcessFrameImpl(frame);
                UpdateStats(success, frame->GetSize(), !success);
                
                if (!success) {
                    VP_LOG_WARNING_F("VideoSink {} failed to process frame", BaseBlock::GetName());
                }
            } catch (const std::exception& e) {
                VP_LOG_ERROR_F("VideoSink {} exception in ProcessFrameImpl: {}", BaseBlock::GetName(), e.what());
                UpdateStats(false, 0, true);
            }
        }
    }
    
    VP_LOG_DEBUG_F("VideoSink {} worker thread stopped", BaseBlock::GetName());
}

} // namespace video_pipeline