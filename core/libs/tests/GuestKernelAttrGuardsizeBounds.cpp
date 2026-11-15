#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEinval = 22;
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

constexpr std::size_t DefaultGuardSize = 4096;
constexpr std::size_t Ps5PageSize = 16384;
constexpr std::size_t MinStackSize = 16384;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void* APS5_VABI Worker(void* arg) {
    if (arg != nullptr) {
        auto* counter = static_cast<std::atomic<int>*>(arg);
        counter->fetch_add(1, std::memory_order_relaxed);
    }
    return nullptr;
}

void TestDefaultGuardSize() {
    PthreadAttr posixAttr = nullptr;
    Require(pthread_attr_init_nid_postfix(&posixAttr) == 0);
    Require(posixAttr != nullptr);

    std::size_t size = 0;
    Require(pthread_attr_getguardsize_nid_postfix(&posixAttr, &size) == 0 && size == DefaultGuardSize);
    size = 0;
    Require(scePthreadAttrGetguardsize(&posixAttr, &size) == 0 && size == DefaultGuardSize);

    Require(pthread_attr_destroy_nid_postfix(&posixAttr) == 0);

    PthreadAttr sceAttr = nullptr;
    Require(scePthreadAttrInit(&sceAttr) == 0);
    Require(sceAttr != nullptr);

    size = 0;
    Require(scePthreadAttrGetguardsize(&sceAttr, &size) == 0 && size == DefaultGuardSize);
    size = 0;
    Require(pthread_attr_getguardsize_nid_postfix(&sceAttr, &size) == 0 && size == DefaultGuardSize);

    Require(scePthreadAttrDestroy(&sceAttr) == 0);
}

void TestZeroGuardSemantics() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);

    Require(pthread_attr_setguardsize_nid_postfix(&attr, 0) == 0);
    std::size_t size = DefaultGuardSize;
    Require(pthread_attr_getguardsize_nid_postfix(&attr, &size) == 0 && size == 0);
    size = DefaultGuardSize;
    Require(scePthreadAttrGetguardsize(&attr, &size) == 0 && size == 0);

    std::atomic<int> executed{0};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &executed, "zero_guard") == 0);
    void* threadReturn = nullptr;
    Require(scePthreadJoin(thread, &threadReturn) == 0);
    Require(executed.load(std::memory_order_relaxed) == 1);

    Require(scePthreadAttrSetguardsize(&attr, 0) == 0);
    size = DefaultGuardSize;
    Require(scePthreadAttrGetguardsize(&attr, &size) == 0 && size == 0);
    size = DefaultGuardSize;
    Require(pthread_attr_getguardsize_nid_postfix(&attr, &size) == 0 && size == 0);

    executed.store(0, std::memory_order_relaxed);
    thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &executed, "zero_guard_sce") == 0);
    Require(scePthreadJoin(thread, &threadReturn) == 0);
    Require(executed.load(std::memory_order_relaxed) == 1);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
}

void TestPageSizeMultiplesAndRounding() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);

    const std::size_t pageMultiples[] = {
        4096,
        8192,
        Ps5PageSize,
        32768,
        65536,
        131072,
        262144,
        524288,
        1048576,
        2097152
    };

    for (const std::size_t multiple : pageMultiples) {
        Require(pthread_attr_setguardsize_nid_postfix(&attr, multiple) == 0);
        std::size_t reported = 0;
        Require(pthread_attr_getguardsize_nid_postfix(&attr, &reported) == 0 && reported == multiple);
        reported = 0;
        Require(scePthreadAttrGetguardsize(&attr, &reported) == 0 && reported == multiple);

        Require(scePthreadAttrSetguardsize(&attr, multiple) == 0);
        reported = 0;
        Require(scePthreadAttrGetguardsize(&attr, &reported) == 0 && reported == multiple);
        reported = 0;
        Require(pthread_attr_getguardsize_nid_postfix(&attr, &reported) == 0 && reported == multiple);
    }

    const std::size_t nonPageSizes[] = {
        1,
        2,
        16,
        100,
        512,
        4095,
        4097,
        8000,
        8191,
        8193,
        Ps5PageSize - 1,
        Ps5PageSize + 1,
        30000,
        65535,
        65537
    };

    for (const std::size_t nonMultiple : nonPageSizes) {
        Require(pthread_attr_setguardsize_nid_postfix(&attr, nonMultiple) == 0);
        std::size_t reported = 0;
        Require(pthread_attr_getguardsize_nid_postfix(&attr, &reported) == 0 && reported == nonMultiple);
        reported = 0;
        Require(scePthreadAttrGetguardsize(&attr, &reported) == 0 && reported == nonMultiple);

        Require(scePthreadAttrSetguardsize(&attr, nonMultiple) == 0);
        reported = 0;
        Require(scePthreadAttrGetguardsize(&attr, &reported) == 0 && reported == nonMultiple);
        reported = 0;
        Require(pthread_attr_getguardsize_nid_postfix(&attr, &reported) == 0 && reported == nonMultiple);
    }

    Require(scePthreadAttrDestroy(&attr) == 0);
}

void TestThreadCreationLifecycleWithGuardSizes() {
    const std::size_t testGuardSizes[] = {0, 4096, Ps5PageSize, 65536};

    for (const std::size_t guardSize : testGuardSizes) {
        PthreadAttr attr = nullptr;
        Require(pthread_attr_init_nid_postfix(&attr) == 0);
        Require(pthread_attr_setguardsize_nid_postfix(&attr, guardSize) == 0);
        Require(pthread_attr_setstacksize_nid_postfix(&attr, MinStackSize * 2) == 0);

        std::atomic<int> counter{0};
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, &attr, Worker, &counter, "guard_worker") == 0);
        Require(pthread_attr_destroy_nid_postfix(&attr) == 0);

        void* retval = nullptr;
        Require(scePthreadJoin(thread, &retval) == 0);
        Require(counter.load(std::memory_order_relaxed) == 1);
    }
}

void TestPointerValidationAndDestroyedRejection() {
    std::size_t size = 0;
    Require(pthread_attr_getguardsize_nid_postfix(nullptr, &size) == PosixEinval);
    Require(pthread_attr_setguardsize_nid_postfix(nullptr, 4096) == PosixEinval);
    Require(scePthreadAttrGetguardsize(nullptr, &size) == SceKernelErrorEinval);
    Require(scePthreadAttrSetguardsize(nullptr, 4096) == SceKernelErrorEinval);

    PthreadAttr unset = nullptr;
    Require(pthread_attr_getguardsize_nid_postfix(&unset, &size) == PosixEinval);
    Require(pthread_attr_setguardsize_nid_postfix(&unset, 4096) == PosixEinval);
    Require(scePthreadAttrGetguardsize(&unset, &size) == SceKernelErrorEinval);
    Require(scePthreadAttrSetguardsize(&unset, 4096) == SceKernelErrorEinval);

    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(pthread_attr_getguardsize_nid_postfix(&attr, nullptr) == PosixEinval);
    Require(scePthreadAttrGetguardsize(&attr, nullptr) == SceKernelErrorEinval);
    Require(pthread_attr_getguardsize_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(scePthreadAttrGetguardsize(nullptr, nullptr) == SceKernelErrorEinval);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_attr_getguardsize_nid_postfix(&attr, &size) == PosixEinval);
    Require(pthread_attr_setguardsize_nid_postfix(&attr, 4096) == PosixEinval);
    Require(scePthreadAttrGetguardsize(&attr, &size) == SceKernelErrorEinval);
    Require(scePthreadAttrSetguardsize(&attr, 4096) == SceKernelErrorEinval);

    PthreadAttr sceDestroyed = nullptr;
    Require(scePthreadAttrInit(&sceDestroyed) == 0);
    Require(scePthreadAttrDestroy(&sceDestroyed) == 0);
    Require(scePthreadAttrGetguardsize(&sceDestroyed, &size) == SceKernelErrorEinval);
    Require(scePthreadAttrSetguardsize(&sceDestroyed, 4096) == SceKernelErrorEinval);
    Require(pthread_attr_getguardsize_nid_postfix(&sceDestroyed, &size) == PosixEinval);
    Require(pthread_attr_setguardsize_nid_postfix(&sceDestroyed, 4096) == PosixEinval);
}

void TestGuardSizePreservationAcrossAttributeMutations() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);

    Require(scePthreadAttrSetguardsize(&attr, 32768) == 0);
    Require(scePthreadAttrSetstacksize(&attr, 1048576) == 0);
    Require(scePthreadAttrSetdetachstate(&attr, 1) == 0);

    std::size_t guardSize = 0;
    std::size_t stackSize = 0;
    int detachState = -1;

    Require(scePthreadAttrGetguardsize(&attr, &guardSize) == 0 && guardSize == 32768);
    Require(scePthreadAttrGetstacksize(&attr, &stackSize) == 0 && stackSize == 1048576);
    Require(scePthreadAttrGetdetachstate(&attr, &detachState) == 0 && detachState == 1);

    Require(pthread_attr_getguardsize_nid_postfix(&attr, &guardSize) == 0 && guardSize == 32768);
    Require(pthread_attr_getstacksize_nid_postfix(&attr, &stackSize) == 0 && stackSize == 1048576);
    Require(pthread_attr_getdetachstate_nid_postfix(&attr, &detachState) == 0 && detachState == 1);

    Require(scePthreadAttrDestroy(&attr) == 0);
}

} // namespace

int main() {
    TestDefaultGuardSize();
    TestZeroGuardSemantics();
    TestPageSizeMultiplesAndRounding();
    TestThreadCreationLifecycleWithGuardSizes();
    TestPointerValidationAndDestroyedRejection();
    TestGuardSizePreservationAcrossAttributeMutations();
    return 0;
}
