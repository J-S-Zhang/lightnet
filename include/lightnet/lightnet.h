#pragma once

/// @file lightnet.h
/// @brief LightNet 网络框架统一入口，包含所有公开模块头文件

#include "lightnet/core/buffer.h"              ///< 读写缓冲区
#include "lightnet/core/channel.h"             ///< fd 事件 Channel
#include "lightnet/core/event_loop.h"          ///< Reactor 事件循环
#include "lightnet/core/event_loop_thread_pool.h"  ///< 多线程 Loop 池
#include "lightnet/core/poller.h"              ///< I/O 多路复用抽象
#include "lightnet/core/timer_wheel.h"         ///< 定时器
#include "lightnet/coro/awaitable.h"           ///< AsyncRead/Write/Sleep
#include "lightnet/coro/scheduler.h"           ///< 协程调度
#include "lightnet/coro/task.h"                ///< Task 协程类型
#include "lightnet/http/http_message.h"        ///< HTTP 请求/响应
#include "lightnet/http/http_parser.h"         ///< HTTP 解析器
#include "lightnet/http/http_server.h"         ///< HTTP 服务器
#include "lightnet/log/logger.h"               ///< 异步日志
#include "lightnet/metrics/metrics.h"          ///< Prometheus 指标
#include "lightnet/net/connection_manager.h"   ///< 连接管理
#include "lightnet/net/socket_ops.h"           ///< socket 封装
#include "lightnet/net/tcp_connection.h"       ///< TCP 连接
#include "lightnet/net/tcp_server.h"           ///< TCP 服务端
#include "lightnet/net/udp_server.h"           ///< UDP 服务端
#include "lightnet/rpc/rpc_codec.h"            ///< RPC 编解码
#include "lightnet/rpc/rpc_server.h"           ///< RPC 服务端
#include "lightnet/game/game_packet.h"         ///< 游戏 UDP 协议
#include "lightnet/game/game_codec.h"          ///< 游戏编解码
#include "lightnet/game/room.h"                ///< 对战房间
#include "lightnet/game/room_manager.h"        ///< 房间管理
#include "lightnet/game/game_server.h"         ///< 游戏 UDP 服务
