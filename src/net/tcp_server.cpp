#include "lightnet/net/tcp_server.h"

#include "lightnet/log/logger.h"
#include "lightnet/net/connection_manager.h"
#include "lightnet/net/socket_ops.h"

#include <arpa/inet.h>
#include <netinet/in.h>

namespace lightnet {

// 创建监听 socket，bind/listen，注册 accept_channel 读回调
TcpServer::TcpServer(EventLoop* loop, const std::string& ip, uint16_t port,
                     const std::string& name, bool reuse_port)
    : loop_(loop),
      name_(name),
      listen_fd_(sockets::create_nonblocking_or_die()),
      thread_pool_(std::make_unique<EventLoopThreadPool>(loop)),
      conn_manager_(std::make_unique<ConnectionManager>()),
      started_(false) {
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    sockets::set_reuse_addr(listen_fd_, true);
    sockets::set_reuse_port(listen_fd_, reuse_port);
    sockets::bind_or_die(listen_fd_, &addr);
    sockets::listen_or_die(listen_fd_);
    // listen_fd 可读表示有新连接可 accept
    accept_channel_ = std::make_unique<Channel>(loop_, listen_fd_);
    accept_channel_->set_read_callback([this] {
        struct sockaddr_in peer_addr {};
        int connfd = sockets::accept(listen_fd_, &peer_addr);
        if (connfd >= 0) {
            new_connection(connfd, peer_addr);
        }
    });
}

// 在 loop 线程关闭 listen_fd 并移除 accept_channel
TcpServer::~TcpServer() {
    loop_->run_in_loop([this] {
        accept_channel_->disable_all();
        accept_channel_->remove();
        sockets::close(listen_fd_);
    });
}

// 设置 I/O 子线程数量（须在 start 之前调用）
void TcpServer::set_thread_num(int num_threads) {
    thread_pool_->set_thread_num(num_threads);
}

// 启动子线程池；accept 监听投递到 loop 线程（须在 loop() 内注册 epoll）
void TcpServer::start() {
    if (started_.exchange(true)) return;
    thread_pool_->start();
    loop_->run_in_loop([this] {
        accept_channel_->enable_reading();
    });
    LN_INFO("TcpServer[" + name_ + "] started");
}

// accept 新连接：Round-Robin 选子 Loop，在目标 I/O 线程创建 TcpConnection
void TcpServer::new_connection(int fd, const struct sockaddr_in& peer_addr) {
    // 从线程池按 Round-Robin 选一个子 EventLoop；无子线程时返回主 loop_
    EventLoop* io_loop = thread_pool_->get_next_loop();
    // 获取本端地址（accept 返回的 fd 上查本地 ip:port）
    struct sockaddr_in local = sockets::get_local_addr(fd);
    // 转成 "192.168.1.1:8080" 形式，供日志与连接对象保存
    std::string local_addr = sockets::to_ip_port(&local);
    // 客户端地址，peer_addr 来自 accept 时内核填入的对端 sockaddr
    std::string peer = sockets::to_ip_port(&peer_addr);
    // accept 发生在主 Loop 线程，但连接必须在其所属的 io_loop 线程上创建/注册 epoll
    io_loop->run_in_loop([this, io_loop, fd, local_addr = std::move(local_addr),
                          peer = std::move(peer)] {
        // 在目标 I/O 线程创建 TcpConnection，分配 conn_id，纳入 ConnectionManager
        auto conn = conn_manager_->create(io_loop, fd, local_addr, peer);
        if (!conn) {
            // 创建失败（如 fd 无效），关闭 fd 避免泄漏
            sockets::close(fd);
            return;
        }
        // 连接建立成功时回调：HttpServer 在此 spawn 协程，Echo 在此启动 session
        conn->set_connection_callback([this](const std::shared_ptr<TcpConnection>& c) {
            on_connection(c);
        });
        // 可读且走 message_callback 模式时触发（HttpServer 用协程，通常不走这条）
        conn->set_message_callback([this](const std::shared_ptr<TcpConnection>& c, Buffer* buf) {
            on_message(c, buf);
        });
        // 连接关闭时从 ConnectionManager 移除，防止悬空引用
        conn->set_close_callback([this](const std::shared_ptr<TcpConnection>& c) {
            remove_connection(c);
        });
        // 标记 kConnected、enable_reading 注册 epoll，并触发 connection_callback_
        conn->connect_established();
    });
}

// 连接关闭：从 ConnectionManager 移除并标记已销毁（投递到主 Loop）
void TcpServer::remove_connection(const std::shared_ptr<TcpConnection>& conn) {
    loop_->run_in_loop([this, conn] {
        conn_manager_->remove(conn);
        conn->connect_destroyed();
    });
}

// 新连接建立，转发给用户注册的 connection_callback_
void TcpServer::on_connection(const std::shared_ptr<TcpConnection>& conn) {
    if (connection_callback_) {
        connection_callback_(conn);
    }
}

// 收到数据，转发给用户注册的 message_callback_
void TcpServer::on_message(const std::shared_ptr<TcpConnection>& conn, Buffer* buf) {
    if (message_callback_) {
        message_callback_(conn, buf);
    }
}

}  // namespace lightnet
