#include "lightnet/game/game_server.h"

#include "lightnet/game/game_codec.h"
#include "lightnet/log/logger.h"
#include "lightnet/net/socket_ops.h"

namespace lightnet {

GameServer::GameServer(EventLoop* loop, const std::string& ip, uint16_t port,
                       uint32_t tick_interval_ms)
    : udp_(loop, ip, port),
      rooms_(tick_interval_ms),
      tick_interval_ms_(tick_interval_ms) {}

GameServer::~GameServer() {
    stop();
}

void GameServer::start() {
    if (running_) {
        return;
    }
    running_ = true;
    udp_.set_message_callback([this](const char* data, size_t len, const sockaddr_in& peer,
                                     UdpServer* server) {
        on_datagram(data, len, peer, server);
    });
    udp_.start();

    tick_timer_ = udp_.loop()->timer_wheel()->add_timer(
        tick_interval_ms_,
        [this] { on_tick(); },
        true);
    LN_INFO("GameServer started on port " + std::to_string(udp_.port()) +
            " tick_ms=" + std::to_string(tick_interval_ms_));
}

void GameServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    if (tick_timer_ != 0) {
        udp_.loop()->timer_wheel()->cancel(tick_timer_);
        tick_timer_ = 0;
    }
}

void GameServer::on_datagram(const char* data, size_t len, const sockaddr_in& peer,
                             UdpServer* server) {
    GamePacket packet;
    if (!GameCodec::decode(data, len, &packet)) {
        LN_WARN("GameServer drop invalid packet from " + sockets::to_ip_port(&peer));
        return;
    }
    rooms_.handle_packet(packet, peer, server);
}

void GameServer::on_tick() {
    if (!running_) {
        return;
    }
    rooms_.tick_all(&udp_);
}

}  // namespace lightnet
