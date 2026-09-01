#pragma once
#include "lightnet/core/event_loop.h"
#include "lightnet/core/event_loop_thread_pool.h"
#include "lightnet/net/tcp_connection.h"
#include <netinet/in.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
namespace lightnet {
class ConnectionManager;
/// @brief TCP 服务端
/// 负责 listen/accept、将新连接分配到子 EventLoop，并转发上层回调
class TcpServer {
public:
    using ConnectionCallback = TcpConnection::ConnectionCallback;  ///< 新连接建立回调
    using MessageCallback = TcpConnection::MessageCallback;          ///< 收到数据回调
    /// @brief 创建监听 socket 并注册 accept Channel
    TcpServer(EventLoop* loop, const std::string& ip, uint16_t port,
              const std::string& name, bool reuse_port = true);
    ~TcpServer();
    /// @brief 设置 I/O 子线程数量（主从 Reactor）
    void set_thread_num(int num_threads);
    /// @brief 启动线程池并开始 accept
    void start();
    /// @brief 设置连接建立时的回调（协程模式下在此 spawn 业务逻辑）
    void set_connection_callback(ConnectionCallback cb) { connection_callback_ = std::move(cb); }
    /// @brief 设置收到消息时的回调（传统回调模式）
    void set_message_callback(MessageCallback cb) { message_callback_ = std::move(cb); }
    EventLoop* loop() const { return loop_; }       ///< 主 EventLoop
    const std::string& name() const { return name_; }  ///< 服务名称
    ConnectionManager* connection_manager() { return conn_manager_.get(); }  ///< 连接管理器
private:
    /// @brief accept 后创建 TcpConnection 并投递到子 Loop
    void new_connection(int fd, const struct sockaddr_in& peer_addr);
    /// @brief 连接关闭时从管理器移除
    void remove_connection(const std::shared_ptr<TcpConnection>& conn);
    /// @brief 转发 connection_callback_
    void on_connection(const std::shared_ptr<TcpConnection>& conn);
    /// @brief 转发 message_callback_
    void on_message(const std::shared_ptr<TcpConnection>& conn, Buffer* buf);
    EventLoop* loop_;                              ///< 主 Reactor，负责 accept
    const std::string name_;                       ///< 服务名
    std::unique_ptr<Channel> accept_channel_;      ///< listen_fd 的 Channel
    int listen_fd_;                                ///< 监听 socket fd
    std::unique_ptr<EventLoopThreadPool> thread_pool_;  ///< 子 I/O 线程池
    std::unique_ptr<ConnectionManager> conn_manager_;   ///< 连接生命周期管理
    ConnectionCallback connection_callback_;  ///< 上层连接回调
    MessageCallback message_callback_;      ///< 上层消息回调
    std::atomic<bool> started_;  ///< 是否已启动（防止重复 start）
};
}  // namespace lightnet
