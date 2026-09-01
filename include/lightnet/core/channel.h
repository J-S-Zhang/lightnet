#pragma once
#include <functional>
#include <memory>
namespace lightnet {
class EventLoop;
/// @brief 封装 fd 及其感兴趣的事件与回调
/// 将文件描述符注册到 EventLoop/Poller，事件就绪时分发对应回调
class Channel {
public:
    using EventCallback = std::function<void()>;  ///< 事件回调函数类型
    /// @brief 绑定 fd 到指定 EventLoop
    Channel(EventLoop* loop, int fd);
    ~Channel();
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    /// @brief 根据 revents 分发读/写/关闭/错误回调
    void handle_event(int revents);
    /// @brief 设置可读事件回调
    void set_read_callback(EventCallback cb) { read_callback_ = std::move(cb); }
    /// @brief 设置可写事件回调
    void set_write_callback(EventCallback cb) { write_callback_ = std::move(cb); }
    /// @brief 设置连接关闭回调
    void set_close_callback(EventCallback cb) { close_callback_ = std::move(cb); }
    /// @brief 设置错误事件回调
    void set_error_callback(EventCallback cb) { error_callback_ = std::move(cb); }
    int fd() const { return fd_; }              ///< 关联的文件描述符
    int events() const { return events_; }      ///< 当前关注的事件掩码
    int index() const { return index_; }        ///< 在 Poller 中的索引（-1 表示未注册）
    void set_index(int idx) { index_ = idx; }
    /// @brief 开启读事件监听
    void enable_reading();
    /// @brief 关闭读事件监听
    void disable_reading();
    /// @brief 开启写事件监听
    void enable_writing();
    /// @brief 关闭写事件监听
    void disable_writing();
    /// @brief 关闭所有事件监听
    void disable_all();
    /// @brief 是否正在监听写事件
    bool is_writing() const { return events_ & kWriteEvent; }
    /// @brief 是否正在监听读事件
    bool is_reading() const { return events_ & kReadEvent; }
    EventLoop* owner_loop() { return loop_; }  ///< 所属 EventLoop
    /// @brief 从 EventLoop/Poller 中移除
    void remove();
    static constexpr int kNoneEvent = 0;    ///< 无事件
    static constexpr int kReadEvent = 0x001;   ///< 读事件
    static constexpr int kWriteEvent = 0x004;  ///< 写事件
private:
    /// @brief 更新 events_ 并通知 EventLoop 同步到 Poller
    void update();
    EventLoop* loop_;           ///< 所属事件循环
    const int fd_;              ///< 文件描述符
    int events_;                ///< 关注的事件集合
    int index_;                 ///< Poller 内部索引
    bool added_to_loop_;        ///< 是否已加入 EventLoop
    EventCallback read_callback_;    ///< 读就绪回调
    EventCallback write_callback_;   ///< 写就绪回调
    EventCallback close_callback_;   ///< 关闭回调
    EventCallback error_callback_;   ///< 错误回调
};
}  // namespace lightnet
