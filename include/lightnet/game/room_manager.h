#pragma once

#include "lightnet/game/game_packet.h"
#include "lightnet/game/room.h"
#include "lightnet/net/udp_server.h"

#include <netinet/in.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace lightnet {

struct PlayerBinding {
    uint32_t room_id = 0;
    uint32_t player_id = 0;
};

/// @brief 管理多个 GameRoom 与 peer 绑定关系
class GameRoomManager {
public:
    struct JoinResult {
        uint32_t room_id = 0;
        uint32_t player_id = 0;
        bool ok = false;
    };

    explicit GameRoomManager(uint32_t tick_interval_ms = 33);

    JoinResult join(const sockaddr_in& peer, uint32_t desired_room_id);
    void leave_by_peer(const sockaddr_in& peer);
    std::optional<PlayerBinding> find_binding(const sockaddr_in& peer) const;

    void handle_packet(const GamePacket& packet, const sockaddr_in& peer, UdpServer* server);
    void tick_all(UdpServer* server);

    uint32_t tick_interval_ms() const { return tick_interval_ms_; }
    size_t room_count() const { return rooms_.size(); }
    size_t player_count() const { return peer_index_.size(); }

private:
    GameRoom* find_room(uint32_t room_id);
    /// @brief 按 peer 或包头 room/player 解析绑定，并刷新 NAT 后的对端地址
    std::optional<PlayerBinding> resolve_and_refresh_peer(const sockaddr_in& peer,
                                                          const GamePacket& packet);
    void send_join_ack(UdpServer* server, const sockaddr_in& peer,
                       uint32_t room_id, uint32_t player_id, uint32_t server_tick);
    void send_snapshot(UdpServer* server, const GameRoom& room, const GameSnapshotPayload& snap);
    void send_event(UdpServer* server, const GameRoom& room, const GameEventPayload& event);
    void send_pong(UdpServer* server, const GamePacket& ping_packet,
                   const sockaddr_in& peer, uint64_t server_recv_ns);
    static std::string peer_key(const sockaddr_in& peer);

    uint32_t tick_interval_ms_;
    uint32_t next_room_id_;
    uint32_t next_player_id_;
    std::unordered_map<uint32_t, GameRoom> rooms_;
    std::unordered_map<std::string, PlayerBinding> peer_index_;
};

}  // namespace lightnet
