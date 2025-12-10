# API Reference

Complete API documentation for the Generic Video Pipeline Framework.

## Core Interfaces

### IBlock

Base interface for all processing blocks in the pipeline.

```cpp
class IBlock {
public:
    // Lifecycle Management
    virtual bool Initialize(const BlockParams& params) = 0;
    virtual bool Start() = 0;
    virtual bool Stop() = 0;
    virtual bool Shutdown() = 0;
    
    // Identity and State
    virtual std::string GetName() const = 0;
    virtual std::string GetType() const = 0;
    virtual void SetName(const std::string& name) = 0;
    virtual BlockState GetState() const = 0;
    virtual std::string GetStateString() const;
    
    // Statistics
    virtual BlockStats GetStats() const = 0;
    virtual void ResetStats() = 0;
    
    // Error Handling
    virtual void SetErrorCallback(ErrorCallback callback) = 0;
    virtual std::string GetLastError() const = 0;
    
    // Configuration
    virtual BlockParams GetConfiguration() const = 0;
    virtual bool SetParameter(const std::string& key, const std::string& value) = 0;
    virtual std::string GetParameter(const std::string& key) const = 0;
};
```

#### Methods

##### `Initialize(const BlockParams& params)`
Initialize the block with configuration parameters.
- **Parameters**: `params` - Configuration key-value pairs
- **Returns**: `true` if initialization successful, `false` otherwise
- **Notes**: Must be called before `Start()`

##### `Start()`
Start block processing. For sources, begins frame generation. For sinks, starts worker thread.
- **Returns**: `true` if start successful, `false` otherwise
- **Preconditions**: Block must be in `INITIALIZED` or `STOPPED` state

##### `Stop()`
Stop block processing gracefully.
- **Returns**: `true` if stop successful, `false` otherwise
- **Notes**: Blocks all pending operations and cleans up resources

##### `GetState()`
Get current block state.
- **Returns**: Current `BlockState` enum value
- **Thread Safety**: Safe to call from any thread

#### Block States

```cpp
enum class BlockState {
    UNINITIALIZED,  // Initial state
    INITIALIZED,    // Configured and ready to start
    STARTING,       // Start() called, transitioning to RUNNING
    RUNNING,        // Actively processing
    STOPPING,       // Stop() called, transitioning to STOPPED
    STOPPED,        // Stopped gracefully
    ERROR           // Error state
};
```

### IVideoSource

Interface for video frame generators (cameras, file readers, test patterns).

```cpp
class IVideoSource : public virtual IBlock {
public:
    // Format Management
    virtual bool SupportsFormat(PixelFormat format) const = 0;
    virtual FrameInfo GetOutputFormat() const = 0;
    virtual bool SetOutputFormat(const FrameInfo& format) = 0;
    
    // Frame Rate Control
    virtual double GetFrameRate() const = 0;
    virtual bool SetFrameRate(double fps) = 0;
    
    // Buffer Management
    virtual size_t GetBufferCount() const = 0;
    virtual bool SetBufferCount(size_t count) = 0;
    
    // Connection
    virtual void SetFrameCallback(FrameCallback callback) = 0;
};
```

#### Methods

##### `SupportsFormat(PixelFormat format)`
Check if the source supports a specific pixel format.
- **Parameters**: `format` - Pixel format to check
- **Returns**: `true` if format is supported
- **Examples**: `RGB24`, `YUV420`, `RGBA32`

##### `SetOutputFormat(const FrameInfo& format)`
Configure the output format for generated frames.
- **Parameters**: `format` - Desired output format including resolution
- **Returns**: `true` if format was set successfully
- **Notes**: Cannot be called while block is in `RUNNING` state

##### `SetFrameCallback(FrameCallback callback)`
Set callback function to receive generated frames.
- **Parameters**: `callback` - Function to call with each frame
- **Notes**: Called from source's generation thread

### IVideoSink

Interface for video frame consumers (displays, file writers, encoders).

```cpp
class IVideoSink : public virtual IBlock {
public:
    // Format Management
    virtual bool SupportsFormat(PixelFormat format) const = 0;
    virtual bool SetInputFormat(const FrameInfo& format) = 0;
    
    // Frame Processing
    virtual bool ProcessFrame(VideoFramePtr frame) = 0;
    
    // Queue Management
    virtual size_t GetQueueDepth() const = 0;
    virtual bool SetQueueDepth(size_t depth) = 0;
    virtual bool IsBlocking() const = 0;
    virtual void SetBlocking(bool blocking) = 0;
};
```

#### Methods

##### `ProcessFrame(VideoFramePtr frame)`
Process an incoming video frame.
- **Parameters**: `frame` - Video frame to process
- **Returns**: `true` if frame was processed successfully
- **Thread Safety**: Safe to call from multiple threads (queued internally)

##### `SetInputFormat(const FrameInfo& format)`
Set the expected input format for frames.
- **Parameters**: `format` - Input format specification
- **Returns**: `true` if format is acceptable
- **Notes**: Should be called before processing frames

## Data Types

### FrameInfo

Describes video frame properties and metadata.

```cpp
struct FrameInfo {
    uint32_t width;           // Frame width in pixels
    uint32_t height;          // Frame height in pixels
    PixelFormat pixel_format; // Pixel format (RGB24, YUV420, etc.)
    uint64_t timestamp_us;    // Timestamp in microseconds
    uint32_t sequence_number; // Frame sequence number
    size_t buffer_size;       // Total buffer size in bytes
    uint32_t stride;          // Row stride in bytes
    
    // Utility methods
    std::string ToString() const;
    size_t GetPixelSize() const;
    bool IsValid() const;
    
    // Operators
    bool operator==(const FrameInfo& other) const;
    bool operator!=(const FrameInfo& other) const;
};
```

#### Examples

```cpp
// Create 1080p RGB frame info
FrameInfo info;
info.width = 1920;
info.height = 1080;
info.pixel_format = PixelFormat::RGB24;
info.buffer_size = 1920 * 1080 * 3;
info.stride = 1920 * 3;

// Print frame info
std::cout << info.ToString() << std::endl;
// Output: "1920x1080 RGB24"
```

### PixelFormat

Enumeration of supported pixel formats.

```cpp
enum class PixelFormat {
    RGB24,    // 24-bit RGB (8 bits per channel)
    RGBA32,   // 32-bit RGBA (8 bits per channel + alpha)
    BGR24,    // 24-bit BGR (reverse RGB)
    BGRA32,   // 32-bit BGRA 
    YUV420,   // YUV 4:2:0 planar
    YUV422,   // YUV 4:2:2 planar
    YUV444,   // YUV 4:4:4 planar
    GRAY8,    // 8-bit grayscale
    GRAY16    // 16-bit grayscale
};
```

### BlockStats

Runtime statistics for blocks.

```cpp
struct BlockStats {
    uint64_t frames_processed;    // Total frames processed
    uint64_t frames_dropped;      // Total frames dropped
    uint64_t bytes_processed;     // Total bytes processed
    double avg_fps;               // Average frames per second
    double avg_latency_ms;        // Average processing latency
    size_t queue_depth;           // Current queue depth
    
    // Timing information
    uint64_t total_processing_time_us;
    uint64_t min_processing_time_us;
    uint64_t max_processing_time_us;
    
    // Reset all counters
    void Reset();
    std::string ToString() const;
};
```

### IBuffer Interface

Video frame buffer interface with reference counting.

```cpp
class IBuffer {
public:
    // Data access
    virtual void* GetData() = 0;
    virtual const void* GetData() const = 0;
    virtual size_t GetSize() const = 0;
    
    // Format information
    virtual PixelFormat GetPixelFormat() const = 0;
    virtual const FrameInfo& GetFrameInfo() const = 0;
    
    // Reference counting
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual int GetRefCount() const = 0;
    
    // Utility
    virtual std::shared_ptr<IBuffer> Clone() const = 0;
};

using VideoFramePtr = std::shared_ptr<IBuffer>;
```

## Framework Classes

### PipelineManager

Manages pipeline lifecycle and orchestrates block execution.

```cpp
class PipelineManager {
public:
    PipelineManager();
    ~PipelineManager();
    
    // Pipeline management
    bool LoadConfiguration(const std::string& filename);
    bool LoadConfiguration(const PipelineConfig& config);
    bool Initialize();
    bool Start();
    bool Stop();
    bool Shutdown();
    
    // Block management
    bool AddBlock(const std::string& name, std::shared_ptr<IBlock> block);
    bool RemoveBlock(const std::string& name);
    std::shared_ptr<IBlock> GetBlock(const std::string& name);
    
    // Connection management
    bool ConnectBlocks(const std::string& source_name, const std::string& sink_name);
    bool DisconnectBlocks(const std::string& source_name, const std::string& sink_name);
    
    // Status and statistics
    PipelineState GetState() const;
    std::vector<BlockStats> GetAllStats() const;
    std::string GetLastError() const;
    
private:
    // Implementation details hidden
};
```

#### Usage Example

```cpp
// Create and configure pipeline
PipelineManager pipeline;

// Load configuration from file
if (!pipeline.LoadConfiguration("my_pipeline.yaml")) {
    std::cerr << "Failed to load config: " << pipeline.GetLastError() << std::endl;
    return -1;
}

// Initialize and start
if (!pipeline.Initialize()) {
    std::cerr << "Failed to initialize: " << pipeline.GetLastError() << std::endl;
    return -1;
}

if (!pipeline.Start()) {
    std::cerr << "Failed to start: " << pipeline.GetLastError() << std::endl;
    return -1;
}

// Run for specified time
std::this_thread::sleep_for(std::chrono::seconds(10));

// Stop gracefully
pipeline.Stop();
pipeline.Shutdown();
```

### BlockRegistry

Factory for creating and managing block types.

```cpp
class BlockRegistry {
public:
    using BlockFactory = std::function<std::shared_ptr<IBlock>()>;
    
    // Registration
    static void RegisterBlock(const std::string& type, BlockFactory factory);
    static void UnregisterBlock(const std::string& type);
    
    // Creation
    static std::shared_ptr<IBlock> CreateBlock(const std::string& type);
    static std::vector<std::string> GetRegisteredTypes();
    
    // Query
    static bool IsTypeRegistered(const std::string& type);
    static size_t GetRegisteredCount();
};
```

#### Usage Example

```cpp
// Register custom block type
BlockRegistry::RegisterBlock("MyCustomSource", []() -> std::shared_ptr<IBlock> {
    return std::make_shared<MyCustomSource>();
});

// Create instance
auto block = BlockRegistry::CreateBlock("MyCustomSource");
if (!block) {
    std::cerr << "Failed to create MyCustomSource block" << std::endl;
}
```

### Framework

Main framework initialization and management.

```cpp
class Framework {
public:
    // Initialization
    static bool Initialize();
    static void Shutdown();
    static bool IsInitialized();
    
    // Version information
    static std::string GetVersion();
    static std::string GetBuildInfo();
    
    // Block registration (convenience methods)
    static void RegisterCommonBlocks();
    static void RegisterPlatformBlocks();
    
    // Configuration
    static void SetLogLevel(LogLevel level);
    static void SetLogFile(const std::string& filename);
};
```

## Utility Classes

### Logger

Thread-safe logging system with multiple output levels.

```cpp
class Logger {
public:
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3
    };
    
    // Logging methods
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    
    // Formatted logging
    template<typename... Args>
    static void DebugF(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void InfoF(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void WarningF(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void ErrorF(const std::string& format, Args&&... args);
    
    // Configuration
    static void SetLevel(Level level);
    static void SetOutputFile(const std::string& filename);
    static void EnableConsoleOutput(bool enable);
    static void EnableTimestamps(bool enable);
};
```

#### Macros

```cpp
// Convenient logging macros
#define VP_LOG_DEBUG(msg)       video_pipeline::Logger::Debug(msg)
#define VP_LOG_INFO(msg)        video_pipeline::Logger::Info(msg)
#define VP_LOG_WARNING(msg)     video_pipeline::Logger::Warning(msg)
#define VP_LOG_ERROR(msg)       video_pipeline::Logger::Error(msg)

#define VP_LOG_DEBUG_F(fmt, ...)   video_pipeline::Logger::DebugF(fmt, __VA_ARGS__)
#define VP_LOG_INFO_F(fmt, ...)    video_pipeline::Logger::InfoF(fmt, __VA_ARGS__)
#define VP_LOG_WARNING_F(fmt, ...) video_pipeline::Logger::WarningF(fmt, __VA_ARGS__)
#define VP_LOG_ERROR_F(fmt, ...)   video_pipeline::Logger::ErrorF(fmt, __VA_ARGS__)
```

### Timer

High-precision timing utilities for performance measurement.

```cpp
class Timer {
public:
    // Static timing methods
    static uint64_t GetCurrentTimestampUs();
    static uint64_t GetCurrentTimestampMs();
    static void SleepUs(uint64_t microseconds);
    static void SleepMs(uint64_t milliseconds);
    
    // Instance timing
    Timer();
    void Start();
    void Stop();
    void Reset();
    
    uint64_t GetElapsedUs() const;
    uint64_t GetElapsedMs() const;
    double GetElapsedSeconds() const;
};
```

## Error Handling

### Error Types

```cpp
// Error callback function type
using ErrorCallback = std::function<void(const std::string& error)>;

// Common error codes
enum class ErrorCode {
    SUCCESS = 0,
    INVALID_PARAMETER,
    RESOURCE_UNAVAILABLE,
    FORMAT_NOT_SUPPORTED,
    OPERATION_FAILED,
    TIMEOUT,
    OUT_OF_MEMORY,
    HARDWARE_ERROR
};
```

### Exception Classes

```cpp
// Base exception class
class PipelineException : public std::exception {
public:
    PipelineException(const std::string& message, ErrorCode code = ErrorCode::OPERATION_FAILED);
    
    const char* what() const noexcept override;
    ErrorCode GetErrorCode() const;
    const std::string& GetMessage() const;
    
private:
    std::string message_;
    ErrorCode code_;
};

// Specific exception types
class ConfigurationException : public PipelineException { };
class ResourceException : public PipelineException { };
class FormatException : public PipelineException { };
```

## Threading Utilities

### ThreadPool

Configurable thread pool for background tasks.

```cpp
class ThreadPool {
public:
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Task submission
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // Management
    void Shutdown();
    size_t GetThreadCount() const;
    size_t GetQueueSize() const;
    
    // CPU affinity (Linux only)
    bool SetThreadAffinity(size_t thread_index, const std::vector<int>& cpu_cores);
};
```

## Configuration Types

### PipelineConfig

Configuration structure for pipeline definition.

```cpp
struct PipelineConfig {
    std::string name;
    std::string platform;
    std::vector<BlockConfig> blocks;
    std::vector<ConnectionConfig> connections;
    
    // Validation
    bool IsValid() const;
    std::string ToString() const;
};

struct BlockConfig {
    std::string name;
    std::string type;
    BlockParams parameters;
};

struct ConnectionConfig {
    std::string source;
    std::string sink;
};

using BlockParams = std::map<std::string, std::string>;
```

This API reference provides the complete interface for developing applications and extending the Video Pipeline Framework. For implementation examples, see the [Block Development Guide](BLOCKS.md) and [Configuration Guide](CONFIGURATION.md).