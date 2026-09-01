#include "lightnet/core/event_loop_thread_pool.h"

#include "lightnet/core/event_loop.h"
#include "lightnet/log/logger.h"

namespace lightnet {

EventLoopThreadPool::EventLoopThreadPool(EventLoop* base_loop)
    : base_loop_(base_loop),
      started_(false),
      num_threads_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() = default;

void EventLoopThreadPool::start() {
    started_ = true;
    for (int i = 0; i < num_threads_; ++i) {
        loops_.emplace_back(std::make_unique<EventLoop>());
        EventLoop* loop = loops_.back().get();
        threads_.emplace_back([this, loop] {
            if (thread_init_cb_) {
                thread_init_cb_(loop);
            }
            loop->loop();
        });
    }
    // 无子线程时占位 nullptr，get_next_loop 回退到 base_loop_
    if (num_threads_ == 0) {
        loops_.push_back(nullptr);
    }
}

EventLoop* EventLoopThreadPool::get_next_loop() {
    if (loops_.empty() || (loops_.size() == 1 && loops_[0] == nullptr)) {
        return base_loop_;
    }
    const int idx = next_.fetch_add(1) % static_cast<int>(loops_.size());
    return loops_[idx].get();
}

}  // namespace lightnet
