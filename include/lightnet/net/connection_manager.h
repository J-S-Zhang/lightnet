#pragma once

#include "lightnet/net/tcp_connection.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>

namespace lightnet {

class EventLoop;

/// @brief 连接生命周期管理
/// 维护全局连接表、分配 ID、限制最大连接数、更新 Metrics
class ConnectionManager {
public:
    explicit ConnectionManager(size_t max_connections = 100000);

    /// @brief 创建新连接；超限时返回 nullptr
    std::shared_ptr<TcpConnection> create(EventLoop* loop, int fd,
                                          const std::string& local_addr,
                                          const std::string& peer_addr);

    /// @brief 手动添加连接（备用接口）
    void add(const std::shared_ptr<TcpConnection>& conn);
    /// @brief 移除连接并递减活跃计数
    void remove(const std::shared_ptr<TcpConnection>& conn);

    size_t size() const;  ///< 当前连接数
    size_t max_connections() const { return max_connections_; }  ///< 连接上限

    void set_idle_timeout_ms(uint64_t ms) { idle_timeout_ms_ = ms; }  ///< 空闲超时（预留）
    uint64_t idle_timeout_ms() const { return idle_timeout_ms_; }

    /// @brief 刷新连接活跃时间（空闲超时 hook 预留）
    void touch(const std::shared_ptr<TcpConnection>& conn);

private:
    mutable std::mutex mutex_;  ///< 保护 connections_
    std::unordered_map<uint64_t, std::shared_ptr<TcpConnection>> connections_;  ///< id → 连接
    std::atomic<uint64_t> next_id_{1};  ///< 下一个连接 ID
    size_t max_connections_;            ///< 最大连接数
    uint64_t idle_timeout_ms_ = 0;      ///< 空闲超时毫秒（未实现）
};

}  // namespace lightnet
