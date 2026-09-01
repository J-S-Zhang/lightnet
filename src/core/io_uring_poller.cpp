#include "lightnet/core/io_uring_poller.h"

#ifdef LIGHTNET_IO_URING

#include "lightnet/core/channel.h"

#include <sys/epoll.h>
#include <liburing.h>

namespace lightnet {

IoUringPoller::IoUringPoller(EventLoop* loop) : Poller(loop) {
    io_uring_queue_init(256, &ring_, 0);
}

IoUringPoller::~IoUringPoller() {
    io_uring_queue_exit(&ring_);
}

void IoUringPoller::update_channel(Channel* channel) {
    channels_[channel->fd()] = channel;
    submit_poll_add(channel, channel->events());
}

void IoUringPoller::remove_channel(Channel* channel) {
    submit_poll_remove(channel);
    channels_.erase(channel->fd());
    channel->set_index(-1);
}

std::vector<PollEvent> IoUringPoller::poll(int timeout_ms) {
    active_events_.clear();
    if (timeout_ms >= 0) {
        struct __kernel_timespec ts{};
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
        io_uring_submit_and_wait_timeout(&ring_, nullptr, 1, &ts, nullptr);
    } else {
        io_uring_submit_and_wait(&ring_, 1);
    }
    io_uring_cqe* cqe = nullptr;
    unsigned head;
    unsigned count = 0;
    io_uring_for_each_cqe(&ring_, head, cqe) {
        Channel* ch = static_cast<Channel*>(io_uring_cqe_get_data(cqe));
        if (ch) {
            int revents = 0;
            if (cqe->res >= 0) {
                // user_data 存的是注册时的 events 掩码
                uint32_t events = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cqe->user_data));
                if (events & Channel::kReadEvent) revents |= EPOLLIN;
                if (events & Channel::kWriteEvent) revents |= EPOLLOUT;
            } else {
                revents = EPOLLERR;
            }
            active_events_.push_back({ch, revents});
        }
        ++count;
    }
    io_uring_cq_advance(&ring_, count);
    return active_events_;
}

void IoUringPoller::submit_poll_add(Channel* channel, int events) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) return;
    io_uring_prep_poll_add(sqe, channel->fd(), events ? events : POLLIN);
    io_uring_sqe_set_data(sqe, channel);
    sqe->user_data = static_cast<uint64_t>(events);
    io_uring_submit(&ring_);
}

void IoUringPoller::submit_poll_remove(Channel* channel) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) return;
    io_uring_prep_poll_remove(sqe, channel->fd());
    io_uring_sqe_set_data(sqe, nullptr);
    io_uring_submit(&ring_);
}

}  // namespace lightnet

#endif  // LIGHTNET_IO_URING
