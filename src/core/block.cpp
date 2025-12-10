#include "video_pipeline/block.h"
#include "video_pipeline/timer.h"
#include <sstream>

namespace video_pipeline {

std::string IBlock::GetStateString() const {
    switch (GetState()) {
        case BlockState::UNINITIALIZED: return "UNINITIALIZED";
        case BlockState::INITIALIZED: return "INITIALIZED";
        case BlockState::STARTING: return "STARTING";
        case BlockState::RUNNING: return "RUNNING";
        case BlockState::STOPPING: return "STOPPING";
        case BlockState::STOPPED: return "STOPPED";
        case BlockState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

BaseBlock::BaseBlock(const std::string& name, const std::string& type)
    : name_(name), type_(type) {
    // Initialize stats
    stats_.last_frame_time = std::chrono::steady_clock::now();
}

std::string BaseBlock::GetStateString() const {
    return IBlock::GetStateString();
}

BlockStats BaseBlock::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - stats_.last_frame_time).count();
    
    BlockStats stats = stats_;
    if (duration > 0 && stats_.frames_processed > 0) {
        stats.avg_fps = static_cast<double>(stats_.frames_processed) / duration;
    }
    
    return stats;
}

void BaseBlock::ResetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = BlockStats{};
    stats_.last_frame_time = std::chrono::steady_clock::now();
}

std::string BaseBlock::GetLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

BlockParams BaseBlock::GetConfiguration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return params_;
}

bool BaseBlock::SetParameter(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    params_[key] = value;
    return true;
}

std::string BaseBlock::GetParameter(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = params_.find(key);
    return (it != params_.end()) ? it->second : "";
}

void BaseBlock::SetState(BlockState state) {
    state_.store(state);
}

void BaseBlock::SetError(const std::string& error) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_error_ = error;
    }
    
    SetState(BlockState::ERROR);
    
    if (error_callback_) {
        error_callback_(this, error);
    }
}

void BaseBlock::UpdateStats(bool frame_processed, size_t bytes, bool dropped) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    if (frame_processed) {
        stats_.frames_processed++;
        stats_.bytes_processed += bytes;
        
        // Calculate latency if we have timing information
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - stats_.last_frame_time);
        if (stats_.frames_processed > 1) {
            double latency_ms = elapsed.count() / 1000.0;
            // Simple moving average for latency
            stats_.avg_latency_ms = (stats_.avg_latency_ms * 0.9) + (latency_ms * 0.1);
        }
        
        stats_.last_frame_time = now;
    }
    
    if (dropped) {
        stats_.frames_dropped++;
    }
}

} // namespace video_pipeline