#pragma once

#include "lightnet/core/poller.h"

#include <liburing.h>

#include <unordered_map>
#include <vector>

namespace lightnet {

class Channel;

#ifdef LIGHTNET_IO_URING

/// @brief Linux io_uring 后端，CMake 开启 LIGHTNET_USE_IO_URING 时使用
class IoUringPoller : public Poller {
public:
    explicit IoUringPoller(EventLoop* loop);
    ~IoUringPoller() override;

    void update_channel(Channel* channel) override;
    void remove_channel(Channel* channel) override;
    std::vector<PollEvent> poll(int timeout_ms) override;

private:
    /// @brief 提交 poll_add 请求监听 fd 事件
    void submit_poll_add(Channel* channel, int events);
    /// @brief 提交 poll_remove 取消监听
    void submit_poll_remove(Channel* channel);

    io_uring ring_;                                  ///< io_uring 实例
    std::unordered_map<int, Channel*> channels_;     ///< fd → Channel
    std::vector<PollEvent> active_events_;           ///< 本次 poll 就绪事件
};

#endif  // LIGHTNET_IO_URING

}  // namespace lightnet
