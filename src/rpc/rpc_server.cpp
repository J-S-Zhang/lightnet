#include "lightnet/rpc/rpc_server.h"

#include "lightnet/coro/awaitable.h"
#include "lightnet/coro/scheduler.h"
#include "lightnet/log/logger.h"
#include "lightnet/metrics/metrics.h"

namespace lightnet {

RpcServer::RpcServer(EventLoop* loop, const std::string& ip, uint16_t port)
    : server_(loop, ip, port, "RpcServer") {
    server_.set_connection_callback([this](const std::shared_ptr<TcpConnection>& conn) {
        spawn_on(conn->loop(), handle_connection(conn));
    });
}

RpcServer::~RpcServer() = default;

void RpcServer::register_handler(const std::string& method, RpcHandler handler) {
    handlers_[method] = std::move(handler);
}

void RpcServer::start() {
    server_.start();
    LN_INFO("RpcServer listening");
}

Task<void> RpcServer::handle_connection(std::shared_ptr<TcpConnection> conn) {
    while (conn->is_connected()) {
        co_await AsyncRead{conn.get(), 1};
        RpcMessage req;
        // 一次读事件可能缓冲多条 RPC 消息，循环 decode
        while (RpcCodec::decode(conn->input_buffer(), &req)) {
            Metrics::instance().inc_requests();
            if (req.type == RpcMessageType::kRequest) {
                std::string result = co_await dispatch(req);
                RpcMessage resp;
                resp.type = RpcMessageType::kResponse;
                resp.request_id = req.request_id;
                resp.method = req.method;
                resp.payload = std::move(result);
                Buffer encoded = RpcCodec::encode(resp);
                const std::string out(encoded.peek(), encoded.readable_bytes());
                co_await AsyncWrite{conn.get(), out.data(), out.size()};
            }
        }
    }
    conn->shutdown();
}

Task<std::string> RpcServer::dispatch(const RpcMessage& req) {
    auto it = handlers_.find(req.method);
    if (it != handlers_.end()) {
        co_return co_await it->second(req.payload);
    }
    co_return std::string("error: unknown method");
}

}  // namespace lightnet
