#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEinval = 22;
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

constexpr std::size_t PthreadStackMin = 16384;
constexpr std::size_t DefaultStackSize = 1048576;
constexpr std::size_t DefaultGuardSize = 16384;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void* APS5_VABI WorkerThread(void* arg) {
    auto* executed = static_cast<bool*>(arg);
    *executed = true;
    return nullptr;
}

void TestAttrStackAndGuardDefaults() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    std::size_t stackSize = 0;
    Require(pthread_attr_getstacksize_nid_postfix(&attr, &stackSize) == 0);
    Require(stackSize == DefaultStackSize);

    std::size_t guardSize = 0;
    Require(pthread_attr_getguardsize_nid_postfix(&attr, &guardSize) == 0);
    Require(guardSize == DefaultGuardSize);

    guardSize = 0;
    Require(scePthreadAttrGetguardsize(&attr, &guardSize) == 0);
    Require(guardSize == DefaultGuardSize);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestStackSizeValidationAndBounds() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    const std::size_t invalidStackSizes[] = {0, 1, 100, 1024, 4096, 8192, 16383};
    for (const auto invalidSize : invalidStackSizes) {
        Require(pthread_attr_setstacksize_nid_postfix(&attr, invalidSize) == PosixEinval);
        Require(scePthreadAttrSetstacksize(&attr, invalidSize) == SceKernelErrorEinval);

        std::size_t currentSize = 0;
        Require(pthread_attr_getstacksize_nid_postfix(&attr, &currentSize) == 0);
        Require(currentSize == DefaultStackSize);
    }

    const std::size_t validStackSizes[] = {
        PthreadStackMin,
        32768,
        65536,
        131072,
        262144,
        524288,
        1048576,
        2097152,
        4194304
    };
    for (const auto validSize : validStackSizes) {
        Require(pthread_attr_setstacksize_nid_postfix(&attr, validSize) == 0);
        std::size_t readBack = 0;
        Require(pthread_attr_getstacksize_nid_postfix(&attr, &readBack) == 0);
        Require(readBack == validSize);

        Require(scePthreadAttrSetstacksize(&attr, validSize) == 0);
        readBack = 0;
        Require(pthread_attr_getstacksize_nid_postfix(&attr, &readBack) == 0);
        Require(readBack == validSize);
    }

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestGuardSizeValidationAndBounds() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    const std::size_t validGuardSizes[] = {0, 4096, 8192, 16384, 32768, 65536, 131072};
    for (const auto validGuard : validGuardSizes) {
        Require(pthread_attr_setguardsize_nid_postfix(&attr, validGuard) == 0);
        std::size_t readBack = 12345;
        Require(pthread_attr_getguardsize_nid_postfix(&attr, &readBack) == 0);
        Require(readBack == validGuard);

        Require(scePthreadAttrSetguardsize(&attr, validGuard) == 0);
        readBack = 12345;
        Require(scePthreadAttrGetguardsize(&attr, &readBack) == 0);
        Require(readBack == validGuard);
    }

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

void TestNullAndUninitializedAttributeHandling() {
    std::size_t stackSize = 0;
    std::size_t guardSize = 0;

    Require(pthread_attr_init_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_attr_destroy_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_attr_setstacksize_nid_postfix(nullptr, 65536) == PosixEinval);
    Require(pthread_attr_getstacksize_nid_postfix(nullptr, &stackSize) == PosixEinval);
    Require(pthread_attr_setguardsize_nid_postfix(nullptr, 4096) == PosixEinval);
    Require(pthread_attr_getguardsize_nid_postfix(nullptr, &guardSize) == PosixEinval);
    Require(scePthreadAttrSetstacksize(nullptr, 65536) == SceKernelErrorEinval);
    Require(scePthreadAttrSetguardsize(nullptr, 4096) == SceKernelErrorEinval);
    Require(scePthreadAttrGetguardsize(nullptr, &guardSize) == SceKernelErrorEinval);

    PthreadAttr uninit = nullptr;
    Require(pthread_attr_destroy_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_attr_setstacksize_nid_postfix(&uninit, 65536) == PosixEinval);
    Require(pthread_attr_getstacksize_nid_postfix(&uninit, &stackSize) == PosixEinval);
    Require(pthread_attr_setguardsize_nid_postfix(&uninit, 4096) == PosixEinval);
    Require(pthread_attr_getguardsize_nid_postfix(&uninit, &guardSize) == PosixEinval);
    Require(scePthreadAttrSetstacksize(&uninit, 65536) == SceKernelErrorEinval);
    Require(scePthreadAttrSetguardsize(&uninit, 4096) == SceKernelErrorEinval);
    Require(scePthreadAttrGetguardsize(&uninit, &guardSize) == SceKernelErrorEinval);
}

void TestDestroyedAttributePrecedence() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    std::size_t stackSize = 0;
    std::size_t guardSize = 0;

    Require(pthread_attr_destroy_nid_postfix(&attr) == PosixEinval);
    Require(pthread_attr_setstacksize_nid_postfix(&attr, 65536) == PosixEinval);
    Require(pthread_attr_getstacksize_nid_postfix(&attr, &stackSize) == PosixEinval);
    Require(pthread_attr_setguardsize_nid_postfix(&attr, 4096) == PosixEinval);
    Require(pthread_attr_getguardsize_nid_postfix(&attr, &guardSize) == PosixEinval);
    Require(scePthreadAttrSetstacksize(&attr, 65536) == SceKernelErrorEinval);
    Require(scePthreadAttrSetguardsize(&attr, 4096) == SceKernelErrorEinval);
    Require(scePthreadAttrGetguardsize(&attr, &guardSize) == SceKernelErrorEinval);
}

void TestNullOutputPointerPrecedence() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_attr_getstacksize_nid_postfix(&attr, nullptr) == PosixEinval);
    Require(pthread_attr_getguardsize_nid_postfix(&attr, nullptr) == PosixEinval);
    Require(scePthreadAttrGetguardsize(&attr, nullptr) == SceKernelErrorEinval);

    Require(pthread_attr_getstacksize_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(pthread_attr_getguardsize_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(scePthreadAttrGetguardsize(nullptr, nullptr) == SceKernelErrorEinval);

    PthreadAttr uninit = nullptr;
    Require(pthread_attr_getstacksize_nid_postfix(&uninit, nullptr) == PosixEinval);
    Require(pthread_attr_getguardsize_nid_postfix(&uninit, nullptr) == PosixEinval);
    Require(scePthreadAttrGetguardsize(&uninit, nullptr) == SceKernelErrorEinval);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    Require(pthread_attr_getstacksize_nid_postfix(&attr, nullptr) == PosixEinval);
    Require(pthread_attr_getguardsize_nid_postfix(&attr, nullptr) == PosixEinval);
    Require(scePthreadAttrGetguardsize(&attr, nullptr) == SceKernelErrorEinval);
}

void TestThreadCreationWithCustomStackAndGuard() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_attr_setstacksize_nid_postfix(&attr, 65536) == 0);
    Require(pthread_attr_setguardsize_nid_postfix(&attr, 8192) == 0);

    bool executed = false;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, WorkerThread, &executed, nullptr) == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(executed);

    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);
}

} // namespace

int main() {
    TestAttrStackAndGuardDefaults();
    TestStackSizeValidationAndBounds();
    TestGuardSizeValidationAndBounds();
    TestNullAndUninitializedAttributeHandling();
    TestDestroyedAttributePrecedence();
    TestNullOutputPointerPrecedence();
    TestThreadCreationWithCustomStackAndGuard();
    return 0;
}
