add_executable(channel_asio channel_asio.cpp)
target_link_libraries(channel_asio asio)

add_executable(channel_condy channel_condy.cpp)
target_link_libraries(channel_condy condy uring)