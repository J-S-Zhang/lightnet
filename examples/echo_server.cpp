/// @file echo_server.cpp
/// @brief Echo 服务器示例 —— 演示 LightNet 最基础的用法
///
/// 功能：客户端发来什么数据，服务端原样回写（回声）
/// 技术点：EventLoop + TcpServer + C++20 协程 (AsyncRead/AsyncWrite)
///
/// 编译运行：
///   ./build/examples/echo_server
/// 测试：
///   echo hello | nc localhost 8080

#include "lightnet/lightnet.h"

#include <csignal>
#include <iostream>
#include <memory>

using namespace lightnet;

/// 全局 EventLoop 指针，供信号处理函数退出主循环时使用
static EventLoop* g_loop = nullptr;

/// @brief SIGINT (Ctrl+C) 信号处理：优雅退出事件循环
/// @param 信号编号（此处未使用）
void signal_handler(int) {
    if (g_loop) g_loop->quit();  // 设置 quit 标志，loop() 会在下一轮退出
}

/// @brief 单条连接的 Echo 协程会话
/// @param conn 新建立的 TCP 连接（shared_ptr 保证协程运行期间连接不被销毁）
///
/// 流程：循环「读 → 原样写回 → 消费缓冲区」，直到连接关闭或读到 EOF
Task<void> echo_session(std::shared_ptr<TcpConnection> conn) {
    LN_INFO("new connection: " + conn->peer_addr());
    // 获取连接的输入缓冲区引用，AsyncRead 读到的数据会落在这里
    Buffer& buf = *conn->input_buffer();

    while (conn->is_connected()) {
        // 协程挂起点：等待至少 1 字节可读；无数据时挂起，epoll 通知后自动恢复
        co_await AsyncRead{conn.get(), 1};
        // 对端关闭时 read 返回 0，buffer 无数据，退出循环
        if (buf.readable_bytes() == 0) break;

        // 将当前可读的全部字节原样写回客户端
        const size_t n = buf.readable_bytes();
        co_await AsyncWrite{conn.get(), buf.peek(), n};  // peek() 不移动读指针
        buf.retrieve(n);  // 消费已回写的数据，避免重复发送
    }
    // 半关闭写端，通知客户端不再发送
    conn->shutdown();
}

int main() {
    // 注册 Ctrl+C 处理，实现优雅退出
    std::signal(SIGINT, signal_handler);
    // 忽略 SIGPIPE：向已关闭的 socket 写时不会导致进程被信号杀死
    std::signal(SIGPIPE, SIG_IGN);

    // 主 Reactor：负责 accept 新连接，以及投递跨线程任务
    EventLoop loop;
    g_loop = &loop;

    // 创建 TCP 服务端，监听 0.0.0.0:8080
    TcpServer server(&loop, "0.0.0.0", 8080, "EchoServer");
    // 开启 4 个子 I/O 线程（主从 Reactor），新连接 Round-Robin 分配到子 Loop
    server.set_thread_num(4);
    // 新连接建立时的回调：在连接所属的 EventLoop 上启动 echo 协程
    server.set_connection_callback([](const std::shared_ptr<TcpConnection>& conn) {
        spawn_on(conn->loop(), echo_session(conn));
    });

    // 启动线程池并开始 accept
    server.start();
    LN_INFO("Echo server listening on 0.0.0.0:8080");
    // 阻塞运行主事件循环，直到 quit() 被调用
    loop.loop();

    return 0;
}
