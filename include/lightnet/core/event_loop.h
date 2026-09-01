#pragma once

#include <coroutine>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace lightnet {

class Channel;
class Poller;
class TimerWheel;

/// @brief Reactor 事件循环（单线程）
/// 负责 poll I/O 事件、执行定时器、处理跨线程任务与协程恢复
class EventLoop {
public:
    using Task = std::function<void()>;  ///< 待执行任务类型

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    /// @brief 主循环：poll → 处理事件 → tick 定时器 → 执行 pending 任务
    void loop();
    /// @brief 请求退出循环；若在其他线程调用则 wakeup
    void quit();

    /// @brief 若在 loop 线程则立即执行，否则加入 pending 队列
    void run_in_loop(Task cb);
    /// @brief 将任务加入 pending 队列并 wakeup
    void queue_in_loop(Task cb);

    /// @brief 更新 Channel 在 Poller 中的注册（必须在 loop 线程）
    void update_channel(Channel* channel);
    /// @brief 从 Poller 移除 Channel（必须在 loop 线程）
    void remove_channel(Channel* channel);

    /// @brief 恢复协程；跨线程时通过 queue_in_loop 投递
    void schedule_coro(std::coroutine_handle<> h);

    Poller* poller() const { return poller_.get(); }       ///< I/O 多路复用后端
    TimerWheel* timer_wheel() { return timer_wheel_.get(); }  ///< 定时器

    /// @brief 当前线程是否为 loop 线程
    bool is_in_loop_thread() const;
    /// @brief 断言当前在 loop 线程（Debug 用）
    void assert_in_loop_thread() const;

    /// @brief 向 wakeup_fd 写入，唤醒阻塞在 poll 的 loop 线程
    void wakeup();

private:
    /// @brief 读取 eventfd，清除 wakeup 信号
    void handle_read_wakeup();
    /// @brief 批量执行 pending_tasks_
    void do_pending_tasks();

    bool looping_;                    ///< 是否正在 loop() 中
    bool quit_;                       ///< 是否请求退出
    bool calling_pending_tasks_;      ///< 是否正在执行 pending 任务（避免递归 wakeup）
    bool wakeup_registered_;          ///< wakeup Channel 是否已在 loop 线程注册
    std::thread::id thread_id_;       ///< loop 线程 ID

    std::unique_ptr<Poller> poller_;           ///< epoll 或 io_uring 后端
    std::unique_ptr<TimerWheel> timer_wheel_;  ///< 时间轮/最小堆定时器

    int wakeup_fd_;                              ///< eventfd，用于跨线程唤醒
    std::unique_ptr<Channel> wakeup_channel_;    ///< wakeup_fd 的 Channel

    std::mutex mutex_;                ///< 保护 pending_tasks_
    std::vector<Task> pending_tasks_; ///< 跨线程投递的任务队列
    std::vector<std::coroutine_handle<>> pending_coros_;  ///< 待恢复协程（预留）
};

}  // namespace lightnet
