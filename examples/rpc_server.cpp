/// @file rpc_server.cpp
/// @brief RPC 服务器示例 —— 演示自定义二进制 RPC 协议与方法注册
///
/// 功能：
///   方法 echo → 原样返回 payload
///   方法 add  → 解析 "a,b" 格式，返回 a+b 的字符串结果
///
/// 协议：LNRP 二进制格式（见 include/lightnet/rpc/rpc_codec.h）
/// 端口：9090（与 HTTP 示例的 8080 区分）
///
/// 编译运行：
///   ./build/examples/rpc_server
///
/// 技术点：RpcServer 封装 TcpServer，按 method 名分发到注册的 handler

#include "lightnet/lightnet.h"

#include <csignal>

using namespace lightnet;

/// 全局 EventLoop 指针，供信号处理退出主循环
static EventLoop* g_loop = nullptr;

/// @brief SIGINT (Ctrl+C) 信号处理
void signal_handler(int) {
    if (g_loop) g_loop->quit();
}

/// @brief RPC 方法 "echo"：回声，原样返回请求 payload
/// @param payload 客户端发来的字符串载荷
/// @return Task<std::string>，结果会编码进 RpcMessage 响应体
Task<std::string> echo_rpc(const std::string& payload) {
    co_return payload;
}

/// @brief RPC 方法 "add"：两整数相加
/// @param payload 格式为 "数字1,数字2"，例如 "3,5"
/// @return 相加结果的字符串；格式错误返回 "error: invalid format"
Task<std::string> add_rpc(const std::string& payload) {
    size_t pos = payload.find(',');
    if (pos == std::string::npos) co_return std::string("error: invalid format");
    const int a = std::stoi(payload.substr(0, pos));
    const int b = std::stoi(payload.substr(pos + 1));
    co_return std::to_string(a + b);
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    EventLoop loop;
    g_loop = &loop;

    // RpcServer 内部持有 TcpServer，监听 9090
    RpcServer server(&loop, "0.0.0.0", 9090);
    server.set_thread_num(4);

    // 注册 RPC 方法名 → 处理函数
    // 客户端发送 RpcMessage{ method="echo", payload="..." } 时会调用 echo_rpc
    server.register_handler("echo", echo_rpc);
    server.register_handler("add", add_rpc);

    // 连接处理流程（框架内部）：
    //   co_await AsyncRead → RpcCodec::decode → dispatch(method) → encode 响应 → AsyncWrite
    server.start();
    LN_INFO("RPC server listening on 0.0.0.0:9090");
    loop.loop();

    return 0;
}
