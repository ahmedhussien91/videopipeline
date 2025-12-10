#include "video_pipeline/threading.h"
#include "video_pipeline/logger.h"

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#endif

namespace video_pipeline {

// ThreadPool implementation
ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // fallback
    }
    
    threads_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::WorkerThread, this);
    }
    
    VP_LOG_INFO_F("ThreadPool created with {} threads", num_threads);
}

ThreadPool::~ThreadPool() {
    Shutdown();
}

size_t ThreadPool::GetPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::WaitForAll() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    condition_.wait(lock, [this] { return tasks_.empty(); });
}

void ThreadPool::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    threads_.clear();
    VP_LOG_DEBUG("ThreadPool shutdown complete");
}

void ThreadPool::WorkerThread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            
            if (stop_ && tasks_.empty()) {
                break;
            }
            
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }
        
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                VP_LOG_ERROR_F("Exception in thread pool task: {}", e.what());
            }
        }
    }
}

// Precision sleep functions
void PreciseSleep(const std::chrono::microseconds& duration) {
    auto start = std::chrono::steady_clock::now();
    auto target = start + duration;
    
    // Use a combination of sleep and busy waiting for precision
    if (duration > std::chrono::milliseconds(1)) {
        std::this_thread::sleep_for(duration - std::chrono::microseconds(500));
    }
    
    // Busy wait for the remaining time
    while (std::chrono::steady_clock::now() < target) {
        std::this_thread::yield();
    }
}

void PreciseSleep(const std::chrono::milliseconds& duration) {
    PreciseSleep(std::chrono::duration_cast<std::chrono::microseconds>(duration));
}

// CPU Affinity implementation
#ifdef __linux__
bool CPUAffinity::SetThreadAffinity(std::thread& thread, const std::vector<int>& cpu_cores) {
    if (cpu_cores.empty()) return false;
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    for (int core : cpu_cores) {
        if (core >= 0 && core < CPU_SETSIZE) {
            CPU_SET(core, &cpuset);
        }
    }
    
    pthread_t native_handle = thread.native_handle();
    int result = pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset);
    
    if (result != 0) {
        VP_LOG_ERROR_F("Failed to set thread affinity: {}", strerror(result));
        return false;
    }
    
    return true;
}

bool CPUAffinity::SetCurrentThreadAffinity(const std::vector<int>& cpu_cores) {
    if (cpu_cores.empty()) return false;
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    for (int core : cpu_cores) {
        if (core >= 0 && core < CPU_SETSIZE) {
            CPU_SET(core, &cpuset);
        }
    }
    
    int result = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    
    if (result != 0) {
        VP_LOG_ERROR_F("Failed to set current thread affinity: {}", strerror(errno));
        return false;
    }
    
    return true;
}

std::vector<int> CPUAffinity::GetAvailableCores() {
    std::vector<int> cores;
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    if (sched_getaffinity(0, sizeof(cpu_set_t), &cpuset) == 0) {
        for (int i = 0; i < CPU_SETSIZE; ++i) {
            if (CPU_ISSET(i, &cpuset)) {
                cores.push_back(i);
            }
        }
    }
    
    return cores;
}

int CPUAffinity::GetCoreCount() {
    return std::thread::hardware_concurrency();
}

bool CPUAffinity::SetThreadPriority(std::thread& thread, int priority) {
    pthread_t native_handle = thread.native_handle();
    
    struct sched_param param;
    param.sched_priority = priority;
    
    int policy = (priority > 0) ? SCHED_RR : SCHED_OTHER;
    int result = pthread_setschedparam(native_handle, policy, &param);
    
    if (result != 0) {
        VP_LOG_ERROR_F("Failed to set thread priority: {}", strerror(result));
        return false;
    }
    
    return true;
}

bool CPUAffinity::SetCurrentThreadPriority(int priority) {
    struct sched_param param;
    param.sched_priority = priority;
    
    int policy = (priority > 0) ? SCHED_RR : SCHED_OTHER;
    int result = sched_setscheduler(0, policy, &param);
    
    if (result != 0) {
        VP_LOG_ERROR_F("Failed to set current thread priority: {}", strerror(errno));
        return false;
    }
    
    return true;
}

#else
// Non-Linux platforms - stub implementations
bool CPUAffinity::SetThreadAffinity(std::thread&, const std::vector<int>&) {
    VP_LOG_WARNING("Thread affinity not supported on this platform");
    return false;
}

bool CPUAffinity::SetCurrentThreadAffinity(const std::vector<int>&) {
    VP_LOG_WARNING("Thread affinity not supported on this platform");
    return false;
}

std::vector<int> CPUAffinity::GetAvailableCores() {
    int core_count = std::thread::hardware_concurrency();
    std::vector<int> cores;
    for (int i = 0; i < core_count; ++i) {
        cores.push_back(i);
    }
    return cores;
}

int CPUAffinity::GetCoreCount() {
    return std::thread::hardware_concurrency();
}

bool CPUAffinity::SetThreadPriority(std::thread&, int) {
    VP_LOG_WARNING("Thread priority not supported on this platform");
    return false;
}

bool CPUAffinity::SetCurrentThreadPriority(int) {
    VP_LOG_WARNING("Thread priority not supported on this platform");
    return false;
}

#endif

} // namespace video_pipeline