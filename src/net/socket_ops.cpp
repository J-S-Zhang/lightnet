#include "lightnet/net/socket_ops.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace lightnet {
namespace sockets {

namespace {

// 致命错误：打印 errno 信息后终止进程
void die_or_log(const char* msg) {
    std::perror(msg);
    std::abort();
}

}  // namespace

// 创建 IPv4 TCP socket，非阻塞 + close-on-exec，供 Reactor 使用
int create_nonblocking_or_die() {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd < 0) die_or_log("socket");
    return fd;
}

// 创建 IPv4 UDP socket，非阻塞 + close-on-exec
int create_nonblocking_udp_or_die() {
    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
    if (fd < 0) die_or_log("socket(UDP)");
    return fd;
}

// SO_REUSEADDR：允许快速重启时复用 TIME_WAIT 状态的端口
void set_reuse_addr(int fd, bool on) {
    int opt = on ? 1 : 0;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
}

// SO_REUSEPORT：多进程/多线程可绑定同一端口（平台不支持则跳过）
void set_reuse_port(int fd, bool on) {
#ifdef SO_REUSEPORT
    int opt = on ? 1 : 0;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof opt);
#else
    (void)fd;
    (void)on;
#endif
}

// TCP_NODELAY：关闭 Nagle 算法，小包立即发送，降低延迟
void set_tcp_nodelay(int fd, bool on) {
    int opt = on ? 1 : 0;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof opt);
}

// 将 fd 绑定到指定 IP:端口，失败则 abort
void bind_or_die(int fd, const struct sockaddr_in* addr) {
    if (::bind(fd, reinterpret_cast<const struct sockaddr*>(addr), sizeof(*addr)) < 0) {
        die_or_log("bind");
    }
}

// 将 fd 设为监听状态，backlog 取系统允许的最大值
void listen_or_die(int fd) {
    if (::listen(fd, SOMAXCONN) < 0) {
        die_or_log("listen");
    }
}

// 非阻塞 accept 新连接，返回 conn_fd；无连接或出错返回 -1
int accept(int fd, struct sockaddr_in* addr) {
    socklen_t len = sizeof(*addr);
    int connfd = ::accept4(fd, reinterpret_cast<struct sockaddr*>(addr),
                           &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    return connfd;
}

// 主动连接远端 IP:端口（客户端用），失败则 abort
void connect_or_die(int fd, const struct sockaddr_in* addr) {
    if (::connect(fd, reinterpret_cast<const struct sockaddr*>(addr), sizeof(*addr)) < 0) {
        die_or_log("connect");
    }
}

// 从 fd 读取数据到 buf，返回字节数；0=EOF，-1=错误
ssize_t read(int fd, void* buf, size_t count) {
    return ::read(fd, buf, count);
}

// 向 fd 写入数据，返回实际写入字节数；-1=错误
ssize_t write(int fd, const void* buf, size_t count) {
    return ::write(fd, buf, count);
}

// 从 UDP socket 读取一个报文及发送方地址
ssize_t recvfrom(int fd, void* buf, size_t count, struct sockaddr_in* peer) {
    socklen_t len = sizeof(*peer);
    return ::recvfrom(fd, buf, count, 0,
                      reinterpret_cast<struct sockaddr*>(peer), &len);
}

// 向指定对端发送 UDP 报文
ssize_t sendto(int fd, const void* buf, size_t count, const struct sockaddr_in* peer) {
    return ::sendto(fd, buf, count, MSG_NOSIGNAL,
                    reinterpret_cast<const struct sockaddr*>(peer), sizeof(*peer));
}

// 关闭 fd，释放内核资源
void close(int fd) {
    ::close(fd);
}

// 半关闭：关闭写端，对端仍可读剩余数据（优雅关闭发送方向）
void shutdown_write(int fd) {
    ::shutdown(fd, SHUT_WR);
}

// 将 sockaddr_in 格式化为 "192.168.1.1:8080" 字符串
std::string to_ip_port(const struct sockaddr_in* addr) {
    char buf[64];
    ::inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof buf);
    return std::string(buf) + ":" + std::to_string(ntohs(addr->sin_port));
}

// 仅提取 IP 字符串，不含端口
std::string to_ip(const struct sockaddr_in* addr) {
    char buf[64];
    ::inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof buf);
    return buf;
}

// 提取端口号（网络字节序转主机字节序）
uint16_t port(const struct sockaddr_in* addr) {
    return ntohs(addr->sin_port);
}

// 获取 socket 本端地址（getsockname）
struct sockaddr_in get_local_addr(int fd) {
    struct sockaddr_in addr {};
    socklen_t len = sizeof addr;
    ::getsockname(fd, reinterpret_cast<struct sockaddr*>(&addr), &len);
    return addr;
}

// 获取 socket 对端地址（getpeername）
struct sockaddr_in get_peer_addr(int fd) {
    struct sockaddr_in addr {};
    socklen_t len = sizeof addr;
    ::getpeername(fd, reinterpret_cast<struct sockaddr*>(&addr), &len);
    return addr;
}

}  // namespace sockets
}  // namespace lightnet
