#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>
#include <future>
#include <memory>

namespace video_pipeline {

/**
 * @brief Thread pool for executing tasks
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    // Submit a task to the thread pool
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // Get number of threads
    size_t GetThreadCount() const { return threads_.size(); }
    
    // Get number of pending tasks
    size_t GetPendingTaskCount() const;
    
    // Wait for all tasks to complete
    void WaitForAll();
    
    // Shutdown the thread pool
    void Shutdown();
    
private:
    void WorkerThread();
    
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
};

/**
 * @brief Thread-safe queue template
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    
    // Add element to queue
    void Push(T item);
    
    // Try to pop element (non-blocking)
    bool TryPop(T& item);
    
    // Pop element (blocking)
    T WaitAndPop();
    
    // Pop element with timeout
    bool WaitAndPop(T& item, const std::chrono::milliseconds& timeout);
    
    // Check if queue is empty
    bool Empty() const;
    
    // Get queue size
    size_t Size() const;
    
    // Clear the queue
    void Clear();
    
private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condition_;
};

/**
 * @brief Scoped thread that automatically joins on destruction
 */
class ScopedThread {
public:
    template<typename Callable, typename... Args>
    explicit ScopedThread(Callable&& func, Args&&... args)
        : thread_(std::forward<Callable>(func), std::forward<Args>(args)...) {}
    
    ~ScopedThread() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    
    // Non-copyable, movable
    ScopedThread(const ScopedThread&) = delete;
    ScopedThread& operator=(const ScopedThread&) = delete;
    ScopedThread(ScopedThread&&) = default;
    ScopedThread& operator=(ScopedThread&&) = default;
    
    std::thread& GetThread() { return thread_; }
    const std::thread& GetThread() const { return thread_; }
    
private:
    std::thread thread_;
};

/**
 * @brief High-precision sleep function
 */
void PreciseSleep(const std::chrono::microseconds& duration);
void PreciseSleep(const std::chrono::milliseconds& duration);

/**
 * @brief CPU affinity utilities (Linux-specific)
 */
class CPUAffinity {
public:
    // Set thread affinity to specific CPU cores
    static bool SetThreadAffinity(std::thread& thread, const std::vector<int>& cpu_cores);
    static bool SetCurrentThreadAffinity(const std::vector<int>& cpu_cores);
    
    // Get available CPU cores
    static std::vector<int> GetAvailableCores();
    static int GetCoreCount();
    
    // Thread priority utilities
    static bool SetThreadPriority(std::thread& thread, int priority);
    static bool SetCurrentThreadPriority(int priority);
};

// Template implementations
template<typename F, typename... Args>
auto ThreadPool::Submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    
    std::future<return_type> result = task->get_future();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        tasks_.emplace([task](){ (*task)(); });
    }
    
    condition_.notify_one();
    return result;
}

template<typename T>
void ThreadSafeQueue<T>::Push(T item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(item));
    condition_.notify_one();
}

template<typename T>
bool ThreadSafeQueue<T>::TryPop(T& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return false;
    }
    item = std::move(queue_.front());
    queue_.pop();
    return true;
}

template<typename T>
T ThreadSafeQueue<T>::WaitAndPop() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return !queue_.empty(); });
    
    T result = std::move(queue_.front());
    queue_.pop();
    return result;
}

template<typename T>
bool ThreadSafeQueue<T>::WaitAndPop(T& item, const std::chrono::milliseconds& timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (condition_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    return false;
}

template<typename T>
bool ThreadSafeQueue<T>::Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

template<typename T>
size_t ThreadSafeQueue<T>::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

template<typename T>
void ThreadSafeQueue<T>::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

} // namespace video_pipeline