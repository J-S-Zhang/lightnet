#include "lightnet/http/http_server.h"

#include "lightnet/coro/awaitable.h"
#include "lightnet/coro/scheduler.h"
#include "lightnet/http/http_parser.h"
#include "lightnet/log/logger.h"
#include "lightnet/metrics/metrics.h"

#include <chrono>
#include <memory>

namespace lightnet {

namespace {

std::string route_key(const std::string& method, const std::string& path) {
    return method + " " + path;
}

}  // namespace

HttpServer::HttpServer(EventLoop* loop, const std::string& ip, uint16_t port)
    : server_(loop, ip, port, "HttpServer") {
    server_.set_connection_callback([this](const std::shared_ptr<TcpConnection>& conn) {
        spawn_on(conn->loop(), handle_connection(conn));
    });
}

HttpServer::~HttpServer() = default;

void HttpServer::get(const std::string& path, Handler handler) {
    add_route("GET", path, std::move(handler));
}

void HttpServer::post(const std::string& path, Handler handler) {
    add_route("POST", path, std::move(handler));
}

void HttpServer::add_route(const std::string& method, const std::string& path, Handler handler) {
    routes_[route_key(method, path)] = std::move(handler);
}

void HttpServer::start() {
    server_.start();
    LN_INFO("HttpServer listening");
}

Task<void> HttpServer::handle_connection(std::shared_ptr<TcpConnection> conn) {
    HttpParser parser;
    while (conn->is_connected()) {
        co_await AsyncRead{conn.get(), 1};
        const auto start = std::chrono::steady_clock::now();
        auto state = parser.parse(conn->input_buffer());
        if (state == HttpParseState::kComplete) {
            Metrics::instance().inc_requests();
            HttpResponse resp = co_await dispatch(parser.request());
            const std::string out = resp.serialize();
            co_await AsyncWrite{conn.get(), out.data(), out.size()};
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count();
            Metrics::instance().record_request_latency_us(static_cast<uint64_t>(us));
            if (!parser.keep_alive()) break;  // Connection: close
            parser.reset();  // Keep-Alive：同连接处理下一请求
        } else if (state == HttpParseState::kError) {
            break;
        }
    }
    conn->shutdown();
}

Task<HttpResponse> HttpServer::dispatch(const HttpRequest& req) {
    const std::string key = route_key(req.method, req.path);
    auto it = routes_.find(key);
    if (it != routes_.end()) {
        co_return co_await it->second(req);
    }
    HttpResponse resp;
    resp.status_code = 404;
    resp.status_message = "Not Found";
    resp.set_content_type("text/plain");
    resp.set_body("404 Not Found");
    co_return resp;
}

}  // namespace lightnet
