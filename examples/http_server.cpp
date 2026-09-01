/// @file http_server.cpp
/// @brief HTTP/1.1 服务器示例 —— 演示路由注册与协程 Handler
///
/// 功能：
///   GET /         → 返回 Hello 文本
///   GET /metrics  → 返回 Prometheus 格式监控指标
///
/// 技术点：HttpServer 封装 TcpServer，内部自动处理 HTTP 解析与 Keep-Alive
///
/// 编译运行：
///   ./build/examples/http_server
/// 测试：
///   curl http://localhost:8080/
///   curl http://localhost:8080/metrics

#include "lightnet/lightnet.h"

#include <csignal>
#include <iostream>

using namespace lightnet;

/// 全局 EventLoop 指针，供 SIGINT 处理函数退出主循环
static EventLoop* g_loop = nullptr;

/// @brief SIGINT (Ctrl+C) 信号处理：优雅退出
void signal_handler(int) {
    if (g_loop) g_loop->quit();
}

/// @brief 根路径 GET / 的处理函数
/// @param req 已解析的 HTTP 请求（含 method、path、headers、body）
/// @return 协程 Task，co_return 返回的 HttpResponse 会序列化后发给客户端
///
/// Handler 签名固定为：Task<HttpResponse>(const HttpRequest&)
Task<HttpResponse> hello_handler(const HttpRequest& req) {
    HttpResponse resp;
    resp.set_content_type("text/plain");  // 设置 Content-Type 头
    // req.path 为请求路径，如 "/" 或 "/api/user"
    resp.set_body("Hello, LightNet! path=" + req.path);  // 自动设置 Content-Length
    co_return resp;
}

/// @brief GET /metrics 的处理函数：导出 Prometheus 监控数据
/// @param req 请求对象（此处未使用路径/参数）
Task<HttpResponse> metrics_handler(const HttpRequest&) {
    HttpResponse resp;
    // Prometheus text 格式要求的 Content-Type
    resp.set_content_type("text/plain; version=0.0.4");
    // 从全局 Metrics 单例导出：连接数、字节数、请求延迟直方图等
    resp.set_body(Metrics::instance().export_prometheus());
    co_return resp;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    EventLoop loop;
    g_loop = &loop;

    // HttpServer 内部持有 TcpServer，监听 8080
    HttpServer server(&loop, "0.0.0.0", 8080);
    server.set_thread_num(4);  // 4 个 I/O 子线程

    // 注册路由：method + path → 协程 handler
    // 每条新连接会 spawn 协程，循环：读 → 解析 HTTP → dispatch → 写响应
    server.get("/", hello_handler);
    server.get("/metrics", metrics_handler);
    // 也可用 server.post("/api", handler) 注册 POST 路由

    server.start();  // 启动底层 TcpServer
    LN_INFO("HTTP server listening on 0.0.0.0:8080");
    loop.loop();  // 主线程运行 accept 所在的主 EventLoop

    return 0;
}
