#include "lightnet/core/channel.h"

#include "lightnet/core/event_loop.h"

#include <sys/epoll.h>

#include <cassert>

namespace lightnet {

// 绑定 fd 到 EventLoop；初始不监听任何事件，回调需由上层 set_xxx_callback 设置
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      index_(-1),
      added_to_loop_(false) {}

// 析构前必须已从 Poller 移除，否则 assert 失败
Channel::~Channel() {
    assert(!added_to_loop_);
}

// Poller 返回就绪事件后调用；按优先级依次检查并分发 close/error/read/write 回调
void Channel::handle_event(int revents) {
    // 对端关闭连接（挂断且无剩余可读数据）
    if ((revents & EPOLLHUP) && !(revents & EPOLLIN)) {
        if (close_callback_) close_callback_();
    }
    if (revents & EPOLLERR) {
        if (error_callback_) error_callback_();
    }
    // 可读：普通读、带外数据、对端 shutdown 写端
    if (revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (read_callback_) read_callback_();
    }
    if (revents & EPOLLOUT) {
        if (write_callback_) write_callback_();
    }
}

// 在 events_ 中加入读事件位，并同步到 epoll
void Channel::enable_reading() {
    events_ |= kReadEvent;
    update();
}

// 清除读事件位；若 events_ 为 0 则从 epoll 移除
void Channel::disable_reading() {
    events_ &= ~kReadEvent;
    update();
}

// 在 events_ 中加入写事件位；有数据待发送时开启，避免无效 EPOLLOUT 唤醒
void Channel::enable_writing() {
    events_ |= kWriteEvent;
    update();
}

// 清除写事件位
void Channel::disable_writing() {
    events_ &= ~kWriteEvent;
    update();
}

// 关闭全部事件监听，连接关闭或销毁 Channel 前调用
void Channel::disable_all() {
    events_ = kNoneEvent;
    update();
}

// 将当前 events_ 注册/更新到所属 EventLoop 的 Poller
void Channel::update() {
    added_to_loop_ = true;
    loop_->update_channel(this);
}

// 从 Poller 中彻底移除该 fd 的监听
void Channel::remove() {
    assert(added_to_loop_);
    added_to_loop_ = false;
    loop_->remove_channel(this);
}

}  // namespace lightnet
