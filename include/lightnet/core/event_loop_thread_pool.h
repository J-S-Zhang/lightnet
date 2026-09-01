#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace lightnet {

class EventLoop;

/// @brief 多线程 EventLoop 池（主从 Reactor）
/// 主 Loop 负责 accept，子 Loop 负责连接 I/O
class EventLoopThreadPool {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;  ///< 子线程启动时的初始化回调

    explicit EventLoopThreadPool(EventLoop* base_loop);
    ~EventLoopThreadPool();

    /// @brief 设置子 EventLoop 线程数量
    void set_thread_num(int num_threads) { num_threads_ = num_threads; }
    /// @brief 设置子线程 loop 启动前的回调
    void set_thread_init_callback(ThreadInitCallback cb) { thread_init_cb_ = std::move(cb); }

    /// @brief 创建子 Loop 并启动线程
    void start();
    /// @brief Round-Robin 返回下一个子 Loop；无子线程时返回 base_loop_
    EventLoop* get_next_loop();
    EventLoop* base_loop() const { return base_loop_; }  ///< 主 EventLoop

    bool started() const { return started_; }  ///< 是否已启动
    size_t size() const { return loops_.size(); }  ///< Loop 数量

private:
    EventLoop* base_loop_;              ///< 主 Reactor（accept 用）
    bool started_;                      ///< 是否已 start
    int num_threads_;                   ///< 子线程数
    std::atomic<int> next_{0};          ///< Round-Robin 计数器
    std::vector<std::unique_ptr<EventLoop>> loops_;  ///< 子 EventLoop 列表
    std::vector<std::thread> threads_;              ///< 子线程
    ThreadInitCallback thread_init_cb_;               ///< 线程初始化回调
};

}  // namespace lightnet
