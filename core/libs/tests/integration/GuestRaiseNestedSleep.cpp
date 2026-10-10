#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

extern "C" {
int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler);
int APS5_VABI sceKernelRemoveExceptionHandler(int signum);
int APS5_VABI sceKernelRaiseException(Pthread thread, int signum);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int SIGUSR1 = 30;
constexpr int SCE_OK = 0;
constexpr KernelUseconds OuterSleep = 1000000;
constexpr KernelUseconds HandlerSleep = 20000;

using Clock = std::chrono::steady_clock;

struct SleepState {
    std::atomic<int> handled{0};
    std::atomic<int> handlerSignal{-1};
    std::atomic<int> handlerSleepResult{-1};
    std::atomic<bool> started{false};
    std::atomic<bool> returned{false};
    std::atomic<int> outerSleepResult{-1};
    Clock::time_point sleepStart;
    Clock::time_point sleepEnd;
};

SleepState* state = nullptr;

void APS5_VABI Handler(int signum, void*) {
    state->handlerSignal.store(signum);
    state->handlerSleepResult.store(sceKernelUsleep_nid_postfix(HandlerSleep));
    state->handled.fetch_add(1);
}

void* APS5_VABI Sleeper(void*) {
    state->sleepStart = Clock::now();
    state->started.store(true);
    state->outerSleepResult.store(sceKernelUsleep_nid_postfix(OuterSleep));
    state->sleepEnd = Clock::now();
    state->returned.store(true);
    return nullptr;
}

template <class TCondition>
bool WaitFor(TCondition condition, std::chrono::milliseconds limit) {
    const auto deadline = Clock::now() + limit;
    while (!condition()) {
        if (Clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

class SleeperFixture {
public:
    SleeperFixture() {
        state = &value;
        RequireEqual(sceKernelInstallExceptionHandler(SIGUSR1, reinterpret_cast<void*>(&Handler)), 0, "install handler");
        installed = true;
    }
    ~SleeperFixture() {
        if (thread != nullptr) scePthreadJoin(thread, nullptr);
        if (installed) sceKernelRemoveExceptionHandler(SIGUSR1);
        state = nullptr;
    }
    SleeperFixture(const SleeperFixture&) = delete;
    SleeperFixture& operator=(const SleeperFixture&) = delete;

    int Join() {
        const int result = scePthreadJoin(thread, nullptr);
        thread = nullptr;
        return result;
    }

    int RemoveHandler() {
        installed = false;
        return sceKernelRemoveExceptionHandler(SIGUSR1);
    }

    SleepState value;
    Pthread thread = nullptr;

private:
    bool installed = false;
};

const Case nestedSleep{"RaiseException_DuringUsleep_HandlerSleepsAndOuterSleepStillCompletes", [] {
    SleeperFixture fixture;
    RequireEqual(scePthreadCreate(&fixture.thread, nullptr, Sleeper, nullptr, "sleeper"), 0, "create sleeper");
    Require(WaitFor([] { return state->started.load(); }, std::chrono::milliseconds(5000)), "sleeper started");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    RequireEqual(sceKernelRaiseException(fixture.thread, SIGUSR1), 0, "raise exception");
    Require(WaitFor([] { return state->handled.load() == 1; }, std::chrono::milliseconds(5000)), "handler ran once");
    Require(WaitFor([] { return state->returned.load(); }, std::chrono::milliseconds(5000)), "outer sleep returned");
    RequireEqual(fixture.value.handlerSignal.load(), SIGUSR1, "handler signal");
    RequireEqual(fixture.value.handlerSleepResult.load(), SCE_OK, "handler usleep result");
    RequireEqual(fixture.value.outerSleepResult.load(), SCE_OK, "outer usleep result");
    const auto slept = std::chrono::duration_cast<std::chrono::microseconds>(fixture.value.sleepEnd - fixture.value.sleepStart).count();
    Require(slept >= OuterSleep && slept < OuterSleep + 1000000,
            "outer sleep lasted the full second, got " + std::to_string(slept) + " us");
    RequireEqual(fixture.Join(), 0, "join sleeper");
    RequireEqual(fixture.RemoveHandler(), 0, "remove handler");
}};

} // namespace
