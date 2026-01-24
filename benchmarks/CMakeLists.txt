macro(asio_bench bench_name)
    add_executable(${bench_name}_asio ${bench_name}/asio.cpp)
    target_link_libraries(${bench_name}_asio PRIVATE asio)
endmacro()

macro(condy_bench bench_name)
    add_executable(${bench_name}_condy ${bench_name}/condy.cpp)
    target_link_libraries(${bench_name}_condy PRIVATE condy uring)
endmacro()

asio_bench(channel)
condy_bench(channel)