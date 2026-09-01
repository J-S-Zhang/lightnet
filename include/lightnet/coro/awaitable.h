#pragma once

#include "lightnet/core/buffer.h"
#include "lightnet/core/event_loop.h"
#include "lightnet/coro/task.h"

#include <functional>
#include <memory>

namespace lightnet {

class TcpConnection;
class Channel;

/// @brief 协程异步读 Awaitable
/// co_await AsyncRead{conn, hint} 时挂起，直到 input_buffer 有至少 hint 字节
struct AsyncRead {
    TcpConnection* conn;  ///< 目标连接
    size_t hint;          ///< 期望最少可读字节数

    /// @brief 若 buffer 已有足够数据则无需挂起
    bool await_ready() const noexcept;
    /// @brief 注册读协程或立即 schedule
    void await_suspend(std::coroutine_handle<> h);
    /// @brief 恢复后调用 read_some 返回可读字节数
    ssize_t await_resume();
};

/// @brief 协程异步写 Awaitable
/// co_await AsyncWrite{conn, data, len} 时挂起，直到数据发送完毕
struct AsyncWrite {
    TcpConnection* conn;  ///< 目标连接
    const char* data;     ///< 待发送数据指针
    size_t len;           ///< 待发送长度

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> h);
    ssize_t await_resume();
};

/// @brief 协程延时 Awaitable，基于 TimerWheel
struct AsyncSleep {
    EventLoop* loop;  ///< 定时器所属 EventLoop
    uint64_t ms;      ///< 延时毫秒数

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> h);
    void await_resume();
};

/// @brief 在指定 EventLoop 线程上启动 void 协程
void spawn_on(EventLoop* loop, Task<void> task);

}  // namespace lightnet
