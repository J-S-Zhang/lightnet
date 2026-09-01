#pragma once

#include "lightnet/core/event_loop.h"
#include "lightnet/core/timer_wheel.h"
#include "lightnet/game/room_manager.h"
#include "lightnet/net/udp_server.h"

#include <cstdint>
#include <string>

namespace lightnet {

/// @brief 游戏 UDP 服务：UdpServer + RoomManager + 固定 Tick 快照广播
class GameServer {
public:
    GameServer(EventLoop* loop, const std::string& ip, uint16_t port,
               uint32_t tick_interval_ms = 33);
    ~GameServer();

    void start();
    void stop();

    EventLoop* loop() const { return udp_.loop(); }
    UdpServer* udp_server() { return &udp_; }
    GameRoomManager* room_manager() { return &rooms_; }
    uint32_t tick_interval_ms() const { return tick_interval_ms_; }

private:
    void on_datagram(const char* data, size_t len, const sockaddr_in& peer, UdpServer* server);
    void on_tick();

    UdpServer udp_;
    GameRoomManager rooms_;
    uint32_t tick_interval_ms_;
    TimerId tick_timer_ = 0;
    bool running_ = false;
};

}  // namespace lightnet
