#include "lightnet/game/room.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lightnet {

namespace {

constexpr float kMoveSpeed = 6.0f;
constexpr float kMaxHitRange = 80.0f;
constexpr float kHitRadius = 1.2f;
constexpr int kFireDamage = 25;
constexpr float kRespawnDelay = 5.0f;
constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;
constexpr float kYawScale = 100.0f;

}  // namespace

GameRoom::GameRoom(uint32_t room_id, uint32_t tick_interval_ms)
    : room_id_(room_id),
      server_tick_(0),
      tick_interval_ms_(tick_interval_ms) {}

uint32_t GameRoom::add_player(const sockaddr_in& peer, uint32_t player_id) {
    if (player_id == 0 || players_.count(player_id) > 0) {
        return 0;
    }
    GamePlayer player;
    player.player_id = player_id;
    player.peer = peer;
    const size_t idx = players_.size();
    player.x = static_cast<float>(idx % 4) * 3.0f;
    player.y = 13.0f;
    player.z = static_cast<float>(idx / 4) * 3.0f;
    player.hp = 100;
    player.name = "Player" + std::to_string(player_id);
    players_.emplace(player_id, player);
    return player_id;
}

bool GameRoom::remove_player(uint32_t player_id) {
    return players_.erase(player_id) > 0;
}

GamePlayer* GameRoom::find_player(uint32_t player_id) {
    auto it = players_.find(player_id);
    return it == players_.end() ? nullptr : &it->second;
}

const GamePlayer* GameRoom::find_player(uint32_t player_id) const {
    auto it = players_.find(player_id);
    return it == players_.end() ? nullptr : &it->second;
}

void GameRoom::apply_input(uint32_t player_id, const GameInputPayload& input) {
    GamePlayer* player = find_player(player_id);
    if (!player || !player->alive || match_over_) {
        return;
    }
    if (input.input_seq <= player->last_input_seq) {
        return;
    }
    const bool first_input = player->last_input_seq == 0;
    player->last_input_seq = input.input_seq;
    player->client_tick = input.client_tick;
    player->move_x = input.move_x;
    player->move_y = input.move_y;
    player->yaw = input.yaw;
    player->pitch = input.pitch;

    if (input.has_pos) {
        player->client_pos_authoritative = true;
        const float dx = input.pos_x - player->x;
        const float dy = input.pos_y - player->y;
        const float dz = input.pos_z - player->z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        // 首包允许直接对齐到客户端真实落点；之后限制单包位移
        const float kMaxStep = first_input ? 100.0f : 3.0f;
        if (dist <= kMaxStep) {
            player->x = input.pos_x;
            player->y = input.pos_y;
            player->z = input.pos_z;
        } else if (dist > 0.0001f) {
            const float s = kMaxStep / dist;
            player->x += dx * s;
            player->y += dy * s;
            player->z += dz * s;
        }
    } else {
        player->client_pos_authoritative = false;
    }
}

void GameRoom::simulate_movement(float dt_sec) {
    for (auto& [id, player] : players_) {
        (void)id;
        if (!player.alive || match_over_) continue;
        if (player.client_pos_authoritative) continue;
        const float yaw_deg = static_cast<float>(player.yaw) / kYawScale;
        const float yaw = yaw_deg * kDeg2Rad;
        const float fx = std::sin(yaw);
        const float fz = std::cos(yaw);
        const float rx = std::cos(yaw);
        const float rz = -std::sin(yaw);
        const float nx = static_cast<float>(player.move_x) / 127.0f;
        const float nz = static_cast<float>(player.move_y) / 127.0f;
        player.x += (fx * nz + rx * nx) * kMoveSpeed * dt_sec;
        player.z += (fz * nz + rz * nx) * kMoveSpeed * dt_sec;
    }
}

uint32_t GameRoom::apply_fire(uint32_t shooter_id, const GameFirePayload& fire,
                              uint16_t* out_hp, bool* out_killed) {
    if (out_hp) *out_hp = 0;
    if (out_killed) *out_killed = false;
    if (match_over_) return 0;

    GamePlayer* shooter = find_player(shooter_id);
    if (!shooter || !shooter->alive) {
        return 0;
    }

    // Use server-known shooter position as origin base (anti-teleport), keep client aim dir
    GameFirePayload validated = fire;
    validated.origin_x = shooter->x;
    validated.origin_y = shooter->y + 1.5f;
    validated.origin_z = shooter->z;

    float len = std::sqrt(validated.dir_x * validated.dir_x +
                          validated.dir_y * validated.dir_y +
                          validated.dir_z * validated.dir_z);
    if (len < 1e-4f) {
        return 0;
    }
    validated.dir_x /= len;
    validated.dir_y /= len;
    validated.dir_z /= len;

    GamePlayer* target = find_hit_target(*shooter, validated);
    if (!target) {
        return 0;
    }

    const uint16_t before = target->hp;
    if (target->hp <= kFireDamage) {
        target->hp = 0;
        target->alive = false;
        target->respawn_timer = kRespawnDelay;
        target->deaths += 1;
        shooter->kills += 1;
        if (out_killed) *out_killed = true;
    } else {
        target->hp = static_cast<uint16_t>(target->hp - kFireDamage);
    }
    if (out_hp) *out_hp = target->hp;
    (void)before;
    return target->player_id;
}

void GameRoom::apply_weapon(uint32_t player_id, uint8_t weapon_id) {
    GamePlayer* player = find_player(player_id);
    if (!player || !player->alive) return;
    if (weapon_id > 5) return;
    player->weapon_id = weapon_id;
}

void GameRoom::apply_profile(uint32_t player_id, const std::string& name) {
    GamePlayer* player = find_player(player_id);
    if (!player) return;
    player->name = name.empty() ? player->name : name.substr(0, 32);
}

void GameRoom::apply_respawn(uint32_t player_id) {
    GamePlayer* player = find_player(player_id);
    if (!player || player->alive || match_over_) return;
    if (player->respawn_timer > 0.f) return;
    player->alive = true;
    player->hp = 100;
    player->x = static_cast<float>((player_id % 4) * 3);
    player->y = 13.0f;
    player->z = static_cast<float>((player_id / 4) * 3);
    player->move_x = 0;
    player->move_y = 0;
}

GameSnapshotPayload GameRoom::tick() {
    ++server_tick_;
    const float dt = static_cast<float>(tick_interval_ms_) / 1000.0f;
    if (!match_over_) {
        match_accum_ += dt;
        while (match_accum_ >= 1.0f && time_left_sec_ > 0) {
            match_accum_ -= 1.0f;
            --time_left_sec_;
        }
        if (time_left_sec_ == 0) {
            match_over_ = true;
        }
    }
    simulate_respawn(dt);
    simulate_movement(dt);

    GameSnapshotPayload snap;
    snap.server_tick = server_tick_;
    snap.players.reserve(players_.size());
    for (const auto& [id, player] : players_) {
        (void)id;
        GamePlayerSnapshot ps;
        ps.player_id = player.player_id;
        ps.x = player.x;
        ps.y = player.y;
        ps.z = player.z;
        ps.yaw = player.yaw;
        ps.pitch = player.pitch;
        ps.hp = player.hp;
        ps.state_flags = player.alive ? 0 : 1;
        ps.last_input_seq = player.last_input_seq;
        ps.weapon_id = player.weapon_id;
        ps.kills = player.kills;
        ps.deaths = player.deaths;
        snap.players.push_back(ps);
    }
    return snap;
}

GameMatchStatePayload GameRoom::match_state() const {
    GameMatchStatePayload m;
    m.time_left_sec = time_left_sec_;
    m.match_over = match_over_ ? 1 : 0;
    return m;
}

std::vector<const GamePlayer*> GameRoom::all_players() const {
    std::vector<const GamePlayer*> out;
    out.reserve(players_.size());
    for (const auto& [id, player] : players_) {
        (void)id;
        out.push_back(&player);
    }
    return out;
}

void GameRoom::simulate_respawn(float dt_sec) {
    for (auto& [id, player] : players_) {
        (void)id;
        if (player.alive) continue;
        if (player.respawn_timer > 0.f) {
            player.respawn_timer -= dt_sec;
            if (player.respawn_timer < 0.f) player.respawn_timer = 0.f;
        }
    }
}

GamePlayer* GameRoom::find_hit_target(const GamePlayer& shooter, const GameFirePayload& fire) {
    GamePlayer* best = nullptr;
    float best_t = kMaxHitRange;
    for (auto& [id, player] : players_) {
        (void)id;
        if (player.player_id == shooter.player_id || !player.alive) continue;

        // Aim at chest height
        const float tx = player.x - fire.origin_x;
        const float ty = (player.y + 1.0f) - fire.origin_y;
        const float tz = player.z - fire.origin_z;
        const float t = tx * fire.dir_x + ty * fire.dir_y + tz * fire.dir_z;
        if (t < 0.5f || t > kMaxHitRange) continue;

        const float px = fire.origin_x + fire.dir_x * t;
        const float py = fire.origin_y + fire.dir_y * t;
        const float pz = fire.origin_z + fire.dir_z * t;
        const float dx = player.x - px;
        const float dy = (player.y + 1.0f) - py;
        const float dz = player.z - pz;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist <= kHitRadius && t < best_t) {
            best_t = t;
            best = &player;
        }
    }
    return best;
}

}  // namespace lightnet
