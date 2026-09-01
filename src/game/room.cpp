#include "lightnet/game/room.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lightnet {

namespace {

constexpr float kMoveSpeed = 4.0f;
constexpr float kHitRadius = 2.5f;
constexpr int kFireDamage = 25;

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
    player.x = static_cast<float>(players_.size()) * 2.0f;
    player.y = 0;
    player.z = 0;
    player.hp = 100;
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
    if (!player || !player->alive) {
        return;
    }
    if (input.input_seq <= player->last_input_seq) {
        return;
    }
    player->last_input_seq = input.input_seq;
    player->client_tick = input.client_tick;
    player->move_x = input.move_x;
    player->move_y = input.move_y;
    player->yaw = input.yaw;
    player->pitch = input.pitch;
}

void GameRoom::apply_fire(uint32_t shooter_id, const GameFirePayload& fire) {
    GamePlayer* shooter = find_player(shooter_id);
    if (!shooter || !shooter->alive) {
        return;
    }
    (void)fire;
    GamePlayer* target = find_nearest_enemy(*shooter, fire);
    if (!target) {
        return;
    }
    if (target->hp <= kFireDamage) {
        target->hp = 0;
        target->alive = false;
    } else {
        target->hp = static_cast<uint16_t>(target->hp - kFireDamage);
    }
}

GameSnapshotPayload GameRoom::tick() {
    ++server_tick_;
    const float dt = static_cast<float>(tick_interval_ms_) / 1000.0f;
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
        snap.players.push_back(ps);
    }
    return snap;
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

void GameRoom::simulate_movement(float dt_sec) {
    for (auto& [id, player] : players_) {
        (void)id;
        if (!player.alive) {
            continue;
        }
        const float nx = static_cast<float>(player.move_x) / 127.0f;
        const float ny = static_cast<float>(player.move_y) / 127.0f;
        player.x += nx * kMoveSpeed * dt_sec;
        player.z += ny * kMoveSpeed * dt_sec;
    }
}

GamePlayer* GameRoom::find_nearest_enemy(const GamePlayer& shooter, const GameFirePayload& fire) {
    GamePlayer* best = nullptr;
    float best_dist = std::numeric_limits<float>::max();
    for (auto& [id, player] : players_) {
        (void)id;
        if (player.player_id == shooter.player_id || !player.alive) {
            continue;
        }
        const float dx = player.x - fire.origin_x;
        const float dy = player.y - fire.origin_y;
        const float dz = player.z - fire.origin_z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < best_dist) {
            best_dist = dist;
            best = &player;
        }
    }
    if (best && best_dist <= kHitRadius * 4.0f) {
        return best;
    }
    return nullptr;
}

}  // namespace lightnet
