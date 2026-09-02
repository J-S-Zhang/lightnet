# LightNet 项目功能说明与 Linux 兼容性评估

> 分析日期：2026-09-02  
> 项目路径：`c:\Users\24675\Desktop\lightnet`  
> 版本：0.1.0（CMake 定义）

---

## 1. 项目概述

**LightNet** 是一个面向 Linux 的 **C++20 高性能网络框架**，采用 **Reactor + 主从多线程 + 协程** 架构，内置 TCP/UDP、HTTP、RPC 以及 UDP 游戏对战服务模块。项目以静态库 `liblightnet.a` 形式提供，并附带 6 个示例可执行程序。

设计目标是为 FPS 等实时游戏提供可部署到 Linux 云服务器的 UDP 权威服务端，同时提供通用的 TCP 网络基础设施。

---

## 2. 技术架构

```
┌─────────────────────────────────────────────────────────┐
│  应用层：HttpServer / RpcServer / GameServer / 示例程序   │
├─────────────────────────────────────────────────────────┤
│  协程层：Task<T> / AsyncRead / AsyncWrite / AsyncSleep   │
├─────────────────────────────────────────────────────────┤
│  网络层：TcpServer / TcpConnection / UdpServer           │
├─────────────────────────────────────────────────────────┤
│  核心层：EventLoop / Channel / Buffer / TimerWheel       │
├─────────────────────────────────────────────────────────┤
│  I/O 后端：epoll（默认）或 io_uring（可选）               │
├─────────────────────────────────────────────────────────┤
│  基础设施：Logger（异步日志）/ Metrics（Prometheus）     │
└─────────────────────────────────────────────────────────┘
```

| 特性 | 说明 |
|------|------|
| 并发模型 | 单线程 Reactor + 主从 Reactor（`EventLoopThreadPool`） |
| I/O 多路复用 | 默认 `epoll`；可选 `io_uring`（需 Linux 5.1+ 与 liburing） |
| 协程 | C++20 `<coroutine>`，`co_await` 异步读写与定时 |
| 跨线程通信 | `eventfd` 唤醒 + `run_in_loop` / `queue_in_loop` |
| 构建系统 | CMake 3.20+，C++20 标准 |

---

## 3. 已实现功能模块

### 3.1 核心层（`core/`）

| 模块 | 文件 | 功能 |
|------|------|------|
| **Buffer** | `buffer.h/cpp` | 动态读写缓冲区，支持增量读写、CRLF 查找、prepend 预留空间 |
| **Channel** | `channel.h/cpp` | 将 fd 封装为事件对象，注册读/写/关闭/错误回调 |
| **Poller** | `poller.h/cpp` | I/O 多路复用抽象接口，工厂方法按编译选项创建 epoll 或 io_uring 后端 |
| **EpollPoller** | `epoll_poller.h/cpp` | Linux epoll 默认实现 |
| **IoUringPoller** | `io_uring_poller.h/cpp` | Linux io_uring 可选实现 |
| **EventLoop** | `event_loop.h/cpp` | 单线程 Reactor 主循环：poll → 事件分发 → 定时器 → 跨线程任务 |
| **TimerWheel** | `timer_wheel.h/cpp` | 基于最小堆的定时器，支持一次性与周期性定时 |
| **EventLoopThreadPool** | `event_loop_thread_pool.h/cpp` | 主从 Reactor 线程池，新连接 Round-Robin 分配到子 Loop |

### 3.2 网络层（`net/`）

| 模块 | 功能 |
|------|------|
| **socket_ops** | POSIX socket 封装：非阻塞 TCP/UDP 创建、`accept4`、`SO_REUSEADDR/PORT`、`TCP_NODELAY`、`sendto/recvfrom` 等 |
| **TcpConnection** | TCP 连接生命周期管理，输入/输出 Buffer，协程读/写挂起与恢复 |
| **TcpServer** | TCP 监听与 accept，支持配置 I/O 子线程数 |
| **UdpServer** | UDP 报文收发，消息回调模式 |
| **ConnectionManager** | TCP 连接集合管理，统计连接数 |

### 3.3 协程层（`coro/`）

| 组件 | 功能 |
|------|------|
| **Task\<T\>** | C++20 协程任务类型 |
| **AsyncRead** | 协程异步读，等待连接 Buffer 达到指定字节数 |
| **AsyncWrite** | 协程异步写，直到数据发送完毕 |
| **AsyncSleep** | 基于 TimerWheel 的协程延时 |
| **Scheduler / spawn_on** | 在指定 EventLoop 线程上启动协程，管理协程生命周期 |

### 3.4 HTTP 层（`http/`）

| 模块 | 功能 |
|------|------|
| **HttpRequest / HttpResponse** | HTTP/1.1 请求/响应对象，支持 header 查找、Keep-Alive 判断、序列化 |
| **HttpParser** | 增量 HTTP 请求解析（请求行 → 头部 → Content-Length body） |
| **HttpServer** | 基于协程的 HTTP/1.1 服务器，支持 GET/POST 路由注册，内部复用 TcpServer |

**已实现：** 路由分发、Keep-Alive、Content-Length body、404 响应  
**未实现：** HTTPS/TLS、Chunked Transfer-Encoding、HTTP/2

### 3.5 RPC 层（`rpc/`）

| 模块 | 功能 |
|------|------|
| **RpcCodec** | 自定义二进制协议 **LNRP**（魔数 `0x4C4E5250`）：magic + version + type + request_id + method + payload |
| **RpcServer** | 基于 TcpServer 的 RPC 服务端，按 method 名分发到注册的协程 handler |

支持消息类型：Request、Response、Oneway。

### 3.6 游戏层（`game/`）

面向 FPS 等多人对战场景的 **UDP 权威服务端**：

| 模块 | 功能 |
|------|------|
| **GamePacket / GameCodec** | 自定义 UDP 协议 **LNGP**（魔数 `0x4C4E4750`），定长 24 字节包头 + 变长 payload |
| **GameRoom** | 单房间权威仿真：玩家加入/离开、输入应用、开火命中、Tick 推进、快照生成 |
| **GameRoomManager** | 多房间管理、玩家绑定、NAT/源端口变化时的 peer 刷新 |
| **GameServer** | UdpServer + RoomManager + 固定间隔 Tick 快照广播 |

**协议消息类型：**

| 方向 | 类型 | 说明 |
|------|------|------|
| C→S | `kC2SJoin` | 请求加入房间（可指定 room_id 或自动分配） |
| S→C | `kS2CJoinAck` | 返回 room_id、player_id、server_tick、tick_interval |
| C→S | `kC2SInput` | 每 tick 输入（移动 x/y、yaw/pitch、buttons） |
| C→S | `kC2SFire` | 开火请求（武器、原点、方向） |
| C→S | `kC2SPing` | RTT 测量 |
| S→C | `kS2CPong` | Ping 响应（含时间戳） |
| S→C | `kS2CSnapshot` | 世界状态快照（所有玩家位置、朝向、HP 等） |
| S→C | `kS2CEvent` | 游戏事件（Hit / Kill / Respawn） |

**游戏逻辑（简化版）：**
- 默认 Tick 间隔 33ms（约 30Hz），可配置
- 移动速度 4.0 单位/秒，基于输入向量归一化
- 开火：查找最近敌人，距离阈值内造成 25 点伤害
- HP 归零触发 Kill 事件
- 每 Tick 向房间内所有玩家广播 Snapshot

### 3.7 基础设施

| 模块 | 功能 |
|------|------|
| **Logger** | 异步日志（后台线程写 stdout），支持 Trace/Debug/Info/Warn/Error 五级 |
| **Metrics** | Prometheus 格式指标：连接数、活跃连接、读写字节、请求数、请求延迟直方图、EventLoop 滞后直方图 |

---

## 4. 示例程序

| 程序 | 端口 | 功能 |
|------|------|------|
| `echo_server` | TCP 8080 | TCP Echo，演示 AsyncRead/AsyncWrite 协程 |
| `http_server` | TCP 8080 | HTTP 服务，`GET /` 与 `GET /metrics` |
| `rpc_server` | TCP 9090 | RPC 服务，注册 `echo` 与 `add` 方法 |
| `udp_echo_server` | UDP 9091 | UDP Echo |
| `udp_game_server` | UDP 9092 | 游戏对战服（云部署入口），支持 `--bind`、`--port`、`--tick-ms` |
| `udp_game_client` | — | 最小测试客户端：Join → Input → Snapshot → Ping |

---

## 5. Linux 服务器编译运行评估

### 5.1 结论

**可以在 Linux 服务器上编译并运行成功。**

本项目 **专为 Linux 设计**，CMake 在非 UNIX 平台会直接报错退出：

```cmake
if(NOT UNIX)
    message(FATAL_ERROR "lightnet requires Linux (epoll / io_uring). Use WSL2 or a Linux VM.")
endif()
```

源码全部使用 POSIX/Linux API（`epoll`、`eventfd`、`pthread`、`arpa/inet.h`、`sys/socket.h` 等），**不含任何 Windows/Winsock 代码**。

### 5.2 系统要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Linux（推荐 Ubuntu 20.04+、Debian 11+、CentOS Stream 8+ 等） |
| 编译器 | **GCC 10+** 或 **Clang 12+**（C++20 协程 `<coroutine>` 支持） |
| CMake | 3.20 及以上 |
| 内核 | 默认 epoll 无特殊要求；io_uring 需 **Linux 5.1+** |
| 依赖库 | 默认仅需 **pthread**（已自动链接）；io_uring 模式需 **liburing-dev** |

### 5.3 编译步骤（Linux 服务器）

**默认 epoll 模式：**

```bash
# 安装依赖（Ubuntu/Debian 示例）
sudo apt update
sudo apt install -y build-essential cmake g++-10

cd /path/to/lightnet
rm -rf build
cmake -B build -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 运行示例
./build/examples/udp_game_server --bind 0.0.0.0 --port 9092
./build/examples/udp_game_client 127.0.0.1 9092
```

**可选 io_uring 模式：**

```bash
sudo apt install -y liburing-dev
cmake -B build -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release \
      -DLIGHTNET_USE_IO_URING=ON
cmake --build build -j
```

### 5.4 运行部署注意事项

| 事项 | 说明 |
|------|------|
| **防火墙 / 安全组** | `udp_game_server` 使用 UDP 端口（默认 9092），需在云服务器安全组放行对应 UDP 端口 |
| **端口占用** | 确保 8080/9090/9091/9092 等端口未被占用 |
| **进程管理** | 建议使用 systemd、supervisor 或 Docker 托管进程 |
| **信号处理** | 示例程序已处理 SIGINT/SIGTERM/SIGPIPE，支持 Ctrl+C 优雅退出 |
| **编译器版本** | Ubuntu 18.04 默认 g++ 7/8 不满足要求，需手动安装 g++-10 并指定 `-DCMAKE_CXX_COMPILER=g++-10` |

### 5.5 潜在限制与风险

| 限制 | 说明 |
|------|------|
| **不支持 Windows 原生编译** | 当前环境（Windows）无法直接编译，需 Linux 虚拟机、WSL2 或远程 Linux 服务器 |
| **HTTP 功能较基础** | 无 HTTPS、无 Chunked、无 HTTP/2，适合内网监控或简单 API |
| **游戏逻辑为 Demo 级** | 命中判定、移动、房间管理均为简化实现，与 Unity FPS 客户端对接需进一步扩展协议与逻辑 |
| **无持久化** | 无数据库、无断线重连状态恢复 |
| **无负载均衡文档** | 多实例部署需自行处理房间分片或 SO_REUSEPORT |
| **io_uring 为可选实验特性** | 默认 epoll 已足够稳定，生产环境建议先用 epoll 验证 |

### 5.6 编译验证说明

当前分析环境为 **Windows 10**，WSL 未安装，未能在本机实际执行 Linux 编译。评估依据为：

1. CMakeLists.txt 的平台与编译器检查逻辑
2. 全量源码的 API 依赖分析（均为 Linux/POSIX 标准接口）
3. 示例程序中的 Linux 专用头文件与系统调用
4. 项目注释中的 Linux 部署说明（`udp_game_server.cpp` 标注为「云部署入口」）

在满足上述系统要求的 Linux 服务器上，**预期可一次编译通过并正常运行所有示例**。

---

## 6. 模块依赖关系

```
lightnet.h（统一入口）
├── core/     → Buffer, Channel, EventLoop, Poller, TimerWheel, EventLoopThreadPool
├── net/      → socket_ops, TcpConnection, TcpServer, UdpServer, ConnectionManager
├── coro/     → Task, AsyncRead/Write/Sleep, Scheduler
├── http/     → HttpMessage, HttpParser, HttpServer（依赖 net + coro）
├── rpc/      → RpcCodec, RpcServer（依赖 net + coro）
├── game/     → GamePacket, GameCodec, Room, RoomManager, GameServer（依赖 net + core）
├── log/      → Logger
└── metrics/  → Metrics
```

---

## 7. 总结

LightNet 是一个功能完整的 **Linux 专用 C++20 网络框架**，已实现：

- Reactor 事件驱动核心（epoll / 可选 io_uring）
- 主从多线程 TCP/UDP 服务
- C++20 协程异步 I/O
- HTTP/1.1 服务器与 Prometheus 监控
- 自定义 RPC 协议与服务端
- UDP 游戏对战服务（房间、Tick、快照、输入、开火、Ping/Pong、事件）
- 异步日志与性能指标

**Linux 服务器兼容性：良好。** 只需 GCC 10+ / Clang 12+、CMake 3.20+ 和标准 build-essential 工具链即可编译运行；游戏 UDP 服务部署时需额外开放防火墙 UDP 端口。
