#pragma once

#include "lightnet/game/game_packet.h"

#include <netinet/in.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace lightnet {

/// @brief 房间内单个玩家运行时状态
struct GamePlayer {
    uint32_t player_id = 0;
    sockaddr_in peer {};
    float x = 0;
    float y = 0;
    float z = 0;
    int16_t yaw = 0;
    int16_t pitch = 0;
    int8_t move_x = 0;
    int8_t move_y = 0;
    uint16_t hp = 100;
    uint32_t last_input_seq = 0;
    uint32_t client_tick = 0;
    bool alive = true;
};

/// @brief 单个对战房间：权威 Tick + 快照广播
class GameRoom {
public:
    explicit GameRoom(uint32_t room_id, uint32_t tick_interval_ms = 33);

    uint32_t id() const { return room_id_; }
    uint32_t server_tick() const { return server_tick_; }
    uint32_t tick_interval_ms() const { return tick_interval_ms_; }
    size_t player_count() const { return players_.size(); }

    /// @brief 新玩家加入，返回 player_id；失败返回 0
    uint32_t add_player(const sockaddr_in& peer, uint32_t player_id);
    bool remove_player(uint32_t player_id);
    GamePlayer* find_player(uint32_t player_id);
    const GamePlayer* find_player(uint32_t player_id) const;

    void apply_input(uint32_t player_id, const GameInputPayload& input);
    void apply_fire(uint32_t shooter_id, const GameFirePayload& fire);

    /// @brief 推进一帧仿真并生成 snapshot
    GameSnapshotPayload tick();

    std::vector<const GamePlayer*> all_players() const;

private:
    void simulate_movement(float dt_sec);
    GamePlayer* find_nearest_enemy(const GamePlayer& shooter, const GameFirePayload& fire);

    uint32_t room_id_;
    uint32_t server_tick_;
    uint32_t tick_interval_ms_;
    std::unordered_map<uint32_t, GamePlayer> players_;
};

}  // namespace lightnet
