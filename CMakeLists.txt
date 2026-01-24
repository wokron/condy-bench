cmake_minimum_required(VERSION 3.5)

project(condy-bench LANGUAGES CXX)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE third_party/asio/include)

add_subdirectory(third_party/condy)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(benchmarks)