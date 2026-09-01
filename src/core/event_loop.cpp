#include "lightnet/core/event_loop.h"

#include "lightnet/core/channel.h"
#include "lightnet/core/poller.h"
#include "lightnet/core/timer_wheel.h"
#include "lightnet/coro/scheduler.h"
#include "lightnet/metrics/metrics.h"

#include <sys/eventfd.h>
#include <unistd.h>

#include <chrono>
#include <cassert>
#include <thread>

namespace lightnet {

namespace {

// 获取当前单调时钟毫秒时间戳，供定时器 tick 和 lag 统计使用
uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

// 初始化 Poller、定时器、eventfd；thread_id_ 在 loop() 启动时绑定实际运行线程
EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      calling_pending_tasks_(false),
      wakeup_registered_(false),
      poller_(Poller::create(this)),
      timer_wheel_(std::make_unique<TimerWheel>(this)),
      wakeup_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
    if (wakeup_fd_ < 0) {
        // fatal
    }
    wakeup_channel_ = std::make_unique<Channel>(this, wakeup_fd_);
    wakeup_channel_->set_read_callback([this] { handle_read_wakeup(); });
}

// 清理 wakeup Channel 并关闭 eventfd
EventLoop::~EventLoop() {
    wakeup_channel_->disable_all();
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
}

// Reactor 主循环：I/O 事件 → 定时器 → 跨线程任务 → 性能指标
void EventLoop::loop() {
    assert(!looping_);
    thread_id_ = std::this_thread::get_id();
    if (!wakeup_registered_) {
        wakeup_channel_->enable_reading();
        wakeup_registered_ = true;
    }
    looping_ = true;
    quit_ = false;
    while (!quit_) {
        const auto loop_start = now_ms();
        // poll 超时取最近定时器，避免 sleep 过久
        const int timeout = timer_wheel_->next_timeout_ms();
        auto active = poller_->poll(timeout);
        // 分发就绪 Channel 的读/写/关闭/错误回调
        for (const auto& ev : active) {
            ev.channel->handle_event(ev.revents);
        }
        // 处理到期的定时器（含 AsyncSleep 等）
        timer_wheel_->tick(now_ms());
        // 执行其他线程投递的任务
        do_pending_tasks();
        const auto lag = now_ms() - loop_start;
        Metrics::instance().record_event_loop_lag_us(lag * 1000);
    }
    looping_ = false;
}

// 设置退出标志；若在其他线程调用则写 eventfd 唤醒 poll
void EventLoop::quit() {
    quit_ = true;
    if (!is_in_loop_thread()) {
        wakeup();
    }
}

// loop 线程内同步执行，否则异步投递到 pending_tasks_
void EventLoop::run_in_loop(Task cb) {
    if (is_in_loop_thread()) {
        cb();
    } else {
        queue_in_loop(std::move(cb));
    }
}

// 加锁入队 pending 任务；跨线程或正在执行 pending 时需 wakeup 打断 poll
void EventLoop::queue_in_loop(Task cb) {
    {
        std::lock_guard lock(mutex_);
        pending_tasks_.push_back(std::move(cb));
    }
    if (!is_in_loop_thread() || calling_pending_tasks_) {
        wakeup();
    }
}

// 将 Channel 注册/更新到 Poller（必须在 loop 线程调用）
void EventLoop::update_channel(Channel* channel) {
    assert_in_loop_thread();
    poller_->update_channel(channel);
}

// 从 Poller 移除 Channel（必须在 loop 线程调用）
void EventLoop::remove_channel(Channel* channel) {
    assert_in_loop_thread();
    poller_->remove_channel(channel);
}

// 恢复协程；统一投递到 pending 队列，避免在 await_suspend 内同步 resume 导致重入崩溃
void EventLoop::schedule_coro(std::coroutine_handle<> h) {
    queue_in_loop([h] {
        h.resume();
        Scheduler::prune_completed_tasks();
    });
}

// 判断当前线程是否为创建 EventLoop 的 loop 线程
bool EventLoop::is_in_loop_thread() const {
    return thread_id_ == std::this_thread::get_id();
}

// Debug 断言：必须在 loop 线程
void EventLoop::assert_in_loop_thread() const {
    assert(is_in_loop_thread());
}

// 向 eventfd 写入 1，使 epoll_wait 立刻返回，处理 pending 任务
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeup_fd_, &one, sizeof one);
    (void)n;
}

// 读取 eventfd 计数，清除可读状态，避免重复触发 EPOLLIN
void EventLoop::handle_read_wakeup() {
    uint64_t one;
    ssize_t n = ::read(wakeup_fd_, &one, sizeof one);
    (void)n;
}

// 交换并批量执行 pending_tasks_，避免持锁执行用户代码
void EventLoop::do_pending_tasks() {
    std::vector<Task> tasks;
    calling_pending_tasks_ = true;
    {
        std::lock_guard lock(mutex_);
        tasks.swap(pending_tasks_);
    }
    for (auto& task : tasks) {
        task();
    }
    calling_pending_tasks_ = false;
}

}  // namespace lightnet
