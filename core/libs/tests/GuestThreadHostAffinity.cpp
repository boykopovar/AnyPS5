#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>

static constexpr int SCE_OK = 0;

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "guest_thread_host_affinity: %s\n", what);
    std::abort();
}

static std::uint64_t CurrentMask() {
    cpu_set_t set;
    CPU_ZERO(&set);
    Require(sched_getaffinity(0, sizeof(set), &set) == 0, "sched_getaffinity failed");
    std::uint64_t mask = 0;
    for (unsigned cpu = 0; cpu < 64; ++cpu)
        if (CPU_ISSET(cpu, &set)) mask |= std::uint64_t{1} << cpu;
    return mask;
}

static void* APS5_VABI Run(void* expected) {
    Require(CurrentMask() == *static_cast<std::uint64_t*>(expected), "a guest thread does not run on the cores APS5_GUEST_AFFINITY_MASK names");
    return nullptr;
}

int main() {
    const std::uint64_t process = CurrentMask();
    Require(process != 0, "the process has no cores in its first 64");
    std::uint64_t chosen = std::bit_floor(process);
    if (chosen == process) {
        std::puts("guest_thread_host_affinity: one core available, nothing to restrict");
        return 0;
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%llx", static_cast<unsigned long long>(chosen));
    Require(setenv("APS5_GUEST_AFFINITY_MASK", text, 1) == 0, "setenv failed");
    for (int round = 0; round < 2; ++round) {
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, Run, &chosen, "affinity") == SCE_OK, "scePthreadCreate failed");
        Require(scePthreadJoin(thread, nullptr) == SCE_OK, "scePthreadJoin failed");
    }
    Require(CurrentMask() == process, "the creating host thread lost its affinity");
}
