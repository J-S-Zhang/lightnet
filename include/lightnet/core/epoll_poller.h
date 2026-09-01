#pragma once

#include "lightnet/core/poller.h"

#include <unordered_map>
#include <vector>

struct epoll_event;

namespace lightnet {

class Channel;

/// @brief Linux epoll 后端，默认 I/O 多路复用实现
class EpollPoller : public Poller {
public:
    explicit EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    void update_channel(Channel* channel) override;
    void remove_channel(Channel* channel) override;
    std::vector<PollEvent> poll(int timeout_ms) override;

private:
    static constexpr int kInitEventListSize = 16;  ///< epoll_wait 初始事件数组大小

    /// @brief 将 epoll 返回的事件填入 channels_ 映射
    void fill_active_channels(int num_events, const epoll_event* events);
    /// @brief 执行 epoll_ctl ADD/MOD/DEL
    void update(int operation, Channel* channel);

    int epoll_fd_;                                    ///< epoll 实例 fd
    std::vector<epoll_event> events_;                 ///< epoll_wait 输出缓冲区
    std::unordered_map<int, Channel*> channels_;     ///< fd → Channel 映射
};

}  // namespace lightnet
