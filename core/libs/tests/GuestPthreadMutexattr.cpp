#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

static constexpr int GUEST_EINVAL = 22;
static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
static constexpr int PRIO_NONE = 0;
static constexpr int PRIO_INHERIT = 1;
static constexpr int PRIO_PROTECT = 2;
static constexpr int GuestSched_Rr = 3;

static void Require(bool value) { if (!value) std::abort(); }

template <typename Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static constexpr KernelTimespec INVALID_DEADLINE{0, 1000000000};

static void* APS5_VABI TimedlockInvalidDeadline(void* arg) {
    auto* mutex = static_cast<PthreadMutex*>(arg);
    return reinterpret_cast<void*>(static_cast<std::intptr_t>(pthread_mutex_timedlock_nid_postfix(mutex, &INVALID_DEADLINE)));
}

static void CheckTimedlockInvalidDeadline() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &INVALID_DEADLINE) == 0);
    Pthread thread{};
    void* result = nullptr;
    Require(scePthreadCreate(&thread, nullptr, TimedlockInvalidDeadline, &mutex, nullptr) == 0);
    Require(scePthreadJoin(thread, &result) == 0 && reinterpret_cast<std::intptr_t>(result) == GUEST_EINVAL);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

static void CheckType() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    int type = -1;
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == 1);
    for (const int set : {2, 3, 4, 1}) {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, set) == 0);
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == set);
    }
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 5) == GUEST_EINVAL);
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == 1);
    Require(Throws([&] { pthread_mutexattr_gettype_nid_postfix(&attr, nullptr); }));
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    type = -1;
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == GUEST_EINVAL && type == -1);
    Require(pthread_mutexattr_gettype_nid_postfix(nullptr, &type) == GUEST_EINVAL && type == -1);
}

static void CheckKind() {
    PthreadMutexattr attr = nullptr;
    *__error_nid_postfix() = 0;
    Require(pthread_mutexattr_getkind_np_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    *__error_nid_postfix() = 0;
    Require(pthread_mutexattr_setkind_np_nid_postfix(nullptr, 2) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    *__error_nid_postfix() = 0;
    Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, 2) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == 1);
    int type = -1;
    for (const int kind : {2, 3, 4, 1}) {
        Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, kind) == 0);
        Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == kind);
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == kind);
    }
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 2) == 0);
    Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == 2);
    for (const int invalid : {0, 5, -1})
        Require(Throws([&] { pthread_mutexattr_setkind_np_nid_postfix(&attr, invalid); }));
    Require(pthread_mutexattr_getkind_np_nid_postfix(attr) == 2);
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    *__error_nid_postfix() = 0;
    Require(pthread_mutexattr_setkind_np_nid_postfix(&attr, 2) == -1 && *__error_nid_postfix() == GUEST_EINVAL);
}

static void CheckSceAccessors() {
    PthreadMutexattr attr = nullptr;
    int value = -1;
    Require(scePthreadMutexattrGettype(nullptr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGettype(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_NONE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetpshared(nullptr, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetpshared(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(value == -1);
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrGettype(&attr, &value) == SCE_OK && value == 1);
    Require(scePthreadMutexattrSettype(&attr, 3) == SCE_OK);
    Require(scePthreadMutexattrGettype(&attr, &value) == SCE_OK && value == 3);
    Require(Throws([&] { scePthreadMutexattrGettype(&attr, nullptr); }));
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_OK && value == PRIO_NONE);
    for (const int invalid : {-1, 3})
        Require(scePthreadMutexattrSetprotocol(&attr, invalid) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_OK && value == PRIO_NONE);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_INHERIT) == SCE_OK);
    Require(scePthreadMutexattrGetprotocol(&attr, &value) == SCE_OK && value == PRIO_INHERIT);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetprotocol(&attr, PRIO_PROTECT) == SCE_OK);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_OK && value == sched_get_priority_max_nid_postfix(GuestSched_Rr));
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SCE_OK);
    Require(scePthreadMutexattrGetprioceiling(&attr, &value) == SCE_OK && value == 300);
    int protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_PROTECT);
    Require(scePthreadMutexattrGetpshared(&attr, &value) == SCE_OK && value == 0);
    Require(scePthreadMutexattrSetpshared(&attr, 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexattrSetpshared(&attr, 0) == SCE_OK);
    Require(scePthreadMutexattrGetpshared(&attr, &value) == SCE_OK && value == 0);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadMutexattrGettype(&attr, &value) == SCE_KERNEL_ERROR_EINVAL);
}

static void CheckProcessSharing() {
    PthreadMutexattr attr = nullptr;
    int sharing = -1;
    Require(pthread_mutexattr_getpshared_nid_postfix(nullptr, &sharing) == GUEST_EINVAL && sharing == -1);
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == GUEST_EINVAL && sharing == -1);
    Require(pthread_mutexattr_setpshared_nid_postfix(nullptr, 0) == GUEST_EINVAL);
    Require(pthread_mutexattr_setpshared_nid_postfix(&attr, 0) == GUEST_EINVAL);
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, 2) == 0);
    Require(pthread_mutexattr_setpshared_nid_postfix(&attr, 0) == 0);
    for (const int invalid : {1, -1, 2, 2147483647})
        Require(pthread_mutexattr_setpshared_nid_postfix(&attr, invalid) == GUEST_EINVAL);
    sharing = -1;
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    int type = -1;
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == 2);
    Require(Throws([&] { pthread_mutexattr_getpshared_nid_postfix(&attr, nullptr); }));
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_setpshared_nid_postfix(&attr, 0) == 0);
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == 0 && sharing == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    sharing = -1;
    Require(pthread_mutexattr_getpshared_nid_postfix(&attr, &sharing) == GUEST_EINVAL && sharing == -1);
    Require(pthread_mutexattr_setpshared_nid_postfix(&attr, 0) == GUEST_EINVAL);
}

static void CheckInvalidPriorityAttribute(PthreadMutexattr* attr) {
    int ceiling = -1;
    int protocol = -1;
    Require(pthread_mutexattr_setprotocol_nid_postfix(attr, PRIO_PROTECT) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(attr, -1) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprotocol_nid_postfix(attr, &protocol) == GUEST_EINVAL && protocol == -1);
    Require(pthread_mutexattr_getprotocol_nid_postfix(attr, nullptr) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(attr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(attr, &ceiling) == GUEST_EINVAL && ceiling == -1);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(attr, nullptr) == GUEST_EINVAL);
}

int main() {
    CheckType();
    CheckKind();
    CheckSceAccessors();
    CheckProcessSharing();
    CheckTimedlockInvalidDeadline();
    PthreadMutexattr attr = nullptr;
    CheckInvalidPriorityAttribute(nullptr);
    CheckInvalidPriorityAttribute(&attr);
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    int ceiling = -1;
    int protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_NONE);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == GUEST_EINVAL && ceiling == -1);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_INHERIT) == 0);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_INHERIT);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == GUEST_EINVAL);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_PROTECT);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == sched_get_priority_max_nid_postfix(GuestSched_Rr));
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == 0);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == 300);
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_PROTECT) == 0);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_PROTECT);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == sched_get_priority_max_nid_postfix(GuestSched_Rr));
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PRIO_NONE) == 0);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PRIO_NONE);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == GUEST_EINVAL);
    Require(Throws([&] { pthread_mutexattr_getprotocol_nid_postfix(&attr, nullptr); }));
    Require(Throws([&] { pthread_mutexattr_getprioceiling_nid_postfix(&attr, nullptr); }));
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    CheckInvalidPriorityAttribute(&attr);
    return 0;
}
