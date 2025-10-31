add_library(arena60_lib
    core/config.cpp
    core/game_loop.cpp
    game/combat.cpp
    game/game_session.cpp
    game/projectile.cpp
    matchmaking/match.cpp
    matchmaking/match_request.cpp
    matchmaking/match_queue.cpp
    matchmaking/matchmaker.cpp
    matchmaking/match_notification_channel.cpp
    network/metrics_http_server.cpp
    network/websocket_server.cpp
    storage/postgres_storage.cpp
)

target_link_libraries(arena60_lib
    PUBLIC
        Boost::system
        libpq::pq
)

add_executable(arena60_server
    main.cpp
)

target_link_libraries(arena60_server
    PRIVATE
        arena60_lib
        Boost::system
        libpq::pq
        protobuf::libprotobuf
)
