#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

#include <string>

namespace lightnet {

/// @brief 底层 socket 系统调用封装
namespace sockets {

/// @brief 创建非阻塞 TCP socket，失败则 abort
int create_nonblocking_or_die();
/// @brief 创建非阻塞 UDP socket，失败则 abort
int create_nonblocking_udp_or_die();
/// @brief 设置 SO_REUSEADDR
void set_reuse_addr(int fd, bool on);
/// @brief 设置 SO_REUSEPORT（平台不支持则忽略）
void set_reuse_port(int fd, bool on);
/// @brief 设置 TCP_NODELAY（禁用 Nagle）
void set_tcp_nodelay(int fd, bool on);
/// @brief bind，失败 abort
void bind_or_die(int fd, const struct sockaddr_in* addr);
/// @brief listen，失败 abort
void listen_or_die(int fd);
/// @brief accept4 非阻塞接受连接，失败返回 -1
int accept(int fd, struct sockaddr_in* addr);
/// @brief connect，失败 abort
void connect_or_die(int fd, const struct sockaddr_in* addr);
/// @brief read 包装
ssize_t read(int fd, void* buf, size_t count);
/// @brief write 包装
ssize_t write(int fd, const void* buf, size_t count);
/// @brief recvfrom 读取 UDP 报文，返回字节数；-1 表示错误（含 EAGAIN）
ssize_t recvfrom(int fd, void* buf, size_t count, struct sockaddr_in* peer);
/// @brief sendto 发送 UDP 报文到指定对端，返回字节数；-1 表示错误
ssize_t sendto(int fd, const void* buf, size_t count, const struct sockaddr_in* peer);
/// @brief close fd
void close(int fd);
/// @brief shutdown 写端（半关闭）
void shutdown_write(int fd);
/// @brief sockaddr_in 转为 "ip:port" 字符串
std::string to_ip_port(const struct sockaddr_in* addr);
/// @brief 仅提取 IP 字符串
std::string to_ip(const struct sockaddr_in* addr);
/// @brief 提取端口号（主机字节序）
uint16_t port(const struct sockaddr_in* addr);

/// @brief 获取 socket 本地地址
struct sockaddr_in get_local_addr(int fd);
/// @brief 获取 socket 对端地址
struct sockaddr_in get_peer_addr(int fd);

}  // namespace sockets

}  // namespace lightnet
