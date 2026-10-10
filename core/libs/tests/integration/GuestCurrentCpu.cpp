#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <bit>
#include <string>

extern "C" {
int APS5_VABI sceKernelGetCurrentCpu(void);
KernelCpumask APS5_VABI sceKernelGetAvailableCpumask(void);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int SCE_OK = 0;
constexpr unsigned MAX_HOST_CPUS = 256;
constexpr int CALLS_PER_THREAD = 64;

bool InMask(int cpu, KernelCpumask mask) {
    return cpu >= 0 && cpu < 64 && (mask >> cpu & 1) != 0;
}

struct PinnedRun {
    KernelCpumask affinity;
    int setResult = -1;
    int outsideCpu = -1;
};

void* APS5_VABI ReportWhilePinned(void* arg) {
    auto* run = static_cast<PinnedRun*>(arg);
    run->setResult = scePthreadSetaffinity(scePthreadSelf(), run->affinity);
    for (int call = 0; call < CALLS_PER_THREAD && run->setResult == SCE_OK && run->outsideCpu < 0; ++call) {
        const int cpu = sceKernelGetCurrentCpu();
        if (!InMask(cpu, run->affinity)) run->outsideCpu = cpu;
    }
    return nullptr;
}

const Case hostMapping{"GuestCpuFromHost_EachAffinity_MapsHostCpusDeterministicallyOntoEveryAllowedCpu", [] {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    const std::array<KernelCpumask, 8> affinities{available, 0x1FFB, 0x3, 0x3F, 0x1000, 0x1, 0xFFFF, 0x10000};
    for (const KernelCpumask affinity : affinities) {
        const KernelCpumask expected = (affinity & available) != 0 ? affinity & available : available;
        const std::string mask = "affinity " + std::to_string(affinity);
        KernelCpumask reached = 0;
        for (unsigned hostCpu = 0; hostCpu < MAX_HOST_CPUS; ++hostCpu) {
            const std::string where = mask + " host cpu " + std::to_string(hostCpu);
            const int cpu = GuestCpuFromHost(hostCpu, affinity);
            Require(InMask(cpu, expected), where + " maps to allowed cpu, got " + std::to_string(cpu));
            RequireEqual(GuestCpuFromHost(hostCpu, affinity), cpu, where + " repeated mapping");
            reached |= KernelCpumask{1} << cpu;
            if (hostCpu + 1 == static_cast<unsigned>(std::popcount(expected))) {
                RequireEqual(reached, expected, where + " first host cpus cover every allowed cpu");
            }
        }
        RequireEqual(reached, expected, mask + " reached cpus");
    }
}};

const Case mainThread{"GetCurrentCpu_MainThread_ReturnsAvailableCpu", [] {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    for (int call = 0; call < CALLS_PER_THREAD; ++call) {
        const int cpu = sceKernelGetCurrentCpu();
        Require(InMask(cpu, available), "call " + std::to_string(call) + " returned cpu " + std::to_string(cpu));
    }
}};

const Case pinnedThread{"GetCurrentCpu_PinnedGuestThread_ReturnsPinnedCpu", [] {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    for (KernelCpumask cpu = 1; cpu != 0; cpu <<= 1) {
        if ((available & cpu) == 0) continue;
        const std::string mask = "cpu mask " + std::to_string(cpu);
        PinnedRun run{cpu};
        Pthread thread = nullptr;
        RequireEqual(scePthreadCreate(&thread, nullptr, ReportWhilePinned, &run, nullptr), SCE_OK, mask + " create");
        RequireEqual(scePthreadJoin(thread, nullptr), SCE_OK, mask + " join");
        RequireEqual(run.setResult, SCE_OK, mask + " set affinity");
        RequireEqual(run.outsideCpu, -1, mask + " cpu reported outside the affinity");
    }
}};

} // namespace
