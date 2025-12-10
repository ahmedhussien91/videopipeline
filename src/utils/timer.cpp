#include "video_pipeline/timer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace video_pipeline {

// Timer implementation
Timer::Timer() {
    Reset();
}

void Timer::Reset() {
    start_time_ = std::chrono::steady_clock::now();
}

double Timer::GetElapsedSeconds() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
    return duration.count() / 1000000.0;
}

double Timer::GetElapsedMilliseconds() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
    return duration.count() / 1000.0;
}

double Timer::GetElapsedMicroseconds() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
    return static_cast<double>(duration.count());
}

uint64_t Timer::GetCurrentTimestampUs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
}

uint64_t Timer::GetCurrentTimestampMs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

double Timer::MicrosecondsToSeconds(uint64_t us) {
    return us / 1000000.0;
}

double Timer::MillisecondsToSeconds(uint64_t ms) {
    return ms / 1000.0;
}

uint64_t Timer::SecondsToMicroseconds(double seconds) {
    return static_cast<uint64_t>(seconds * 1000000.0);
}

uint64_t Timer::SecondsToMilliseconds(double seconds) {
    return static_cast<uint64_t>(seconds * 1000.0);
}

std::string Timer::ToString() const {
    return FormatDuration(GetElapsedSeconds());
}

std::string Timer::FormatDuration(double seconds) {
    std::ostringstream oss;
    
    if (seconds < 0.001) {
        oss << std::fixed << std::setprecision(1) << (seconds * 1000000) << "us";
    } else if (seconds < 1.0) {
        oss << std::fixed << std::setprecision(1) << (seconds * 1000) << "ms";
    } else if (seconds < 60.0) {
        oss << std::fixed << std::setprecision(2) << seconds << "s";
    } else {
        int minutes = static_cast<int>(seconds / 60);
        double remaining_seconds = seconds - (minutes * 60);
        oss << minutes << "m" << std::fixed << std::setprecision(1) << remaining_seconds << "s";
    }
    
    return oss.str();
}

// FrameRateCalculator implementation
FrameRateCalculator::FrameRateCalculator(size_t window_size)
    : window_size_(window_size)
    , frame_count_(0)
    , first_frame_time_(0)
    , last_frame_time_(0)
    , frame_index_(0) {
    frame_times_.resize(window_size_);
}

void FrameRateCalculator::AddFrame(uint64_t timestamp_us) {
    if (timestamp_us == 0) {
        timestamp_us = Timer::GetCurrentTimestampUs();
    }
    
    if (frame_count_ == 0) {
        first_frame_time_ = timestamp_us;
    }
    
    last_frame_time_ = timestamp_us;
    frame_times_[frame_index_] = timestamp_us;
    frame_index_ = (frame_index_ + 1) % window_size_;
    frame_count_++;
}

double FrameRateCalculator::GetFrameRate() const {
    if (frame_count_ < 2) return 0.0;
    
    size_t samples = std::min(frame_count_, window_size_);
    if (samples < 2) return 0.0;
    
    // Find oldest and newest timestamps in the window
    uint64_t oldest = frame_times_[0];
    uint64_t newest = frame_times_[0];
    
    for (size_t i = 1; i < samples; ++i) {
        oldest = std::min(oldest, frame_times_[i]);
        newest = std::max(newest, frame_times_[i]);
    }
    
    uint64_t duration_us = newest - oldest;
    if (duration_us == 0) return 0.0;
    
    return (samples - 1) * 1000000.0 / duration_us;
}

double FrameRateCalculator::GetAverageFrameRate() const {
    if (frame_count_ < 2) return 0.0;
    
    uint64_t total_time_us = last_frame_time_ - first_frame_time_;
    if (total_time_us == 0) return 0.0;
    
    return (frame_count_ - 1) * 1000000.0 / total_time_us;
}

uint64_t FrameRateCalculator::GetTotalTime() const {
    if (frame_count_ < 2) return 0;
    return last_frame_time_ - first_frame_time_;
}

void FrameRateCalculator::Reset() {
    frame_count_ = 0;
    first_frame_time_ = 0;
    last_frame_time_ = 0;
    frame_index_ = 0;
    std::fill(frame_times_.begin(), frame_times_.end(), 0);
}

// LatencyTracker implementation
LatencyTracker::LatencyTracker(size_t history_size)
    : history_size_(history_size)
    , index_(0)
    , count_(0)
    , cache_valid_(false) {
    latencies_.resize(history_size_);
}

void LatencyTracker::RecordLatency(double latency_ms) {
    latencies_[index_] = latency_ms;
    index_ = (index_ + 1) % history_size_;
    
    if (count_ < history_size_) {
        count_++;
    }
    
    InvalidateCache();
}

double LatencyTracker::GetAverageLatency() const {
    if (count_ == 0) return 0.0;
    
    double sum = 0.0;
    for (size_t i = 0; i < count_; ++i) {
        sum += latencies_[i];
    }
    
    return sum / count_;
}

double LatencyTracker::GetMinLatency() const {
    if (count_ == 0) return 0.0;
    
    UpdateCache();
    return sorted_cache_[0];
}

double LatencyTracker::GetMaxLatency() const {
    if (count_ == 0) return 0.0;
    
    UpdateCache();
    return sorted_cache_[count_ - 1];
}

double LatencyTracker::GetLastLatency() const {
    if (count_ == 0) return 0.0;
    
    size_t last_index = (index_ + history_size_ - 1) % history_size_;
    return latencies_[last_index];
}

double LatencyTracker::GetPercentile(double percentile) const {
    if (count_ == 0) return 0.0;
    
    percentile = std::max(0.0, std::min(100.0, percentile));
    
    UpdateCache();
    
    double index = (percentile / 100.0) * (count_ - 1);
    size_t lower_index = static_cast<size_t>(std::floor(index));
    size_t upper_index = static_cast<size_t>(std::ceil(index));
    
    if (lower_index == upper_index) {
        return sorted_cache_[lower_index];
    }
    
    double weight = index - lower_index;
    return sorted_cache_[lower_index] * (1.0 - weight) + sorted_cache_[upper_index] * weight;
}

void LatencyTracker::Reset() {
    count_ = 0;
    index_ = 0;
    InvalidateCache();
}

std::string LatencyTracker::ToString() const {
    if (count_ == 0) {
        return "No latency data";
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "Latency: avg=" << GetAverageLatency() << "ms";
    oss << ", min=" << GetMinLatency() << "ms";
    oss << ", max=" << GetMaxLatency() << "ms";
    oss << ", p95=" << GetPercentile(95.0) << "ms";
    oss << " (" << count_ << " samples)";
    
    return oss.str();
}

void LatencyTracker::InvalidateCache() {
    cache_valid_ = false;
}

void LatencyTracker::UpdateCache() const {
    if (cache_valid_) return;
    
    sorted_cache_.clear();
    sorted_cache_.reserve(count_);
    
    for (size_t i = 0; i < count_; ++i) {
        sorted_cache_.push_back(latencies_[i]);
    }
    
    std::sort(sorted_cache_.begin(), sorted_cache_.end());
    cache_valid_ = true;
}

} // namespace video_pipeline