#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <future>
#include <string>

extern "C" {
KernelCpumask APS5_VABI sceKernelGetAvailableCpumask(void);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadGetaffinity(Pthread thread, KernelCpumask* mask);
int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int SCE_OK = 0;
constexpr int PS5_LOGICAL_CPUS = 16;

class ThreadAttr {
public:
    ThreadAttr() {
        RequireEqual(scePthreadAttrInit(&attr), SCE_OK, "attr init");
        initialized = true;
    }
    ~ThreadAttr() {
        if (initialized) scePthreadAttrDestroy(&attr);
    }
    ThreadAttr(const ThreadAttr&) = delete;
    ThreadAttr& operator=(const ThreadAttr&) = delete;

    int Destroy() {
        initialized = false;
        return scePthreadAttrDestroy(&attr);
    }

    PthreadAttr attr = nullptr;

private:
    bool initialized = false;
};

void* APS5_VABI WaitForRelease(void* arg) {
    static_cast<std::shared_future<void>*>(arg)->wait();
    return nullptr;
}

void* APS5_VABI ReportMask(void* arg) {
    *static_cast<KernelCpumask*>(arg) = sceKernelGetAvailableCpumask();
    return nullptr;
}

class BlockedThread {
public:
    explicit BlockedThread(const ThreadAttr& attr) : released(release.get_future().share()) {
        RequireEqual(scePthreadCreate(&thread, &attr.attr, WaitForRelease, &released, nullptr), SCE_OK, "create thread");
    }
    ~BlockedThread() {
        if (thread != nullptr) ReleaseAndJoin();
    }
    BlockedThread(const BlockedThread&) = delete;
    BlockedThread& operator=(const BlockedThread&) = delete;

    KernelCpumask Affinity() {
        KernelCpumask mask = 0;
        RequireEqual(scePthreadGetaffinity(thread, &mask), SCE_OK, "get affinity");
        return mask;
    }

    int ReleaseAndJoin() {
        release.set_value();
        const int result = scePthreadJoin(thread, nullptr);
        thread = nullptr;
        return result;
    }

    Pthread thread = nullptr;

private:
    std::promise<void> release;
    std::shared_future<void> released;
};

const Case mask{"GetAvailableCpumask_Read_ReturnsStableNonEmptyMaskWithinPs5Cpus", [] {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    RequireEqual(available != 0, true, "mask is not empty");
    RequireEqual(available >> PS5_LOGICAL_CPUS, KernelCpumask{0}, "bits above the 16 logical cpus");
    RequireEqual(sceKernelGetAvailableCpumask(), available, "second read");
}};

const Case attrDefault{"PthreadAttrGetaffinity_DefaultAttr_ReturnsAvailableMask", [] {
    ThreadAttr attr;
    KernelCpumask affinity = 0;
    RequireEqual(scePthreadAttrGetaffinity(&attr.attr, &affinity), SCE_OK, "attr get affinity");
    RequireEqual(affinity, sceKernelGetAvailableCpumask(), "default affinity");
    RequireEqual(attr.Destroy(), SCE_OK, "attr destroy");
}};

const Case threadDefault{"PthreadGetaffinity_NewThread_ReturnsAvailableMask", [] {
    ThreadAttr attr;
    BlockedThread thread(attr);
    RequireEqual(thread.Affinity(), sceKernelGetAvailableCpumask(), "thread affinity");
}};

const Case setEachCpu{"PthreadSetaffinity_EachAvailableCpu_IsReportedBack", [] {
    const KernelCpumask available = sceKernelGetAvailableCpumask();
    ThreadAttr attr;
    BlockedThread thread(attr);
    for (KernelCpumask cpu = 1; cpu != 0; cpu <<= 1) {
        if ((available & cpu) == 0) continue;
        const std::string name = "cpu mask " + std::to_string(cpu);
        RequireEqual(scePthreadSetaffinity(thread.thread, cpu), SCE_OK, "set " + name);
        RequireEqual(thread.Affinity(), cpu, "get " + name);
    }
    RequireEqual(thread.ReleaseAndJoin(), SCE_OK, "join thread");
}};

const Case fromGuestThread{"GetAvailableCpumask_FromGuestThread_MatchesMainThread", [] {
    ThreadAttr attr;
    KernelCpumask fromThread = 0;
    Pthread thread = nullptr;
    RequireEqual(scePthreadCreate(&thread, &attr.attr, ReportMask, &fromThread, nullptr), SCE_OK, "create thread");
    RequireEqual(scePthreadJoin(thread, nullptr), SCE_OK, "join thread");
    RequireEqual(fromThread, sceKernelGetAvailableCpumask(), "mask seen by guest thread");
}};

} // namespace
