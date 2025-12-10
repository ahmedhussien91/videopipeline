#include "video_pipeline/logger.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <chrono>

namespace video_pipeline {

// Static members
std::shared_ptr<ILogger> Logger::instance_;
std::mutex Logger::mutex_;

// ConsoleLogger implementation
ConsoleLogger::ConsoleLogger(LogLevel level) : level_(level) {}

void ConsoleLogger::Log(LogLevel level, const std::string& message) {
    if (level < level_) return;
    
    std::string timestamp = GetTimestamp();
    std::string level_str = LevelToString(level);
    
    // Use appropriate stream based on log level
    auto& stream = (level >= LogLevel::ERROR) ? std::cerr : std::cout;
    
    stream << "[" << timestamp << "] [" << level_str << "] " << message << std::endl;
}

std::string ConsoleLogger::LevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string ConsoleLogger::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// FileLogger implementation
FileLogger::FileLogger(const std::string& filename, LogLevel level)
    : filename_(filename), level_(level) {
    file_ = std::make_unique<std::ofstream>(filename, std::ios::app);
}

FileLogger::~FileLogger() {
    if (file_ && file_->is_open()) {
        file_->close();
    }
}

void FileLogger::Log(LogLevel level, const std::string& message) {
    if (level < level_ || !file_ || !file_->is_open()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string timestamp = GetTimestamp();
    std::string level_str = LevelToString(level);
    
    *file_ << "[" << timestamp << "] [" << level_str << "] " << message << std::endl;
    file_->flush();
}

bool FileLogger::IsOpen() const {
    return file_ && file_->is_open();
}

void FileLogger::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_) {
        file_->flush();
    }
}

std::string FileLogger::LevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string FileLogger::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// Logger static methods
void Logger::SetLogger(std::shared_ptr<ILogger> logger) {
    std::lock_guard<std::mutex> lock(mutex_);
    instance_ = logger;
}

std::shared_ptr<ILogger> Logger::GetLogger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance_) {
        instance_ = std::make_shared<ConsoleLogger>();
    }
    return instance_;
}

void Logger::Debug(const std::string& message) {
    Log(LogLevel::DEBUG, message);
}

void Logger::Info(const std::string& message) {
    Log(LogLevel::INFO, message);
}

void Logger::Warning(const std::string& message) {
    Log(LogLevel::WARNING, message);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::ERROR, message);
}

void Logger::Fatal(const std::string& message) {
    Log(LogLevel::FATAL, message);
}

void Logger::Log(LogLevel level, const std::string& message) {
    auto logger = GetLogger();
    if (logger) {
        logger->Log(level, message);
    }
}

} // namespace video_pipeline