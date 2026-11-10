#include "prx/libSceRudp/libSceRudp.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <limits>
#include <source_location>
#include <thread>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "empty poll check failed at line %u\n", location.line());
    std::abort();
}

static void APS5_VABI Handler(int, int, int, void*) {}

static void RequireUntouched(const RudpPollEvent& event) {
    Require(event.contextId == 0x12345678 && event.events == 0x1234 && event.returnedEvents == 0x5678);
}

static void CheckTimeout(int poll, std::uint64_t timeout) {
    RudpPollEvent event{0x12345678, 0x1234, 0x5678};
    struct Result {
        int code;
        std::chrono::steady_clock::duration elapsed;
    };
    std::promise<Result> result;
    auto completed = result.get_future();
    std::thread waiter([&] {
        const auto start = std::chrono::steady_clock::now();
        const int code = sceRudpPollWait(poll, &event, 1, timeout);
        result.set_value({code, std::chrono::steady_clock::now() - start});
    });
    const bool ready = completed.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!ready) sceRudpPollDestroy(poll);
    waiter.join();
    Require(ready);
    const auto value = completed.get();
    Require(value.code == 0 && value.elapsed >= std::chrono::microseconds(timeout));
    RequireUntouched(event);
}

static void CheckInfiniteWait() {
    const int poll = sceRudpPollCreate(1);
    Require(poll > 0);
    RudpPollEvent event{0x12345678, 0x1234, 0x5678};
    std::promise<void> started;
    auto entered = started.get_future();
    std::promise<void> result;
    auto completed = result.get_future();
    std::thread waiter([&] {
        started.set_value();
        sceRudpPollWait(poll, &event, 1, std::numeric_limits<std::uint64_t>::max());
        result.set_value();
    });
    entered.wait();
    const bool blocked = completed.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
    sceRudpPollDestroy(poll);
    const bool woken = completed.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    if (!woken) sceRudpEnd();
    waiter.join();
    Require(blocked && woken);
    RequireUntouched(event);
}

int main() {
    constexpr int notInitialized = static_cast<int>(0x80770001u);
    constexpr int invalidArgument = static_cast<int>(0x80770004u);
    constexpr int invalidPoll = static_cast<int>(0x8077001fu);
    alignas(16) static std::array<unsigned char, 0x10000> pool{};
    RudpPollEvent event{0x12345678, 0x1234, 0x5678};
    Require(sceRudpPollWait(-1, &event, 1, 0) == notInitialized);
    Require(sceRudpPollWait(-1, nullptr, 0, 0) == notInitialized);
    RequireUntouched(event);
    Require(sceRudpInit_nid_postfix(pool.data(), pool.size()) == 0);
    const int poll = sceRudpPollCreate(1);
    Require(poll > 0);
    Require(sceRudpPollWait(-1, &event, 1, 0) == invalidArgument);
    Require(sceRudpPollWait(12345, nullptr, 1, 0) == invalidArgument);
    Require(sceRudpPollWait(12345, &event, 0, 0) == invalidArgument);
    Require(sceRudpPollWait(12345, &event, 1, 0) == invalidPoll);
    RequireUntouched(event);
    CheckTimeout(poll, 0);
    CheckTimeout(poll, 20000);
    CheckTimeout(poll, 50000);
    int context = -1;
    Require(sceRudpCreateContext(Handler, nullptr, &context) == 0);
    Require(sceRudpPollControl(poll, 1, context, 1) == 0);
    Require(sceRudpPollControl(poll, 3, context, 0) == 0);
    CheckTimeout(poll, 20000);
    Require(sceRudpPollControl(poll, 1, context, 1) == 0);
    Require(sceRudpTerminate(context) == 0);
    CheckTimeout(poll, 20000);
    Require(sceRudpPollDestroy(poll) == 0);
    CheckInfiniteWait();
    Require(sceRudpEnd() == 0);
}
