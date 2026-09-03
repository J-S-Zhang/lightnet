#include "lightnet/game/room_manager.h"

#include "lightnet/game/game_codec.h"
#include "lightnet/log/logger.h"
#include "lightnet/net/socket_ops.h"

#include <algorithm>
#include <chrono>

namespace lightnet {

namespace {

constexpr int kFireDamage = 25;

uint64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

GameRoomManager::GameRoomManager(uint32_t tick_interval_ms)
    : tick_interval_ms_(tick_interval_ms),
      next_room_id_(1),
      next_player_id_(1) {}

GameRoomManager::JoinResult GameRoomManager::join(const sockaddr_in& peer,
                                                  uint32_t /*desired_room_id*/) {
    // 全服只维持一局：固定 room_id = 1，忽略客户端 desired_room_id。
    constexpr uint32_t kSingleRoomId = 1;

    JoinResult result;
    const std::string key = peer_key(peer);
    if (peer_index_.count(key) > 0) {
        const PlayerBinding& binding = peer_index_[key];
        result.room_id = binding.room_id;
        result.player_id = binding.player_id;
        result.ok = true;
        return result;
    }

    if (rooms_.find(kSingleRoomId) == rooms_.end()) {
        rooms_.emplace(kSingleRoomId, GameRoom(kSingleRoomId, tick_interval_ms_));
        next_room_id_ = 2;
    }

    GameRoom* room = find_room(kSingleRoomId);
    if (!room) {
        return result;
    }

    const uint32_t player_id = next_player_id_++;
    if (room->add_player(peer, player_id) == 0) {
        return result;
    }

    peer_index_[key] = PlayerBinding{kSingleRoomId, player_id};
    result.room_id = kSingleRoomId;
    result.player_id = player_id;
    result.ok = true;
    return result;
}

void GameRoomManager::leave_by_peer(const sockaddr_in& peer) {
    const auto binding = find_binding(peer);
    if (!binding) {
        return;
    }
    if (GameRoom* room = find_room(binding->room_id)) {
        room->remove_player(binding->player_id);
        // 单对局模式：房间常驻，人不清空房间本身
    }
    peer_index_.erase(peer_key(peer));
}

std::optional<PlayerBinding> GameRoomManager::find_binding(const sockaddr_in& peer) const {
    const auto it = peer_index_.find(peer_key(peer));
    if (it == peer_index_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<PlayerBinding> GameRoomManager::resolve_and_refresh_peer(
    const sockaddr_in& peer, const GamePacket& packet) {
    if (auto binding = find_binding(peer)) {
        if (GameRoom* room = find_room(binding->room_id)) {
            if (GamePlayer* player = room->find_player(binding->player_id)) {
                player->peer = peer;
            }
        }
        return binding;
    }

    // NAT / 源端口变化：用包头 room_id + player_id 找回玩家并刷新对端
    if (packet.header.room_id == 0 || packet.header.player_id == 0) {
        return std::nullopt;
    }
    GameRoom* room = find_room(packet.header.room_id);
    if (!room) {
        return std::nullopt;
    }
    GamePlayer* player = room->find_player(packet.header.player_id);
    if (!player) {
        return std::nullopt;
    }

    peer_index_.erase(peer_key(player->peer));
    player->peer = peer;
    PlayerBinding binding{packet.header.room_id, packet.header.player_id};
    peer_index_[peer_key(peer)] = binding;
    return binding;
}

void GameRoomManager::handle_packet(const GamePacket& packet, const sockaddr_in& peer,
                                    UdpServer* server) {
    switch (packet.header.msg_type) {
        case GameMsgType::kC2SJoin: {
            GameJoinPayload join_req;
            if (!GameCodec::decode_join(packet, &join_req)) {
                return;
            }
            const JoinResult joined = join(peer, join_req.desired_room_id);
            if (!joined.ok) {
                return;
            }
            GameRoom* room = find_room(joined.room_id);
            if (room) {
                if (GamePlayer* player = room->find_player(joined.player_id)) {
                    player->peer = peer;
                }
            }
            const uint32_t tick = room ? room->server_tick() : 0;
            send_join_ack(server, peer, joined.room_id, joined.player_id, tick);
            LN_INFO("player joined room=" + std::to_string(joined.room_id) +
                    " player=" + std::to_string(joined.player_id) +
                    " from " + sockets::to_ip_port(&peer));
            break;
        }
        case GameMsgType::kC2SInput: {
            const auto binding = resolve_and_refresh_peer(peer, packet);
            if (!binding) return;
            GameInputPayload input;
            if (!GameCodec::decode_input(packet, &input)) return;
            if (GameRoom* room = find_room(binding->room_id)) {
                room->apply_input(binding->player_id, input);
            }
            break;
        }
        case GameMsgType::kC2SFire: {
            const auto binding = resolve_and_refresh_peer(peer, packet);
            if (!binding) return;
            GameFirePayload fire;
            if (!GameCodec::decode_fire(packet, &fire)) return;
            GameRoom* room = find_room(binding->room_id);
            if (!room) return;

            uint16_t hp_after = 0;
            bool killed = false;
            const uint32_t target_id =
                room->apply_fire(binding->player_id, fire, &hp_after, &killed);
            if (target_id != 0) {
                GameEventPayload hit;
                hit.event_type = GameEventType::kHit;
                hit.target_id = target_id;
                hit.value = -kFireDamage;
                hit.extra = binding->player_id;
                send_event(server, *room, hit);
                if (killed) {
                    GameEventPayload kill;
                    kill.event_type = GameEventType::kKill;
                    kill.target_id = target_id;
                    kill.value = 0;
                    kill.extra = binding->player_id;
                    send_event(server, *room, kill);
                }
            }
            break;
        }
        case GameMsgType::kC2SChat: {
            const auto binding = resolve_and_refresh_peer(peer, packet);
            if (!binding) return;
            GameChatPayload chat;
            if (!GameCodec::decode_chat(packet, &chat)) return;
            chat.sender_id = binding->player_id;
            if (GameRoom* room = find_room(binding->room_id)) {
                send_chat(server, *room, chat);
            }
            break;
        }
        case GameMsgType::kC2SWeapon: {
            const auto binding = resolve_and_refresh_peer(peer, packet);
            if (!binding) return;
            GameWeaponPayload w;
            if (!GameCodec::decode_weapon(packet, &w)) return;
            if (GameRoom* room = find_room(binding->room_id)) {
                room->apply_weapon(binding->player_id, w.weapon_id);
            }
            break;
        }
        case GameMsgType::kC2SRespawn: {
            const auto binding = resolve_and_refresh_peer(peer, packet);
            if (!binding) return;
            if (GameRoom* room = find_room(binding->room_id)) {
                room->apply_respawn(binding->player_id);
                GameEventPayload ev;
                ev.event_type = GameEventType::kRespawn;
                ev.target_id = binding->player_id;
                ev.value = 100;
                send_event(server, *room, ev);
            }
            break;
        }
        case GameMsgType::kC2SProfile: {
            const auto binding = resolve_and_refresh_peer(peer, packet);
            if (!binding) return;
            GameProfilePayload profile;
            if (!GameCodec::decode_profile(packet, &profile)) return;
            if (GameRoom* room = find_room(binding->room_id)) {
                room->apply_profile(binding->player_id, profile.name);
            }
            break;
        }
        case GameMsgType::kC2SPing: {
            resolve_and_refresh_peer(peer, packet);
            send_pong(server, packet, peer, now_ns());
            break;
        }
        default:
            break;
    }
}

void GameRoomManager::tick_all(UdpServer* server) {
    ++match_broadcast_counter_;
    for (auto& [room_id, room] : rooms_) {
        (void)room_id;
        const GameSnapshotPayload snap = room.tick();
        send_snapshot(server, room, snap);
        // ~1Hz match state
        if (tick_interval_ms_ > 0 &&
            (match_broadcast_counter_ % std::max(1u, 1000u / tick_interval_ms_)) == 0) {
            send_match_state(server, room);
        }
    }
}

GameRoom* GameRoomManager::find_room(uint32_t room_id) {
    const auto it = rooms_.find(room_id);
    return it == rooms_.end() ? nullptr : &it->second;
}

void GameRoomManager::send_join_ack(UdpServer* server, const sockaddr_in& peer,
                                    uint32_t room_id, uint32_t player_id,
                                    uint32_t server_tick) {
    GameJoinAckPayload ack;
    ack.room_id = room_id;
    ack.player_id = player_id;
    ack.server_tick = server_tick;
    ack.tick_interval_ms = tick_interval_ms_;
    std::vector<uint8_t> payload;
    GameCodec::encode_join_ack(ack, &payload);
    Buffer buf = GameCodec::build_packet(
        GameMsgType::kS2CJoinAck, room_id, player_id, 0, server_tick, payload);
    server->send_to(buf.peek(), buf.readable_bytes(), peer);
}

void GameRoomManager::send_snapshot(UdpServer* server, const GameRoom& room,
                                    const GameSnapshotPayload& snap) {
    std::vector<uint8_t> payload;
    GameCodec::encode_snapshot(snap, &payload);
    for (const GamePlayer* player : room.all_players()) {
        Buffer buf = GameCodec::build_packet(
            GameMsgType::kS2CSnapshot,
            room.id(),
            player->player_id,
            snap.server_tick,
            snap.server_tick,
            payload);
        server->send_to(buf.peek(), buf.readable_bytes(), player->peer);
    }
}

void GameRoomManager::send_event(UdpServer* server, const GameRoom& room,
                                 const GameEventPayload& event) {
    std::vector<uint8_t> payload;
    GameCodec::encode_event(event, &payload);
    for (const GamePlayer* player : room.all_players()) {
        Buffer buf = GameCodec::build_packet(
            GameMsgType::kS2CEvent,
            room.id(),
            player->player_id,
            0,
            room.server_tick(),
            payload);
        server->send_to(buf.peek(), buf.readable_bytes(), player->peer);
    }
}

void GameRoomManager::send_chat(UdpServer* server, const GameRoom& room,
                                const GameChatPayload& chat) {
    std::vector<uint8_t> payload;
    GameCodec::encode_chat(chat, &payload);
    for (const GamePlayer* player : room.all_players()) {
        Buffer buf = GameCodec::build_packet(
            GameMsgType::kS2CChat,
            room.id(),
            player->player_id,
            0,
            room.server_tick(),
            payload);
        server->send_to(buf.peek(), buf.readable_bytes(), player->peer);
    }
}

void GameRoomManager::send_match_state(UdpServer* server, const GameRoom& room) {
    std::vector<uint8_t> payload;
    GameCodec::encode_match_state(room.match_state(), &payload);
    for (const GamePlayer* player : room.all_players()) {
        Buffer buf = GameCodec::build_packet(
            GameMsgType::kS2CMatchState,
            room.id(),
            player->player_id,
            0,
            room.server_tick(),
            payload);
        server->send_to(buf.peek(), buf.readable_bytes(), player->peer);
    }
}

void GameRoomManager::send_pong(UdpServer* server, const GamePacket& ping_packet,
                                const sockaddr_in& peer, uint64_t server_recv_ns) {
    uint64_t client_send = 0;
    if (!GameCodec::decode_ping(ping_packet, &client_send)) {
        return;
    }
    const uint64_t server_send_ns = now_ns();
    std::vector<uint8_t> payload;
    GameCodec::encode_pong(client_send, server_recv_ns, server_send_ns, &payload);
    const auto binding = find_binding(peer);
    const uint32_t room_id = binding ? binding->room_id : ping_packet.header.room_id;
    const uint32_t player_id = binding ? binding->player_id : ping_packet.header.player_id;
    Buffer buf = GameCodec::build_packet(
        GameMsgType::kS2CPong, room_id, player_id, ping_packet.header.seq,
        ping_packet.header.tick, payload);
    server->send_to(buf.peek(), buf.readable_bytes(), peer);
}

std::string GameRoomManager::peer_key(const sockaddr_in& peer) {
    return sockets::to_ip_port(&peer);
}

}  // namespace lightnet
