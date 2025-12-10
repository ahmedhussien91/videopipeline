# Architecture Overview

This document describes the architecture and design principles of the Generic Video Pipeline Framework.

## Design Philosophy

The framework is built on several key principles:

1. **Modularity**: Clean separation between core framework and block implementations
2. **Performance**: Zero-copy buffers, efficient threading, minimal overhead
3. **Extensibility**: Easy to add new blocks, platforms, and configuration formats
4. **Type Safety**: Strong typing with virtual inheritance to prevent diamond inheritance issues
5. **Resource Management**: RAII principles with automatic cleanup and error recovery

## Core Architecture

### High-Level Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Layer                            │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │  pipeline_cli   │  │ simple_example  │  │ performance_test│ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────┐
│                      Framework Core                             │
│  ┌──────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │ PipelineManager  │  │  BlockRegistry  │  │ ConfigParser    │ │
│  └──────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────┐
│                      Block Layer                                │
│  ┌──────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │    IBlock        │  │   IVideoSource  │  │   IVideoSink    │ │
│  │   BaseBlock      │  │ BaseVideoSource │  │ BaseVideoSink   │ │
│  └──────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────┐
│                    Utility Layer                                │
│  ┌──────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │     Buffer       │  │     Logger      │  │     Timer       │ │
│  │   SimpleBuffer   │  │   Threading     │  │   Statistics    │ │
│  └──────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Block System

The block system forms the foundation of the framework, implementing a graph-based processing model.

#### IBlock Interface

```cpp
class IBlock {
public:
    // Lifecycle management
    virtual bool Initialize(const BlockParams& params) = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual bool Shutdown() = 0;
    
    // Identity and state
    virtual std::string GetName() const = 0;
    virtual std::string GetType() const = 0;
    virtual BlockState GetState() const = 0;
    
    // Configuration
    virtual bool SetParameter(const std::string& key, const std::string& value) = 0;
    virtual std::string GetParameter(const std::string& key) const = 0;
    
    // Statistics and error handling
    virtual BlockStats GetStats() const = 0;
    virtual std::string GetLastError() const = 0;
};
```

#### Virtual Inheritance Solution

The framework uses virtual inheritance to solve the diamond inheritance problem:

```cpp
class IBlock { /* base interface */ };

class IVideoSource : public virtual IBlock { /* source interface */ };
class IVideoSink : public virtual IBlock { /* sink interface */ };
class BaseBlock : public virtual IBlock { /* base implementation */ };

// Diamond resolved:
class BaseVideoSource : public BaseBlock, public IVideoSource { };
class BaseVideoSink : public BaseBlock, public IVideoSink { };
```

### 2. Buffer Management

High-performance zero-copy buffer system with reference counting.

#### Buffer Architecture

```cpp
class IBuffer {
public:
    virtual void* GetData() = 0;
    virtual size_t GetSize() const = 0;
    virtual PixelFormat GetPixelFormat() const = 0;
    virtual const FrameInfo& GetFrameInfo() const = 0;
    
    // Reference counting for zero-copy
    virtual void AddRef() = 0;
    virtual void Release() = 0;
};

using VideoFramePtr = std::shared_ptr<IBuffer>;
```

#### Memory Management Strategy

1. **Reference Counting**: Automatic memory management with `std::shared_ptr`
2. **Zero-Copy**: Buffers passed by reference, not copied
3. **Pool Allocation**: Pre-allocated buffer pools for performance
4. **Alignment**: Memory-aligned buffers for SIMD optimizations

### 3. Threading Model

Multi-threaded architecture designed for real-time performance.

#### Threading Strategy

- **Source Threads**: Each video source runs in its own thread for frame generation
- **Sink Worker Threads**: Each video sink has a worker thread for frame processing
- **Queue Management**: Lock-free or minimally-locking queues between threads
- **CPU Affinity**: Optional CPU core binding for deterministic performance

#### Synchronization

```cpp
class BaseVideoSink {
private:
    std::queue<VideoFramePtr> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_not_full_condition_;
    std::condition_variable queue_not_empty_condition_;
    std::atomic<bool> stop_worker_{false};
    std::thread worker_thread_;
};
```

### 4. Pipeline Management

The PipelineManager orchestrates the entire processing graph.

#### Pipeline Lifecycle

1. **Configuration**: Load pipeline definition from file or API
2. **Block Creation**: Instantiate blocks using BlockRegistry
3. **Initialization**: Initialize blocks with parameters
4. **Connection**: Connect sources to sinks
5. **Execution**: Start all blocks in dependency order
6. **Monitoring**: Real-time statistics and health monitoring
7. **Shutdown**: Stop blocks gracefully in reverse order

#### Connection Management

```cpp
struct Connection {
    std::string source_name;
    std::string sink_name;
    std::shared_ptr<IVideoSource> source;
    std::shared_ptr<IVideoSink> sink;
};
```

## Data Flow

### Frame Processing Pipeline

```
┌──────────────┐    ┌─────────────┐    ┌──────────────┐
│              │    │             │    │              │
│ VideoSource  │───▶│ FrameQueue  │───▶│  VideoSink   │
│              │    │             │    │              │
│ [Generator   │    │ [Buffering] │    │ [Consumer    │
│  Thread]     │    │             │    │  Thread]     │
└──────────────┘    └─────────────┘    └──────────────┘

        │                   │                   │
        ▼                   ▼                   ▼
   GenerateFrame      QueueManagement    ProcessFrameImpl
   EmitFrame(...)     - Depth Control    - Format Handling
   UpdateStats()      - Drop Policy      - Output Writing
                      - Flow Control      - Statistics
```

### Frame Metadata

Each frame carries comprehensive metadata:

```cpp
struct FrameInfo {
    uint32_t width;
    uint32_t height;
    PixelFormat pixel_format;
    uint64_t timestamp_us;      // Microsecond timestamp
    uint32_t sequence_number;   // Frame sequence
    size_t buffer_size;         // Total buffer size
    uint32_t stride;            // Row stride in bytes
    
    std::string ToString() const;
};
```

## Block Development

### Creating Custom Blocks

#### 1. Video Source Block

```cpp
class MyCustomSource : public BaseVideoSource {
public:
    MyCustomSource() : BaseVideoSource("MyCustomSource", "CustomSource") {}
    
    // Required overrides
    bool SupportsFormat(PixelFormat format) const override;
    bool SetOutputFormat(const FrameInfo& format) override;
    FrameInfo GetOutputFormat() const override;
    
protected:
    // Generation logic
    void GeneratorThread() override;
    void GenerateFrame(VideoFramePtr frame) override;
};
```

#### 2. Video Sink Block

```cpp
class MyCustomSink : public BaseVideoSink {
public:
    MyCustomSink() : BaseVideoSink("MyCustomSink", "CustomSink") {}
    
    // Required overrides
    bool SupportsFormat(PixelFormat format) const override;
    bool SetInputFormat(const FrameInfo& format) override;
    
protected:
    // Processing logic
    bool ProcessFrameImpl(VideoFramePtr frame) override;
};
```

### Registration System

```cpp
// Register custom blocks
Framework::RegisterBlock("CustomSource", []() -> std::shared_ptr<IBlock> {
    return std::make_shared<MyCustomSource>();
});

Framework::RegisterBlock("CustomSink", []() -> std::shared_ptr<IBlock> {
    return std::make_shared<MyCustomSink>();
});
```

## Performance Characteristics

### Latency Sources

1. **Frame Generation**: Time to create/capture a frame (~1-5ms)
2. **Queue Latency**: Time spent in inter-thread queues (~1-10ms)
3. **Processing**: Time spent in ProcessFrameImpl (~10-50ms)
4. **Synchronization**: Lock contention and context switching (~1-5ms)

### Optimization Strategies

1. **Buffer Pools**: Pre-allocate buffers to avoid malloc/free
2. **CPU Affinity**: Bind threads to specific cores
3. **Queue Sizing**: Tune queue depths for latency vs throughput
4. **Zero-Copy**: Minimize data copying between blocks
5. **SIMD**: Use vectorized operations where possible

### Memory Layout

```
Frame Buffer Layout (RGB24 example):
┌─────────────────────────────────────────────────┐
│ Frame Header (FrameInfo)                        │ 64 bytes
├─────────────────────────────────────────────────┤
│ Pixel Data                                      │
│ Row 0: R G B R G B R G B ... (width * 3 bytes) │
│ Row 1: R G B R G B R G B ...                    │
│ ...                                             │ height * width * 3
│ Row N: R G B R G B R G B ...                    │
└─────────────────────────────────────────────────┘
```

## Error Handling

### Error Categories

1. **Configuration Errors**: Invalid parameters, missing required values
2. **Resource Errors**: Memory allocation failures, hardware access issues
3. **Runtime Errors**: Processing failures, format mismatches
4. **System Errors**: Thread creation failures, file I/O errors

### Error Propagation

```cpp
// Error callback system
using ErrorCallback = std::function<void(const std::string& error)>;

class BaseBlock {
protected:
    void SetError(const std::string& error) {
        last_error_ = error;
        if (error_callback_) {
            error_callback_(error);
        }
        Logger::Error("Block {}: {}", GetName(), error);
    }
};
```

## Future Architecture Considerations

### Planned Enhancements

1. **Plugin System**: Dynamic loading of block implementations
2. **GPU Acceleration**: CUDA/OpenCL buffer types and processing
3. **Network Transparency**: Distributed processing across machines
4. **Hardware Abstraction**: Camera APIs, display systems, encoders
5. **Format Negotiation**: Automatic format conversion between blocks

### Scalability

- **Horizontal**: Multiple pipeline instances
- **Vertical**: Higher resolution and frame rates
- **Distributed**: Network-connected processing nodes

---

This architecture provides a solid foundation for building sophisticated video processing applications while maintaining performance, flexibility, and ease of use.