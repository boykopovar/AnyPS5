#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <array>
#include <bit>
#include <cstdlib>

extern "C" {
int APS5_VABI sceKernelGetCurrentCpu(void);
int APS5_VABI sceKernelGetMainSocId(void);
int APS5_VABI sceKernelGetCpumode(void);
std::int64_t APS5_VABI sceKernelGetCpuFrequency(void);
std::uint64_t APS5_VABI sceKernelGetTscFrequency(void);
KernelCpumask APS5_VABI sceKernelGetAvailableCpumask(void);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask);
}

static constexpr int SCE_OK = 0;
static constexpr unsigned MAX_HOST_CPUS = 256;
static constexpr int CALLS_PER_THREAD = 64;

static void Require(bool value) { if (!value) std::abort(); }

static bool InMask(int cpu, KernelCpumask mask) {
    return cpu >= 0 && cpu < 64 && (mask >> cpu & 1) != 0;
}

struct PinnedRun {
    KernelCpumask affinity;
    bool inside;
};

static void* APS5_VABI ReportWhilePinned(void* arg) {
    auto* run = static_cast<PinnedRun*>(arg);
    run->inside = scePthreadSetaffinity(scePthreadSelf(), run->affinity) == SCE_OK;
    for (int call = 0; call < CALLS_PER_THREAD && run->inside; ++call)
        run->inside = InMask(sceKernelGetCurrentCpu(), run->affinity);
    return nullptr;
}

int main() {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    Require(sceKernelGetMainSocId() == 0 && sceKernelGetCpumode() == 0);
    Require(sceKernelGetCpuFrequency() > 0 && static_cast<std::uint64_t>(sceKernelGetCpuFrequency()) == sceKernelGetTscFrequency());
    Require(sceKernelGetTscFrequency() >= 100000000u && sceKernelGetTscFrequency() <= 10000000000u);

    const std::array<KernelCpumask, 8> affinities{available, 0x1FFB, 0x3, 0x3F, 0x1000, 0x1, 0xFFFF, 0x10000};
    for (const KernelCpumask affinity : affinities) {
        const KernelCpumask expected = (affinity & available) != 0 ? affinity & available : available;
        KernelCpumask reached = 0;
        for (unsigned hostCpu = 0; hostCpu < MAX_HOST_CPUS; ++hostCpu) {
            const int cpu = GuestCpuFromHost(hostCpu, affinity);
            Require(InMask(cpu, expected));
            Require(GuestCpuFromHost(hostCpu, affinity) == cpu);
            reached |= KernelCpumask{1} << cpu;
            if (hostCpu + 1 == static_cast<unsigned>(std::popcount(expected))) Require(reached == expected);
        }
        Require(reached == expected);
    }

    for (int call = 0; call < CALLS_PER_THREAD; ++call) Require(InMask(sceKernelGetCurrentCpu(), available));

    for (KernelCpumask cpu = 1; cpu != 0; cpu <<= 1) {
        if ((available & cpu) == 0) continue;
        PinnedRun run{cpu, false};
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, ReportWhilePinned, &run, nullptr) == SCE_OK);
        Require(scePthreadJoin(thread, nullptr) == SCE_OK);
        Require(run.inside);
    }
}
