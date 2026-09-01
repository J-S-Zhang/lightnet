add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE lightnet)

add_executable(http_server http_server.cpp)
target_link_libraries(http_server PRIVATE lightnet)

add_executable(rpc_server rpc_server.cpp)
target_link_libraries(rpc_server PRIVATE lightnet)

add_executable(udp_echo_server udp_echo_server.cpp)
target_link_libraries(udp_echo_server PRIVATE lightnet)

add_executable(udp_game_server udp_game_server.cpp)
target_link_libraries(udp_game_server PRIVATE lightnet)

add_executable(udp_game_client udp_game_client.cpp)
target_link_libraries(udp_game_client PRIVATE lightnet)
