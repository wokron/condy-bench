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