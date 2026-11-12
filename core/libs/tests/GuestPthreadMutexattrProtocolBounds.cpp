#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EFAULT = 14;
static constexpr int SCE_OK = 0;
static constexpr int PRIO_NONE = 0;
static constexpr int PRIO_INHERIT = 1;
static constexpr int PRIO_PROTECT = 2;
static constexpr int SCHED_RR_POLICY = 3;

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static void TestUninitializedAndNullAttributes() {
    int value = -1;
    Require(pthread_mutexattr_setprotocol_nid_postfix(nullptr, PRIO_NONE) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(nullptr, PRIO_PROTECT) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprotocol_nid_postfix(nullptr, &value) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(nullptr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(nullptr, &value) == GUEST_EINVAL);

    Require(scePthreadMutexattrSetprotocol(nullptr, PRIO_NONE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprotocol(nullptr, PRIO_PROTECT) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprotocol(nullptr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprioceiling(nullptr, 300) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(nullptr, &value) == SCE_KERNEL_ERROR_EINVAL);

    PthreadMutexattr uninit = nullptr;
    Require(pthread_mutexattr_setprotocol_nid_postfix(&uninit, PRIO_NONE) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&uninit, PRIO_PROTECT) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&uninit, &value) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&uninit, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&uninit, &value) == GUEST_EINVAL);

    Require(scePthreadMutexattrSetprotocol(&uninit, PRIO_NONE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprotocol(&uninit, PRIO_PROTECT) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprotocol(&uninit, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprioceiling(&uninit, 300) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&uninit, &value) == SCE_KERNEL_ERROR_EINVAL);

    auto* const badAttr = reinterpret_cast<PthreadMutexattr*>(static_cast<std::uintptr_t>(0x10));
    Require(pthread_mutexattr_setprotocol_nid_postfix(badAttr, PRIO_NONE) == GUEST_EFAULT);
    Require(pthread_mutexattr_getprotocol_nid_postfix(badAttr, &value) == GUEST_EFAULT);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(badAttr, 300) == GUEST_EFAULT);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(badAttr, &value) == GUEST_EFAULT);

    Require(scePthreadMutexattrSetprotocol(badAttr, PRIO_NONE) == SCE_KERNEL_ERROR_EFAULT);
    Require(scePthreadMutexattrGetprotocol(badAttr, &value) == SCE_KERNEL_ERROR_EFAULT);
    Require(scePthreadMutexattrSetprioceiling(badAttr, 300) == SCE_KERNEL_ERROR_EFAULT);
    Require(scePthreadMutexattrGetprioceiling(badAttr, &value) == SCE_KERNEL_ERROR_EFAULT);
}

static void TestProtocolValues() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);

    int protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0);
    Require(protocol == PRIO_NONE);

    int sceProtocol = -1;
    Require(scePthreadMutexattrGetprotocol(&attr, &sceProtocol) == SCE_OK);
    Require(sceProtocol == PRIO_NONE);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_INHERIT) == 0);
    protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0);
    Require(protocol == PRIO_INHERIT);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0);
    Require(protocol == PRIO_PROTECT);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_NONE) == 0);
    protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0);
    Require(protocol == PRIO_NONE);

    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_INHERIT) == SCE_OK);
    sceProtocol = -1;
    Require(scePthreadMutexattrGetprotocol(&attr, &sceProtocol) == SCE_OK);
    Require(sceProtocol == PRIO_INHERIT);

    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_PROTECT) == SCE_OK);
    sceProtocol = -1;
    Require(scePthreadMutexattrGetprotocol(&attr, &sceProtocol) == SCE_OK);
    Require(sceProtocol == PRIO_PROTECT);

    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_NONE) == SCE_OK);
    sceProtocol = -1;
    Require(scePthreadMutexattrGetprotocol(&attr, &sceProtocol) == SCE_OK);
    Require(sceProtocol == PRIO_NONE);

    constexpr int invalidProtocols[] = {-1, -100, 3, 4, 100, 2147483647, -2147483647 - 1};
    for (const int invalid : invalidProtocols) {
        Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, invalid) == GUEST_EINVAL);
        Require(scePthreadMutexattrSetprotocol(&attr, invalid) == SCE_KERNEL_ERROR_EINVAL);
        protocol = -1;
        Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0);
        Require(protocol == PRIO_NONE);
    }

    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestPrioceilingUnderProtocols() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);

    int ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == GUEST_EINVAL);
    Require(ceiling == -1);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == GUEST_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SCE_KERNEL_ERROR_EINVAL);
    Require(ceiling == -1);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_KERNEL_ERROR_EINVAL);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_INHERIT) == 0);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == GUEST_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_KERNEL_ERROR_EINVAL);

    const int maxSched = sched_get_priority_max_nid_postfix(SCHED_RR_POLICY);
    Require(maxSched == 767);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
    Require(ceiling == maxSched);

    int sceCeiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &sceCeiling) == SCE_OK);
    Require(sceCeiling == maxSched);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 256) == 0);
    ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
    Require(ceiling == 256);

    Require(scePthreadMutexattrSetprioceiling(&attr, 512) == SCE_OK);
    sceCeiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &sceCeiling) == SCE_OK);
    Require(sceCeiling == 512);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 767) == 0);
    ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
    Require(ceiling == 767);

    constexpr int invalidCeilings[] = {255, 0, -1, -500, 768, 769, 1000, 2147483647, -2147483647 - 1};
    for (const int invalid : invalidCeilings) {
        Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, invalid) == GUEST_EINVAL);
        Require(scePthreadMutexattrSetprioceiling(&attr, invalid) == SCE_KERNEL_ERROR_EINVAL);
        ceiling = -1;
        Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
        Require(ceiling == 767);
    }

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 350) == 0);
    ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
    Require(ceiling == 350);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
    Require(ceiling == maxSched);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 400) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_NONE) == 0);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
    Require(ceiling == maxSched);

    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestFaultPointers() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);

    auto* const badInt = reinterpret_cast<int*>(static_cast<std::uintptr_t>(0x10));

    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, nullptr) == GUEST_EFAULT);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, badInt) == GUEST_EFAULT);
    Require(scePthreadMutexattrGetprotocol(&attr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(scePthreadMutexattrGetprotocol(&attr, badInt) == SCE_KERNEL_ERROR_EFAULT);

    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, badInt) == GUEST_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, badInt) == SCE_KERNEL_ERROR_EINVAL);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);

    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, nullptr) == GUEST_EFAULT);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, badInt) == GUEST_EFAULT);
    Require(scePthreadMutexattrGetprioceiling(&attr, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(scePthreadMutexattrGetprioceiling(&attr, badInt) == SCE_KERNEL_ERROR_EFAULT);

    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void TestDestroyedAttribute() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    int value = -1;
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_NONE) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &value) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &value) == GUEST_EINVAL);

    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_NONE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);

    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(attr == nullptr);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_NONE) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &value) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &value) == GUEST_EINVAL);

    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_NONE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
}

int main() {
    TestUninitializedAndNullAttributes();
    TestProtocolValues();
    TestPrioceilingUnderProtocols();
    TestFaultPointers();
    TestDestroyedAttribute();
    return 0;
}
