#include "lightnet/core/poller.h"

#ifdef LIGHTNET_IO_URING
#include "lightnet/core/io_uring_poller.h"
#else
#include "lightnet/core/epoll_poller.h"
#endif

namespace lightnet {

/// @brief 工厂方法：io_uring 或 epoll 二选一
std::unique_ptr<Poller> Poller::create(EventLoop* loop) {
#ifdef LIGHTNET_IO_URING
    return std::make_unique<IoUringPoller>(loop);
#else
    return std::make_unique<EpollPoller>(loop);
#endif
}

}  // namespace lightnet
