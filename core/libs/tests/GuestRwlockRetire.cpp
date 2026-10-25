#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include <atomic>
#include <cstdlib>
#include <thread>

static void Require(bool result) {
    if (!result) std::abort();
}

static void CheckHandoff(bool write) {
    constexpr int rounds = 2000;
    std::atomic<int> phase{0};
    PthreadRwlock published = nullptr;
    std::thread successor([&] {
        for (int round = 0; round < rounds; ++round) {
            phase.wait(0);
            auto slot = published;
            phase.store(2);
            phase.notify_one();
            for (;;) {
                const int result = scePthreadRwlockTrywrlock(&slot);
                if (result == 0) break;
                Require(result == static_cast<int>(0x80020010u));
                std::this_thread::yield();
            }
            Require(scePthreadRwlockUnlock(&slot) == 0);
            Require(scePthreadRwlockDestroy(&slot) == 0);
            phase.store(3);
            phase.notify_one();
            phase.wait(3);
        }
    });
    for (int round = 0; round < rounds; ++round) {
        PthreadRwlock slot = nullptr;
        Require(scePthreadRwlockInit(&slot, nullptr, "retire") == 0);
        Require((write ? scePthreadRwlockWrlock(&slot) : scePthreadRwlockRdlock(&slot)) == 0);
        published = slot;
        phase.store(1);
        phase.notify_one();
        phase.wait(1);
        Require(scePthreadRwlockUnlock(&slot) == 0);
        while (phase.load() != 3) phase.wait(2);
        phase.store(0);
        phase.notify_one();
    }
    successor.join();
}

int main() {
    CheckHandoff(false);
    CheckHandoff(true);
}
