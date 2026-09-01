#include "lightnet/core/timer_wheel.h"

#include "lightnet/core/event_loop.h"

#include <algorithm>
#include <chrono>
#include <limits>

namespace lightnet {

namespace {

uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

TimerWheel::TimerWheel(EventLoop* loop)
    : loop_(loop), next_id_(1), last_tick_ms_(now_ms()) {}

TimerId TimerWheel::add_timer(uint64_t delay_ms, TimerCallback cb, bool repeat) {
    TimerNode node;
    node.id = next_id_++;
    node.expire_ms = now_ms() + delay_ms;
    node.interval_ms = repeat ? delay_ms : 0;
    node.repeat = repeat;
    node.callback = std::move(cb);
    active_[node.id] = true;
    const TimerId id = node.id;
    timers_.push(std::move(node));
    return id;
}

void TimerWheel::cancel(TimerId id) {
    active_.erase(id);
}

void TimerWheel::tick(uint64_t now) {
    last_tick_ms_ = now;
    while (!timers_.empty() && timers_.top().expire_ms <= now) {
        TimerNode node = timers_.top();
        timers_.pop();
        if (!active_.count(node.id)) continue;  // 已 cancel
        if (node.callback) node.callback();
        if (node.repeat && active_.count(node.id)) {
            node.expire_ms = now + node.interval_ms;
            timers_.push(std::move(node));
        } else {
            active_.erase(node.id);
        }
    }
}

int TimerWheel::next_timeout_ms() const {
    if (timers_.empty()) return 1000;
    const uint64_t now = now_ms();
    if (timers_.top().expire_ms <= now) return 0;
    const uint64_t diff = timers_.top().expire_ms - now;
    return static_cast<int>(std::min<uint64_t>(diff, 1000));
}

}  // namespace lightnet
