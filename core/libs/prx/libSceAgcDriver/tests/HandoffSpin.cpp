#include "prx/libSceAgcDriver/Execution/include/HandoffSpin.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;
using AgcDriver::HandoffSpinWindowFor;
using AgcDriver::LockSpinning;
using AgcDriver::SpinUntil;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckWindowPolicy() {
    Require(HandoffSpinWindowFor(nullptr, 20) == 150us, "20 usable processors do not spin for 150 us");
    Require(HandoffSpinWindowFor(nullptr, 9) == 150us, "9 usable processors do not spin for 150 us");
    Require(HandoffSpinWindowFor(nullptr, 8) == 0ns, "8 usable processors spin");
    Require(HandoffSpinWindowFor(nullptr, 4) == 0ns, "4 usable processors spin");
    Require(HandoffSpinWindowFor(nullptr, 0) == 0ns, "an unknown processor count spins");
    Require(HandoffSpinWindowFor("0", 20) == 0ns, "APS5_HANDOFF_SPIN_US=0 still spins");
    Require(HandoffSpinWindowFor("400", 2) == 400us, "APS5_HANDOFF_SPIN_US=400 does not set 400 us on 2 logical processors");
    Require(HandoffSpinWindowFor("-5", 20) == 0ns, "a negative window spins");
    Require(HandoffSpinWindowFor("text", 20) == 0ns, "a window that is no number spins");
    Require(HandoffSpinWindowFor("99999999999", 20) == 10000us, "a huge window is not limited to 10000 us");
}

void CheckUsableProcessors() {
    const unsigned usable = AgcDriver::UsableProcessors();
    Require(usable >= 1, "no usable processor was counted");
    Require(usable <= (std::max)(std::thread::hardware_concurrency(), 64u), "more usable processors than the machine has");
}

void CheckSpinUntil() {
    int calls = 0;
    Require(SpinUntil(0ns, [&] { ++calls; return true; }) && calls == 1, "a ready condition was evaluated more than once");
    calls = 0;
    Require(!SpinUntil(0ns, [&] { ++calls; return false; }) && calls == 1, "a zero window evaluated the condition more than once");
    calls = 0;
    Require(SpinUntil(1s, [&] { return ++calls == 5; }) && calls == 5, "a condition that holds on its fifth evaluation was not seen");
    const auto start = std::chrono::steady_clock::now();
    Require(!SpinUntil(3ms, [] { return false; }), "a condition that never holds was reported ready");
    Require(std::chrono::steady_clock::now() - start >= 3ms, "the spin returned before its window ended");
    std::atomic<bool> flag{false};
    std::thread setter([&] {
        std::this_thread::sleep_for(500us);
        flag.store(true, std::memory_order_release);
    });
    const bool seen = SpinUntil(5s, [&] { return flag.load(std::memory_order_acquire); });
    setter.join();
    Require(seen, "a flag set by another thread was not seen within the window");
}

template<typename TMutex>
void Contend(std::chrono::nanoseconds window, const char* what) {
    constexpr int Workers = 4;
    constexpr int Rounds = 20000;
    TMutex mutex;
    std::atomic<int> inside{0};
    std::atomic<bool> overlapped{false};
    std::uint64_t counter = 0;
    std::vector<std::thread> workers;
    for (int worker = 0; worker < Workers; ++worker) {
        workers.emplace_back([&] {
            for (int round = 0; round < Rounds; ++round) {
                LockSpinning(mutex, window);
                if (inside.fetch_add(1) != 0) overlapped.store(true);
                ++counter;
                inside.fetch_sub(1);
                mutex.unlock();
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(!overlapped.load(), std::string(what) + ": two threads held the mutex at once");
    Require(counter == static_cast<std::uint64_t>(Workers) * Rounds, std::string(what) + ": increments were lost");
}

void CheckBlockingFallback() {
    std::mutex mutex;
    std::atomic<bool> held{false};
    std::atomic<bool> released{false};
    std::thread holder([&] {
        mutex.lock();
        held.store(true);
        std::this_thread::sleep_for(20ms);
        released.store(true);
        mutex.unlock();
    });
    while (!held.load()) std::this_thread::yield();
    LockSpinning(mutex, 100us);
    const bool afterRelease = released.load();
    mutex.unlock();
    holder.join();
    Require(afterRelease, "a spin that outlasted its window did not wait for the holder");
}

void CheckRecursion() {
    std::recursive_mutex mutex;
    LockSpinning(mutex, 100us);
    LockSpinning(mutex, 100us);
    mutex.unlock();
    mutex.unlock();
}

}

int main() {
    try {
        CheckWindowPolicy();
        CheckUsableProcessors();
        CheckSpinUntil();
        Contend<std::mutex>(0ns, "plain lock");
        Contend<std::mutex>(50us, "spin 50 us");
        Contend<std::mutex>(1ms, "spin 1 ms");
        Contend<std::recursive_mutex>(150us, "recursive mutex");
        CheckBlockingFallback();
        CheckRecursion();
        std::cout << "handoff spin tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
