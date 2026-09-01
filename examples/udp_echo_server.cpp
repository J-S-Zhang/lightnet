/// @file udp_echo_server.cpp
/// @brief UDP Echo 服务器示例
///
/// 功能：收到 UDP 报文后原样发回发送方
/// 端口：9091（与 TCP echo 8080、RPC 9090 区分）
///
/// 编译运行：
///   cmake --build build -j --target udp_echo_server
///   ./build/examples/udp_echo_server
///
/// 测试（Linux VM）：
///   echo hello | nc -u 127.0.0.1 9091

#include "lightnet/lightnet.h"

#include <csignal>

using namespace lightnet;

static EventLoop* g_loop = nullptr;

void signal_handler(int) {
    if (g_loop) {
        g_loop->quit();
    }
}

void on_udp_message(const char* data, size_t len, const sockaddr_in& peer, UdpServer* server) {
    LN_INFO("udp recv from " + sockets::to_ip_port(&peer) +
            " len=" + std::to_string(len));
    server->send_to(data, len, peer);
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    EventLoop loop;
    g_loop = &loop;

    UdpServer server(&loop, "0.0.0.0", 9091);
    server.set_message_callback(on_udp_message);
    server.start();

    LN_INFO("UDP echo server listening on 0.0.0.0:9091");
    loop.loop();
    return 0;
}
