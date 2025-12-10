# Performance Tuning Guide

Optimize your video pipeline for maximum performance, minimal latency, and efficient resource utilization.

## Performance Metrics

### Key Performance Indicators

| Metric | Description | Target | Measurement |
|--------|-------------|--------|-------------|
| **Frame Rate** | Frames processed per second | 30+ FPS | `BlockStats.avg_fps` |
| **Latency** | End-to-end processing delay | <50ms | `BlockStats.avg_latency_ms` |
| **Throughput** | Data processed per second | 25+ MB/s | `bytes_processed / time` |
| **Frame Drops** | Percentage of dropped frames | <1% | `frames_dropped / frames_total` |
| **CPU Usage** | Processor utilization | <80% | System monitoring |
| **Memory Usage** | RAM consumption | <512MB | Process monitoring |

### Baseline Performance

Current framework performance on a modern Linux system:

```
Test Configuration: 640x480 RGB24, TestPatternSource → ConsoleSink
Hardware: Intel Core i7, 16GB RAM, Ubuntu 22.04
Results:
  - Frame Rate: 28.7 FPS sustained
  - Latency: ~35ms average
  - Throughput: 25.2 MB/s
  - Frame Drops: 0%
  - CPU Usage: ~8% single core
  - Memory Usage: ~45MB
```

## CPU Optimization

### Thread Management

#### 1. CPU Affinity

Bind processing threads to specific CPU cores for consistent performance:

```cpp
// Set CPU affinity for source thread (Linux)
void OptimizedVideoSource::StartGeneratorThread() {
    generator_thread_ = std::thread([this]() {
        // Bind to CPU core 1
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(1, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        
        GeneratorThread();
    });
}
```

#### 2. Thread Priorities

Set appropriate thread priorities for real-time performance:

```cpp
void SetRealtimePriority() {
    #ifdef __linux__
    struct sched_param param;
    param.sched_priority = 50; // 1-99, higher = more priority
    
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        VP_LOG_WARNING("Failed to set real-time priority");
        // Fallback to nice value
        nice(-10); // Higher priority than normal processes
    }
    #endif
}
```

#### 3. NUMA Awareness

For multi-socket systems, ensure memory and processing stay on the same NUMA node:

```cpp
void OptimizeNUMA() {
    #ifdef __linux__
    // Get current CPU
    int cpu = sched_getcpu();
    
    // Get NUMA node for this CPU
    int node = numa_node_of_cpu(cpu);
    
    // Bind memory allocation to this node
    numa_set_preferred(node);
    
    VP_LOG_INFO_F("Bound to NUMA node {} (CPU {})", node, cpu);
    #endif
}
```

### SIMD Optimizations

Use vectorized operations for frame processing:

```cpp
void ProcessFrameSIMD(const uint8_t* input, uint8_t* output, size_t size) {
    #ifdef __AVX2__
    // Process 32 bytes at a time with AVX2
    const __m256i* input_vec = reinterpret_cast<const __m256i*>(input);
    __m256i* output_vec = reinterpret_cast<__m256i*>(output);
    
    size_t vec_count = size / 32;
    
    for (size_t i = 0; i < vec_count; i++) {
        __m256i data = _mm256_loadu_si256(&input_vec[i]);
        
        // Example: Invert all bytes
        __m256i inverted = _mm256_xor_si256(data, _mm256_set1_epi8(0xFF));
        
        _mm256_storeu_si256(&output_vec[i], inverted);
    }
    
    // Process remaining bytes
    size_t remaining_start = vec_count * 32;
    for (size_t i = remaining_start; i < size; i++) {
        output[i] = ~input[i];
    }
    #else
    // Fallback to scalar processing
    for (size_t i = 0; i < size; i++) {
        output[i] = ~input[i];
    }
    #endif
}
```

## Memory Optimization

### Buffer Pool Management

Pre-allocate buffers to avoid malloc/free overhead:

```cpp
class BufferPool {
public:
    BufferPool(size_t pool_size, size_t buffer_size) {
        for (size_t i = 0; i < pool_size; ++i) {
            // Allocate aligned memory for SIMD
            void* aligned_ptr = nullptr;
            if (posix_memalign(&aligned_ptr, 32, buffer_size) == 0) {
                available_buffers_.emplace(aligned_ptr, buffer_size);
            }
        }
    }
    
    ~BufferPool() {
        while (!available_buffers_.empty()) {
            free(available_buffers_.front().first);
            available_buffers_.pop();
        }
    }
    
    std::pair<void*, size_t> GetBuffer() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (available_buffers_.empty()) {
            // Pool exhausted, allocate new buffer
            void* ptr = nullptr;
            posix_memalign(&ptr, 32, buffer_size_);
            return {ptr, buffer_size_};
        }
        
        auto buffer = available_buffers_.front();
        available_buffers_.pop();
        return buffer;
    }
    
    void ReturnBuffer(void* ptr, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_buffers_.emplace(ptr, size);
    }
    
private:
    std::queue<std::pair<void*, size_t>> available_buffers_;
    std::mutex mutex_;
    size_t buffer_size_;
};
```

### Memory-Mapped Files

For large video files, use memory mapping instead of reading into buffers:

```cpp
class MappedFileSource : public BaseVideoSource {
public:
    bool Initialize(const BlockParams& params) override {
        auto file_path = GetParameter("path");
        
        // Open file
        fd_ = open(file_path.c_str(), O_RDONLY);
        if (fd_ == -1) {
            return false;
        }
        
        // Get file size
        struct stat st;
        fstat(fd_, &st);
        file_size_ = st.st_size;
        
        // Memory map the file
        mapped_data_ = mmap(nullptr, file_size_, PROT_READ, MAP_SHARED, fd_, 0);
        if (mapped_data_ == MAP_FAILED) {
            close(fd_);
            return false;
        }
        
        // Advise kernel about access pattern
        madvise(mapped_data_, file_size_, MADV_SEQUENTIAL);
        
        return true;
    }
    
private:
    int fd_;
    void* mapped_data_;
    size_t file_size_;
};
```

### Cache Optimization

Optimize memory access patterns for CPU cache efficiency:

```cpp
// Cache-friendly row-major processing
void ProcessFrameCacheFriendly(uint8_t* data, int width, int height, int channels) {
    // Process in blocks that fit in L1 cache (32KB typical)
    const int block_size = 128; // Adjust based on cache size
    
    for (int y = 0; y < height; y += block_size) {
        for (int x = 0; x < width; x += block_size) {
            int max_y = std::min(y + block_size, height);
            int max_x = std::min(x + block_size, width);
            
            // Process block
            for (int by = y; by < max_y; by++) {
                for (int bx = x; bx < max_x; bx++) {
                    int offset = (by * width + bx) * channels;
                    // Process pixel at data[offset]
                    ProcessPixel(&data[offset], channels);
                }
            }
        }
    }
}
```

## Queue Optimization

### Lock-Free Queues

Replace mutex-based queues with lock-free implementations for high-performance scenarios:

```cpp
#include <boost/lockfree/spsc_queue.hpp>

class LockFreeVideoSink : public BaseVideoSink {
private:
    // Single producer, single consumer queue
    boost::lockfree::spsc_queue<VideoFramePtr> frame_queue_{1024};
    
public:
    bool ProcessFrame(VideoFramePtr frame) override {
        // Non-blocking push
        if (!frame_queue_.push(frame)) {
            // Queue full, drop frame
            UpdateStats(false, 0, true);
            return false;
        }
        
        return true;
    }
    
private:
    void WorkerThread() {
        VideoFramePtr frame;
        while (!stop_worker_.load()) {
            // Non-blocking pop
            if (frame_queue_.pop(frame)) {
                ProcessFrameImpl(frame);
            } else {
                // No frames available, brief sleep
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
};
```

### Adaptive Queue Sizing

Dynamically adjust queue sizes based on performance metrics:

```cpp
class AdaptiveVideoSink : public BaseVideoSink {
private:
    void UpdateQueueSize() {
        auto stats = GetStats();
        
        // If latency is too high, reduce queue size
        if (stats.avg_latency_ms > target_latency_ms_) {
            max_queue_depth_ = std::max(min_queue_depth_, max_queue_depth_ - 1);
            VP_LOG_DEBUG_F("Reduced queue depth to {}", max_queue_depth_);
        }
        // If we're dropping frames, increase queue size
        else if (stats.frames_dropped > previous_drops_) {
            max_queue_depth_ = std::min(max_queue_depth_, max_queue_depth_ + 1);
            VP_LOG_DEBUG_F("Increased queue depth to {}", max_queue_depth_);
        }
        
        previous_drops_ = stats.frames_dropped;
    }
    
private:
    double target_latency_ms_ = 50.0;
    size_t min_queue_depth_ = 2;
    size_t max_queue_depth_ = 10;
    uint64_t previous_drops_ = 0;
};
```

## I/O Optimization

### Asynchronous I/O

Use asynchronous I/O for file operations to avoid blocking:

```cpp
#include <aio.h>

class AsyncFileSink : public BaseVideoSink {
public:
    bool ProcessFrameImpl(VideoFramePtr frame) override {
        // Prepare AIO control block
        struct aiocb* aio = new struct aiocb{};
        aio->aio_fildes = output_fd_;
        aio->aio_buf = frame->GetData();
        aio->aio_nbytes = frame->GetSize();
        aio->aio_offset = write_offset_;
        
        // Store frame reference to keep buffer alive
        pending_writes_[aio] = frame;
        
        // Start async write
        if (aio_write(aio) != 0) {
            delete aio;
            pending_writes_.erase(aio);
            return false;
        }
        
        write_offset_ += frame->GetSize();
        
        // Check for completed writes
        CheckCompletedWrites();
        
        return true;
    }
    
private:
    void CheckCompletedWrites() {
        auto it = pending_writes_.begin();
        while (it != pending_writes_.end()) {
            struct aiocb* aio = it->first;
            
            int status = aio_error(aio);
            if (status == 0) {
                // Write completed successfully
                delete aio;
                it = pending_writes_.erase(it);
            } else if (status != EINPROGRESS) {
                // Write failed
                VP_LOG_ERROR_F("Async write failed: {}", strerror(status));
                delete aio;
                it = pending_writes_.erase(it);
            } else {
                // Still in progress
                ++it;
            }
        }
    }
    
private:
    int output_fd_;
    off_t write_offset_ = 0;
    std::map<struct aiocb*, VideoFramePtr> pending_writes_;
};
```

### Direct I/O

Bypass OS page cache for large sequential I/O:

```cpp
class DirectIOFileSink : public BaseVideoSink {
public:
    bool Initialize(const BlockParams& params) override {
        auto path = GetParameter("path");
        
        // Open with direct I/O flag
        output_fd_ = open(path.c_str(), O_WRONLY | O_CREAT | O_DIRECT, 0644);
        if (output_fd_ == -1) {
            SetError("Failed to open file for direct I/O");
            return false;
        }
        
        return BaseVideoSink::Initialize(params);
    }
    
    bool ProcessFrameImpl(VideoFramePtr frame) override {
        // Direct I/O requires aligned buffers and sizes
        size_t size = frame->GetSize();
        size_t aligned_size = (size + 511) & ~511; // Align to 512 bytes
        
        // Use aligned buffer if frame buffer isn't aligned
        void* write_buffer = frame->GetData();
        bool need_copy = false;
        
        if (reinterpret_cast<uintptr_t>(write_buffer) & 511) {
            // Buffer not aligned, need to copy
            if (!aligned_buffer_ || aligned_buffer_size_ < aligned_size) {
                if (aligned_buffer_) {
                    free(aligned_buffer_);
                }
                posix_memalign(&aligned_buffer_, 512, aligned_size);
                aligned_buffer_size_ = aligned_size;
            }
            
            memcpy(aligned_buffer_, write_buffer, size);
            write_buffer = aligned_buffer_;
            need_copy = true;
        }
        
        // Write with proper size alignment
        ssize_t written = write(output_fd_, write_buffer, aligned_size);
        
        return written == static_cast<ssize_t>(aligned_size);
    }
    
private:
    int output_fd_;
    void* aligned_buffer_ = nullptr;
    size_t aligned_buffer_size_ = 0;
};
```

## Network Optimization

### Zero-Copy Networking

Use sendfile() or splice() for zero-copy network transmission:

```cpp
class ZeroCopyNetworkSink : public BaseVideoSink {
public:
    bool ProcessFrameImpl(VideoFramePtr frame) override {
        // Write frame to temporary file
        int temp_fd = CreateTempFile();
        write(temp_fd, frame->GetData(), frame->GetSize());
        
        // Send file directly to network socket
        off_t offset = 0;
        ssize_t sent = sendfile(socket_fd_, temp_fd, &offset, frame->GetSize());
        
        close(temp_fd);
        
        return sent == static_cast<ssize_t>(frame->GetSize());
    }
    
private:
    int CreateTempFile() {
        char template_name[] = "/tmp/frame_XXXXXX";
        return mkstemp(template_name);
    }
    
    int socket_fd_;
};
```

### UDP Bulk Transfer

Optimize UDP for high-throughput video streaming:

```cpp
class OptimizedUDPSink : public BaseVideoSink {
public:
    bool Initialize(const BlockParams& params) override {
        // Create UDP socket
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        
        // Increase socket buffer sizes
        int buffer_size = 1024 * 1024; // 1MB
        setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
        
        // Enable UDP checksums for performance
        int no_checksum = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_NO_CHECK, &no_checksum, sizeof(no_checksum));
        
        return true;
    }
    
    bool ProcessFrameImpl(VideoFramePtr frame) override {
        // Fragment large frames into UDP packets
        const size_t max_udp_size = 1400; // Avoid fragmentation
        const uint8_t* data = static_cast<const uint8_t*>(frame->GetData());
        size_t remaining = frame->GetSize();
        
        while (remaining > 0) {
            size_t chunk_size = std::min(remaining, max_udp_size);
            
            ssize_t sent = sendto(socket_fd_, data, chunk_size, MSG_DONTWAIT,
                                  reinterpret_cast<sockaddr*>(&dest_addr_), sizeof(dest_addr_));
            
            if (sent == -1 && errno == EWOULDBLOCK) {
                // Socket buffer full, drop frame
                return false;
            }
            
            data += chunk_size;
            remaining -= chunk_size;
        }
        
        return true;
    }
    
private:
    int socket_fd_;
    struct sockaddr_in dest_addr_;
};
```

## Platform-Specific Optimizations

### Linux Optimizations

```cpp
// Enable transparent huge pages
void EnableHugePages() {
    #ifdef __linux__
    system("echo madvise > /sys/kernel/mm/transparent_hugepage/enabled");
    #endif
}

// Set I/O scheduler for better video performance
void OptimizeIOScheduler() {
    #ifdef __linux__
    // Set deadline scheduler for predictable I/O
    system("echo deadline > /sys/block/sda/queue/scheduler");
    
    // Increase read-ahead for sequential access
    system("echo 1024 > /sys/block/sda/queue/read_ahead_kb");
    #endif
}

// Disable CPU frequency scaling for consistent performance
void DisableCPUScaling() {
    #ifdef __linux__
    system("echo performance > /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor");
    #endif
}
```

### GPU Acceleration

Use OpenGL or CUDA for hardware-accelerated processing:

```cpp
#ifdef USE_CUDA
#include <cuda_runtime.h>

class CUDAVideoProcessor : public BaseVideoSink {
public:
    bool Initialize(const BlockParams& params) override {
        // Initialize CUDA
        cudaError_t error = cudaSetDevice(0);
        if (error != cudaSuccess) {
            SetError("Failed to initialize CUDA");
            return false;
        }
        
        // Allocate GPU memory
        size_t buffer_size = 1920 * 1080 * 4; // Max frame size
        cudaMalloc(&gpu_buffer_, buffer_size);
        
        return true;
    }
    
    bool ProcessFrameImpl(VideoFramePtr frame) override {
        // Copy frame to GPU
        cudaMemcpy(gpu_buffer_, frame->GetData(), frame->GetSize(), cudaMemcpyHostToDevice);
        
        // Launch CUDA kernel for processing
        dim3 block_size(16, 16);
        dim3 grid_size((width_ + 15) / 16, (height_ + 15) / 16);
        
        ProcessFrameKernel<<<grid_size, block_size>>>(
            static_cast<uint8_t*>(gpu_buffer_), width_, height_);
        
        // Copy result back to CPU
        cudaMemcpy(frame->GetData(), gpu_buffer_, frame->GetSize(), cudaMemcpyDeviceToHost);
        
        return true;
    }
    
private:
    void* gpu_buffer_;
    int width_, height_;
};
```

## Performance Monitoring

### Real-Time Profiling

```cpp
class PerformanceProfiler {
public:
    void StartFrame() {
        frame_start_ = std::chrono::high_resolution_clock::now();
    }
    
    void EndFrame() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - frame_start_).count();
        
        frame_times_.push_back(duration);
        
        // Keep only recent measurements
        if (frame_times_.size() > 1000) {
            frame_times_.pop_front();
        }
        
        // Update statistics
        UpdateStatistics();
    }
    
    void PrintStatistics() {
        if (frame_times_.empty()) return;
        
        auto sorted_times = frame_times_;
        std::sort(sorted_times.begin(), sorted_times.end());
        
        double avg = std::accumulate(sorted_times.begin(), sorted_times.end(), 0.0) 
                    / sorted_times.size();
        double p50 = sorted_times[sorted_times.size() / 2];
        double p95 = sorted_times[sorted_times.size() * 95 / 100];
        double p99 = sorted_times[sorted_times.size() * 99 / 100];
        
        VP_LOG_INFO_F("Frame timing - Avg: {:.1f}us, P50: {:.1f}us, P95: {:.1f}us, P99: {:.1f}us",
                      avg, p50, p95, p99);
    }
    
private:
    std::chrono::high_resolution_clock::time_point frame_start_;
    std::deque<double> frame_times_;
};
```

### System Resource Monitoring

```cpp
class ResourceMonitor {
public:
    void UpdateMetrics() {
        #ifdef __linux__
        // Read CPU usage
        std::ifstream stat("/proc/stat");
        std::string line;
        std::getline(stat, line);
        // Parse CPU statistics...
        
        // Read memory usage  
        std::ifstream meminfo("/proc/meminfo");
        // Parse memory statistics...
        
        // Read process-specific metrics
        std::ifstream status("/proc/self/status");
        // Parse process statistics...
        #endif
    }
    
    void LogMetrics() {
        VP_LOG_INFO_F("CPU: {:.1f}%, Memory: {:.1f}MB, Threads: {}", 
                      cpu_usage_, memory_mb_, thread_count_);
    }
    
private:
    double cpu_usage_ = 0.0;
    double memory_mb_ = 0.0;
    int thread_count_ = 0;
};
```

## Configuration for Performance

### High-Performance Pipeline Configuration

```yaml
pipeline:
  name: "high_performance"
  
# Performance settings
performance:
  source_thread_priority: "realtime"
  sink_thread_priority: "high"
  cpu_affinity:
    sources: [0, 1]
    sinks: [2, 3]
  buffer_alignment: 32
  zero_copy: true
  
blocks:
  - name: "source"
    type: "OptimizedTestPatternSource"
    parameters:
      width: "1920"
      height: "1080"
      fps: "60"
      format: "RGB24"
      buffer_pool_size: "20"
      
  - name: "sink"
    type: "OptimizedFileSink"
    parameters:
      path: "/dev/shm/output.raw"  # Use RAM disk
      format: "raw"
      direct_io: "true"
      async_writes: "true"
      queue_depth: "4"  # Small queue for low latency
      
connections:
  - source: "source"
    sink: "sink"

# Monitoring
monitoring:
  enable_profiling: true
  profile_interval: 1000
  enable_resource_monitoring: true
```

This comprehensive performance guide covers all major optimization techniques. For specific use cases, combine these techniques based on your performance requirements and system constraints.