cmake_minimum_required(VERSION 3.5)

project(condy-bench LANGUAGES CXX)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE third_party/asio/include)

set(LINK_LIBURING OFF)
add_subdirectory(third_party/condy)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test test.cpp)
target_link_libraries(test condy asio)