#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <thread>
#include <vector>

namespace {

constexpr int Term = 15;
constexpr int Threads = 64;
constexpr int SignalsPerThread = 4;

std::atomic<int> handled{0};
std::atomic<bool> finished{false};

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "blocked signal allocator: %s\n", what);
    std::_Exit(1);
}

void APS5_VABI OnTerm(int) {
    ++handled;
}

}

int main() {
    GuestSigaction action{};
    action.handler = reinterpret_cast<std::uintptr_t>(&OnTerm);
    Require(sigaction_nid_postfix(Term, &action, nullptr) == 0, "install the handler");
    std::thread watchdog([] {
        for (int tick = 0; tick < 150 && !finished.load(); ++tick) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (finished.load()) return;
        std::fprintf(stderr, "blocked signal allocator: a worker never left the allocator after the handler ran in it, handled %d of %d\n", handled.load(), Threads * SignalsPerThread);
        std::_Exit(2);
    });
    std::atomic<bool> stop{false};
    std::atomic<int> ready{0};
    std::vector<pthread_t> natives(Threads);
    std::vector<std::thread> workers;
    GuestSignalSet blocked{};
    blocked.bits[0] = 1u << (Term - 1);
    for (int index = 0; index < Threads; ++index) {
        workers.emplace_back([&, index] {
            Require(pthread_sigmask_nid_postfix(1, &blocked, nullptr) == 0, "block the signal on a worker");
            natives[index] = pthread_self();
            ++ready;
            std::vector<void*> blocks;
            while (!stop.load(std::memory_order_relaxed)) {
                for (int n = 0; n < 64; ++n) blocks.push_back(std::malloc(2048 + (n % 7) * 512));
                for (void* block : blocks) std::free(block);
                blocks.clear();
            }
            Require(pthread_sigmask_nid_postfix(2, &blocked, nullptr) == 0, "unblock the signal on a worker");
        });
    }
    while (ready.load() < Threads) std::this_thread::yield();
    for (int round = 0; round < SignalsPerThread; ++round) {
        for (int index = 0; index < Threads; ++index) {
            Require(pthread_kill(natives[index], SIGTERM) == 0, "send the host signal to a worker");
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop = true;
    for (auto& worker : workers) worker.join();
    Require(handled.load() >= Threads && handled.load() <= Threads * SignalsPerThread, "the pending signals are delivered once the workers unblock them");
    finished = true;
    watchdog.join();
    return 0;
}
