macro(bench bench_name impl_name)
    add_executable(${bench_name}_${impl_name} ${bench_name}/${impl_name}.cpp)
    target_link_libraries(${bench_name}_${impl_name} PRIVATE ${ARGN})
endmacro()

bench(channel asio asio)
bench(channel condy condy uring)

bench(spawn asio asio)
bench(spawn condy condy uring)

bench(post asio asio)
bench(post condy condy uring)

bench(file_read asio asio uring)
bench(file_read condy condy uring)
bench(file_read sync)
bench(file_read aio aio)
bench(file_read uring uring)

bench(file_random_read asio asio uring)
bench(file_random_read condy condy uring)
bench(file_random_read sync)
bench(file_random_read aio aio)

bench(echo_server condy condy uring)
bench(echo_server asio asio)
bench(echo_server epoll)

add_executable(echo_stress echo_stress.cpp)