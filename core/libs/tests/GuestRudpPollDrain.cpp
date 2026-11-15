#include "prx/libSceRudp/include/Api.hpp"
#include "SceTypes.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <vector>

namespace {

constexpr int NotInitialized = static_cast<int>(0x80770001u);
constexpr int Cancelled = static_cast<int>(0x80770014u);
constexpr int InvalidPoll = static_cast<int>(0x8077001Fu);
constexpr std::uint64_t Forever = std::numeric_limits<std::uint64_t>::max();
constexpr int Rounds = 20;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Rudp poll drain: %s\n", message);
    std::abort();
}

struct Round {
    int callResult;
    int backWhenReturned;
    std::vector<int> results;
};

template <typename Call>
Round RunRound(int poll, const std::vector<std::uint64_t>& timeouts, Call call) {
    std::atomic<int> entering{0};
    std::atomic<int> back{0};
    std::vector<int> results(timeouts.size(), 0);
    std::vector<std::thread> waiters;
    for (std::size_t index = 0; index < timeouts.size(); ++index) {
        waiters.emplace_back([&, index] {
            std::array<RudpPollEvent, 4> events{};
            entering.fetch_add(1);
            results[index] = sceRudpPollWait(poll, events.data(), static_cast<std::uint32_t>(events.size()), timeouts[index]);
            back.fetch_add(1);
        });
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (entering.load() != static_cast<int>(timeouts.size()) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const int callResult = call();
    const int backWhenReturned = back.load();
    for (auto& waiter : waiters) waiter.join();
    return {callResult, backWhenReturned, results};
}

}

int main() {
    alignas(16) static std::array<unsigned char, 0x10000> pool{};
    Require(sceRudpInit_nid_postfix(pool.data(), static_cast<int>(pool.size())) == 0, "sceRudpInit failed");

    bool destroyWoke = false;
    bool destroyDrained = false;
    for (int round = 0; round < Rounds && !destroyDrained; ++round) {
        const int poll = sceRudpPollCreate(1);
        Require(poll > 0, "sceRudpPollCreate failed");
        const Round result = RunRound(poll, {Forever, 10000000}, [poll] { return sceRudpPollDestroy(poll); });
        Require(result.callResult == 0, "sceRudpPollDestroy with waiters failed");
        if (std::count(result.results.begin(), result.results.end(), InvalidPoll) != 0) continue;
        Require(std::all_of(result.results.begin(), result.results.end(), [](int code) { return code == Cancelled; }),
                "a waiter woken by sceRudpPollDestroy did not return 0x80770014");
        destroyWoke = true;
        destroyDrained = result.backWhenReturned == 2;
    }
    Require(destroyWoke, "no waiter woken by sceRudpPollDestroy returned 0x80770014");
    Require(destroyDrained, "sceRudpPollDestroy returned before its waiters had left the wait");

    bool endWoke = false;
    bool endDrained = false;
    for (int round = 0; round < Rounds && !endDrained; ++round) {
        const int poll = sceRudpPollCreate(1);
        Require(poll > 0, "sceRudpPollCreate failed");
        const Round result = RunRound(poll, {Forever}, [] { return sceRudpEnd(); });
        Require(result.callResult == 0, "sceRudpEnd with a waiter failed");
        Require(sceRudpInit_nid_postfix(pool.data(), static_cast<int>(pool.size())) == 0, "sceRudpInit after sceRudpEnd failed");
        if (result.results[0] == NotInitialized) continue;
        Require(result.results[0] == Cancelled, "a waiter woken by sceRudpEnd did not return 0x80770014");
        endWoke = true;
        endDrained = result.backWhenReturned == 1;
    }
    Require(endWoke, "no waiter woken by sceRudpEnd returned 0x80770014");
    Require(endDrained, "sceRudpEnd returned before its waiter had left the wait");

    Require(sceRudpEnd() == 0, "the last sceRudpEnd failed");
    std::puts("Rudp poll drain tests passed");
    return 0;
}
