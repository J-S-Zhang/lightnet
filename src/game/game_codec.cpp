#include "lightnet/game/game_codec.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace lightnet {

namespace {

void append_u8(std::vector<uint8_t>* out, uint8_t v) {
    out->push_back(v);
}

void append_u16(std::vector<uint8_t>* out, uint16_t v) {
    out->push_back(static_cast<uint8_t>(v & 0xFF));
    out->push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void append_u32(std::vector<uint8_t>* out, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out->push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

void append_u64(std::vector<uint8_t>* out, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out->push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

void append_i8(std::vector<uint8_t>* out, int8_t v) {
    append_u8(out, static_cast<uint8_t>(v));
}

void append_i16(std::vector<uint8_t>* out, int16_t v) {
    append_u16(out, static_cast<uint16_t>(v));
}

void append_f32(std::vector<uint8_t>* out, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(v));
    append_u32(out, bits);
}

bool read_u8(const uint8_t*& p, const uint8_t* end, uint8_t* out) {
    if (p + 1 > end) return false;
    *out = *p++;
    return true;
}

bool read_u16(const uint8_t*& p, const uint8_t* end, uint16_t* out) {
    if (p + 2 > end) return false;
    *out = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
    p += 2;
    return true;
}

bool read_u32(const uint8_t*& p, const uint8_t* end, uint32_t* out) {
    if (p + 4 > end) return false;
    *out = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
    return true;
}

bool read_u64(const uint8_t*& p, const uint8_t* end, uint64_t* out) {
    if (p + 8 > end) return false;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    *out = v;
    p += 8;
    return true;
}

bool read_i8(const uint8_t*& p, const uint8_t* end, int8_t* out) {
    uint8_t v = 0;
    if (!read_u8(p, end, &v)) return false;
    *out = static_cast<int8_t>(v);
    return true;
}

bool read_i16(const uint8_t*& p, const uint8_t* end, int16_t* out) {
    uint16_t v = 0;
    if (!read_u16(p, end, &v)) return false;
    *out = static_cast<int16_t>(v);
    return true;
}

bool read_f32(const uint8_t*& p, const uint8_t* end, float* out) {
    uint32_t bits = 0;
    if (!read_u32(p, end, &bits)) return false;
    std::memcpy(out, &bits, sizeof(*out));
    return true;
}

const uint8_t* payload_begin(const GamePacket& packet) {
    return packet.payload.data();
}

const uint8_t* payload_end(const GamePacket& packet) {
    return packet.payload.data() + packet.payload.size();
}

}  // namespace

Buffer GameCodec::encode(const GamePacket& packet) {
    Buffer buf;
    buf.ensure_writable(static_cast<size_t>(GamePacketHeader::kHeaderSize + packet.payload.size()));

    uint32_t magic = GamePacketHeader::kMagic;
    buf.append(reinterpret_cast<const char*>(&magic), 4);
    uint8_t version = packet.header.version;
    buf.append(reinterpret_cast<const char*>(&version), 1);
    uint8_t msg_type = static_cast<uint8_t>(packet.header.msg_type);
    buf.append(reinterpret_cast<const char*>(&msg_type), 1);
    uint16_t flags = packet.header.flags;
    buf.append(reinterpret_cast<const char*>(&flags), 2);
    buf.append(reinterpret_cast<const char*>(&packet.header.room_id), 4);
    buf.append(reinterpret_cast<const char*>(&packet.header.player_id), 4);
    buf.append(reinterpret_cast<const char*>(&packet.header.seq), 4);
    buf.append(reinterpret_cast<const char*>(&packet.header.tick), 4);
    uint16_t payload_len = static_cast<uint16_t>(packet.payload.size());
    buf.append(reinterpret_cast<const char*>(&payload_len), 2);
    uint16_t reserved = packet.header.reserved;
    buf.append(reinterpret_cast<const char*>(&reserved), 2);
    if (!packet.payload.empty()) {
        buf.append(reinterpret_cast<const char*>(packet.payload.data()), packet.payload.size());
    }
    return buf;
}

bool GameCodec::decode(const char* data, size_t len, GamePacket* out) {
    if (len < GamePacketHeader::kHeaderSize) return false;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    const uint8_t* end = p + len;

    uint32_t magic = 0;
    if (!read_u32(p, end, &magic) || magic != GamePacketHeader::kMagic) return false;

    uint8_t version = 0;
    if (!read_u8(p, end, &version) || version != GamePacketHeader::kVersion) return false;

    uint8_t msg_type_raw = 0;
    if (!read_u8(p, end, &msg_type_raw)) return false;

    out->header.version = version;
    out->header.msg_type = static_cast<GameMsgType>(msg_type_raw);
    if (!read_u16(p, end, &out->header.flags)) return false;
    if (!read_u32(p, end, &out->header.room_id)) return false;
    if (!read_u32(p, end, &out->header.player_id)) return false;
    if (!read_u32(p, end, &out->header.seq)) return false;
    if (!read_u32(p, end, &out->header.tick)) return false;
    if (!read_u16(p, end, &out->header.payload_len)) return false;
    if (!read_u16(p, end, &out->header.reserved)) return false;

    if (p + out->header.payload_len > end) return false;
    out->payload.assign(p, p + out->header.payload_len);
    return true;
}

void GameCodec::encode_input(const GameInputPayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u32(out, in.input_seq);
    append_u32(out, in.client_tick);
    append_i8(out, in.move_x);
    append_i8(out, in.move_y);
    append_i16(out, in.yaw);
    append_i16(out, in.pitch);
    append_u16(out, in.buttons);
}

bool GameCodec::decode_input(const GamePacket& packet, GameInputPayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    uint16_t buttons = 0;
    return read_u32(p, end, &out->input_seq) &&
           read_u32(p, end, &out->client_tick) &&
           read_i8(p, end, &out->move_x) &&
           read_i8(p, end, &out->move_y) &&
           read_i16(p, end, &out->yaw) &&
           read_i16(p, end, &out->pitch) &&
           read_u16(p, end, &buttons) &&
           (out->buttons = buttons, true);
}

void GameCodec::encode_fire(const GameFirePayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u32(out, in.client_tick);
    append_u8(out, in.weapon_id);
    append_u8(out, 0);
    append_u8(out, 0);
    append_u8(out, 0);
    append_f32(out, in.origin_x);
    append_f32(out, in.origin_y);
    append_f32(out, in.origin_z);
    append_f32(out, in.dir_x);
    append_f32(out, in.dir_y);
    append_f32(out, in.dir_z);
}

bool GameCodec::decode_fire(const GamePacket& packet, GameFirePayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    uint8_t pad[3] = {};
    return read_u32(p, end, &out->client_tick) &&
           read_u8(p, end, &out->weapon_id) &&
           read_u8(p, end, &pad[0]) &&
           read_u8(p, end, &pad[1]) &&
           read_u8(p, end, &pad[2]) &&
           read_f32(p, end, &out->origin_x) &&
           read_f32(p, end, &out->origin_y) &&
           read_f32(p, end, &out->origin_z) &&
           read_f32(p, end, &out->dir_x) &&
           read_f32(p, end, &out->dir_y) &&
           read_f32(p, end, &out->dir_z);
}

void GameCodec::encode_join(const GameJoinPayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u32(out, in.desired_room_id);
}

bool GameCodec::decode_join(const GamePacket& packet, GameJoinPayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    return read_u32(p, end, &out->desired_room_id);
}

void GameCodec::encode_join_ack(const GameJoinAckPayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u32(out, in.room_id);
    append_u32(out, in.player_id);
    append_u32(out, in.server_tick);
    append_u32(out, in.tick_interval_ms);
}

bool GameCodec::decode_join_ack(const GamePacket& packet, GameJoinAckPayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    return read_u32(p, end, &out->room_id) &&
           read_u32(p, end, &out->player_id) &&
           read_u32(p, end, &out->server_tick) &&
           read_u32(p, end, &out->tick_interval_ms);
}

void GameCodec::encode_snapshot(const GameSnapshotPayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u32(out, in.server_tick);
    append_u8(out, static_cast<uint8_t>(in.players.size()));
    append_u8(out, 0);
    append_u8(out, 0);
    append_u8(out, 0);
    for (const auto& pl : in.players) {
        append_u32(out, pl.player_id);
        append_f32(out, pl.x);
        append_f32(out, pl.y);
        append_f32(out, pl.z);
        append_i16(out, pl.yaw);
        append_i16(out, pl.pitch);
        append_u16(out, pl.hp);
        append_u16(out, pl.state_flags);
        append_u32(out, pl.last_input_seq);
        append_u8(out, pl.weapon_id);
        append_u8(out, pl.pad0);
        append_u16(out, pl.kills);
        append_u16(out, pl.deaths);
    }
}

bool GameCodec::decode_snapshot(const GamePacket& packet, GameSnapshotPayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    uint8_t count = 0;
    uint8_t pad[3] = {};
    if (!read_u32(p, end, &out->server_tick) ||
        !read_u8(p, end, &count) ||
        !read_u8(p, end, &pad[0]) ||
        !read_u8(p, end, &pad[1]) ||
        !read_u8(p, end, &pad[2])) {
        return false;
    }
    out->players.clear();
    out->players.reserve(count);
    for (uint8_t i = 0; i < count; ++i) {
        GamePlayerSnapshot pl;
        if (!read_u32(p, end, &pl.player_id) ||
            !read_f32(p, end, &pl.x) ||
            !read_f32(p, end, &pl.y) ||
            !read_f32(p, end, &pl.z) ||
            !read_i16(p, end, &pl.yaw) ||
            !read_i16(p, end, &pl.pitch) ||
            !read_u16(p, end, &pl.hp) ||
            !read_u16(p, end, &pl.state_flags) ||
            !read_u32(p, end, &pl.last_input_seq) ||
            !read_u8(p, end, &pl.weapon_id) ||
            !read_u8(p, end, &pl.pad0) ||
            !read_u16(p, end, &pl.kills) ||
            !read_u16(p, end, &pl.deaths)) {
            return false;
        }
        out->players.push_back(pl);
    }
    return true;
}

void GameCodec::encode_event(const GameEventPayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u8(out, static_cast<uint8_t>(in.event_type));
    append_u8(out, 0);
    append_u8(out, 0);
    append_u8(out, 0);
    append_u32(out, in.target_id);
    append_u32(out, static_cast<uint32_t>(in.value));
    append_u32(out, in.extra);
}

bool GameCodec::decode_event(const GamePacket& packet, GameEventPayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    uint8_t type_raw = 0;
    uint8_t pad[3] = {};
    uint32_t value_u = 0;
    if (!read_u8(p, end, &type_raw) ||
        !read_u8(p, end, &pad[0]) ||
        !read_u8(p, end, &pad[1]) ||
        !read_u8(p, end, &pad[2]) ||
        !read_u32(p, end, &out->target_id) ||
        !read_u32(p, end, &value_u) ||
        !read_u32(p, end, &out->extra)) {
        return false;
    }
    out->event_type = static_cast<GameEventType>(type_raw);
    out->value = static_cast<int32_t>(value_u);
    return true;
}

void GameCodec::encode_ping(uint64_t client_send_time, std::vector<uint8_t>* out) {
    out->clear();
    append_u64(out, client_send_time);
}

bool GameCodec::decode_ping(const GamePacket& packet, uint64_t* client_send_time) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    return read_u64(p, end, client_send_time);
}

void GameCodec::encode_pong(uint64_t client_send, uint64_t server_recv,
                            uint64_t server_send, std::vector<uint8_t>* out) {
    out->clear();
    append_u64(out, client_send);
    append_u64(out, server_recv);
    append_u64(out, server_send);
}

bool GameCodec::decode_pong(const GamePacket& packet, uint64_t* client_send,
                            uint64_t* server_recv, uint64_t* server_send) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    return read_u64(p, end, client_send) &&
           read_u64(p, end, server_recv) &&
           read_u64(p, end, server_send);
}

void GameCodec::encode_chat(const GameChatPayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u32(out, in.sender_id);
    const uint16_t n = static_cast<uint16_t>(std::min<size_t>(in.text.size(), 200));
    append_u16(out, n);
    for (uint16_t i = 0; i < n; ++i) {
        append_u8(out, static_cast<uint8_t>(in.text[i]));
    }
}

bool GameCodec::decode_chat(const GamePacket& packet, GameChatPayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    uint16_t n = 0;
    if (!read_u32(p, end, &out->sender_id) || !read_u16(p, end, &n)) return false;
    if (p + n > end) return false;
    out->text.assign(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p) + n);
    return true;
}

void GameCodec::encode_weapon(const GameWeaponPayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u8(out, in.weapon_id);
    append_u8(out, 0);
    append_u8(out, 0);
    append_u8(out, 0);
}

bool GameCodec::decode_weapon(const GamePacket& packet, GameWeaponPayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    uint8_t pad = 0;
    return read_u8(p, end, &out->weapon_id) &&
           read_u8(p, end, &pad) &&
           read_u8(p, end, &pad) &&
           read_u8(p, end, &pad);
}

void GameCodec::encode_match_state(const GameMatchStatePayload& in, std::vector<uint8_t>* out) {
    out->clear();
    append_u32(out, in.time_left_sec);
    append_u8(out, in.match_over);
    append_u8(out, 0);
    append_u8(out, 0);
    append_u8(out, 0);
}

bool GameCodec::decode_match_state(const GamePacket& packet, GameMatchStatePayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    uint8_t pad = 0;
    return read_u32(p, end, &out->time_left_sec) &&
           read_u8(p, end, &out->match_over) &&
           read_u8(p, end, &pad) &&
           read_u8(p, end, &pad) &&
           read_u8(p, end, &pad);
}

void GameCodec::encode_profile(const GameProfilePayload& in, std::vector<uint8_t>* out) {
    out->clear();
    const uint16_t n = static_cast<uint16_t>(std::min<size_t>(in.name.size(), 32));
    append_u16(out, n);
    for (uint16_t i = 0; i < n; ++i) {
        append_u8(out, static_cast<uint8_t>(in.name[i]));
    }
}

bool GameCodec::decode_profile(const GamePacket& packet, GameProfilePayload* out) {
    const uint8_t* p = payload_begin(packet);
    const uint8_t* end = payload_end(packet);
    uint16_t n = 0;
    if (!read_u16(p, end, &n) || p + n > end) return false;
    out->name.assign(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(p) + n);
    return true;
}

Buffer GameCodec::build_packet(GameMsgType type, uint32_t room_id, uint32_t player_id,
                               uint32_t seq, uint32_t tick,
                               const std::vector<uint8_t>& payload) {
    GamePacket packet;
    packet.header.msg_type = type;
    packet.header.room_id = room_id;
    packet.header.player_id = player_id;
    packet.header.seq = seq;
    packet.header.tick = tick;
    packet.payload = payload;
    return encode(packet);
}

}  // namespace lightnet
