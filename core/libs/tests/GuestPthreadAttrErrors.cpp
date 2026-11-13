#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int SCE_OK = 0;

static void Require(bool value) { if (!value) std::abort(); }

class GuardPage {
public:
    GuardPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        memory = static_cast<char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
#endif
    }

    ~GuardPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * page);
#endif
    }

    GuardPage(const GuardPage&) = delete;
    GuardPage& operator=(const GuardPage&) = delete;

    void* Unreadable() const { return memory + page; }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

static void CheckBadPointers() {
    GuardPage guard;
    void* const bad[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    for (void* pointer : bad) {
        auto* attrs = static_cast<PthreadAttr*>(pointer);
        auto* ints = static_cast<int*>(pointer);
        auto* sizes = static_cast<std::size_t*>(pointer);
        auto* params = static_cast<KernelSchedParam*>(pointer);
        Require(scePthreadAttrInit(attrs) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrDestroy(attrs) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrSetdetachstate(attrs, 0) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrSetguardsize(attrs, 0) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrSetinheritsched(attrs, 0) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrSetschedparam(attrs, params) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrSetschedpolicy(attrs, 1) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrSetsolosched(attrs, 0) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrSetstacksize(attrs, 16384) == SCE_KERNEL_ERROR_EFAULT);
        int value = -1;
        std::size_t size = 1;
        KernelSchedParam param{};
        void* address = nullptr;
        Require(scePthreadAttrGetdetachstate(attrs, &value) == SCE_KERNEL_ERROR_EFAULT && value == -1);
        Require(scePthreadAttrGetguardsize(attrs, &size) == SCE_KERNEL_ERROR_EFAULT && size == 1);
        Require(scePthreadAttrGetinheritsched(attrs, &value) == SCE_KERNEL_ERROR_EFAULT && value == -1);
        Require(scePthreadAttrGetschedparam(attrs, &param) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetschedpolicy(attrs, &value) == SCE_KERNEL_ERROR_EFAULT && value == -1);
        Require(scePthreadAttrGetstackaddr(attrs, &address) == SCE_KERNEL_ERROR_EFAULT && address == nullptr);
        Require(scePthreadAttrGetstacksize(attrs, &size) == SCE_KERNEL_ERROR_EFAULT && size == 1);

        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrSetschedparam(&attr, params) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetdetachstate(&attr, ints) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetguardsize(&attr, sizes) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetinheritsched(&attr, ints) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetschedparam(&attr, params) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetschedpolicy(&attr, ints) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetstackaddr(&attr, static_cast<void**>(pointer)) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrGetstacksize(&attr, sizes) == SCE_KERNEL_ERROR_EFAULT);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    }
}

int main() {
    CheckBadPointers();
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    int value = -1;
    Require(scePthreadAttrGetschedpolicy(&attr, &value) == SCE_OK && value == 1);
    Require(scePthreadAttrGetinheritsched(&attr, &value) == SCE_OK && value == 4);

    for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{16383}})
        Require(scePthreadAttrSetstacksize(&attr, size) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetstacksize(&attr, 16384) == SCE_OK);

    for (int state : {-1, 2, 3, 100}) Require(scePthreadAttrSetdetachstate(&attr, state) == SCE_KERNEL_ERROR_EINVAL);
    for (int state : {0, 1}) Require(scePthreadAttrSetdetachstate(&attr, state) == SCE_OK);

    for (int policy : {-1, 0, 2, 4}) {
        Require(scePthreadAttrSetschedpolicy(&attr, policy) == SCE_KERNEL_ERROR_EOPNOTSUPP);
        Require(scePthreadAttrGetschedpolicy(&attr, &value) == SCE_OK && value == 1);
    }
    for (int policy : {3, 1}) {
        Require(scePthreadAttrSetschedpolicy(&attr, policy) == SCE_OK);
        Require(scePthreadAttrGetschedpolicy(&attr, &value) == SCE_OK && value == policy);
    }

    for (int inherit : {-1, 1, 2, 8}) {
        Require(scePthreadAttrSetinheritsched(&attr, inherit) == SCE_KERNEL_ERROR_EOPNOTSUPP);
        Require(scePthreadAttrGetinheritsched(&attr, &value) == SCE_OK && value == 4);
    }
    for (int inherit : {0, 4}) {
        Require(scePthreadAttrSetinheritsched(&attr, inherit) == SCE_OK);
        Require(scePthreadAttrGetinheritsched(&attr, &value) == SCE_OK && value == inherit);
    }

    KernelSchedParam param{};
    for (int priority : {-1, 0, 255, 768, 900, 960}) {
        param.sched_priority = priority;
        Require(scePthreadAttrSetschedparam(&attr, &param) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    }
    Require(scePthreadAttrSetschedparam(&attr, nullptr) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    for (int priority : {256, 767}) {
        param.sched_priority = priority;
        Require(scePthreadAttrSetschedparam(&attr, &param) == SCE_OK);
        KernelSchedParam read{};
        Require(scePthreadAttrGetschedparam(&attr, &read) == SCE_OK && read.sched_priority == priority);
    }
    for (KernelCpumask mask : {KernelCpumask{0}, KernelCpumask{1}, ~KernelCpumask{0}})
        Require(scePthreadAttrSetaffinity(&attr, mask) == SCE_OK);

    std::size_t size = 0;
    int state = 0;
    KernelCpumask mask = 0;
    Require(scePthreadAttrGetstacksize(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetdetachstate(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetschedparam(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetschedpolicy(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetinheritsched(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetguardsize(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetstackaddr(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    for (PthreadAttr* missing : {static_cast<PthreadAttr*>(nullptr), static_cast<PthreadAttr*>(&attr)}) {
        PthreadAttr unset = nullptr;
        PthreadAttr* target = missing ? &unset : nullptr;
        Require(scePthreadAttrSetstacksize(target, 16384) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrSetdetachstate(target, 0) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrSetschedpolicy(target, 1) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrSetinheritsched(target, 0) == SCE_KERNEL_ERROR_EINVAL);
        param.sched_priority = 700;
        Require(scePthreadAttrSetschedparam(target, &param) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrSetaffinity(target, 1) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetstacksize(target, &size) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetdetachstate(target, &state) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetschedparam(target, &param) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetschedpolicy(target, &state) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetinheritsched(target, &state) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetaffinity(target, &mask) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrDestroy(target) == SCE_KERNEL_ERROR_EINVAL);
    }

    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);
}
