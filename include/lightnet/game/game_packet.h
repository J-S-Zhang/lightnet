#pragma once

#include <cstdint>
#include <vector>

namespace lightnet {

/// @brief 游戏 UDP 消息类型
enum class GameMsgType : uint8_t {
    kC2SJoin = 1,       ///< 客户端请求加入房间
    kS2CJoinAck = 2,    ///< 服务端分配 room_id / player_id
    kC2SInput = 3,      ///< 每 tick 输入（移动/朝向）
    kC2SFire = 4,       ///< 开火请求
    kC2SPing = 5,       ///< 测 RTT
    kS2CPong = 6,       ///< Ping 响应
    kS2CSnapshot = 7,   ///< 世界状态快照
    kS2CEvent = 8,      ///< 命中/击杀等事件
};

/// @brief 游戏事件类型
enum class GameEventType : uint8_t {
    kHit = 1,
    kKill = 2,
    kRespawn = 3,
};

/// @brief 通用包头（定长 24 字节）
struct GamePacketHeader {
    static constexpr uint32_t kMagic = 0x4C4E4750;  ///< "LNGP"
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

/// @brief 解码后的完整游戏包
struct GamePacket {
    GamePacketHeader header;
    std::vector<uint8_t> payload;
};

/// @brief C2S_INPUT payload
struct GameInputPayload {
    uint32_t input_seq = 0;
    uint32_t client_tick = 0;
    int8_t move_x = 0;
    int8_t move_y = 0;
    int16_t yaw = 0;
    int16_t pitch = 0;
    uint16_t buttons = 0;
};

/// @brief C2S_FIRE payload
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

/// @brief C2S_JOIN payload
struct GameJoinPayload {
    uint32_t desired_room_id = 0;  ///< 0 表示自动分配新房间
};

/// @brief S2C_JOIN_ACK payload
struct GameJoinAckPayload {
    uint32_t room_id = 0;
    uint32_t player_id = 0;
    uint32_t server_tick = 0;
    uint32_t tick_interval_ms = 33;
};

/// @brief 快照中的单个玩家状态
struct GamePlayerSnapshot {
    uint32_t player_id = 0;
    float x = 0;
    float y = 0;
    float z = 0;
    int16_t yaw = 0;
    int16_t pitch = 0;
    uint16_t hp = 100;
    uint16_t state_flags = 0;
    uint32_t last_input_seq = 0;
};

/// @brief S2C_SNAPSHOT payload
struct GameSnapshotPayload {
    uint32_t server_tick = 0;
    std::vector<GamePlayerSnapshot> players;
};

/// @brief S2C_EVENT payload
struct GameEventPayload {
    GameEventType event_type = GameEventType::kHit;
    uint32_t target_id = 0;
    int32_t value = 0;
    uint32_t extra = 0;
};

}  // namespace lightnet
