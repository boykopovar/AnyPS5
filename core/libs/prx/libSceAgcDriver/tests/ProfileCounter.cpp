#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using AgcDriver::ProfileCounter;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckSums(std::size_t threads) {
    constexpr std::uint64_t Adds = 200000;
    static ProfileCounter counter;
    const auto before = counter.Sum();
    std::vector<std::size_t> slots(threads);
    std::vector<std::thread> pool;
    for (std::size_t index = 0; index < threads; ++index) {
        pool.emplace_back([&, index] {
            slots[index] = ProfileCounter::ThreadSlot();
            for (std::uint64_t add = 0; add < Adds; ++add) counter.Add(add % 3 == 0 ? 2 : 1);
        });
    }
    for (auto& thread : pool) thread.join();
    std::uint64_t expected = 0;
    for (std::uint64_t add = 0; add < Adds; ++add) expected += add % 3 == 0 ? 2 : 1;
    Require(counter.Sum() - before == expected * threads, "a profile counter lost adds across " + std::to_string(threads) + " threads");
    const std::set<std::size_t> distinct(slots.begin(), slots.end());
    if (ProfileCounter::Shared()) Require(distinct.size() == 1 && *distinct.begin() == 0, "APS5_SHARED_PROFILE_COUNTERS did not put every thread on one slot");
    else Require(distinct.size() == std::min<std::size_t>(threads, ProfileCounter::Slots), "threads up to the slot count did not get slots of their own");
}

void CheckTake() {
    static ProfileCounter counter;
    std::vector<std::thread> pool;
    for (int index = 0; index < 6; ++index) pool.emplace_back([] { counter.Add(5); });
    for (auto& thread : pool) thread.join();
    Require(counter.Take() == 30, "Take did not return the adds of every thread");
    Require(counter.Sum() == 0 && counter.Take() == 0, "Take did not reset every slot");
}

void RunBench(int threads) {
    constexpr std::uint64_t Adds = 5000000;
    static ProfileCounter sharded;
    static std::atomic<std::uint64_t> shared{0};
    const auto measure = [&](auto&& add) {
        std::atomic<bool> start{false};
        std::vector<double> nanoseconds(static_cast<std::size_t>(threads));
        std::vector<std::thread> pool;
        for (int index = 0; index < threads; ++index) {
            pool.emplace_back([&, index] {
                while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
                const auto begin = std::chrono::steady_clock::now();
                for (std::uint64_t value = 0; value < Adds; ++value) add();
                nanoseconds[static_cast<std::size_t>(index)] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count() / static_cast<double>(Adds);
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& thread : pool) thread.join();
        double total = 0;
        for (const auto value : nanoseconds) total += value;
        return total / threads;
    };
    const double one = measure([] { shared.fetch_add(1, std::memory_order_relaxed); });
    const double many = measure([] { sharded.Add(); });
    std::printf("profile counter bench, %d threads: one shared atomic %.1f ns per add, ProfileCounter %.1f ns per add%s\n", threads, one, many, ProfileCounter::Shared() ? " (APS5_SHARED_PROFILE_COUNTERS)" : "");
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--bench") {
            RunBench(std::stoi(argv[2]));
            return 0;
        }
#ifdef _WIN32
        if (argc == 2 && std::string_view(argv[1]) == "--tls-expansion") {
            for (int taken = 0; taken < 64; ++taken) if (TlsAlloc() >= 64) break;
            Require(ProfileCounter::ThreadSlot() < ProfileCounter::Slots, "no slot from an expansion TLS index");
        }
#endif
        for (const std::size_t threads : {1u, 2u, 4u, 16u, 20u}) CheckSums(threads);
        CheckTake();
        std::puts("profile counter tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
