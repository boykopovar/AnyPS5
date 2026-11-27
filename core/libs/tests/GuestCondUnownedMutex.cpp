#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

extern "C" bool Aps5GuestTlsComputeLayout_nid_no_patch(
    std::uint64_t templateAddress,
    std::uint64_t templateSize,
    std::uint64_t blockSize,
    std::uint64_t alignment,
    std::size_t* outEffectiveAlignment,
    std::size_t* outThreadPointerOffset,
    std::size_t* outBytesOffset,
    std::size_t* outAllocationSize);

#ifdef __APPLE__
struct MacOsGuestTlsDescriptor {
    std::uint64_t version;
    std::uint64_t templateAddress;
    std::uint64_t templateSize;
    std::uint64_t blockSize;
    std::uint64_t alignment;
    std::uint64_t key;
};

struct MacOsTlsIndex {
    const MacOsGuestTlsDescriptor* module;
    std::uint64_t offset;
};

extern "C" void Aps5GuestTlsRegister_nid_no_patch(MacOsGuestTlsDescriptor* descriptor);
extern "C" void* Aps5GuestTlsAllocateThreadPointer_nid_no_patch(const MacOsGuestTlsDescriptor* descriptor);
extern "C" void* Aps5GuestTlsGetAddr_nid_no_patch(const MacOsTlsIndex* index);
#endif

static constexpr int SCE_OK = 0;
static constexpr int POSIX_EPERM = 1;
static constexpr int POSIX_EINVAL = 22;
static constexpr std::uintptr_t ADAPTIVE_INITIALIZER = 1;

static void Require(bool value) { if (!value) std::abort(); }

static int PosixTimed(PthreadCond* cond, PthreadMutex* mutex) {
    KernelTimespec deadline{};
    Require(clock_gettime_nid_postfix(0, &deadline) == 0);
    deadline.tv_sec += 60;
    return pthread_cond_timedwait_nid_postfix(cond, mutex, &deadline);
}

struct Wait {
    int (*run)(PthreadCond*, PthreadMutex*);
    int notOwned;
    int destroyed;
};

static int SceWait(PthreadCond* cond, PthreadMutex* mutex) { return scePthreadCondWait(cond, mutex); }
static int SceTimed(PthreadCond* cond, PthreadMutex* mutex) { return scePthreadCondTimedwait(cond, mutex, 60000000); }
static int PosixWait(PthreadCond* cond, PthreadMutex* mutex) { return pthread_cond_wait_nid_postfix(cond, mutex); }

struct Holder {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> locked{false};
    std::atomic<bool> release{false};
};

static void* APS5_VABI Hold(void* arg) {
    auto& holder = *static_cast<Holder*>(arg);
    Require(scePthreadMutexLock(&holder.mutex) == SCE_OK);
    holder.locked.store(true);
    while (!holder.release.load())
        std::this_thread::yield();
    Require(scePthreadMutexUnlock(&holder.mutex) == SCE_OK);
    return nullptr;
}

static void Check(const Wait& wait) {
    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    PthreadMutex unlocked = nullptr;
    Require(scePthreadMutexInit(&unlocked, nullptr, nullptr) == SCE_OK);
    Require(wait.run(&cond, &unlocked) == wait.notOwned);
    Require(scePthreadMutexLock(&unlocked) == SCE_OK);
    Require(scePthreadMutexUnlock(&unlocked) == SCE_OK);
    Require(scePthreadMutexDestroy(&unlocked) == SCE_OK);

    PthreadMutex uninitialized = nullptr;
    Require(wait.run(&cond, &uninitialized) == wait.notOwned);

    PthreadMutex adaptive = reinterpret_cast<PthreadMutex>(ADAPTIVE_INITIALIZER);
    Require(wait.run(&cond, &adaptive) == wait.notOwned);

    PthreadMutex destroyed = nullptr;
    Require(scePthreadMutexInit(&destroyed, nullptr, nullptr) == SCE_OK);
    Require(scePthreadMutexDestroy(&destroyed) == SCE_OK);
    Require(wait.run(&cond, &destroyed) == wait.destroyed);

    Holder holder;
    Require(scePthreadMutexInit(&holder.mutex, nullptr, nullptr) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Hold, &holder, nullptr) == SCE_OK);
    while (!holder.locked.load())
        std::this_thread::yield();
    Require(wait.run(&cond, &holder.mutex) == wait.notOwned);
    holder.release.store(true);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadMutexDestroy(&holder.mutex) == SCE_OK);

    Require(scePthreadCondDestroy(&cond) == SCE_OK);
}

static void TestGuestTlsMacOsLayout() {
    std::size_t effectiveAlignment = 0xA5;
    std::size_t threadPointerOffset = 0xA5;
    std::size_t bytesOffset = 0xA5;
    std::size_t allocationSize = 0xA5;
    constexpr std::uint64_t dummyTemplate = 0x1000;

    Require(Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 4, 4, 4, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(effectiveAlignment == 16);
    Require(threadPointerOffset == 16);
    Require(threadPointerOffset % 16 == 0);
    Require(bytesOffset == threadPointerOffset - 4);
    Require(bytesOffset == 12);
    Require(bytesOffset % 4 == 0);
    Require(allocationSize == 16 + 0x100);

    effectiveAlignment = 0xA5;
    threadPointerOffset = 0xA5;
    bytesOffset = 0xA5;
    allocationSize = 0xA5;
    Require(Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 3, 5, 1, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(effectiveAlignment == 16);
    Require(threadPointerOffset == 16);
    Require(bytesOffset == 11);
    Require(allocationSize == 16 + 0x100);

    effectiveAlignment = 0xA5;
    threadPointerOffset = 0xA5;
    bytesOffset = 0xA5;
    allocationSize = 0xA5;
    Require(Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 0, 32, 32, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(effectiveAlignment == 32);
    Require(threadPointerOffset == 32);
    Require(bytesOffset == 0);
    Require(allocationSize == 32 + 0x100);

    effectiveAlignment = 0xA5;
    threadPointerOffset = 0xA5;
    bytesOffset = 0xA5;
    allocationSize = 0xA5;
    Require(Aps5GuestTlsComputeLayout_nid_no_patch(0, 0, 0, 1, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(effectiveAlignment == 16);
    Require(threadPointerOffset == 0);
    Require(bytesOffset == 0);
    Require(allocationSize == 0x100);

    Require(!Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 0, 4, 0, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(!Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 0, 6, 3, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(!Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 0, 6, 4, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(!Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 5, 4, 4, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(!Aps5GuestTlsComputeLayout_nid_no_patch(0, 4, 4, 4, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    constexpr std::uint64_t maxSize = std::numeric_limits<std::size_t>::max();
    Require(!Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 0, maxSize, 1, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));
    Require(!Aps5GuestTlsComputeLayout_nid_no_patch(dummyTemplate, 0, maxSize - 15, 16, &effectiveAlignment, &threadPointerOffset, &bytesOffset, &allocationSize));

#ifdef __APPLE__
    const std::uint8_t templateBytes[4] = {0x11, 0x22, 0x33, 0x44};
    MacOsGuestTlsDescriptor descriptor{
        1,
        reinterpret_cast<std::uintptr_t>(templateBytes),
        sizeof(templateBytes),
        4,
        4,
        0,
    };
    Aps5GuestTlsRegister_nid_no_patch(&descriptor);
    std::thread([&] {
        void* tp = Aps5GuestTlsAllocateThreadPointer_nid_no_patch(&descriptor);
        Require(tp != nullptr);
        Require(reinterpret_cast<std::uintptr_t>(tp) % 16 == 0);
        const MacOsTlsIndex index{&descriptor, 0};
        void* addr = Aps5GuestTlsGetAddr_nid_no_patch(&index);
        Require(addr == static_cast<std::uint8_t*>(tp) - 4);
        Require(std::memcmp(addr, templateBytes, sizeof(templateBytes)) == 0);
    }).join();
#endif
}

int main() {
    Check({SceWait, SCE_KERNEL_ERROR_EPERM, SCE_KERNEL_ERROR_EINVAL});
    Check({SceTimed, SCE_KERNEL_ERROR_EPERM, SCE_KERNEL_ERROR_EINVAL});
    Check({PosixWait, POSIX_EPERM, POSIX_EINVAL});
    Check({PosixTimed, POSIX_EPERM, POSIX_EINVAL});
    TestGuestTlsMacOsLayout();
}