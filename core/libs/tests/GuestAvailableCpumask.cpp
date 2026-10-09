#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <future>

extern "C" {
KernelCpumask APS5_VABI sceKernelGetAvailableCpumask(void);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadGetaffinity(Pthread thread, KernelCpumask* mask);
int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask);
int APS5_VABI cpuset_getaffinity_nid_postfix(int level, int which, std::int64_t id, std::size_t size, void* mask);
int* APS5_VABI __error_nid_postfix();
}

static constexpr int SCE_OK = 0;
static constexpr int PS5_LOGICAL_CPUS = 16;

static void Require(bool value) { if (!value) std::abort(); }

static void* APS5_VABI Worker(void* arg) {
    static_cast<std::future<void>*>(arg)->get();
    return nullptr;
}

static void* APS5_VABI ReportCpuset(void* arg) {
    auto* result = static_cast<std::uint64_t*>(arg);
    std::uint64_t set[4] = {~0ull, ~0ull, ~0ull, ~0ull};
    result[0] = cpuset_getaffinity_nid_postfix(3, 1, -1, 8, set);
    result[1] = set[0];
    result[2] = set[1];
    result[3] = cpuset_getaffinity_nid_postfix(3, 1, -1, sizeof(set), set);
    result[4] = set[0] | (set[1] != 0 || set[2] != 0 || set[3] != 0 ? 1ull << 63 : 0);
    result[5] = cpuset_getaffinity_nid_postfix(3, 1, -1, 4, set) == -1 && *__error_nid_postfix() == 34;
    result[6] = cpuset_getaffinity_nid_postfix(3, 1, -1, 33, set) == -1 && *__error_nid_postfix() == 34;
    result[7] = cpuset_getaffinity_nid_postfix(3, 1, -1, 8, nullptr) == -1 && *__error_nid_postfix() == 14;
    return nullptr;
}

static void* APS5_VABI ReportMask(void* arg) {
    *static_cast<KernelCpumask*>(arg) = sceKernelGetAvailableCpumask();
    return nullptr;
}

int main() {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    Require(available != 0);
    Require((available >> PS5_LOGICAL_CPUS) == 0);
    Require(sceKernelGetAvailableCpumask() == available);

    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    KernelCpumask defaultAffinity = 0;
    Require(scePthreadAttrGetaffinity(&attr, &defaultAffinity) == SCE_OK);
    Require(defaultAffinity == available);

    std::promise<void> release;
    auto released = release.get_future();
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &released, nullptr) == SCE_OK);
    KernelCpumask threadAffinity = 0;
    Require(scePthreadGetaffinity(thread, &threadAffinity) == SCE_OK);
    Require(threadAffinity == available);

    for (KernelCpumask cpu = 1; cpu != 0; cpu <<= 1) {
        if ((available & cpu) == 0) continue;
        Require(scePthreadSetaffinity(thread, cpu) == SCE_OK);
        Require(scePthreadGetaffinity(thread, &threadAffinity) == SCE_OK);
        Require(threadAffinity == cpu);
    }
    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    KernelCpumask fromThread = 0;
    Require(scePthreadCreate(&thread, &attr, ReportMask, &fromThread, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(fromThread == available);

    std::uint64_t cpuset[8] = {};
    Require(scePthreadCreate(&thread, &attr, ReportCpuset, cpuset, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(cpuset[0] == 0 && cpuset[1] == available && cpuset[2] == ~0ull);
    Require(cpuset[3] == 0 && cpuset[4] == available);
    Require(cpuset[5] == 1 && cpuset[6] == 1 && cpuset[7] == 1);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
}
