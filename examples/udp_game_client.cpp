/// @file udp_game_client.cpp
/// @brief 最小游戏 UDP 客户端：Join → Input → Snapshot → Ping
///
/// 用法：./udp_game_client [host] [port] [desired_room_id]
/// 示例：
///   ./udp_game_client 127.0.0.1 9092
///   ./udp_game_client 1.2.3.4 9092 1

#include "lightnet/game/game_codec.h"
#include "lightnet/net/socket_ops.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using namespace lightnet;

namespace {

uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool recv_packet(int fd, GamePacket* out, sockaddr_in* from, int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv {};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    const int sel = ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
    if (sel <= 0) {
        return false;
    }
    char buffer[2048];
    socklen_t len = sizeof(*from);
    const ssize_t n = ::recvfrom(fd, buffer, sizeof(buffer), 0,
                                 reinterpret_cast<sockaddr*>(from), &len);
    if (n <= 0) {
        return false;
    }
    return GameCodec::decode(buffer, static_cast<size_t>(n), out);
}

void send_buffer(int fd, const Buffer& buf, const sockaddr_in& to) {
    const ssize_t n = ::sendto(fd, buf.peek(), buf.readable_bytes(), MSG_NOSIGNAL,
                               reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    if (n < 0) {
        std::cerr << "sendto failed: " << std::strerror(errno) << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    const char* host = (argc >= 2) ? argv[1] : "127.0.0.1";
    const uint16_t port = (argc >= 3) ? static_cast<uint16_t>(std::stoi(argv[2])) : 9092;
    const uint32_t desired_room =
        (argc >= 4) ? static_cast<uint32_t>(std::stoul(argv[3])) : 0;

    const int fd = sockets::create_nonblocking_udp_or_die();
    // 阻塞模式更利于简单客户端 select 语义
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host, &server_addr.sin_addr) != 1) {
        std::cerr << "invalid host: " << host << '\n';
        return 1;
    }

    std::cout << "connecting to " << host << ':' << port
              << " desired_room=" << desired_room << '\n';

    std::vector<uint8_t> payload;
    GameJoinPayload join_req;
    join_req.desired_room_id = desired_room;
    GameCodec::encode_join(join_req, &payload);
    Buffer join_buf = GameCodec::build_packet(
        GameMsgType::kC2SJoin, 0, 0, 1, 0, payload);
    send_buffer(fd, join_buf, server_addr);

    GamePacket packet;
    sockaddr_in from {};
    if (!recv_packet(fd, &packet, &from, 5000) ||
        packet.header.msg_type != GameMsgType::kS2CJoinAck) {
        std::cerr << "join timeout or failed (check server / UDP firewall)\n";
        sockets::close(fd);
        return 1;
    }

    GameJoinAckPayload ack;
    if (!GameCodec::decode_join_ack(packet, &ack)) {
        std::cerr << "bad join ack payload\n";
        sockets::close(fd);
        return 1;
    }
    std::cout << "joined room=" << ack.room_id
              << " player=" << ack.player_id
              << " tick_interval_ms=" << ack.tick_interval_ms << '\n';

    uint32_t seq = 2;
    uint32_t input_seq = 1;
    int snapshots = 0;
    for (int i = 0; i < 20; ++i) {
        GameInputPayload input;
        input.input_seq = input_seq++;
        input.client_tick = static_cast<uint32_t>(i);
        input.move_x = 127;
        input.move_y = 0;
        input.yaw = static_cast<int16_t>(i * 100);
        GameCodec::encode_input(input, &payload);
        Buffer input_buf = GameCodec::build_packet(
            GameMsgType::kC2SInput,
            ack.room_id,
            ack.player_id,
            seq++,
            input.client_tick,
            payload);
        send_buffer(fd, input_buf, server_addr);

        // 排空本轮到达的包（可能有多帧 Snapshot）
        while (recv_packet(fd, &packet, &from, 80)) {
            if (packet.header.msg_type == GameMsgType::kS2CSnapshot) {
                GameSnapshotPayload snap;
                if (!GameCodec::decode_snapshot(packet, &snap)) {
                    continue;
                }
                ++snapshots;
                std::cout << "snapshot tick=" << snap.server_tick
                          << " players=" << snap.players.size();
                for (const auto& p : snap.players) {
                    std::cout << " [id=" << p.player_id
                              << " pos=(" << p.x << "," << p.y << "," << p.z << ")"
                              << " hp=" << p.hp << "]";
                }
                std::cout << '\n';
            } else if (packet.header.msg_type == GameMsgType::kS2CEvent) {
                GameEventPayload ev;
                if (GameCodec::decode_event(packet, &ev)) {
                    std::cout << "event type=" << static_cast<int>(ev.event_type)
                              << " target=" << ev.target_id
                              << " value=" << ev.value << '\n';
                }
            }
        }
        usleep(40 * 1000);
    }

    if (snapshots == 0) {
        std::cerr << "warning: no snapshots received\n";
    }

    const uint64_t t0 = now_ms();
    GameCodec::encode_ping(t0, &payload);
    Buffer ping_buf = GameCodec::build_packet(
        GameMsgType::kC2SPing, ack.room_id, ack.player_id, seq++, 0, payload);
    send_buffer(fd, ping_buf, server_addr);
    if (recv_packet(fd, &packet, &from, 2000) &&
        packet.header.msg_type == GameMsgType::kS2CPong) {
        uint64_t cs = 0, sr = 0, ss = 0;
        GameCodec::decode_pong(packet, &cs, &sr, &ss);
        std::cout << "pong rtt approx=" << (now_ms() - t0) << "ms\n";
    } else {
        std::cerr << "pong timeout\n";
    }

    std::cout << "ok snapshots=" << snapshots << '\n';
    sockets::close(fd);
    return snapshots > 0 ? 0 : 2;
}
