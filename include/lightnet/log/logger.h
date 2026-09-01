#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace lightnet {

/// @brief 日志级别
enum class LogLevel {
    kTrace = 0,
    kDebug = 1,
    kInfo = 2,
    kWarn = 3,
    kError = 4,
};

/// @brief 异步日志系统（单例）
/// 业务线程入队，后台线程批量写 stdout
class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level) { level_ = level; }  ///< 设置最低输出级别
    LogLevel level() const { return level_; }

    /// @brief 格式化并入队日志（含时间戳、级别、文件行号）
    void log(LogLevel level, const char* file, int line, const std::string& message);
    /// @brief 唤醒后台写线程
    void flush();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /// @brief 后台线程：从 queue_ 取日志写 stdout
    void background_write();

    LogLevel level_;                    ///< 当前最低日志级别
    std::queue<std::string> queue_;     ///< 待写日志队列
    std::mutex mutex_;                  ///< 保护 queue_
    std::condition_variable cv_;        ///< 通知后台线程
    std::thread writer_thread_;         ///< 后台写线程
    std::atomic<bool> running_;         ///< 是否运行中
};

/// @brief 日志级别转字符串
const char* log_level_string(LogLevel level);

#define LN_LOG(level, msg) \
    ::lightnet::Logger::instance().log(level, __FILE__, __LINE__, msg)

#define LN_TRACE(msg) LN_LOG(::lightnet::LogLevel::kTrace, msg)
#define LN_DEBUG(msg) LN_LOG(::lightnet::LogLevel::kDebug, msg)
#define LN_INFO(msg)  LN_LOG(::lightnet::LogLevel::kInfo, msg)
#define LN_WARN(msg)  LN_LOG(::lightnet::LogLevel::kWarn, msg)
#define LN_ERROR(msg) LN_LOG(::lightnet::LogLevel::kError, msg)

}  // namespace lightnet
