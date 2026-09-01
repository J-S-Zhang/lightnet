#include "lightnet/core/epoll_poller.h"

#include "lightnet/core/channel.h"
#include "lightnet/core/event_loop.h"

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>

namespace lightnet {

// 创建 epoll 实例，预分配 events_ 数组供 epoll_wait 输出使用
EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop),
      epoll_fd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
    if (epoll_fd_ < 0) {
        // fatal
    }
}

// 关闭 epoll 实例 fd
EpollPoller::~EpollPoller() {
    ::close(epoll_fd_);
}

// Channel 首次注册用 ADD，已存在用 MOD 更新关注的事件掩码
void EpollPoller::update_channel(Channel* channel) {
    const int fd = channel->fd();
    if (channel->index() < 0) {
        update(EPOLL_CTL_ADD, channel);
        channel->set_index(0);
    } else {
        update(EPOLL_CTL_MOD, channel);
    }
}

// 从 epoll 和 channels_ 映射中移除 fd，并重置 Channel 的 index
void EpollPoller::remove_channel(Channel* channel) {
    const int fd = channel->fd();
    if (channels_.find(fd) != channels_.end()) {
        update(EPOLL_CTL_DEL, channel);
        channels_.erase(fd);
    }
    channel->set_index(-1);
}

// 阻塞等待 I/O 就绪，将就绪 Channel 及事件掩码填入 active 返回给 EventLoop
std::vector<PollEvent> EpollPoller::poll(int timeout_ms) {
    int num_events = ::epoll_wait(epoll_fd_, events_.data(),
                                  static_cast<int>(events_.size()), timeout_ms);
    std::vector<PollEvent> active;
    if (num_events > 0) {
        fill_active_channels(num_events, events_.data());
        active.reserve(static_cast<size_t>(num_events));
        // 从 epoll_event.data.ptr 取回注册时的 Channel 指针
        for (int i = 0; i < num_events; ++i) {
            Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
            int revents = events_[i].events;
            active.push_back({ch, revents});
        }
        // 返回事件数等于数组容量时扩容，避免下次 epoll_wait 截断
        if (static_cast<size_t>(num_events) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (num_events < 0 && errno != EINTR) {
        // log error
    }
    return active;
}

// 维护 fd → Channel* 映射，供 remove_channel 时查找
void EpollPoller::fill_active_channels(int num_events, const epoll_event* events) {
    for (int i = 0; i < num_events; ++i) {
        Channel* ch = static_cast<Channel*>(events[i].data.ptr);
        channels_[ch->fd()] = ch;
    }
}

// 执行 epoll_ctl；event.data.ptr 存 Channel*，poll 返回时可直接定位回调对象
void EpollPoller::update(int operation, Channel* channel) {
    epoll_event event{};
    std::memset(&event, 0, sizeof event);
    event.events = channel->events();
    event.data.ptr = channel;
    const int fd = channel->fd();
    if (::epoll_ctl(epoll_fd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            // ignore
        }
    }
}

}  // namespace lightnet
