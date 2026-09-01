/// @file udp_game_server.cpp
/// @brief 游戏 UDP 对战服（云部署入口）：Join / Input / Fire / Snapshot / Ping
///
/// 默认监听 0.0.0.0:9092，Tick 33ms（约 30Hz）。
///
/// 用法：
///   ./udp_game_server [--bind IP] [--port N] [--tick-ms N]
/// 示例：
///   ./udp_game_server --bind 0.0.0.0 --port 9092 --tick-ms 33
///
/// 安全组需放行 UDP 端口；本机验证：
///   ./udp_game_client 127.0.0.1 9092

#include "lightnet/lightnet.h"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

using namespace lightnet;

namespace {

EventLoop* g_loop = nullptr;

void signal_handler(int) {
    if (g_loop) {
        g_loop->quit();
    }
}

void print_usage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " [--bind IP] [--port N] [--tick-ms N]\n"
              << "  --bind     listen address (default 0.0.0.0)\n"
              << "  --port     UDP port (default 9092)\n"
              << "  --tick-ms  room tick interval (default 33)\n";
}

bool parse_args(int argc, char* argv[], std::string* bind_ip, uint16_t* port,
                uint32_t* tick_ms) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        }
        auto need_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << name << '\n';
                return nullptr;
            }
            return argv[++i];
        };
        if (arg == "--bind") {
            const char* v = need_value("--bind");
            if (!v) return false;
            *bind_ip = v;
        } else if (arg == "--port") {
            const char* v = need_value("--port");
            if (!v) return false;
            const int p = std::atoi(v);
            if (p <= 0 || p > 65535) {
                std::cerr << "invalid --port\n";
                return false;
            }
            *port = static_cast<uint16_t>(p);
        } else if (arg == "--tick-ms") {
            const char* v = need_value("--tick-ms");
            if (!v) return false;
            const int t = std::atoi(v);
            if (t < 1 || t > 1000) {
                std::cerr << "invalid --tick-ms (1..1000)\n";
                return false;
            }
            *tick_ms = static_cast<uint32_t>(t);
        } else {
            std::cerr << "unknown argument: " << arg << '\n';
            print_usage(argv[0]);
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string bind_ip = "0.0.0.0";
    uint16_t port = 9092;
    uint32_t tick_ms = 33;
    if (!parse_args(argc, argv, &bind_ip, &port, &tick_ms)) {
        return 1;
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    Logger::instance().set_level(LogLevel::kInfo);

    EventLoop loop;
    g_loop = &loop;

    GameServer server(&loop, bind_ip, port, tick_ms);
    server.start();

    LN_INFO("UDP game server listening on " + bind_ip + ":" + std::to_string(port) +
            " tick_ms=" + std::to_string(tick_ms));
    std::cout << "lightnet udp_game_server ready on " << bind_ip << ':' << port
              << " (tick " << tick_ms << " ms). Ctrl+C to stop.\n"
              << std::flush;

    loop.loop();
    server.stop();
    LN_INFO("UDP game server stopped");
    Logger::instance().flush();
    return 0;
}
