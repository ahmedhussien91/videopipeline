#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <cmath>

namespace video_pipeline {

/**
 * @brief High-precision timer for performance measurement
 */
class Timer {
public:
    Timer();
    
    // Reset timer to current time
    void Reset();
    
    // Get elapsed time since construction or last reset
    double GetElapsedSeconds() const;
    double GetElapsedMilliseconds() const;
    double GetElapsedMicroseconds() const;
    
    // Get current timestamp
    static uint64_t GetCurrentTimestampUs();
    static uint64_t GetCurrentTimestampMs();
    
    // Convert between time units
    static double MicrosecondsToSeconds(uint64_t us);
    static double MillisecondsToSeconds(uint64_t ms);
    static uint64_t SecondsToMicroseconds(double seconds);
    static uint64_t SecondsToMilliseconds(double seconds);
    
    // Formatting
    std::string ToString() const;
    static std::string FormatDuration(double seconds);

private:
    std::chrono::steady_clock::time_point start_time_;
};

/**
 * @brief Frame rate calculator
 */
class FrameRateCalculator {
public:
    explicit FrameRateCalculator(size_t window_size = 30);
    
    // Add a frame timestamp
    void AddFrame(uint64_t timestamp_us = 0);
    
    // Get current frame rate
    double GetFrameRate() const;
    double GetAverageFrameRate() const;
    
    // Statistics
    size_t GetFrameCount() const { return frame_count_; }
    uint64_t GetTotalTime() const;
    
    // Reset
    void Reset();

private:
    size_t window_size_;
    size_t frame_count_;
    uint64_t first_frame_time_;
    uint64_t last_frame_time_;
    
    // Circular buffer for recent frame times
    std::vector<uint64_t> frame_times_;
    size_t frame_index_;
};

/**
 * @brief Latency tracker
 */
class LatencyTracker {
public:
    explicit LatencyTracker(size_t history_size = 100);
    
    // Record a latency measurement
    void RecordLatency(double latency_ms);
    
    // Statistics
    double GetAverageLatency() const;
    double GetMinLatency() const;
    double GetMaxLatency() const;
    double GetLastLatency() const;
    
    // Percentiles
    double GetPercentile(double percentile) const;
    
    // Reset
    void Reset();
    
    // Formatting
    std::string ToString() const;

private:
    size_t history_size_;
    std::vector<double> latencies_;
    size_t index_;
    size_t count_;
    
    mutable std::vector<double> sorted_cache_;
    mutable bool cache_valid_;
    
    void InvalidateCache();
    void UpdateCache() const;
};

} // namespace video_pipeline