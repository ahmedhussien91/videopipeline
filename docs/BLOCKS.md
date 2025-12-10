# Block Development Guide

Learn how to create custom blocks for the Generic Video Pipeline Framework.

## Overview

Blocks are the fundamental processing units in the pipeline. They can be:
- **Video Sources**: Generate frames (cameras, files, test patterns)
- **Video Sinks**: Consume frames (displays, files, network streams)  
- **Video Processors**: Transform frames (filters, encoders, scalers)

## Block Development Process

### 1. Choose Block Type

Determine what type of block you need:

| Block Type | Base Class | Use Cases |
|------------|------------|-----------|
| Video Source | `BaseVideoSource` | Cameras, file readers, generators |
| Video Sink | `BaseVideoSink` | Displays, file writers, encoders |
| Video Processor | `BaseVideoProcessor` | Filters, format converters |

### 2. Implement Required Methods

Every block must implement the core `IBlock` interface methods through its base class.

## Creating a Video Source

### Basic Template

```cpp
#include "video_pipeline/video_source.h"

class MyVideoSource : public BaseVideoSource {
public:
    MyVideoSource() : BaseVideoSource("MyVideoSource", "MyVideoSource") {
        // Initialize member variables
        frame_count_ = 0;
        is_generating_ = false;
    }
    
    virtual ~MyVideoSource() {
        Stop();
        Shutdown();
    }
    
    // Required format support methods
    bool SupportsFormat(PixelFormat format) const override {
        return format == PixelFormat::RGB24 || format == PixelFormat::RGBA32;
    }
    
    bool SetOutputFormat(const FrameInfo& format) override {
        if (BaseBlock::GetState() == BlockState::RUNNING) {
            SetError("Cannot change format while running");
            return false;
        }
        
        if (!SupportsFormat(format.pixel_format)) {
            SetError("Unsupported pixel format");
            return false;
        }
        
        output_format_ = format;
        return true;
    }
    
    FrameInfo GetOutputFormat() const override {
        return output_format_;
    }
    
    // Required initialization
    bool Initialize(const BlockParams& params) override {
        if (!BaseVideoSource::Initialize(params)) {
            return false;
        }
        
        // Parse custom parameters
        auto width_str = BaseBlock::GetParameter("width");
        auto height_str = BaseBlock::GetParameter("height");
        
        if (!width_str.empty()) {
            output_format_.width = std::stoul(width_str);
        }
        if (!height_str.empty()) {
            output_format_.height = std::stoul(height_str);
        }
        
        // Set defaults
        if (output_format_.width == 0) output_format_.width = 640;
        if (output_format_.height == 0) output_format_.height = 480;
        output_format_.pixel_format = PixelFormat::RGB24;
        output_format_.buffer_size = output_format_.width * output_format_.height * 3;
        
        VP_LOG_INFO_F("MyVideoSource initialized: {}", output_format_.ToString());
        return true;
    }
    
    // Required lifecycle methods
    bool Start() override {
        if (BaseBlock::GetState() == BlockState::RUNNING) {
            return true;
        }
        
        // Start generation thread
        is_generating_ = true;
        generator_thread_ = std::thread(&MyVideoSource::GeneratorThread, this);
        
        SetState(BlockState::RUNNING);
        VP_LOG_INFO_F("MyVideoSource '{}' started", BaseBlock::GetName());
        return true;
    }
    
    bool Stop() override {
        if (BaseBlock::GetState() != BlockState::RUNNING) {
            return true;
        }
        
        SetState(BlockState::STOPPING);
        
        // Stop generation
        is_generating_ = false;
        if (generator_thread_.joinable()) {
            generator_thread_.join();
        }
        
        SetState(BlockState::STOPPED);
        VP_LOG_INFO_F("MyVideoSource '{}' stopped", BaseBlock::GetName());
        return true;
    }

private:
    // Generation thread implementation
    void GeneratorThread() {
        VP_LOG_DEBUG_F("MyVideoSource '{}' generator thread started", BaseBlock::GetName());
        
        while (is_generating_) {
            if (!ShouldEmitFrame()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            // Create frame buffer
            auto frame = CreateBuffer();
            if (!frame) {
                VP_LOG_ERROR("Failed to create frame buffer");
                break;
            }
            
            // Generate frame content
            GenerateFrame(frame);
            
            // Emit frame to connected sinks
            EmitFrame(frame);
            
            frame_count_++;
        }
        
        VP_LOG_DEBUG_F("MyVideoSource '{}' generator thread stopped", BaseBlock::GetName());
    }
    
    void GenerateFrame(VideoFramePtr frame) {
        // Fill buffer with generated content
        uint8_t* data = static_cast<uint8_t*>(frame->GetData());
        const auto& info = frame->GetFrameInfo();
        
        // Example: Fill with solid color that changes over time
        uint8_t red = (frame_count_ * 5) % 255;
        uint8_t green = (frame_count_ * 3) % 255;
        uint8_t blue = (frame_count_ * 7) % 255;
        
        for (size_t i = 0; i < info.buffer_size; i += 3) {
            data[i] = red;     // R
            data[i+1] = green; // G  
            data[i+2] = blue;  // B
        }
    }
    
private:
    FrameInfo output_format_;
    std::atomic<bool> is_generating_{false};
    std::thread generator_thread_;
    uint32_t frame_count_;
};
```

## Creating a Video Sink

### Basic Template

```cpp
#include "video_pipeline/video_sink.h"
#include <fstream>

class MyVideoSink : public BaseVideoSink {
public:
    MyVideoSink() : BaseVideoSink("MyVideoSink", "MyVideoSink") {
        frames_processed_ = 0;
    }
    
    virtual ~MyVideoSink() {
        Stop();
        Shutdown();
        CloseOutput();
    }
    
    // Required format support
    bool SupportsFormat(PixelFormat format) const override {
        return format == PixelFormat::RGB24;
    }
    
    // Required initialization
    bool Initialize(const BlockParams& params) override {
        if (!BaseVideoSink::Initialize(params)) {
            return false;
        }
        
        // Parse custom parameters
        auto output_path = BaseBlock::GetParameter("output_path");
        if (output_path.empty()) {
            output_path = "output.raw";
        }
        
        // Open output file
        output_file_.open(output_path, std::ios::binary);
        if (!output_file_.is_open()) {
            SetError("Failed to open output file: " + output_path);
            return false;
        }
        
        VP_LOG_INFO_F("MyVideoSink initialized: output_path='{}'", output_path);
        return true;
    }
    
    bool Shutdown() override {
        CloseOutput();
        return BaseVideoSink::Shutdown();
    }

protected:
    // Required frame processing implementation
    bool ProcessFrameImpl(VideoFramePtr frame) override {
        if (!frame) {
            VP_LOG_ERROR("Received null frame");
            return false;
        }
        
        // Write frame data to file
        const void* data = frame->GetData();
        size_t size = frame->GetSize();
        
        output_file_.write(static_cast<const char*>(data), size);
        if (!output_file_.good()) {
            VP_LOG_ERROR("Failed to write frame data");
            return false;
        }
        
        frames_processed_++;
        
        // Log progress
        if (frames_processed_ % 100 == 0) {
            VP_LOG_INFO_F("MyVideoSink processed {} frames", frames_processed_);
        }
        
        return true;
    }
    
private:
    void CloseOutput() {
        if (output_file_.is_open()) {
            output_file_.close();
            VP_LOG_INFO_F("Closed output file after {} frames", frames_processed_);
        }
    }
    
private:
    std::ofstream output_file_;
    uint32_t frames_processed_;
};
```

## Creating a Video Processor

Video processors both consume and produce frames, inheriting from both source and sink interfaces.

```cpp
class MyVideoProcessor : public BaseVideoSource, public BaseVideoSink {
public:
    MyVideoProcessor() 
        : BaseVideoSource("MyVideoProcessor", "MyVideoProcessor")
        , BaseVideoSink("MyVideoProcessor", "MyVideoProcessor") {
    }
    
    // Implement both source and sink interfaces
    bool SupportsFormat(PixelFormat format) const override {
        return format == PixelFormat::RGB24;
    }
    
    bool Initialize(const BlockParams& params) override {
        // Initialize both base classes
        if (!BaseVideoSource::Initialize(params) || 
            !BaseVideoSink::Initialize(params)) {
            return false;
        }
        
        // Processor-specific initialization
        return true;
    }
    
protected:
    // Process incoming frames and generate output
    bool ProcessFrameImpl(VideoFramePtr input_frame) override {
        // Create output frame
        auto output_frame = CreateBuffer();
        if (!output_frame) {
            return false;
        }
        
        // Apply processing (example: invert colors)
        ProcessFrame(input_frame, output_frame);
        
        // Emit processed frame
        EmitFrame(output_frame);
        
        return true;
    }
    
private:
    void ProcessFrame(VideoFramePtr input, VideoFramePtr output) {
        const uint8_t* input_data = static_cast<const uint8_t*>(input->GetData());
        uint8_t* output_data = static_cast<uint8_t*>(output->GetData());
        size_t size = input->GetSize();
        
        // Invert RGB values
        for (size_t i = 0; i < size; ++i) {
            output_data[i] = 255 - input_data[i];
        }
    }
};
```

## Registration and Factory

### Register Your Block

```cpp
// In your initialization code or plugin entry point
void RegisterMyBlocks() {
    BlockRegistry::RegisterBlock("MyVideoSource", []() -> std::shared_ptr<IBlock> {
        return std::make_shared<MyVideoSource>();
    });
    
    BlockRegistry::RegisterBlock("MyVideoSink", []() -> std::shared_ptr<IBlock> {
        return std::make_shared<MyVideoSink>();
    });
    
    BlockRegistry::RegisterBlock("MyVideoProcessor", []() -> std::shared_ptr<IBlock> {
        return std::make_shared<MyVideoProcessor>();
    });
}
```

### Using in Configuration

```yaml
pipeline:
  name: "my_custom_pipeline"
  
blocks:
  - name: "source"
    type: "MyVideoSource"
    parameters:
      width: "1920"
      height: "1080"
      fps: "30"
      
  - name: "processor"
    type: "MyVideoProcessor"
    
  - name: "sink"
    type: "MyVideoSink"
    parameters:
      output_path: "/tmp/processed_video.raw"
      
connections:
  - source: "source"
    sink: "processor"
  - source: "processor"
    sink: "sink"
```

## Advanced Features

### Parameter Validation

```cpp
bool MyVideoSource::Initialize(const BlockParams& params) {
    if (!BaseVideoSource::Initialize(params)) {
        return false;
    }
    
    // Validate required parameters
    auto device_param = BaseBlock::GetParameter("device");
    if (device_param.empty()) {
        SetError("Required parameter 'device' not specified");
        return false;
    }
    
    // Validate parameter ranges
    auto fps_param = BaseBlock::GetParameter("fps");
    if (!fps_param.empty()) {
        double fps = std::stod(fps_param);
        if (fps < 1.0 || fps > 120.0) {
            SetError("FPS must be between 1.0 and 120.0");
            return false;
        }
    }
    
    return true;
}
```

### Custom Statistics

```cpp
class MyVideoSource : public BaseVideoSource {
public:
    BlockStats GetStats() const override {
        auto stats = BaseVideoSource::GetStats();
        
        // Add custom statistics
        stats.custom_data["device_errors"] = std::to_string(device_errors_);
        stats.custom_data["buffer_overruns"] = std::to_string(buffer_overruns_);
        
        return stats;
    }
    
private:
    std::atomic<uint32_t> device_errors_{0};
    std::atomic<uint32_t> buffer_overruns_{0};
};
```

### Error Handling

```cpp
void MyVideoSource::GeneratorThread() {
    try {
        while (is_generating_) {
            // Frame generation logic that might throw
            GenerateFrame();
        }
    }
    catch (const std::exception& e) {
        SetError("Generation thread exception: " + std::string(e.what()));
        SetState(BlockState::ERROR);
    }
    catch (...) {
        SetError("Unknown exception in generation thread");
        SetState(BlockState::ERROR);
    }
}
```

### Format Negotiation

```cpp
bool MyVideoProcessor::SetInputFormat(const FrameInfo& format) override {
    if (!BaseVideoSink::SetInputFormat(format)) {
        return false;
    }
    
    // Set output format based on input
    FrameInfo output_format = format;
    
    // Example: Convert RGB24 input to RGBA32 output
    if (format.pixel_format == PixelFormat::RGB24) {
        output_format.pixel_format = PixelFormat::RGBA32;
        output_format.buffer_size = format.width * format.height * 4;
    }
    
    return SetOutputFormat(output_format);
}
```

## Performance Optimization

### Buffer Management

```cpp
class OptimizedVideoSource : public BaseVideoSource {
private:
    // Pre-allocate buffer pool
    std::queue<VideoFramePtr> buffer_pool_;
    std::mutex pool_mutex_;
    
    void InitializeBufferPool() {
        const size_t pool_size = 10;
        for (size_t i = 0; i < pool_size; ++i) {
            auto buffer = CreateBuffer();
            buffer_pool_.push(buffer);
        }
    }
    
    VideoFramePtr GetPooledBuffer() {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (buffer_pool_.empty()) {
            return CreateBuffer(); // Fallback
        }
        
        auto buffer = buffer_pool_.front();
        buffer_pool_.pop();
        return buffer;
    }
    
    void ReturnBuffer(VideoFramePtr buffer) {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        buffer_pool_.push(buffer);
    }
};
```

### Threading Optimization

```cpp
class ThreadedVideoProcessor : public BaseVideoSink {
private:
    void WorkerThread() {
        // Set thread priority (Linux)
        #ifdef __linux__
        pthread_t thread = pthread_self();
        int policy = SCHED_FIFO;
        struct sched_param param;
        param.sched_priority = 50; // High priority
        pthread_setschedparam(thread, policy, &param);
        #endif
        
        // Set CPU affinity
        #ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(2, &cpuset); // Bind to CPU core 2
        pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        #endif
        
        // Main processing loop
        ProcessingLoop();
    }
};
```

## Testing Your Blocks

### Unit Test Template

```cpp
#include <gtest/gtest.h>
#include "MyVideoSource.h"

class MyVideoSourceTest : public ::testing::Test {
protected:
    void SetUp() override {
        source_ = std::make_shared<MyVideoSource>();
        
        // Set up test parameters
        BlockParams params;
        params["width"] = "320";
        params["height"] = "240";
        params["fps"] = "30";
        
        ASSERT_TRUE(source_->Initialize(params));
    }
    
    void TearDown() override {
        if (source_) {
            source_->Stop();
            source_->Shutdown();
        }
    }
    
    std::shared_ptr<MyVideoSource> source_;
};

TEST_F(MyVideoSourceTest, InitializationSuccess) {
    EXPECT_EQ(source_->GetState(), BlockState::INITIALIZED);
    
    auto format = source_->GetOutputFormat();
    EXPECT_EQ(format.width, 320);
    EXPECT_EQ(format.height, 240);
    EXPECT_EQ(format.pixel_format, PixelFormat::RGB24);
}

TEST_F(MyVideoSourceTest, StartStopCycle) {
    EXPECT_TRUE(source_->Start());
    EXPECT_EQ(source_->GetState(), BlockState::RUNNING);
    
    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(source_->Stop());
    EXPECT_EQ(source_->GetState(), BlockState::STOPPED);
}

TEST_F(MyVideoSourceTest, FrameGeneration) {
    std::atomic<int> frame_count{0};
    
    // Set up frame callback
    source_->SetFrameCallback([&](VideoFramePtr frame) {
        EXPECT_NE(frame, nullptr);
        EXPECT_GT(frame->GetSize(), 0);
        frame_count++;
    });
    
    EXPECT_TRUE(source_->Start());
    
    // Wait for frames
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(source_->Stop());
    EXPECT_GT(frame_count.load(), 0);
}
```

## Debugging Tips

### Enable Detailed Logging

```cpp
// In your block constructor
VP_LOG_INFO_F("Creating {} block", GetType());

// In critical methods
VP_LOG_DEBUG_F("Processing frame {}: {}x{} {} bytes", 
               frame_number, width, height, size);

// Before potential failure points
VP_LOG_DEBUG("About to open camera device");
```

### Add Timing Measurements

```cpp
bool MyVideoSink::ProcessFrameImpl(VideoFramePtr frame) {
    Timer timer;
    timer.Start();
    
    // Processing logic
    bool result = ProcessFrame(frame);
    
    timer.Stop();
    auto elapsed = timer.GetElapsedUs();
    
    // Log slow frames
    if (elapsed > 10000) { // > 10ms
        VP_LOG_WARNING_F("Slow frame processing: {}us", elapsed);
    }
    
    return result;
}
```

### Memory Leak Detection

```cpp
// Track buffer allocations in debug builds
#ifdef DEBUG
class DebugBuffer : public SimpleBuffer {
public:
    DebugBuffer(size_t size, PixelFormat format) 
        : SimpleBuffer(size, format) {
        allocated_buffers_.fetch_add(1);
        VP_LOG_DEBUG_F("Buffer allocated: {} total", allocated_buffers_.load());
    }
    
    ~DebugBuffer() {
        allocated_buffers_.fetch_sub(1);
        VP_LOG_DEBUG_F("Buffer freed: {} remaining", allocated_buffers_.load());
    }
    
private:
    static std::atomic<int> allocated_buffers_;
};
#endif
```

This guide provides a comprehensive foundation for developing custom blocks. For more examples, study the built-in blocks in the `src/blocks/` directory.