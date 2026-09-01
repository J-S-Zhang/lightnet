cmake_minimum_required(VERSION 3.20)
project(lightnet VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++20 协程需要 GCC 10+ 或 Clang 12+
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR
            "LightNet 需要 GCC 10+ 才能使用 C++20 协程 (<coroutine>)。\n"
            "当前编译器: GCC ${CMAKE_CXX_COMPILER_VERSION}\n"
            "请安装并指定较新的编译器:\n"
            "  sudo apt update && sudo apt install -y g++-10\n"
            "  rm -rf build\n"
            "  cmake -B build -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release\n"
            "  cmake --build build -j")
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0")
        message(FATAL_ERROR
            "LightNet 需要 Clang 12+ 才能使用 C++20 协程。\n"
            "当前编译器: Clang ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
endif()

option(LIGHTNET_USE_IO_URING "Use io_uring as I/O backend (Linux 5.1+)" OFF)
option(LIGHTNET_BUILD_EXAMPLES "Build example programs" ON)

if(NOT UNIX)
    message(FATAL_ERROR "lightnet requires Linux (epoll / io_uring). Use WSL2 or a Linux VM.")
endif()

if(LIGHTNET_USE_IO_URING)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBURING REQUIRED liburing)
    add_compile_definitions(LIGHTNET_IO_URING)
else()
    add_compile_definitions(LIGHTNET_EPOLL)
endif()

set(LIGHTNET_SOURCES
    src/core/buffer.cpp
    src/core/channel.cpp
    src/core/poller.cpp
    src/core/epoll_poller.cpp
    src/core/event_loop.cpp
    src/core/timer_wheel.cpp
    src/core/event_loop_thread_pool.cpp
    src/net/socket_ops.cpp
    src/net/tcp_connection.cpp
    src/net/tcp_server.cpp
    src/net/udp_server.cpp
    src/net/connection_manager.cpp
    src/coro/scheduler.cpp
    src/http/http_message.cpp
    src/http/http_parser.cpp
    src/http/http_server.cpp
    src/rpc/rpc_codec.cpp
    src/rpc/rpc_server.cpp
    src/game/game_codec.cpp
    src/game/room.cpp
    src/game/room_manager.cpp
    src/game/game_server.cpp
    src/log/logger.cpp
    src/metrics/metrics.cpp
)

if(LIGHTNET_USE_IO_URING)
    list(APPEND LIGHTNET_SOURCES src/core/io_uring_poller.cpp)
endif()

add_library(lightnet STATIC ${LIGHTNET_SOURCES})
target_include_directories(lightnet PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(lightnet PUBLIC pthread)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(lightnet PUBLIC -fcoroutines)
endif()

if(LIGHTNET_USE_IO_URING)
    target_include_directories(lightnet PRIVATE ${LIBURING_INCLUDE_DIRS})
    target_link_libraries(lightnet PUBLIC ${LIBURING_LIBRARIES})
endif()

if(LIGHTNET_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
