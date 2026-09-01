#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace lightnet {

class EventLoop;

using TimerId = uint64_t;                       ///< 定时器唯一 ID
using TimerCallback = std::function<void()>;    ///< 定时器到期回调

/// @brief 定时器（最小堆实现）
/// 支持一次性与重复定时，供 AsyncSleep 和 EventLoop poll 超时使用
class TimerWheel {
public:
    explicit TimerWheel(EventLoop* loop);

    /// @brief 添加定时器，delay_ms 后触发；repeat 为 true 则周期性重复
    TimerId add_timer(uint64_t delay_ms, TimerCallback cb, bool repeat = false);
    /// @brief 取消定时器（标记为非活跃）
    void cancel(TimerId id);

    /// @brief 处理所有已到期的定时器
    void tick(uint64_t now_ms);
    /// @brief 返回距离下一个定时器到期的毫秒数（供 poll 超时）
    int next_timeout_ms() const;

private:
    /// @brief 堆中单个定时器节点
    struct TimerNode {
        TimerId id;              ///< 定时器 ID
        uint64_t expire_ms;      ///< 到期时间戳（毫秒）
        uint64_t interval_ms;    ///< 重复间隔（repeat 时用）
        bool repeat;             ///< 是否重复
        TimerCallback callback;  ///< 到期回调
    };

    /// @brief 最小堆比较器：expire_ms 小的优先
    struct Compare {
        bool operator()(const TimerNode& a, const TimerNode& b) const {
            return a.expire_ms > b.expire_ms;
        }
    };

    EventLoop* loop_;                                              ///< 所属 EventLoop
    std::priority_queue<TimerNode, std::vector<TimerNode>, Compare> timers_;  ///< 最小堆
    std::unordered_map<TimerId, bool> active_;                     ///< 活跃定时器 ID 集合
    TimerId next_id_;                                              ///< 下一个分配的 ID
    uint64_t last_tick_ms_;                                        ///< 上次 tick 时间
};

}  // namespace lightnet
