#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

static constexpr int SigUsr1 = 30;
static constexpr int SigBlock = 1;
static constexpr int SigUnblock = 2;
static constexpr GuestSignalSet Usr1Set{{1u << (SigUsr1 - 1), 0, 0, 0}};

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

static std::atomic<int> deliveries{0};
static std::atomic<Pthread> deliveredOn{nullptr};
static std::atomic<int> step{0};
static std::atomic<int> worked{0};

static void APS5_VABI Handler(int, void*) {
    deliveredOn.store(scePthreadSelf());
    deliveries.fetch_add(1);
}

static void WaitStep(int value) {
    while (step.load() != value) std::this_thread::yield();
}

static void SpinBlocked(int expected, const char* what) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < until) Require(deliveries.load() == expected, what);
}

static void* APS5_VABI Worker(void*) {
    Require(pthread_sigmask_nid_postfix(SigBlock, &Usr1Set, nullptr) == 0, "blocking SIGUSR1 failed");
    step.store(1);
    WaitStep(2);
    SpinBlocked(0, "a raise reached a thread that blocks the signal");
    Require(pthread_sigmask_nid_postfix(SigUnblock, &Usr1Set, nullptr) == 0, "unblocking SIGUSR1 failed");
    Require(deliveries.load() == 1, "the held raise was not delivered when the thread unblocked the signal");
    Require(deliveredOn.load() == scePthreadSelf(), "the held raise ran on another thread");
    Require(pthread_sigmask_nid_postfix(SigBlock, &Usr1Set, nullptr) == 0, "blocking SIGUSR1 again failed");
    step.store(3);
    WaitStep(4);
    SpinBlocked(1, "a second raise reached a thread that blocks the signal");
    Require(pthread_sigmask_nid_postfix(SigUnblock, &Usr1Set, nullptr) == 0, "unblocking SIGUSR1 again failed");
    Require(deliveries.load() == 2, "two raises held by one blocked thread were not delivered once");
    step.store(5);
    while (step.load() != 6) worked.fetch_add(1);
    Require(pthread_sigmask_nid_postfix(SigBlock, &Usr1Set, nullptr) == 0, "blocking SIGUSR1 before sigsuspend failed");
    step.store(7);
    WaitStep(8);
    Require(deliveries.load() == 3, "a raise reached a thread that blocks the signal before sigsuspend");
    const GuestSignalSet none{{0, 0, 0, 0}};
    Require(sigsuspend_nid_postfix(&none) == -1, "sigsuspend did not return -1");
    Require(deliveries.load() == 4, "sigsuspend did not deliver the held raise");
    Require(deliveredOn.load() == scePthreadSelf(), "the raise held before sigsuspend ran on another thread");
    return nullptr;
}

int main() {
    std::atomic<bool> done{false};
    std::thread watchdog([&done] {
        for (int i = 0; i < 20000 && !done.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!done.load()) std::abort();
    });
    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Handler)) == 0, "installing the handler failed");
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, Worker, nullptr, "masked") == 0, "creating the worker failed");
    WaitStep(1);
    Require(sceKernelRaiseException(worker, SigUsr1) == 0, "raising at a blocked thread failed");
    step.store(2);
    WaitStep(3);
    Require(sceKernelRaiseException(worker, SigUsr1) == 0, "the first of two raises at a blocked thread failed");
    Require(sceKernelRaiseException(worker, SigUsr1) == 0, "the second of two raises at a blocked thread failed");
    step.store(4);
    WaitStep(5);
    while (worked.load() == 0) std::this_thread::yield();
    Require(sceKernelRaiseException(worker, SigUsr1) == 0, "raising at an unblocked thread failed");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (deliveries.load() != 3) Require(std::chrono::steady_clock::now() < deadline, "a raise at a thread that unblocked the signal was not delivered within 3 s");
    Require(deliveredOn.load() == worker, "the raise at the unblocked thread ran on another thread");
    step.store(6);
    WaitStep(7);
    Require(sceKernelRaiseException(worker, SigUsr1) == 0, "raising at a thread that blocks the signal before sigsuspend failed");
    step.store(8);
    Require(scePthreadJoin(worker, nullptr) == 0, "joining the worker failed");
    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0, "removing the handler failed");
    std::printf("%d raises delivered\n", deliveries.load());
    done.store(true);
    watchdog.join();
}
