#pragma once

#include "lightnet/coro/task.h"
#include "lightnet/net/tcp_server.h"
#include "lightnet/rpc/rpc_codec.h"

#include <functional>
#include <unordered_map>

namespace lightnet {

/// @brief RPC 服务端
/// 基于 TcpServer + RpcCodec，按 method 名分发到注册的 handler
class RpcServer {
public:
    using RpcHandler = std::function<Task<std::string>(const std::string& payload)>;

    RpcServer(EventLoop* loop, const std::string& ip, uint16_t port);
    ~RpcServer();

    /// @brief 注册 RPC 方法 handler
    void register_handler(const std::string& method, RpcHandler handler);
    /// @brief 启动底层 TcpServer
    void start();
    void set_thread_num(int n) { server_.set_thread_num(n); }

private:
    /// @brief 连接协程：读 → decode → dispatch → encode 响应
    Task<void> handle_connection(std::shared_ptr<TcpConnection> conn);
    /// @brief 按 method 查找 handler
    Task<std::string> dispatch(const RpcMessage& req);

    TcpServer server_;                                    ///< 底层 TCP 服务
    std::unordered_map<std::string, RpcHandler> handlers_;  ///< method → handler
};

}  // namespace lightnet
