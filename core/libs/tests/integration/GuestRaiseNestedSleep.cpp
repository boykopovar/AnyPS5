#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler);
int APS5_VABI sceKernelRemoveExceptionHandler(int signum);
int APS5_VABI sceKernelRaiseException(Pthread thread, int signum);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
}

static constexpr int SIGUSR1 = 30;
static constexpr int SCE_OK = 0;
static constexpr KernelUseconds OuterSleep = 1000000;
static constexpr KernelUseconds HandlerSleep = 20000;

using Clock = std::chrono::steady_clock;

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> handled{0};
static std::atomic<bool> started{false};
static std::atomic<bool> returned{false};
static Clock::time_point sleepStart;
static Clock::time_point sleepEnd;

static void APS5_VABI Handler(int signum, void*) {
    Require(signum == SIGUSR1);
    Require(sceKernelUsleep_nid_postfix(HandlerSleep) == SCE_OK);
    handled.fetch_add(1);
}

static void* APS5_VABI Sleeper(void*) {
    sleepStart = Clock::now();
    started.store(true);
    Require(sceKernelUsleep_nid_postfix(OuterSleep) == SCE_OK);
    sleepEnd = Clock::now();
    returned.store(true);
    return nullptr;
}

template <class TCondition>
static bool WaitFor(TCondition condition, std::chrono::milliseconds limit) {
    const auto deadline = Clock::now() + limit;
    while (!condition()) {
        if (Clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

int main() {
    Require(sceKernelInstallExceptionHandler(SIGUSR1, reinterpret_cast<void*>(&Handler)) == 0);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Sleeper, nullptr, "sleeper") == 0);
    Require(WaitFor([] { return started.load(); }, std::chrono::milliseconds(5000)));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(sceKernelRaiseException(thread, SIGUSR1) == 0);
    Require(WaitFor([] { return handled.load() == 1; }, std::chrono::milliseconds(5000)));
    Require(WaitFor([] { return returned.load(); }, std::chrono::milliseconds(5000)));
    const auto slept = std::chrono::duration_cast<std::chrono::microseconds>(sleepEnd - sleepStart).count();
    Require(slept >= OuterSleep && slept < OuterSleep + 1000000);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(sceKernelRemoveExceptionHandler(SIGUSR1) == 0);
    return 0;
}
