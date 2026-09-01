#pragma once
#include "lightnet/core/buffer.h"
#include "lightnet/core/channel.h"
#include "lightnet/core/event_loop.h"
#include <atomic>
#include <coroutine>
#include <functional>
#include <memory>
#include <string>
namespace lightnet {
class ConnectionManager;
/// @brief TCP 连接状态
enum class ConnectionState {
    kConnecting,    ///< 已创建，尚未 enable_reading
    kConnected,     ///< 正常通信中
    kDisconnecting, ///< 正在优雅关闭（等待发送缓冲区清空）
    kDisconnected,  ///< 已断开
};
/// @brief TCP 连接
/// 封装单个 fd 的读写、Channel 事件、协程 I/O 接口
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
    using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&, Buffer*)>;
    using WriteCompleteCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
    using CloseCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
    TcpConnection(EventLoop* loop, int fd, uint64_t conn_id,
                  const std::string& local_addr, const std::string& peer_addr);
    ~TcpConnection();
    /// @brief 发送数据（线程安全，内部 run_in_loop）
    void send(const void* data, size_t len);
    void send(const std::string& message);
    /// @brief 优雅关闭：发完 output_buffer 后 shutdown 写端
    void shutdown();
    /// @brief 强制关闭连接
    void force_close();
    void set_connection_callback(ConnectionCallback cb) { connection_callback_ = std::move(cb); }
    void set_message_callback(MessageCallback cb) { message_callback_ = std::move(cb); }
    void set_write_complete_callback(WriteCompleteCallback cb) { write_complete_callback_ = std::move(cb); }
    void set_close_callback(CloseCallback cb) { close_callback_ = std::move(cb); }
    /// @brief 连接建立：enable 读并触发 connection_callback_
    void connect_established();
    /// @brief 连接销毁：标记 kDisconnected
    void connect_destroyed();
    EventLoop* loop() const { return loop_; }           ///< 所属 EventLoop
    int fd() const { return channel_->fd(); }           ///< socket fd
    uint64_t id() const { return conn_id_; }           ///< 连接唯一 ID
    ConnectionState state() const { return state_; }    ///< 当前状态
    Buffer* input_buffer() { return &input_buffer_; }   ///< 输入缓冲区
    Buffer* output_buffer() { return &output_buffer_; }   ///< 输出缓冲区
    const std::string& local_addr() const { return local_addr_; }  ///< 本地地址
    const std::string& peer_addr() const { return peer_addr_; }    ///< 对端地址
    bool is_connected() const { return state_ == ConnectionState::kConnected; }
    // --- 协程 I/O 接口 ---
    /// @brief 尝试从 fd 读入 input_buffer_，返回可读字节数
    ssize_t read_some();
    /// @brief 写入数据，若立即写完返回 len，否则返回 0
    ssize_t write_some(const char* data, size_t len);
    /// @brief 注册等待读的协程句柄，数据到达时 resume
    void resume_read_coro(std::coroutine_handle<> h);
    /// @brief 注册等待写的协程句柄，发送完成时 resume
    void resume_write_coro(std::coroutine_handle<> h);
private:
    void handle_read();   ///< 读事件：读 fd 或唤醒读协程/消息回调
    void handle_write();  ///< 写事件：写 output_buffer 或唤醒写协程
    void handle_close();  ///< 关闭连接并触发 close_callback_
    void handle_error();  ///< 错误时关闭连接
    void send_in_loop(const void* data, size_t len);  ///< 在 loop 线程内发送
    EventLoop* loop_;              ///< 所属 EventLoop
    const uint64_t conn_id_;       ///< 连接 ID
    ConnectionState state_;        ///< 连接状态
    std::unique_ptr<Channel> channel_;  ///< fd 的 Channel
    Buffer input_buffer_;               ///< 接收缓冲区
    Buffer output_buffer_;              ///< 发送缓冲区
    std::string local_addr_;  ///< 本地 ip:port
    std::string peer_addr_;   ///< 对端 ip:port
    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    WriteCompleteCallback write_complete_callback_;
    CloseCallback close_callback_;
    std::coroutine_handle<> read_coro_;   ///< 等待读的协程
    std::coroutine_handle<> write_coro_;  ///< 等待写的协程
    bool read_coro_pending_ = false;      ///< 是否有协程在等待读
    bool write_coro_pending_ = false;     ///< 是否有协程在等待写
};
}  // namespace lightnet
