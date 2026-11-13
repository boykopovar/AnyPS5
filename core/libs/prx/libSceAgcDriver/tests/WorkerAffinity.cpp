#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#ifdef __linux__
#include <sched.h>
#endif

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
}

#ifdef __linux__
std::vector<int> Allowed() {
    cpu_set_t set;
    CPU_ZERO(&set);
    Require(sched_getaffinity(0, sizeof(set), &set) == 0, "sched_getaffinity failed");
    std::vector<int> cpus;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (CPU_ISSET(cpu, &set)) cpus.push_back(cpu);
    }
    return cpus;
}

bool Contains(const std::vector<int>& cpus, int cpu) {
    for (const auto value : cpus) {
        if (value == cpu) return true;
    }
    return false;
}
#endif

}

int main() {
#ifndef __linux__
    std::puts("worker affinity reservation is checked on Linux");
    return 77;
#else
    const auto& layout = CpuTopology::Get();
    const auto sets = CpuTopology::SysfsCpuSets("/sys/devices/system/cpu", layout.process, layout.hybrid ? layout.efficient : 0);
    const char* requested = std::getenv("APS5_COMMITTER_CPU");
    const auto expected = CpuTopology::ChooseReservedCore(sets, {}, requested != nullptr ? std::optional<unsigned>(static_cast<unsigned>(std::strtoul(requested, nullptr, 10))) : std::nullopt);
    if (!expected.has_value()) {
        std::puts("this host reserves no committer core");
        return 77;
    }
    const auto before = Allowed();
    std::vector<int> committer;
    std::thread thread([&] {
        AgcDriver::DriverDetail::ReserveCommitterCore();
        committer = Allowed();
    });
    thread.join();
    const int chosen = static_cast<int>(expected->chosen.id);
    Require(Contains(committer, chosen), "the committer is not allowed its reserved processor");
    if (requested != nullptr) {
        Require(committer.size() == 1, "an explicit APS5_COMMITTER_CPU did not pin the committer to that processor alone");
    } else {
        Require(committer.size() > 1 && committer.size() <= before.size(), "the committer was pinned to one processor instead of its core and the unreserved ones");
    }
    const auto after = Allowed();
    Require(!Contains(after, chosen), "another thread of the process kept the reserved processor");
    std::printf("committer allowed %zu processors, reserved %d, others %zu\n", committer.size(), chosen, after.size());
    return 0;
#endif
}
