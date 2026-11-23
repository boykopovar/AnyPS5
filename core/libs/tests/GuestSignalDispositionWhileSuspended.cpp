#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <thread>

using Handler = void (APS5_VABI *)(int);

static constexpr int SigUsr1 = 30;
static constexpr int SigUsr2 = 31;
static constexpr int SigBlock = 1;
static constexpr int Rounds = 2000;

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> handled{0};
static std::atomic<bool> done{false};

static void APS5_VABI Count(int) { handled.fetch_add(1); }

struct Suspender {
    int interrupted = 0;
};

static void* APS5_VABI SuspendRounds(void* arg) {
    auto& state = *static_cast<Suspender*>(arg);
    const GuestSignalSet user1{{1u << (SigUsr1 - 1), 0, 0, 0}};
    const GuestSignalSet empty{{0, 0, 0, 0}};
    Require(sigprocmask_nid_postfix(SigBlock, &user1, nullptr) == 0);
    for (int round = 0; round < Rounds; ++round) {
        if (sigsuspend_nid_postfix(&empty) == -1 && *__error_nid_postfix() == 4) ++state.interrupted;
    }
    done.store(true);
    return nullptr;
}

static void* APS5_VABI SendRounds(void* arg) {
    const auto target = static_cast<Pthread>(arg);
    for (int round = 0; round < Rounds; ++round) {
        while (handled.load() < round) std::this_thread::yield();
        Require(pthread_kill_nid_postfix(target, SigUsr1) == 0);
    }
    return nullptr;
}

int main() {
    const auto ignore = reinterpret_cast<Handler>(std::uintptr_t{1});
    Require(signal_nid_postfix(SigUsr1, Count) != reinterpret_cast<Handler>(std::uintptr_t(-1)));
    Suspender state;
    Pthread suspender = nullptr;
    Require(scePthreadCreate(&suspender, nullptr, SuspendRounds, &state, nullptr) == 0);
    Pthread sender = nullptr;
    Require(scePthreadCreate(&sender, nullptr, SendRounds, suspender, nullptr) == 0);
    while (!done.load()) {
        signal_nid_postfix(SigUsr2, ignore);
        signal_nid_postfix(SigUsr2, nullptr);
    }
    Require(scePthreadJoin(sender, nullptr) == 0);
    Require(scePthreadJoin(suspender, nullptr) == 0);
    Require(state.interrupted == Rounds);
    Require(handled.load() == Rounds);
    return 0;
}
