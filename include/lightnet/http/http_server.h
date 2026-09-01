#pragma once

#include "lightnet/coro/task.h"
#include "lightnet/http/http_message.h"
#include "lightnet/net/tcp_server.h"

#include <functional>
#include <unordered_map>

namespace lightnet {

/// @brief 基于协程的 HTTP/1.1 服务器
/// 内部使用 TcpServer，每条连接 spawn 协程处理请求
class HttpServer {
public:
    using Handler = std::function<Task<HttpResponse>(const HttpRequest&)>;  ///< 路由处理函数

    HttpServer(EventLoop* loop, const std::string& ip, uint16_t port);
    ~HttpServer();

    /// @brief 注册 GET 路由
    void get(const std::string& path, Handler handler);
    /// @brief 注册 POST 路由
    void post(const std::string& path, Handler handler);
    /// @brief 注册任意 method + path 路由
    void add_route(const std::string& method, const std::string& path, Handler handler);

    /// @brief 启动底层 TcpServer
    void start();
    /// @brief 设置 I/O 子线程数
    void set_thread_num(int n) { server_.set_thread_num(n); }

    TcpServer& tcp_server() { return server_; }  ///< 访问底层 TCP 服务

private:
    /// @brief 单连接协程：读 → 解析 → dispatch → 写响应
    Task<void> handle_connection(std::shared_ptr<TcpConnection> conn);
    /// @brief 按 method+path 查找 handler，未找到返回 404
    Task<HttpResponse> dispatch(const HttpRequest& req);

    TcpServer server_;                              ///< 底层 TCP 服务端
    std::unordered_map<std::string, Handler> routes_;  ///< "METHOD path" → handler
};

}  // namespace lightnet
