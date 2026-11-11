#include "prx/libSceAgc/Cb/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t Threads = 8;
constexpr std::size_t PacketsPerThread = 20000;
constexpr std::uint32_t FirstSize = 2;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::size_t totalWords() {
    std::size_t words = 0;
    for (std::size_t thread = 0; thread < Threads; ++thread) words += PacketsPerThread * (FirstSize + thread);
    return words;
}

void testConcurrentNops() {
    std::vector<std::uint32_t> words(totalWords(), 0xaaaaaaaau);
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    std::atomic<std::size_t> ready{0};
    std::vector<std::thread> workers;
    std::array<std::string, Threads> errors;
    for (std::size_t thread = 0; thread < Threads; ++thread) {
        workers.emplace_back([&, thread] {
            const auto size = static_cast<std::uint32_t>(FirstSize + thread);
            ready.fetch_add(1);
            while (ready.load() != Threads) std::this_thread::yield();
            try {
                for (std::size_t packet = 0; packet < PacketsPerThread; ++packet) sceAgcCbNop_nid_postfix(&buffer, size);
            } catch (const std::exception& error) {
                errors[thread] = error.what();
            }
        });
    }
    for (auto& worker : workers) worker.join();
    for (const auto& error : errors) check(error.empty(), "a concurrent NOP failed: " + error);
    check(buffer.cursor_up == words.data() + words.size(), "concurrent allocations lost cursor advances");
    std::array<std::size_t, Threads> seen{};
    for (std::size_t cursor = 0; cursor < words.size();) {
        const auto header = words[cursor];
        check((header & 0xc000ffffu) == 0xc0001000u, "packet " + std::to_string(cursor) + " has no NOP header");
        const auto size = ((header >> 16u) & 0x3fffu) + 2u;
        check(size >= FirstSize && size < FirstSize + Threads && cursor + size <= words.size(), "NOP at " + std::to_string(cursor) + " has a size no thread wrote");
        for (std::uint32_t word = 1; word < size; ++word) check(words[cursor + word] == 0, "NOP at " + std::to_string(cursor) + " was overwritten");
        ++seen[size - FirstSize];
        cursor += size;
    }
    for (std::size_t thread = 0; thread < Threads; ++thread) check(seen[thread] == PacketsPerThread, "thread " + std::to_string(thread) + " packets were overwritten");
}

}

int main() {
    try {
        testConcurrentNops();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
