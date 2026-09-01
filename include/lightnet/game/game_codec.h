#pragma once

#include "lightnet/core/buffer.h"
#include "lightnet/game/game_packet.h"

namespace lightnet {

/// @brief 游戏 UDP 协议编解码
class GameCodec {
public:
    static Buffer encode(const GamePacket& packet);
    static bool decode(const char* data, size_t len, GamePacket* out);

    static void encode_input(const GameInputPayload& in, std::vector<uint8_t>* out);
    static bool decode_input(const GamePacket& packet, GameInputPayload* out);

    static void encode_fire(const GameFirePayload& in, std::vector<uint8_t>* out);
    static bool decode_fire(const GamePacket& packet, GameFirePayload* out);

    static void encode_join(const GameJoinPayload& in, std::vector<uint8_t>* out);
    static bool decode_join(const GamePacket& packet, GameJoinPayload* out);

    static void encode_join_ack(const GameJoinAckPayload& in, std::vector<uint8_t>* out);
    static bool decode_join_ack(const GamePacket& packet, GameJoinAckPayload* out);

    static void encode_snapshot(const GameSnapshotPayload& in, std::vector<uint8_t>* out);
    static bool decode_snapshot(const GamePacket& packet, GameSnapshotPayload* out);

    static void encode_event(const GameEventPayload& in, std::vector<uint8_t>* out);
    static bool decode_event(const GamePacket& packet, GameEventPayload* out);

    static void encode_ping(uint64_t client_send_time, std::vector<uint8_t>* out);
    static bool decode_ping(const GamePacket& packet, uint64_t* client_send_time);

    static void encode_pong(uint64_t client_send, uint64_t server_recv,
                            uint64_t server_send, std::vector<uint8_t>* out);
    static bool decode_pong(const GamePacket& packet, uint64_t* client_send,
                            uint64_t* server_recv, uint64_t* server_send);

    /// @brief 构造带 payload 的完整包并编码为 Buffer
    static Buffer build_packet(GameMsgType type, uint32_t room_id, uint32_t player_id,
                               uint32_t seq, uint32_t tick,
                               const std::vector<uint8_t>& payload);
};

}  // namespace lightnet
