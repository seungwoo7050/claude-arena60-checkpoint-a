add_executable(arena60_server main.cpp)

target_link_libraries(arena60_server
    Boost::system
    libpq::pq
    protobuf::libprotobuf
)
