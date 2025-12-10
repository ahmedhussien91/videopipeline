# Troubleshooting Guide

This guide helps diagnose and resolve common issues encountered when working with the video pipeline framework.

## Quick Diagnostics

### Health Check Script

```bash
#!/bin/bash
# health_check.sh - Quick system diagnostic

echo "=== Video Pipeline Health Check ==="
echo

# Check build environment
echo "1. Build Environment:"
echo "   CMake: $(cmake --version | head -1)"
echo "   GCC:   $(gcc --version | head -1)"
echo "   Make:  $(make --version | head -1)"
echo

# Check system resources
echo "2. System Resources:"
echo "   CPU Cores: $(nproc)"
echo "   Memory:    $(free -h | grep Mem | awk '{print $2}')"
echo "   Disk:      $(df -h . | tail -1 | awk '{print $4}')"
echo

# Check pipeline binary
echo "3. Pipeline Binary:"
if [ -f "build/pipeline_cli" ]; then
    echo "   ✓ Binary exists"
    echo "   Size: $(ls -lh build/pipeline_cli | awk '{print $5}')"
    echo "   ✓ Can execute: $(build/pipeline_cli --version 2>/dev/null && echo 'Yes' || echo 'No')"
else
    echo "   ✗ Binary not found"
fi
echo

# Check example configs
echo "4. Example Configurations:"
for config in examples/*.yaml; do
    if [ -f "$config" ]; then
        echo "   ✓ $config"
    fi
done
echo

# Test basic functionality
echo "5. Basic Functionality Test:"
if [ -f "build/pipeline_cli" ] && [ -f "examples/simple.yaml" ]; then
    timeout 5s build/pipeline_cli --config examples/simple.yaml --frames 1 >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "   ✓ Simple pipeline test passed"
    else
        echo "   ✗ Simple pipeline test failed"
    fi
else
    echo "   ✗ Cannot run test (missing binary or config)"
fi

echo
echo "=== Health Check Complete ==="
```

### System Requirements Check

```bash
#!/bin/bash
# check_requirements.sh

echo "Checking system requirements..."

# Check OS
OS=$(uname -s)
echo "Operating System: $OS"

# Check kernel version (Linux)
if [ "$OS" = "Linux" ]; then
    KERNEL=$(uname -r)
    echo "Kernel Version: $KERNEL"
    
    # Check for required kernel features
    if [ -f /proc/version ]; then
        echo "Kernel supports /proc filesystem: ✓"
    fi
fi

# Check compiler
if command -v gcc >/dev/null 2>&1; then
    GCC_VERSION=$(gcc -dumpversion)
    echo "GCC Version: $GCC_VERSION"
    
    # Check if version >= 8
    if [ "$(echo "$GCC_VERSION >= 8" | bc)" -eq 1 ]; then
        echo "GCC version requirement: ✓"
    else
        echo "GCC version requirement: ✗ (need >= 8.0)"
    fi
else
    echo "GCC not found: ✗"
fi

# Check CMake
if command -v cmake >/dev/null 2>&1; then
    CMAKE_VERSION=$(cmake --version | head -1 | grep -o '[0-9]\+\.[0-9]\+')
    echo "CMake Version: $CMAKE_VERSION"
    
    if [ "$(echo "$CMAKE_VERSION >= 3.16" | bc)" -eq 1 ]; then
        echo "CMake version requirement: ✓"
    else
        echo "CMake version requirement: ✗ (need >= 3.16)"
    fi
else
    echo "CMake not found: ✗"
fi
```

## Build Issues

### Common Build Errors

#### 1. CMake Configuration Failed

**Error:**
```
CMake Error: The source directory does not appear to contain CMakeLists.txt
```

**Solution:**
```bash
# Ensure you're in the correct directory
pwd
ls -la CMakeLists.txt

# If CMakeLists.txt is missing, you might be in the wrong directory
cd /path/to/cameraCapture
```

**Error:**
```
CMake Error: CMAKE_CXX_COMPILER not set
```

**Solution:**
```bash
# Install build tools on Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake

# Install on CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install cmake

# Manually specify compiler
cmake -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc ..
```

#### 2. Missing Dependencies

**Error:**
```
fatal error: 'thread' file not found
```

**Solution:**
```bash
# Ensure C++17 support
cmake -DCMAKE_CXX_STANDARD=17 ..

# Or check compiler version
g++ --version  # Should be >= 8.0
clang++ --version  # Should be >= 10.0
```

**Error:**
```
undefined reference to `pthread_create'
```

**Solution:**
```bash
# Link pthread library
cmake -DCMAKE_EXE_LINKER_FLAGS="-lpthread" ..

# Or modify CMakeLists.txt
find_package(Threads REQUIRED)
target_link_libraries(your_target Threads::Threads)
```

#### 3. Compilation Errors

**Error:**
```
error: 'mutex' is not a member of 'std'
```

**Solution:**
```cpp
// Add missing include
#include <mutex>

// Verify C++17 compilation
static_assert(__cplusplus >= 201703L, "C++17 required");
```

**Error:**
```
error: call to 'GetName' is ambiguous
```

**Solution:**
```cpp
// Use explicit scope resolution
std::string GetName() const override {
    return BaseBlock::GetName();  // Specify which GetName()
}
```

### Build Optimization

#### Debug Build Issues

```bash
# Create debug build
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-g -O0 -DDEBUG" \
      ..

# Enable verbose output
make VERBOSE=1

# Or with ninja
ninja -v
```

#### Release Build Issues

```bash
# Create optimized release build
mkdir build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -DNDEBUG" \
      ..

# Check for undefined behavior in optimized code
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O2 -g -fsanitize=undefined" \
      ..
```

## Runtime Issues

### Application Startup Problems

#### 1. Segmentation Faults

**Symptoms:**
```
Segmentation fault (core dumped)
```

**Debugging:**
```bash
# Enable core dumps
ulimit -c unlimited

# Run with GDB
gdb ./build/pipeline_cli
(gdb) run --config examples/simple.yaml
(gdb) bt  # When it crashes, get backtrace

# Run with AddressSanitizer
export ASAN_OPTIONS="abort_on_error=1:print_stacktrace=1"
./build/pipeline_cli --config examples/simple.yaml

# Quick stack trace without GDB
./build/pipeline_cli --config examples/simple.yaml 2>&1 | addr2line -e build/pipeline_cli
```

**Common Causes:**
- Null pointer dereference
- Buffer overflow
- Use after free
- Stack overflow

#### 2. Configuration Loading Errors

**Error:**
```
Error: Failed to load configuration file
```

**Debugging:**
```bash
# Check file existence and permissions
ls -la examples/simple.yaml
file examples/simple.yaml

# Validate YAML syntax
python3 -c "import yaml; yaml.safe_load(open('examples/simple.yaml'))"

# Check file encoding
file -bi examples/simple.yaml

# Test with minimal config
cat > test_minimal.yaml << 'EOF'
pipeline:
  name: "test"
blocks:
  - name: "source"
    type: "TestPatternSource"
    parameters:
      width: "320"
      height: "240"
  - name: "sink"
    type: "ConsoleSink"
connections:
  - source: "source"
    sink: "sink"
EOF
```

#### 3. Block Initialization Failures

**Error:**
```
Error: Block 'source' failed to initialize
```

**Debugging:**
```bash
# Enable debug logging
export VP_LOG_LEVEL=DEBUG
./build/pipeline_cli --config examples/simple.yaml

# Check specific block parameters
./build/pipeline_cli --config examples/simple.yaml --verbose

# Test block in isolation
./build/simple_test_pattern  # If available
```

### Performance Issues

#### 1. Low Frame Rate

**Symptoms:**
- Frame rate below expected
- High CPU usage
- Frame drops

**Diagnostic:**
```bash
# Profile CPU usage
perf record -g ./build/pipeline_cli --config examples/performance.yaml
perf report

# Monitor in real-time
top -p $(pgrep pipeline_cli)

# Check system limits
ulimit -a

# Monitor I/O
iotop -p $(pgrep pipeline_cli)
```

**Solutions:**

```bash
# Increase process priority
nice -10 ./build/pipeline_cli --config examples/performance.yaml

# Set CPU affinity
taskset -c 0,1 ./build/pipeline_cli --config examples/performance.yaml

# Increase system limits
echo '* soft nofile 65536' >> /etc/security/limits.conf
echo '* hard nofile 65536' >> /etc/security/limits.conf

# Optimize kernel parameters
echo 'net.core.rmem_max = 16777216' >> /etc/sysctl.conf
echo 'net.core.wmem_max = 16777216' >> /etc/sysctl.conf
sysctl -p
```

#### 2. Memory Issues

**Symptoms:**
- Memory usage grows over time
- Out of memory errors
- Swap usage increases

**Debugging:**
```bash
# Monitor memory usage
watch 'ps -p $(pgrep pipeline_cli) -o pid,ppid,rss,vsz,comm'

# Check for memory leaks
valgrind --tool=memcheck --leak-check=full \
         ./build/pipeline_cli --config examples/simple.yaml

# Use AddressSanitizer for detailed analysis
export ASAN_OPTIONS="detect_leaks=1:print_stats=1"
./build/pipeline_cli --config examples/simple.yaml
```

**Solutions:**
```cpp
// Implement proper cleanup in destructors
class VideoBlock {
public:
    ~VideoBlock() {
        Shutdown();  // Ensure cleanup
        
        // Clean up resources
        if (buffer_) {
            free(buffer_);
            buffer_ = nullptr;
        }
    }
};

// Use RAII for automatic resource management
class ResourceManager {
    std::unique_ptr<Resource> resource_;
public:
    ResourceManager() : resource_(std::make_unique<Resource>()) {}
    // Automatic cleanup when object goes out of scope
};
```

### Threading Issues

#### 1. Deadlocks

**Symptoms:**
- Application hangs
- No CPU usage
- Threads blocked

**Debugging:**
```bash
# Get thread dump
gdb -p $(pgrep pipeline_cli)
(gdb) info threads
(gdb) thread apply all bt

# Use ThreadSanitizer
export TSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
./build/pipeline_cli --config examples/simple.yaml

# Monitor threads
top -H -p $(pgrep pipeline_cli)
```

**Prevention:**
```cpp
// Always acquire locks in the same order
class SafeBlock {
    std::mutex mutex1_;
    std::mutex mutex2_;
    
public:
    void Method1() {
        std::lock(mutex1_, mutex2_);  // Acquire both atomically
        std::lock_guard<std::mutex> lock1(mutex1_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(mutex2_, std::adopt_lock);
        // ... work
    }
};

// Use timeout for lock acquisition
bool TryProcessWithTimeout() {
    if (mutex_.try_lock_for(std::chrono::seconds(1))) {
        std::lock_guard<std::timed_mutex> lock(mutex_, std::adopt_lock);
        // Do work
        return true;
    }
    return false; // Timeout
}
```

#### 2. Race Conditions

**Symptoms:**
- Inconsistent results
- Occasional crashes
- Data corruption

**Debugging:**
```bash
# Use ThreadSanitizer
export TSAN_OPTIONS="detect_thread_leaks=1"
./build/pipeline_cli --config examples/threaded.yaml

# Run multiple times to catch races
for i in {1..100}; do
    echo "Run $i"
    ./build/pipeline_cli --config examples/simple.yaml --frames 10
done
```

**Solutions:**
```cpp
// Protect shared data with mutexes
class ThreadSafeCounter {
    std::atomic<int> count_{0};  // Use atomic for simple operations
    
public:
    void Increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    int Get() const {
        return count_.load(std::memory_order_relaxed);
    }
};

// Use proper memory ordering for complex operations
class ThreadSafeQueue {
    std::queue<Item> queue_;
    mutable std::mutex mutex_;
    
public:
    void Push(const Item& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
    }
    
    bool Pop(Item& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }
};
```

## Network Issues

### Connection Problems

#### 1. Socket Errors

**Error:**
```
Error: Failed to bind socket: Address already in use
```

**Solutions:**
```bash
# Find process using the port
netstat -tulpn | grep :8080
lsof -i :8080

# Kill process using port
kill $(lsof -t -i:8080)

# Use SO_REUSEADDR in code
int opt = 1;
setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

**Error:**
```
Error: Connection refused
```

**Debugging:**
```bash
# Check if server is listening
netstat -tulpn | grep :8080

# Test connectivity
telnet localhost 8080
nc -zv localhost 8080

# Check firewall
sudo iptables -L
sudo ufw status
```

#### 2. Network Performance Issues

**Symptoms:**
- High latency
- Packet loss
- Low throughput

**Debugging:**
```bash
# Monitor network traffic
iftop -i eth0
nethogs -p

# Check network statistics
ss -s
netstat -i

# Test network performance
iperf3 -s  # On server
iperf3 -c server_ip -t 10  # On client
```

**Optimization:**
```cpp
// Optimize socket buffer sizes
int buffer_size = 1024 * 1024;  // 1MB
setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));

// Disable Nagle's algorithm for low latency
int flag = 1;
setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

// Use non-blocking I/O
fcntl(socket_fd, F_SETFL, O_NONBLOCK);
```

## Platform-Specific Issues

### Linux Issues

#### 1. Permission Problems

**Error:**
```
Error: Permission denied accessing /dev/video0
```

**Solutions:**
```bash
# Add user to video group
sudo usermod -a -G video $USER

# Set device permissions
sudo chmod 666 /dev/video0

# Check device ownership
ls -la /dev/video*
```

#### 2. Library Issues

**Error:**
```
error while loading shared libraries: libXXX.so.1: cannot open shared object
```

**Solutions:**
```bash
# Find missing library
ldd ./build/pipeline_cli | grep "not found"

# Install missing packages
sudo apt search lib<name>
sudo apt install lib<name>-dev

# Update library cache
sudo ldconfig

# Add custom library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Hardware-Specific Issues

#### 1. Camera Access Problems

**Error:**
```
Error: Failed to open camera device
```

**Debugging:**
```bash
# List available cameras
ls /dev/video*
v4l2-ctl --list-devices

# Test camera functionality
ffmpeg -f v4l2 -i /dev/video0 -t 5 test.mp4

# Check camera capabilities
v4l2-ctl -d /dev/video0 --all
```

#### 2. GPU Acceleration Issues

**Error:**
```
Error: CUDA initialization failed
```

**Debugging:**
```bash
# Check CUDA installation
nvidia-smi
nvcc --version

# Verify GPU access
ls -la /dev/nvidia*

# Check driver compatibility
cat /proc/driver/nvidia/version
```

## Logging and Monitoring

### Enhanced Logging

```cpp
// Enable comprehensive logging
class DiagnosticLogger {
public:
    static void EnableDiagnostics() {
        Logger::SetLevel(LogLevel::Debug);
        Logger::EnableTimestamp(true);
        Logger::EnableThreadId(true);
        Logger::EnableFunction(true);
    }
    
    static void LogSystemInfo() {
        VP_LOG_INFO("=== System Information ===");
        VP_LOG_INFO_F("OS: {}", GetOSInfo());
        VP_LOG_INFO_F("CPU: {} cores", GetCPUCount());
        VP_LOG_INFO_F("Memory: {} MB", GetMemorySize() / 1024 / 1024);
        VP_LOG_INFO_F("Build: {} {}", __DATE__, __TIME__);
    }
};
```

### Crash Handling

```cpp
// Install crash handler for better diagnostics
#include <signal.h>
#include <execinfo.h>

void CrashHandler(int sig) {
    VP_LOG_FATAL_F("Crashed with signal {}", sig);
    
    // Print stack trace
    void* array[10];
    size_t size = backtrace(array, 10);
    char** strings = backtrace_symbols(array, size);
    
    VP_LOG_FATAL("Stack trace:");
    for (size_t i = 0; i < size; i++) {
        VP_LOG_FATAL_F("  {}: {}", i, strings[i]);
    }
    
    free(strings);
    abort();
}

void InstallCrashHandler() {
    signal(SIGSEGV, CrashHandler);
    signal(SIGABRT, CrashHandler);
    signal(SIGFPE, CrashHandler);
}
```

## Getting Help

### Information to Collect

When reporting issues, include:

1. **System Information**
   ```bash
   uname -a
   lsb_release -a
   gcc --version
   cmake --version
   ```

2. **Build Information**
   ```bash
   cmake --version
   make --version
   git log --oneline -5
   ```

3. **Runtime Information**
   ```bash
   ldd ./build/pipeline_cli
   ./build/pipeline_cli --version
   cat /proc/cpuinfo | grep "model name" | head -1
   free -h
   ```

4. **Error Details**
   - Full error message
   - Configuration file
   - Command line used
   - Log output with debug enabled

### Support Channels

- **GitHub Issues**: For bugs and feature requests
- **Discussions**: For questions and help
- **Documentation**: Check existing docs first
- **Stack Overflow**: Tag with `video-pipeline-framework`

This troubleshooting guide should help resolve most common issues encountered with the video pipeline framework.