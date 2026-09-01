#pragma once

#include <memory>
#include <vector>

namespace lightnet {

class Channel;
class EventLoop;

/// @brief poll 返回的单个就绪事件
struct PollEvent {
    Channel* channel = nullptr;  ///< 就绪的 Channel
    int revents = 0;             ///< 实际发生的事件掩码
};

/// @brief I/O 多路复用抽象接口（epoll / io_uring）
class Poller {
public:
    /// @brief 根据编译选项创建 EpollPoller 或 IoUringPoller
    static std::unique_ptr<Poller> create(EventLoop* loop);

    virtual ~Poller() = default;

    /// @brief 注册或修改 Channel 关注的事件
    virtual void update_channel(Channel* channel) = 0;
    /// @brief 从 Poller 移除 Channel
    virtual void remove_channel(Channel* channel) = 0;
    /// @brief 阻塞等待 I/O 事件，timeout_ms 为超时毫秒数
    virtual std::vector<PollEvent> poll(int timeout_ms) = 0;

protected:
    explicit Poller(EventLoop* loop) : owner_loop_(loop) {}
    EventLoop* owner_loop() const { return owner_loop_; }

private:
    EventLoop* owner_loop_;  ///< 所属 EventLoop
};

}  // namespace lightnet
