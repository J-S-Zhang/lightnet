#pragma once



#include "lightnet/core/event_loop.h"

#include "lightnet/coro/task.h"



namespace lightnet {



/// @brief 协程调度辅助

/// 管理 Task 生命周期，防止协程 frame 过早销毁

class Scheduler {

public:

    /// @brief 在 loop 线程启动协程并加入 alive 列表

    static void spawn(EventLoop* loop, Task<void> task);

    /// @brief 清理已完成的 Task，释放协程 frame

    static void prune_completed_tasks();

};



}  // namespace lightnet

