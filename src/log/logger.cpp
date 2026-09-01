#include "lightnet/log/logger.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lightnet {

const char* log_level_string(LogLevel level) {
    switch (level) {
        case LogLevel::kTrace: return "TRACE";
        case LogLevel::kDebug: return "DEBUG";
        case LogLevel::kInfo:  return "INFO";
        case LogLevel::kWarn:  return "WARN";
        case LogLevel::kError: return "ERROR";
    }
    return "UNKNOWN";
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() : level_(LogLevel::kInfo), running_(true) {
    writer_thread_ = std::thread([this] { background_write(); });
}

Logger::~Logger() {
    running_ = false;
    cv_.notify_all();
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
}

void Logger::log(LogLevel level, const char* file, int line, const std::string& message) {
    if (level < level_) return;
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << " [" << log_level_string(level) << "] "
        << file << ':' << line << " " << message;
    {
        std::lock_guard lock(mutex_);
        queue_.push(oss.str());
    }
    cv_.notify_one();
}

void Logger::flush() {
    cv_.notify_one();
}

void Logger::background_write() {
    while (running_) {
        std::queue<std::string> batch;
        {
            std::unique_lock lock(mutex_);
            // 最多等待 100ms，或队列非空/退出时唤醒
            cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !queue_.empty() || !running_;
            });
            batch.swap(queue_);
        }
        while (!batch.empty()) {
            std::cout << batch.front() << std::endl;
            batch.pop();
        }
    }
}

}  // namespace lightnet
