#include "lightnet/net/udp_server.h"

#include "lightnet/core/channel.h"
#include "lightnet/log/logger.h"
#include "lightnet/net/socket_ops.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace lightnet {

namespace {

constexpr size_t kMaxDatagramSize = 65507;

}  // namespace

UdpServer::UdpServer(EventLoop* loop, const std::string& ip, uint16_t port, bool reuse_port)
    : loop_(loop),
      port_(port),
      udp_fd_(sockets::create_nonblocking_udp_or_die()),
      started_(false) {
    sockets::set_reuse_addr(udp_fd_, true);
    sockets::set_reuse_port(udp_fd_, reuse_port);

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        sockets::close(udp_fd_);
        std::perror("inet_pton");
        std::abort();
    }
    sockets::bind_or_die(udp_fd_, &addr);

    channel_ = std::make_unique<Channel>(loop_, udp_fd_);
    channel_->set_read_callback([this] { handle_read(); });
    channel_->set_error_callback([this] {
        LN_ERROR("UdpServer socket error on fd=" + std::to_string(udp_fd_));
    });
}

UdpServer::~UdpServer() {
    if (udp_fd_ < 0) {
        return;
    }
    loop_->run_in_loop([this] {
        if (channel_) {
            channel_->disable_all();
            channel_->remove();
        }
        sockets::close(udp_fd_);
        udp_fd_ = -1;
    });
}

void UdpServer::start() {
    if (started_.exchange(true)) {
        return;
    }
    loop_->run_in_loop([this] {
        channel_->enable_reading();
    });
}

void UdpServer::send_to(const void* data, size_t len, const sockaddr_in& peer) {
    if (udp_fd_ < 0 || len == 0) {
        return;
    }

    std::string copy(static_cast<const char*>(data), len);
    sockaddr_in peer_copy = peer;

    loop_->run_in_loop([this, copy = std::move(copy), peer_copy]() mutable {
        if (udp_fd_ < 0) {
            return;
        }
        const ssize_t n = sockets::sendto(
            udp_fd_, copy.data(), copy.size(), &peer_copy);
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            LN_ERROR("UdpServer sendto failed: " + std::string(std::strerror(errno)));
        }
    });
}

void UdpServer::send_to(const std::string& message, const sockaddr_in& peer) {
    send_to(message.data(), message.size(), peer);
}

void UdpServer::handle_read() {
    char buffer[kMaxDatagramSize];

    while (true) {
        struct sockaddr_in peer {};
        const ssize_t n = sockets::recvfrom(udp_fd_, buffer, sizeof(buffer), &peer);
        if (n > 0) {
            if (message_callback_) {
                message_callback_(buffer, static_cast<size_t>(n), peer, this);
            }
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        LN_ERROR("UdpServer recvfrom failed: " + std::string(std::strerror(errno)));
        break;
    }
}

}  // namespace lightnet
