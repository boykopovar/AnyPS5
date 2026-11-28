#include "prx/libSceAgc/Cb/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <future>
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

constexpr std::size_t OtherBuffers = 256;
constexpr std::uint32_t GrownWords = 64;

struct BlockedGrowth {
    std::vector<std::uint32_t> grown = std::vector<std::uint32_t>(GrownWords, 0xaaaaaaaau);
    std::promise<void> entered;
    std::future<void> others;
    bool othersFinished = false;
};

bool APS5_VABI growAfterOthers(CommandBuffer* buffer, std::uint32_t, void* user) {
    auto& growth = *static_cast<BlockedGrowth*>(user);
    growth.entered.set_value();
    growth.othersFinished = growth.others.wait_for(std::chrono::seconds(20)) == std::future_status::ready;
    buffer->bottom = growth.grown.data();
    buffer->top = growth.grown.data() + growth.grown.size();
    buffer->cursor_up = buffer->bottom;
    buffer->cursor_down = buffer->top;
    return true;
}

void testGrowthDoesNotBlockOtherBuffers() {
    std::array<std::uint32_t, 2> full{};
    BlockedGrowth growth;
    CommandBuffer buffer{full.data(), full.data() + full.size(), full.data() + full.size(), full.data() + full.size(), growAfterOthers, &growth, 0};
    std::vector<std::array<std::uint32_t, 2>> storage(OtherBuffers);
    std::vector<CommandBuffer> others;
    others.reserve(OtherBuffers);
    for (auto& words : storage) others.push_back({words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0});
    auto entered = growth.entered.get_future();
    std::string error;
    std::promise<void> finished;
    growth.others = finished.get_future();
    std::thread appender([&] {
        entered.wait();
        try {
            for (auto& other : others) sceAgcCbNop_nid_postfix(&other, 2);
        } catch (const std::exception& failure) {
            error = failure.what();
        }
        finished.set_value();
    });
    sceAgcCbNop_nid_postfix(&buffer, 2);
    appender.join();
    check(error.empty(), "an append to another buffer failed: " + error);
    check(growth.othersFinished, "appends to other buffers waited for a buffer that was growing");
    check(buffer.cursor_up == growth.grown.data() + 2, "the grown buffer did not receive its packet");
    for (const auto& other : others) check(other.cursor_up == other.top, "another buffer lost its packet");
}

struct SlowGrowth {
    std::vector<std::uint32_t> grown = std::vector<std::uint32_t>(GrownWords, 0xaaaaaaaau);
    std::atomic<bool> entered{false};
    std::atomic<std::uint32_t> calls{0};
};

bool APS5_VABI growSlowly(CommandBuffer* buffer, std::uint32_t, void* user) {
    auto& growth = *static_cast<SlowGrowth*>(user);
    growth.calls.fetch_add(1);
    growth.entered.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    buffer->bottom = growth.grown.data();
    buffer->top = growth.grown.data() + growth.grown.size();
    buffer->cursor_up = buffer->bottom;
    buffer->cursor_down = buffer->top;
    sceAgcCbNop_nid_postfix(buffer, 2);
    return true;
}

void testGrowthSerializesItsOwnBuffer() {
    std::array<std::uint32_t, 2> full{};
    SlowGrowth growth;
    CommandBuffer buffer{full.data(), full.data() + full.size(), full.data() + full.size(), full.data() + full.size(), growSlowly, &growth, 0};
    std::string error;
    std::thread second([&] {
        while (!growth.entered.load()) std::this_thread::yield();
        try {
            sceAgcCbNop_nid_postfix(&buffer, 4);
        } catch (const std::exception& failure) {
            error = failure.what();
        }
    });
    sceAgcCbNop_nid_postfix(&buffer, 3);
    second.join();
    check(error.empty(), "an append during growth failed: " + error);
    check(growth.calls.load() == 1, "an append during growth called the allocation callback again");
    check(buffer.cursor_up == growth.grown.data() + 9, "appends around growth lost cursor advances");
    check(growth.grown[0] == 0xc0001000u, "the callback's packet is not first in the grown buffer");
    const auto first = ((growth.grown[2] >> 16u) & 0x3fffu) + 2u;
    check(first == 3 || first == 4, "the grown buffer holds a packet no thread wrote");
    const auto next = ((growth.grown[2 + first] >> 16u) & 0x3fffu) + 2u;
    check(first + next == 7, "a packet appended around growth was overwritten");
}

}

int main() {
    try {
        testConcurrentNops();
        testGrowthDoesNotBlockOtherBuffers();
        testGrowthSerializesItsOwnBuffer();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
