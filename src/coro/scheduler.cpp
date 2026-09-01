#include "lightnet/coro/awaitable.h"

#include "lightnet/core/timer_wheel.h"
#include "lightnet/coro/scheduler.h"
#include "lightnet/net/tcp_connection.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace lightnet {

namespace {

std::vector<std::shared_ptr<Task<void>>>& alive_tasks() {
    static std::vector<std::shared_ptr<Task<void>>> tasks;
    return tasks;
}

}  // namespace

void Scheduler::prune_completed_tasks() {
    auto& tasks = alive_tasks();
    tasks.erase(
        std::remove_if(tasks.begin(), tasks.end(),
                       [](const std::shared_ptr<Task<void>>& t) { return t->done(); }),
        tasks.end());
}

void Scheduler::spawn(EventLoop* loop, Task<void> task) {
    auto t = std::make_shared<Task<void>>(std::move(task));
    alive_tasks().push_back(t);
    loop->run_in_loop([t]() {
        t->start();
        prune_completed_tasks();
    });
}

bool AsyncRead::await_ready() const noexcept {
    return conn->input_buffer()->readable_bytes() >= hint;
}

void AsyncRead::await_suspend(std::coroutine_handle<> h) {
    if (conn->input_buffer()->readable_bytes() > 0) {
        // 已有部分数据，下一轮 loop 立即恢复
        conn->loop()->schedule_coro(h);
        return;
    }
    // 注册到连接，等 epoll 可读事件再 resume
    conn->resume_read_coro(h);
}

ssize_t AsyncRead::await_resume() {
    return conn->read_some();
}

bool AsyncWrite::await_ready() const noexcept {
    return false;
}

void AsyncWrite::await_suspend(std::coroutine_handle<> h) {
    conn->send(data, len);
    if (conn->output_buffer()->readable_bytes() == 0) {
        // 数据已全部写入内核或同步发完
        conn->loop()->schedule_coro(h);
    } else {
        // 输出缓冲区仍有待发数据，等 EPOLLOUT
        conn->resume_write_coro(h);
    }
}

ssize_t AsyncWrite::await_resume() {
    return static_cast<ssize_t>(len);
}

bool AsyncSleep::await_ready() const noexcept {
    return ms == 0;
}

void AsyncSleep::await_suspend(std::coroutine_handle<> h) {
    loop->timer_wheel()->add_timer(ms, [loop = loop, h] {
        loop->schedule_coro(h);
    });
}

void AsyncSleep::await_resume() {}

void spawn_on(EventLoop* loop, Task<void> task) {
    Scheduler::spawn(loop, std::move(task));
}

}  // namespace lightnet
