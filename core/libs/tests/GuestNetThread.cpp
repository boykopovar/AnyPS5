#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI sceNetThreadCreate(Pthread*, void*, const char*);
void APS5_VABI sceNetThreadExit();
int APS5_VABI sceNetThreadJoin(Pthread);
int APS5_VABI sceNetEmulationGet();
int APS5_VABI sceNetEmulationSet();
int* APS5_VABI sceNetErrnoLoc();
}

namespace {

constexpr int Einval = 22;
constexpr int Eopnotsupp = 45;
constexpr int NetError = static_cast<int>(0x80410100u);

struct ThreadParam {
    void (APS5_VABI *entry)(void*);
    void* arg;
    std::uint64_t reserved;
    std::uint32_t flags;
};

struct Run {
    std::atomic<int> started{0};
    std::atomic<bool> pastExit{false};
};

Run run;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

void APS5_VABI Count(void* arg) {
    ++static_cast<Run*>(arg)->started;
}

void APS5_VABI ExitEarly(void* arg) {
    auto* run = static_cast<Run*>(arg);
    ++run->started;
    sceNetThreadExit();
    run->pastExit = true;
}

}

int main() {
    ThreadParam counting{Count, &run, 0, 0};
    Pthread thread = nullptr;
    Require(sceNetThreadCreate(&thread, &counting, "anyps5-net-thread") == 0 && thread != nullptr, "sceNetThreadCreate failed");
    Require(sceNetThreadJoin(thread) == 0, "sceNetThreadJoin failed");
    Require(run.started == 1, "the thread did not run its entry with its argument");

    Pthread untouched = reinterpret_cast<Pthread>(&run);
    *sceNetErrnoLoc() = 0;
    Require(sceNetThreadCreate(&untouched, nullptr, "anyps5-net-thread") == (NetError | Einval), "a null parameter was not refused with EINVAL");
    Require(*sceNetErrnoLoc() == Einval && untouched == reinterpret_cast<Pthread>(&run), "a refused create set no errno or wrote the thread");

    ThreadParam exiting{ExitEarly, &run, 0, 1};
    Require(sceNetThreadCreate(&thread, &exiting, nullptr) == 0, "sceNetThreadCreate failed without a name");
    Require(sceNetThreadJoin(thread) == 0, "joining an exited thread failed");
    Require(run.started == 2 && !run.pastExit, "sceNetThreadExit did not end the thread");

    static ThreadParam detached{Count, &run, 0, 0};
    Require(sceNetThreadCreate(nullptr, &detached, "anyps5-net-thread") == 0, "a null thread output was refused");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (run.started != 3 && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(run.started == 3, "the thread created without an output did not run");

    *sceNetErrnoLoc() = 0;
    Require(sceNetEmulationGet() == (NetError | Eopnotsupp) && *sceNetErrnoLoc() == Eopnotsupp, "sceNetEmulationGet did not return EOPNOTSUPP");
    *sceNetErrnoLoc() = 0;
    Require(sceNetEmulationSet() == (NetError | Eopnotsupp) && *sceNetErrnoLoc() == Eopnotsupp, "sceNetEmulationSet did not return EOPNOTSUPP");
    std::puts("net thread tests passed");
    return 0;
}
