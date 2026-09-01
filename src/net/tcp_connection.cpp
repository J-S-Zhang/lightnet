#include "lightnet/net/tcp_connection.h"

#include "lightnet/log/logger.h"
#include "lightnet/metrics/metrics.h"
#include "lightnet/net/socket_ops.h"

#include <errno.h>

namespace lightnet {

// 绑定 conn_fd 到 Channel，注册读/写/关闭/错误四类事件回调
TcpConnection::TcpConnection(EventLoop* loop, int fd, uint64_t conn_id,
                             const std::string& local_addr, const std::string& peer_addr)
    : loop_(loop),
      conn_id_(conn_id),
      state_(ConnectionState::kConnecting),
      channel_(std::make_unique<Channel>(loop, fd)),
      local_addr_(local_addr),
      peer_addr_(peer_addr) {
    channel_->set_read_callback([this] { handle_read(); });
    channel_->set_write_callback([this] { handle_write(); });
    channel_->set_close_callback([this] { handle_close(); });
    channel_->set_error_callback([this] { handle_error(); });
}

// 析构时关闭 socket fd
TcpConnection::~TcpConnection() {
    if (channel_->fd() >= 0) {
        sockets::close(channel_->fd());
    }
}

// 发送入口（线程安全）：仅在已连接状态下发送，最终统一到 loop 线程的 send_in_loop
void TcpConnection::send(const void* data, size_t len) {
    // 非 kConnected 状态（断开中/已断开）直接丢弃，避免向无效 fd 写数据
    if (state_ == ConnectionState::kConnected) {
        if (loop_->is_in_loop_thread()) {
            // 已在所属 EventLoop 线程：直接写 socket / 进 output_buffer
            send_in_loop(data, len);
        } else {
            // 其他线程调用：必须拷贝数据，因为原 buffer 可能在 send 返回后被释放
            std::string copy(static_cast<const char*>(data), len);
            // 投递到 loop 线程执行；lambda 按值捕获 copy，保证异步执行时数据仍有效
            loop_->run_in_loop([this, copy = std::move(copy)] {
                send_in_loop(copy.data(), copy.size());
            });
        }
    }
}

void TcpConnection::send(const std::string& message) {
    send(message.data(), message.size());
}

// 优雅关闭：发完 output_buffer 后 shutdown 写端
void TcpConnection::shutdown() {
    if (state_ == ConnectionState::kConnected) {
        loop_->run_in_loop([this] {
            state_ = ConnectionState::kDisconnecting;
            if (!channel_->is_writing()) {
                sockets::shutdown_write(channel_->fd());
            }
        });
    }
}

// 强制关闭：直接触发 handle_close
void TcpConnection::force_close() {
    if (state_ == ConnectionState::kConnected || state_ == ConnectionState::kConnecting) {
        loop_->run_in_loop([this] { handle_close(); });
    }
}

// accept 完成后调用：开启读监听，通知上层 connection_callback
void TcpConnection::connect_established() {
    state_ = ConnectionState::kConnected;
    channel_->enable_reading();
    if (connection_callback_) {
        connection_callback_(shared_from_this());
    }
}

// 连接从 ConnectionManager 移除后标记为已断开
void TcpConnection::connect_destroyed() {
    state_ = ConnectionState::kDisconnected;
}

// fd 可读：read_fd 读入 input_buffer，触发 message_callback 或恢复读协程
void TcpConnection::handle_read() {
    int saved_errno = 0;
    ssize_t n = input_buffer_.read_fd(channel_->fd(), &saved_errno);
    if (n > 0) {
        Metrics::instance().add_bytes_read(static_cast<uint64_t>(n));
        if (read_coro_pending_ && read_coro_) {
            // 协程模式：唤醒 co_await AsyncRead
            read_coro_pending_ = false;
            auto h = read_coro_;
            read_coro_ = {};
            loop_->schedule_coro(h);
        } else if (message_callback_) {
            message_callback_(shared_from_this(), &input_buffer_);
        }
    } else if (n == 0) {
        handle_close();
    } else {
        if (saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
            handle_error();
        }
    }
}

// fd 可写：从 output_buffer 写出数据，写完后 disable_writing 或恢复写协程
void TcpConnection::handle_write() {
    if (channel_->is_writing()) {
        ssize_t n = sockets::write(channel_->fd(), output_buffer_.peek(), output_buffer_.readable_bytes());
        if (n > 0) {
            output_buffer_.retrieve(static_cast<size_t>(n));
            Metrics::instance().add_bytes_written(static_cast<uint64_t>(n));
            if (output_buffer_.readable_bytes() == 0) {
                channel_->disable_writing();
                if (write_complete_callback_) {
                    write_complete_callback_(shared_from_this());
                }
                if (write_coro_pending_ && write_coro_) {
                    write_coro_pending_ = false;
                    auto h = write_coro_;
                    write_coro_ = {};
                    loop_->schedule_coro(h);
                }
                // 优雅关闭：数据发完后 shutdown 写端
                if (state_ == ConnectionState::kDisconnecting) {
                    sockets::shutdown_write(channel_->fd());
                }
            }
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                handle_error();
            }
        }
    }
}

// 关闭连接：disable Channel，回调 close_callback 通知 TcpServer 移除
void TcpConnection::handle_close() {
    state_ = ConnectionState::kDisconnected;
    channel_->disable_all();
    std::shared_ptr<TcpConnection> conn = shared_from_this();
    if (close_callback_) {
        close_callback_(conn);
    }
}

void TcpConnection::handle_error() {
    handle_close();
}

// loop 线程内发送：优先直接 write，写不下则缓存到 output_buffer 并 enable_writing
void TcpConnection::send_in_loop(const void* data, size_t len) {
    ssize_t nwrote = 0;
    // 输出队列空且未监听写事件时，优先尝试直接 write（减少拷贝）
    if (!channel_->is_writing() && output_buffer_.readable_bytes() == 0) {
        nwrote = sockets::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            Metrics::instance().add_bytes_written(static_cast<uint64_t>(nwrote));
            if (static_cast<size_t>(nwrote) >= len) {
                if (write_complete_callback_) {
                    write_complete_callback_(shared_from_this());
                }
                return;
            }
            len -= static_cast<size_t>(nwrote);
            data = static_cast<const char*>(data) + nwrote;
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                handle_error();
                return;
            }
        }
    }
    if (len > 0) {
        // 未写完的部分进 output_buffer，注册可写事件
        output_buffer_.append(static_cast<const char*>(data), len);
        if (!channel_->is_writing()) {
            channel_->enable_writing();
        }
    }
}

// 协程 AsyncRead 用：读 socket 到 input_buffer，返回可读字节数
ssize_t TcpConnection::read_some() {
    if (input_buffer_.readable_bytes() > 0) {
        return static_cast<ssize_t>(input_buffer_.readable_bytes());
    }
    int saved_errno = 0;
    ssize_t n = input_buffer_.read_fd(channel_->fd(), &saved_errno);
    if (n > 0) {
        Metrics::instance().add_bytes_read(static_cast<uint64_t>(n));
    }
    return n;
}

// 协程 AsyncWrite 用：send 后若 output_buffer 已空则认为全部提交
ssize_t TcpConnection::write_some(const char* data, size_t len) {
    send(data, len);
    if (output_buffer_.readable_bytes() == 0) {
        return static_cast<ssize_t>(len);
    }
    return 0;
}

// 注册读协程句柄，等待 handle_read 就绪后 schedule_coro 恢复
void TcpConnection::resume_read_coro(std::coroutine_handle<> h) {
    read_coro_ = h;
    read_coro_pending_ = true;
    if (!channel_->is_reading()) {
        channel_->enable_reading();
    }
}

// 注册写协程句柄，等待 handle_write 发完后 schedule_coro 恢复
void TcpConnection::resume_write_coro(std::coroutine_handle<> h) {
    write_coro_ = h;
    write_coro_pending_ = true;
    if (!channel_->is_writing() && output_buffer_.readable_bytes() > 0) {
        channel_->enable_writing();
    }
}

}  // namespace lightnet
