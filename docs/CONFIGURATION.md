# Configuration Guide

Learn how to configure video pipelines using YAML, JSON, and simple text formats.

## Configuration Formats

The framework supports three configuration formats:

1. **YAML** - Human-readable, hierarchical (recommended)
2. **JSON** - Machine-readable, widely supported  
3. **Simple Text** - Key-value pairs, easiest for basic pipelines

## YAML Configuration

### Complete Example

```yaml
# Pipeline definition
pipeline:
  name: "advanced_video_pipeline"
  platform: "generic"  # generic, linux, windows, macos
  description: "Multi-source video processing pipeline"
  version: "1.0"

# Global pipeline settings
settings:
  log_level: "INFO"        # DEBUG, INFO, WARNING, ERROR
  max_latency_ms: 100      # Maximum acceptable latency
  buffer_pool_size: 20     # Pre-allocated buffer count
  thread_pool_size: 4      # Background thread pool size

# Block definitions
blocks:
  # Test pattern source
  - name: "test_source"
    type: "TestPatternSource"
    parameters:
      pattern: "bars"        # solid, bars, checkerboard, gradient, noise, moving_box
      width: "1920"
      height: "1080"
      fps: "30"
      format: "RGB24"        # RGB24, RGBA32, YUV420, etc.
      color: "#FF0000"       # Hex color for solid pattern
    
  # File input source  
  - name: "file_input"
    type: "FileSource"
    enabled: false           # Disabled by default
    parameters:
      path: "/path/to/video.raw"
      width: "1920"
      height: "1080"
      format: "RGB24"
      fps: "25"
      loop: "true"
      
  # Camera source (Linux V4L2)
  - name: "camera_source"
    type: "V4L2Source"
    enabled: false
    parameters:
      device: "/dev/video0"
      width: "1280"
      height: "720"
      fps: "30"
      format: "YUYV"
      
  # Processing blocks
  - name: "scaler"
    type: "ScaleProcessor"
    enabled: false
    parameters:
      output_width: "640"
      output_height: "480"
      algorithm: "bilinear"   # nearest, bilinear, bicubic
      
  - name: "converter"
    type: "FormatConverter" 
    enabled: false
    parameters:
      output_format: "RGBA32"
      
  # Output blocks
  - name: "console_output"
    type: "ConsoleSink"
    parameters:
      verbose: "true"
      show_pixels: "false"
      max_pixels: "16"
      stats_interval: "1000"  # ms between stats
      
  - name: "file_output"
    type: "FileSink"
    parameters:
      path: "/tmp/output"
      format: "raw"           # raw, ppm, bmp, png
      single_file: "false"    # true = overwrite, false = sequence
      filename_pattern: "frame_%06d.raw"
      
  - name: "display_output"
    type: "FramebufferSink"
    enabled: false
    parameters:
      device: "/dev/fb0"
      x_offset: "0"
      y_offset: "0"

# Pipeline connections
connections:
  # Basic pipeline: source -> sink
  - source: "test_source"
    sink: "console_output"
    
  # Multi-output: source -> multiple sinks  
  - source: "test_source"
    sink: "file_output"
    
  # Processing chain: source -> processor -> sink
  # - source: "camera_source"
  #   sink: "scaler"
  # - source: "scaler" 
  #   sink: "converter"
  # - source: "converter"
  #   sink: "display_output"

# Performance tuning
performance:
  # Threading configuration
  source_thread_priority: "high"    # low, normal, high, realtime
  sink_thread_priority: "normal"
  cpu_affinity:
    sources: [0, 1]         # Bind source threads to cores 0,1
    sinks: [2, 3]           # Bind sink threads to cores 2,3
    
  # Buffer management  
  buffer_alignment: 32      # Memory alignment in bytes
  zero_copy: true          # Enable zero-copy optimizations
  
  # Queue settings
  default_queue_depth: 10   # Default sink queue depth
  queue_policy: "drop_oldest"  # drop_oldest, drop_newest, block
  
# Monitoring and debugging
monitoring:
  enable_statistics: true
  stats_interval: 1000      # Update interval in ms
  enable_profiling: false   # Detailed timing measurements
  profile_output: "/tmp/profile.json"
  
debug:
  enable_frame_dumps: false # Dump frames to files
  dump_directory: "/tmp/frames"
  dump_format: "ppm"
  max_dump_frames: 100
```

### Minimal Example

```yaml
pipeline:
  name: "simple_test"
  
blocks:
  - name: "source"
    type: "TestPatternSource"
    parameters:
      width: "640"
      height: "480"
      
  - name: "sink" 
    type: "ConsoleSink"
    
connections:
  - source: "source"
    sink: "sink"
```

## JSON Configuration

### Complete Example

```json
{
  "pipeline": {
    "name": "json_video_pipeline",
    "platform": "generic",
    "description": "JSON-configured video pipeline"
  },
  
  "settings": {
    "log_level": "INFO",
    "max_latency_ms": 100,
    "buffer_pool_size": 10
  },
  
  "blocks": [
    {
      "name": "pattern_source",
      "type": "TestPatternSource", 
      "parameters": {
        "pattern": "checkerboard",
        "width": "800",
        "height": "600",
        "fps": "25",
        "format": "RGB24"
      }
    },
    {
      "name": "file_writer",
      "type": "FileSink",
      "parameters": {
        "path": "output.raw",
        "format": "raw",
        "single_file": "true"
      }
    }
  ],
  
  "connections": [
    {
      "source": "pattern_source",
      "sink": "file_writer"
    }
  ]
}
```

### Minimal JSON Example

```json
{
  "pipeline": {
    "name": "minimal_json"
  },
  "blocks": [
    {
      "name": "src",
      "type": "TestPatternSource",
      "parameters": {
        "width": "320",
        "height": "240"
      }
    },
    {
      "name": "out",
      "type": "ConsoleSink"
    }
  ],
  "connections": [
    {"source": "src", "sink": "out"}
  ]
}
```

## Simple Text Configuration

### Format Specification

```
# Comments start with #
# Pipeline metadata
pipeline=pipeline_name
platform=generic
description=Optional description

# Block definitions
block=name,type,param1=value1;param2=value2;param3=value3

# Connections  
connection=source_name,sink_name

# Settings
setting=key,value
```

### Complete Example

```
# Simple video pipeline
pipeline=simple_pipeline
platform=generic

# Source block with multiple parameters
block=video_source,TestPatternSource,pattern=gradient;width=1280;height=720;fps=30;format=RGB24

# Processing block
block=format_conv,FormatConverter,output_format=RGBA32

# Multiple sink blocks
block=console_out,ConsoleSink,verbose=true;show_pixels=false
block=file_out,FileSink,path=/tmp/video.raw;format=raw;single_file=false

# Pipeline connections
connection=video_source,format_conv
connection=format_conv,console_out  
connection=format_conv,file_out

# Global settings
setting=log_level,DEBUG
setting=max_latency_ms,50
```

### Minimal Example

```
pipeline=test
block=src,TestPatternSource,width=640;height=480
block=sink,ConsoleSink,verbose=true
connection=src,sink
```

## Block Parameters

### Common Source Parameters

| Parameter | Description | Default | Examples |
|-----------|-------------|---------|----------|
| `width` | Frame width in pixels | 640 | "320", "1920", "3840" |
| `height` | Frame height in pixels | 480 | "240", "1080", "2160" |
| `fps` | Frames per second | 30 | "15", "25", "60" |
| `format` | Pixel format | RGB24 | "RGBA32", "YUV420", "GRAY8" |

### TestPatternSource Parameters

| Parameter | Description | Default | Options |
|-----------|-------------|---------|---------|
| `pattern` | Pattern type | solid | solid, bars, checkerboard, gradient, noise, moving_box |
| `color` | Solid color (hex) | #FFFFFF | "#FF0000", "#00FF00", "#0000FF" |

### ConsoleSink Parameters  

| Parameter | Description | Default | Options |
|-----------|-------------|---------|---------|
| `verbose` | Detailed frame info | false | "true", "false" |
| `show_pixels` | Display pixel values | false | "true", "false" |
| `max_pixels` | Max pixels to show | 16 | "8", "32", "64" |
| `stats_interval` | Stats update interval (ms) | 1000 | "500", "2000" |

### FileSink Parameters

| Parameter | Description | Default | Options |
|-----------|-------------|---------|---------|
| `path` | Output file path | output.raw | Any valid file path |
| `format` | Output format | raw | raw, ppm, bmp |
| `single_file` | Overwrite vs sequence | false | "true", "false" |
| `filename_pattern` | Pattern for sequence | frame_%06d | printf-style format |

## Advanced Configuration

### Conditional Blocks

```yaml
blocks:
  - name: "debug_sink"
    type: "ConsoleSink"
    enabled: ${DEBUG_MODE}  # Environment variable
    parameters:
      verbose: "true"
      
  - name: "production_sink"
    type: "FileSink" 
    enabled: !${DEBUG_MODE} # Negated condition
    parameters:
      path: "/var/log/video.raw"
```

### Parameter Substitution

```yaml
# Define variables
variables:
  output_dir: "/tmp/video_output"
  resolution_width: "1920"
  resolution_height: "1080"

blocks:
  - name: "source"
    type: "TestPatternSource"
    parameters:
      width: ${resolution_width}
      height: ${resolution_height}
      
  - name: "sink"
    type: "FileSink"
    parameters:
      path: "${output_dir}/frames.raw"
```

### Multi-Pipeline Configuration

```yaml
# Multiple pipelines in one file
pipelines:
  - pipeline:
      name: "preview_pipeline"
    blocks:
      - name: "cam"
        type: "V4L2Source"
        parameters:
          device: "/dev/video0"
          width: "640"
          height: "480"
      - name: "preview"
        type: "ConsoleSink"
    connections:
      - {source: "cam", sink: "preview"}
      
  - pipeline:
      name: "recording_pipeline" 
    blocks:
      - name: "cam"
        type: "V4L2Source"
        parameters:
          device: "/dev/video0"
          width: "1920"
          height: "1080"
      - name: "recorder"
        type: "FileSink"
        parameters:
          path: "/tmp/recording.raw"
    connections:
      - {source: "cam", sink: "recorder"}
```

## Configuration Validation

### Required Fields

All configurations must specify:
- Pipeline name
- At least one block
- At least one connection (unless only one block)

### Validation Rules

1. **Block Names**: Must be unique within pipeline
2. **Connections**: Source and sink blocks must exist
3. **Parameters**: Block-specific required parameters must be present
4. **Formats**: Pixel formats must be supported by connected blocks
5. **Cycles**: No circular dependencies in connections

### Error Handling

```yaml
# Invalid configuration example
pipeline:
  name: "invalid_pipeline"

blocks:
  - name: "source"
    type: "TestPatternSource"
    # Missing required width/height parameters
    
  - name: "sink"
    type: "ConsoleSink"
    
connections:
  - source: "nonexistent_source"  # Error: source doesn't exist
    sink: "sink"
  - source: "source"
    sink: "nonexistent_sink"      # Error: sink doesn't exist
```

## Environment Integration

### Environment Variables

```bash
# Set environment variables
export VIDEO_WIDTH=1920
export VIDEO_HEIGHT=1080
export OUTPUT_PATH=/tmp/video
export DEBUG_MODE=true

# Use in configuration
./pipeline_cli --config pipeline.yaml
```

```yaml
# Reference environment variables
blocks:
  - name: "source"
    type: "TestPatternSource"
    parameters:
      width: "${VIDEO_WIDTH:-640}"    # Default to 640 if not set
      height: "${VIDEO_HEIGHT:-480}"  # Default to 480 if not set
      
  - name: "sink"
    type: "FileSink"
    parameters:
      path: "${OUTPUT_PATH}/output.raw"
```

### Command Line Overrides

```bash
# Override configuration parameters
./pipeline_cli --config pipeline.yaml \
  --set source.width=1280 \
  --set source.height=720 \
  --set sink.path=/custom/output.raw
```

## Configuration Examples

### Camera to Display

```yaml
pipeline:
  name: "camera_display"
  
blocks:
  - name: "camera"
    type: "V4L2Source"
    parameters:
      device: "/dev/video0"
      width: "1280"
      height: "720"
      fps: "30"
      
  - name: "display"
    type: "FramebufferSink"
    parameters:
      device: "/dev/fb0"
      
connections:
  - source: "camera"
    sink: "display"
```

### Multi-Output Recording

```yaml
pipeline:
  name: "multi_output"
  
blocks:
  - name: "camera"
    type: "V4L2Source"
    parameters:
      device: "/dev/video0"
      width: "1920"
      height: "1080"
      fps: "30"
      
  - name: "preview"
    type: "ConsoleSink"
    parameters:
      verbose: "false"
      
  - name: "recorder"
    type: "FileSink"
    parameters:
      path: "/recordings/video.raw"
      format: "raw"
      
  - name: "streamer"
    type: "RTPSink"
    enabled: false
    parameters:
      host: "192.168.1.100"
      port: "5004"
      
connections:
  - source: "camera"
    sink: "preview"
  - source: "camera" 
    sink: "recorder"
  - source: "camera"
    sink: "streamer"
```

### Processing Pipeline

```yaml
pipeline:
  name: "processing_chain"
  
blocks:
  - name: "input"
    type: "FileSource"
    parameters:
      path: "input_video.raw"
      width: "1920"
      height: "1080"
      format: "RGB24"
      
  - name: "scaler"
    type: "ScaleProcessor"
    parameters:
      output_width: "1280"
      output_height: "720"
      algorithm: "bilinear"
      
  - name: "converter"  
    type: "FormatConverter"
    parameters:
      output_format: "RGBA32"
      
  - name: "filter"
    type: "BlurFilter"
    parameters:
      radius: "2"
      strength: "0.5"
      
  - name: "output"
    type: "FileSink"
    parameters:
      path: "processed_video.raw"
      format: "raw"
      
connections:
  - source: "input"
    sink: "scaler"
  - source: "scaler"
    sink: "converter" 
  - source: "converter"
    sink: "filter"
  - source: "filter"
    sink: "output"
```

## Best Practices

### Configuration Organization

1. **Use YAML** for complex pipelines (most readable)
2. **Use JSON** for machine-generated configurations  
3. **Use Simple Text** for basic pipelines and testing
4. **Comment extensively** in YAML configurations
5. **Use meaningful block names** (not just "source", "sink")

### Parameter Management

1. **Use variables** for repeated values
2. **Environment variables** for deployment-specific settings
3. **Validate parameters** before deployment
4. **Document custom parameters** for custom blocks

### Performance Tuning

1. **Monitor statistics** to identify bottlenecks
2. **Adjust queue depths** based on latency requirements
3. **Use CPU affinity** for deterministic performance
4. **Enable zero-copy** for high-throughput pipelines

### Debugging Configuration

```bash
# Validate configuration without running
./pipeline_cli --config pipeline.yaml --validate-only

# Dry run with detailed logging
./pipeline_cli --config pipeline.yaml --dry-run --verbose

# Enable debug logging
./pipeline_cli --config pipeline.yaml --log-level DEBUG
```

This guide covers all aspects of pipeline configuration. For specific block parameters, refer to the [Block Development Guide](BLOCKS.md) and [API Reference](API.md).