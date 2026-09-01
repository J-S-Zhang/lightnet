#include "lightnet/net/connection_manager.h"

#include "lightnet/log/logger.h"
#include "lightnet/metrics/metrics.h"

namespace lightnet {

ConnectionManager::ConnectionManager(size_t max_connections)
    : max_connections_(max_connections) {}

std::shared_ptr<TcpConnection> ConnectionManager::create(EventLoop* loop, int fd,
                                                         const std::string& local_addr,
                                                         const std::string& peer_addr) {
    std::lock_guard lock(mutex_);
    if (connections_.size() >= max_connections_) {
        LN_WARN("connection limit reached: " + std::to_string(max_connections_));
        return nullptr;
    }
    const uint64_t id = next_id_.fetch_add(1);
    auto conn = std::make_shared<TcpConnection>(loop, fd, id, local_addr, peer_addr);
    connections_[id] = conn;
    Metrics::instance().inc_connections_total();
    Metrics::instance().inc_active_connections();
    return conn;
}

void ConnectionManager::add(const std::shared_ptr<TcpConnection>& conn) {
    std::lock_guard lock(mutex_);
    connections_[conn->id()] = conn;
}

void ConnectionManager::remove(const std::shared_ptr<TcpConnection>& conn) {
    std::lock_guard lock(mutex_);
    connections_.erase(conn->id());
    Metrics::instance().dec_active_connections();
}

size_t ConnectionManager::size() const {
    std::lock_guard lock(mutex_);
    return connections_.size();
}

void ConnectionManager::touch(const std::shared_ptr<TcpConnection>& conn) {
    (void)conn;
    // idle timeout hook point
}

}  // namespace lightnet
