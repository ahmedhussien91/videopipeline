#pragma once

#include <string>
#include <sstream>
#include <memory>
#include <chrono>
#include <mutex>
#include <fstream>

namespace video_pipeline {

/**
 * @brief Log levels
 */
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

/**
 * @brief Logger interface
 */
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void Log(LogLevel level, const std::string& message) = 0;
    virtual void SetLevel(LogLevel level) = 0;
    virtual LogLevel GetLevel() const = 0;
};

/**
 * @brief Console logger implementation
 */
class ConsoleLogger : public ILogger {
public:
    ConsoleLogger(LogLevel level = LogLevel::INFO);
    void Log(LogLevel level, const std::string& message) override;
    void SetLevel(LogLevel level) override { level_ = level; }
    LogLevel GetLevel() const override { return level_; }

private:
    LogLevel level_;
    std::string LevelToString(LogLevel level) const;
    std::string GetTimestamp() const;
};

/**
 * @brief File logger implementation
 */
class FileLogger : public ILogger {
public:
    FileLogger(const std::string& filename, LogLevel level = LogLevel::INFO);
    ~FileLogger();
    
    void Log(LogLevel level, const std::string& message) override;
    void SetLevel(LogLevel level) override { level_ = level; }
    LogLevel GetLevel() const override { return level_; }
    
    bool IsOpen() const;
    void Flush();

private:
    std::string filename_;
    std::unique_ptr<std::ofstream> file_;
    LogLevel level_;
    std::mutex mutex_;
    
    std::string LevelToString(LogLevel level) const;
    std::string GetTimestamp() const;
};

/**
 * @brief Global logger management
 */
class Logger {
public:
    static void SetLogger(std::shared_ptr<ILogger> logger);
    static std::shared_ptr<ILogger> GetLogger();
    
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    static void Fatal(const std::string& message);
    
    template<typename... Args>
    static void Debug(const std::string& format, Args... args) {
        Log(LogLevel::DEBUG, StringFormat(format, args...));
    }
    
    template<typename... Args>
    static void Info(const std::string& format, Args... args) {
        Log(LogLevel::INFO, StringFormat(format, args...));
    }
    
    template<typename... Args>
    static void Warning(const std::string& format, Args... args) {
        Log(LogLevel::WARNING, StringFormat(format, args...));
    }
    
    template<typename... Args>
    static void Error(const std::string& format, Args... args) {
        Log(LogLevel::ERROR, StringFormat(format, args...));
    }
    
    template<typename... Args>
    static void Fatal(const std::string& format, Args... args) {
        Log(LogLevel::FATAL, StringFormat(format, args...));
    }
    
private:
    static void Log(LogLevel level, const std::string& message);
    
    template<typename... Args>
    static std::string StringFormat(const std::string& format, Args... args) {
        std::ostringstream oss;
        StringFormatHelper(oss, format, args...);
        return oss.str();
    }
    
    template<typename T>
    static void StringFormatHelper(std::ostringstream& oss, const std::string& format, T&& arg) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << arg << format.substr(pos + 2);
        } else {
            oss << format;
        }
    }
    
    template<typename T, typename... Args>
    static void StringFormatHelper(std::ostringstream& oss, const std::string& format, T&& arg, Args... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << arg;
            StringFormatHelper(oss, format.substr(pos + 2), args...);
        } else {
            oss << format;
        }
    }
    
    static std::shared_ptr<ILogger> instance_;
    static std::mutex mutex_;
};

// Convenience macros
#define VP_LOG_DEBUG(msg) video_pipeline::Logger::Debug(msg)
#define VP_LOG_INFO(msg) video_pipeline::Logger::Info(msg)
#define VP_LOG_WARNING(msg) video_pipeline::Logger::Warning(msg)
#define VP_LOG_ERROR(msg) video_pipeline::Logger::Error(msg)
#define VP_LOG_FATAL(msg) video_pipeline::Logger::Fatal(msg)

#define VP_LOG_DEBUG_F(fmt, ...) video_pipeline::Logger::Debug(fmt, __VA_ARGS__)
#define VP_LOG_INFO_F(fmt, ...) video_pipeline::Logger::Info(fmt, __VA_ARGS__)
#define VP_LOG_WARNING_F(fmt, ...) video_pipeline::Logger::Warning(fmt, __VA_ARGS__)
#define VP_LOG_ERROR_F(fmt, ...) video_pipeline::Logger::Error(fmt, __VA_ARGS__)
#define VP_LOG_FATAL_F(fmt, ...) video_pipeline::Logger::Fatal(fmt, __VA_ARGS__)

} // namespace video_pipeline