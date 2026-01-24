#include <chrono>
#include <condy.hpp>
#include <iostream>
#include <optional>

constexpr int num_messages = 10000000;
constexpr int buffer_size = 1024;

condy::Coro<void> producer(condy::Channel<std::optional<int>>& ch) {
    for (int i = 0; i < num_messages; ++i) {
        co_await ch.push(i);
    }
    ch.push_close();
    co_return;
}

condy::Coro<void> consumer(condy::Channel<std::optional<int>>& ch) {
    int count = 0;
    while (true) {
        auto value = co_await ch.pop();
        if (!value.has_value())
            break;
        ++count;
    }
    std::cout << "Received messages: " << count << std::endl;
    co_return;
}

int main() {
    condy::Channel<std::optional<int>> ch(buffer_size);

    auto start = std::chrono::high_resolution_clock::now();

    condy::Runtime runtime;
    condy::co_spawn(runtime, producer(ch)).detach();
    condy::co_spawn(runtime, consumer(ch)).detach();

    runtime.allow_exit();
    runtime.run();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Time taken: " << duration << " ms" << std::endl;

    return 0;
}