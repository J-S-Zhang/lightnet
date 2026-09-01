#pragma once

#include "lightnet/core/event_loop.h"

#include <netinet/in.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace lightnet {

class Channel;

/// @brief UDP 服务端（单 socket，Reactor 驱动）
/// 通过 epoll 监听 udp_fd 的 EPOLLIN，收到报文后回调上层；send_to 向指定对端回复
class UdpServer {
public:
    /// @param data 报文 payload
    /// @param len  长度
    /// @param peer 发送方地址（recvfrom 得到，reply 时原样传回 send_to）
    using MessageCallback = std::function<void(const char* data, size_t len,
                                               const sockaddr_in& peer,
                                               UdpServer* server)>;

    UdpServer(EventLoop* loop, const std::string& ip, uint16_t port, bool reuse_port = true);
    ~UdpServer();

    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;

    /// @brief 注册 EPOLLIN 并开始接收
    void start();
    void set_message_callback(MessageCallback cb) { message_callback_ = std::move(cb); }

    /// @brief 向指定对端发送 UDP 报文（线程安全，内部 run_in_loop）
    void send_to(const void* data, size_t len, const sockaddr_in& peer);
    void send_to(const std::string& message, const sockaddr_in& peer);

    EventLoop* loop() const { return loop_; }
    int fd() const { return udp_fd_; }
    uint16_t port() const { return port_; }

private:
    void handle_read();

    EventLoop* loop_;
    uint16_t port_;
    int udp_fd_;
    std::unique_ptr<Channel> channel_;
    MessageCallback message_callback_;
    std::atomic<bool> started_;
};

}  // namespace lightnet
