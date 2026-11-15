#include "prx/libSceAgcDriver/Execution/include/CompileBudget.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using AgcDriver::BackgroundCompileLimitFor;
using AgcDriver::CompileBudget;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckLimitPolicy() {
    Require(BackgroundCompileLimitFor(nullptr) == 8192u * 1024u, "an unset APS5_BACKGROUND_COMPILE_KIB is not 8 MiB");
    Require(BackgroundCompileLimitFor("16") == 16u * 1024u, "APS5_BACKGROUND_COMPILE_KIB=16 is not 16 KiB");
    Require(BackgroundCompileLimitFor("0") == 0u, "APS5_BACKGROUND_COMPILE_KIB=0 leaves room");
}

std::uint64_t MostHeldAtOnce(CompileBudget& budget, const std::vector<std::uint64_t>& sizes) {
    std::atomic<std::uint64_t> held{0};
    std::atomic<std::uint64_t> most{0};
    std::vector<std::thread> threads;
    for (const auto bytes : sizes) {
        threads.emplace_back([&, bytes] {
            const CompileBudget::Hold hold(budget, bytes);
            const auto now = held.fetch_add(bytes) + bytes;
            auto seen = most.load();
            while (seen < now && !most.compare_exchange_weak(seen, now)) {}
            std::this_thread::sleep_for(20ms);
            held.fetch_sub(bytes);
        });
    }
    for (auto& thread : threads) thread.join();
    return most.load();
}

void CheckSharedLimit() {
    CompileBudget budget(100);
    const auto most = MostHeldAtOnce(budget, std::vector<std::uint64_t>(8, 40));
    Require(most <= 80, "eight 40-byte holds under a 100-byte budget held " + std::to_string(most) + " bytes at once");
    Require(budget.Peak() >= 40 && budget.Peak() <= 80, "eight 40-byte holds under a 100-byte budget peaked at " + std::to_string(budget.Peak()) + " bytes");
}

void CheckZeroLimitRunsOneAtATime() {
    CompileBudget budget(0);
    const auto most = MostHeldAtOnce(budget, std::vector<std::uint64_t>(6, 1));
    Require(most == 1 && budget.Peak() == 1, "a zero budget let " + std::to_string(most) + " one-byte holds run at once");
}

void CheckLargerHoldRunsAlone() {
    CompileBudget budget(100);
    const auto most = MostHeldAtOnce(budget, {500, 10, 10, 10});
    Require(most == 500 && budget.Peak() == 500, "a 500-byte hold under a 100-byte budget did not run alone: " + std::to_string(most) + " bytes were held at once");
}

void CheckWaitingLargerHoldKeepsNewHoldsOut() {
    CompileBudget budget(100);
    std::atomic<bool> firstHeld{false};
    std::atomic<bool> releaseFirst{false};
    std::atomic<int> entries{0};
    std::atomic<int> largeEntry{-1};
    std::atomic<int> smallEntry{-1};
    std::thread first([&] {
        const CompileBudget::Hold hold(budget, 40);
        firstHeld = true;
        while (!releaseFirst) std::this_thread::sleep_for(1ms);
    });
    while (!firstHeld) std::this_thread::sleep_for(1ms);
    std::thread large([&] {
        const CompileBudget::Hold hold(budget, 500);
        largeEntry = entries++;
        std::this_thread::sleep_for(10ms);
    });
    while (budget.WaitingAlone() == 0) std::this_thread::sleep_for(1ms);
    std::thread small([&] {
        const CompileBudget::Hold hold(budget, 40);
        smallEntry = entries++;
    });
    std::this_thread::sleep_for(20ms);
    const bool enteredEarly = smallEntry != -1;
    releaseFirst = true;
    first.join();
    large.join();
    small.join();
    Require(!enteredEarly, "a 40-byte hold that fits entered while a 500-byte hold was waiting to run alone");
    Require(largeEntry == 0 && smallEntry == 1, "the waiting 500-byte hold did not run before the 40-byte hold that came after it");
}

}

int main() {
    try {
        CheckLimitPolicy();
        CheckSharedLimit();
        CheckZeroLimitRunsOneAtATime();
        CheckLargerHoldRunsAlone();
        CheckWaitingLargerHoldKeepsNewHoldsOut();
        std::cout << "compile budget tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
