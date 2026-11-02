#include "prx/libSceRudp/include/Api.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "SceTypes.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <thread>

namespace {

thread_local bool failAllocations = false;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

}

void* operator new(std::size_t bytes) {
    if (failAllocations) throw std::bad_alloc();
    if (void* memory = std::malloc(bytes == 0 ? 1 : bytes)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

int main() {
    if (TimedWait::Coarse()) return 77;
    constexpr int cancelled = static_cast<int>(0x80770014u);
    constexpr std::uint64_t forever = std::numeric_limits<std::uint64_t>::max();
    alignas(16) static std::array<unsigned char, 0x10000> pool{};
    Require(sceRudpInit_nid_postfix(pool.data(), static_cast<int>(pool.size())) == 0, "sceRudpInit");
    const int poll = sceRudpPollCreate(1);
    Require(poll > 0, "sceRudpPollCreate");
    for (const std::uint64_t timeout : {forever, std::uint64_t{10000000}}) {
        std::atomic<bool> threw{false};
        std::thread waiter([&] {
            std::array<RudpPollEvent, 4> events{};
            failAllocations = true;
            try {
                sceRudpPollWait(poll, events.data(), static_cast<std::uint32_t>(events.size()), timeout);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            failAllocations = false;
        });
        waiter.join();
        Require(threw, "the first wait on a fresh thread fails to allocate its wait state");
        std::atomic<bool> cancelReturned{false};
        std::thread canceller([&] {
            Require(sceRudpPollCancel(poll) == 0, "sceRudpPollCancel");
            cancelReturned = true;
        });
        const auto start = std::chrono::steady_clock::now();
        while (!cancelReturned && std::chrono::steady_clock::now() - start < std::chrono::seconds(2))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!cancelReturned) {
            std::fprintf(stderr, "failed: a wait that threw left its registration behind and the cancel did not return\n");
            Require(sceRudpPollDestroy(poll) == 0, "sceRudpPollDestroy releases the cancel");
            canceller.join();
            return 1;
        }
        canceller.join();
        std::array<RudpPollEvent, 4> events{};
        Require(sceRudpPollWait(poll, events.data(), static_cast<std::uint32_t>(events.size()), 0) == cancelled,
                "the cancel with no registered waiter is kept for the next wait");
        Require(sceRudpPollWait(poll, events.data(), static_cast<std::uint32_t>(events.size()), 0) == 0,
                "only one cancellation is kept");
    }
    Require(sceRudpPollDestroy(poll) == 0, "sceRudpPollDestroy");
    Require(sceRudpEnd() == 0, "sceRudpEnd");
    return 0;
}
