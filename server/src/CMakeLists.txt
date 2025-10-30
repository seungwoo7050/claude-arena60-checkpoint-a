add_executable(arena60_server
    main.cpp
)

target_include_directories(arena60_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(arena60_server
    Boost::system
    ${PostgreSQL_LIBRARIES}
    protobuf::libprotobuf
)
