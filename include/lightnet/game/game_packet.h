#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lightnet {

enum class GameMsgType : uint8_t {
    kC2SJoin = 1,
    kS2CJoinAck = 2,
    kC2SInput = 3,
    kC2SFire = 4,
    kC2SPing = 5,
    kS2CPong = 6,
    kS2CSnapshot = 7,
    kS2CEvent = 8,
    kC2SChat = 9,
    kS2CChat = 10,
    kC2SWeapon = 11,
    kC2SRespawn = 12,
    kS2CMatchState = 13,
    kC2SProfile = 14,
};

enum class GameEventType : uint8_t {
    kHit = 1,
    kKill = 2,
    kRespawn = 3,
};

struct GamePacketHeader {
    static constexpr uint32_t kMagic = 0x4C4E4750;  // "LNGP"
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kHeaderSize = 24;

    uint8_t version = kVersion;
    GameMsgType msg_type = GameMsgType::kC2SInput;
    uint16_t flags = 0;
    uint32_t room_id = 0;
    uint32_t player_id = 0;
    uint32_t seq = 0;
    uint32_t tick = 0;
    uint16_t payload_len = 0;
    uint16_t reserved = 0;
};

struct GamePacket {
    GamePacketHeader header;
    std::vector<uint8_t> payload;
};

struct GameInputPayload {
    uint32_t input_seq = 0;
    uint32_t client_tick = 0;
    int8_t move_x = 0;
    int8_t move_y = 0;
    int16_t yaw = 0;    // degrees * 100
    int16_t pitch = 0;  // degrees * 100
    uint16_t buttons = 0;  // bit0 fire, bit1 jump, bit2 crouch
    // 客户端上报位置（服务端做速度钳制后写入权威状态，便于与 Unity CharacterController 对齐）
    float pos_x = 0;
    float pos_y = 13.0f;
    float pos_z = 0;
    uint8_t has_pos = 0;
    uint8_t pad[3] = {};
};

struct GameFirePayload {
    uint32_t client_tick = 0;
    uint8_t weapon_id = 0;
    float origin_x = 0;
    float origin_y = 0;
    float origin_z = 0;
    float dir_x = 0;
    float dir_y = 0;
    float dir_z = 1;
};

struct GameJoinPayload {
    uint32_t desired_room_id = 0;
};

struct GameJoinAckPayload {
    uint32_t room_id = 0;
    uint32_t player_id = 0;
    uint32_t server_tick = 0;
    uint32_t tick_interval_ms = 33;
};

struct GamePlayerSnapshot {
    uint32_t player_id = 0;
    float x = 0;
    float y = 0;
    float z = 0;
    int16_t yaw = 0;
    int16_t pitch = 0;
    uint16_t hp = 100;
    uint16_t state_flags = 0;  // bit0 dead
    uint32_t last_input_seq = 0;
    uint8_t weapon_id = 0;
    uint8_t pad0 = 0;
    uint16_t kills = 0;
    uint16_t deaths = 0;
};

struct GameSnapshotPayload {
    uint32_t server_tick = 0;
    std::vector<GamePlayerSnapshot> players;
};

struct GameEventPayload {
    GameEventType event_type = GameEventType::kHit;
    uint32_t target_id = 0;
    int32_t value = 0;
    uint32_t extra = 0;  // killer id for kill, etc.
};

struct GameChatPayload {
    uint32_t sender_id = 0;
    std::string text;  // utf-8, max 200
};

struct GameWeaponPayload {
    uint8_t weapon_id = 0;
};

struct GameMatchStatePayload {
    uint32_t time_left_sec = 0;
    uint8_t match_over = 0;
};

struct GameProfilePayload {
    std::string name;  // max 32
};

}  // namespace lightnet
