#pragma once

#include "lightnet/game/game_packet.h"

#include <netinet/in.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace lightnet {

struct GamePlayer {
    uint32_t player_id = 0;
    sockaddr_in peer {};
    float x = 0;
    float y = 13.0f;
    float z = 0;
    int16_t yaw = 0;
    int16_t pitch = 0;
    int8_t move_x = 0;
    int8_t move_y = 0;
    uint16_t hp = 100;
    uint32_t last_input_seq = 0;
    uint32_t client_tick = 0;
    bool alive = true;
    uint8_t weapon_id = 0;
    uint16_t kills = 0;
    uint16_t deaths = 0;
    std::string name;
    float respawn_timer = 0.f;
};

class GameRoom {
public:
    explicit GameRoom(uint32_t room_id, uint32_t tick_interval_ms = 33);

    uint32_t id() const { return room_id_; }
    uint32_t server_tick() const { return server_tick_; }
    uint32_t tick_interval_ms() const { return tick_interval_ms_; }
    size_t player_count() const { return players_.size(); }
    uint32_t time_left_sec() const { return time_left_sec_; }
    bool match_over() const { return match_over_; }

    uint32_t add_player(const sockaddr_in& peer, uint32_t player_id);
    bool remove_player(uint32_t player_id);
    GamePlayer* find_player(uint32_t player_id);
    const GamePlayer* find_player(uint32_t player_id) const;

    void apply_input(uint32_t player_id, const GameInputPayload& input);
    /// @return damaged target id (0 if miss); sets out_hp/out_killed
    uint32_t apply_fire(uint32_t shooter_id, const GameFirePayload& fire,
                        uint16_t* out_hp, bool* out_killed);
    void apply_weapon(uint32_t player_id, uint8_t weapon_id);
    void apply_profile(uint32_t player_id, const std::string& name);
    void apply_respawn(uint32_t player_id);

    GameSnapshotPayload tick();
    GameMatchStatePayload match_state() const;

    std::vector<const GamePlayer*> all_players() const;

private:
    void simulate_movement(float dt_sec);
    void simulate_respawn(float dt_sec);
    GamePlayer* find_hit_target(const GamePlayer& shooter, const GameFirePayload& fire);

    uint32_t room_id_;
    uint32_t server_tick_;
    uint32_t tick_interval_ms_;
    uint32_t time_left_sec_ = 300;  // 5 min
    float match_accum_ = 0.f;
    bool match_over_ = false;
    std::unordered_map<uint32_t, GamePlayer> players_;
};

}  // namespace lightnet
