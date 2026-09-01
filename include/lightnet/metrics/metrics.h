#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace lightnet {

/// @brief 性能监控与 Prometheus 格式指标导出（单例）
class Metrics {
public:
    static Metrics& instance();

    void inc_connections_total() { connections_total_.fetch_add(1, std::memory_order_relaxed); }
    void dec_active_connections() { active_connections_.fetch_sub(1, std::memory_order_relaxed); }
    void inc_active_connections() { active_connections_.fetch_add(1, std::memory_order_relaxed); }

    void add_bytes_read(uint64_t n) { bytes_read_.fetch_add(n, std::memory_order_relaxed); }
    void add_bytes_written(uint64_t n) { bytes_written_.fetch_add(n, std::memory_order_relaxed); }
    void inc_requests() { requests_total_.fetch_add(1, std::memory_order_relaxed); }

    /// @brief 记录单次请求处理延迟（微秒）
    void record_request_latency_us(uint64_t us);
    /// @brief 记录 EventLoop 单次迭代耗时（微秒）
    void record_event_loop_lag_us(uint64_t us);

    /// @brief 导出全部指标为 Prometheus text 格式
    std::string export_prometheus() const;

private:
    Metrics() = default;

    /// @brief 直方图，用于延迟类指标
    struct Histogram {
        static constexpr size_t kBuckets = 12;
        std::array<std::atomic<uint64_t>, kBuckets> buckets{};  ///< 各桶计数
        std::atomic<uint64_t> sum{0};   ///< 观测值总和
        std::atomic<uint64_t> count{0}; ///< 观测次数

        void observe(uint64_t value);
        void export_to(std::string& out, const std::string& name) const;
    };

    std::atomic<uint64_t> connections_total_{0};   ///< 累计连接数 counter
    std::atomic<uint64_t> active_connections_{0};  ///< 当前活跃连接 gauge
    std::atomic<uint64_t> bytes_read_{0};          ///< 累计读字节 counter
    std::atomic<uint64_t> bytes_written_{0};       ///< 累计写字节 counter
    std::atomic<uint64_t> requests_total_{0};      ///< 累计请求数 counter

    Histogram request_latency_;   ///< 请求延迟直方图
    Histogram event_loop_lag_;    ///< EventLoop 滞后直方图

    mutable std::mutex mutex_;  ///< 导出时保护（预留）
};

}  // namespace lightnet
