#include "SceTypes.hpp"
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string_view>

extern "C" {
int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr);
int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr);
int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr);
int APS5_VABI pthread_mutexattr_gettype_nid_postfix(const PthreadMutexattr* attr, int* type);
int APS5_VABI scePthreadMutexattrSetkind(PthreadMutexattr* attr, int kind);
int APS5_VABI scePthreadMutexattrGetkind(PthreadMutexattr attr);
int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr);
int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_unlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlockattr_init_nid_postfix(PthreadRwlockattr* attr);
int APS5_VABI pthread_rwlockattr_destroy_nid_postfix(PthreadRwlockattr* attr);
int APS5_VABI pthread_rwlockattr_settype_np_nid_postfix(PthreadRwlockattr* attr, int type);
int APS5_VABI scePthreadRwlockattrSettype(PthreadRwlockattr* attr, int type);
}

namespace {

constexpr int GUEST_EDEADLK = 11;
constexpr int GUEST_EINVAL = 22;
constexpr int SCE_FAILED_CALL = static_cast<int>(0x8001ffffu);
constexpr unsigned SCE_KERNEL_ERROR_BASE = 0x80020000u;
constexpr int KIND_ERRORCHECK = 1;
constexpr int KIND_RECURSIVE = 2;
constexpr int KIND_NORMAL = 3;
constexpr int KIND_ADAPTIVE = 4;

int failures = 0;

template<typename TCall>
void Case(const char* name, TCall call) {
    try {
        if (call()) return;
        std::printf("%s: wrong answer\n", name);
    } catch (const std::exception& error) {
        std::printf("%s: %s\n", name, error.what());
    }
    ++failures;
}

template<typename TException, typename TCall>
bool Throws(TCall call) {
    try {
        static_cast<void>(call());
    } catch (const TException&) {
        return true;
    }
    return false;
}

bool HasKind(const PthreadMutexattr* attr, int kind) {
    int type = -1;
    return pthread_mutexattr_gettype_nid_postfix(attr, &type) == 0 && type == kind;
}

bool LocksTwice(const PthreadMutexattr* attr, int secondLock) {
    PthreadMutex mutex = nullptr;
    if (pthread_mutex_init_nid_postfix(&mutex, attr) != 0) return false;
    const bool locked = pthread_mutex_lock_nid_postfix(&mutex) == 0 && pthread_mutex_lock_nid_postfix(&mutex) == secondLock;
    if (secondLock == 0) pthread_mutex_unlock_nid_postfix(&mutex);
    return locked && pthread_mutex_unlock_nid_postfix(&mutex) == 0 && pthread_mutex_destroy_nid_postfix(&mutex) == 0;
}

void TestKind() {
    Case("scePthreadMutexattrSetkind", [] {
        PthreadMutexattr attr = nullptr;
        if (pthread_mutexattr_init_nid_postfix(&attr) != 0) return false;
        for (const int kind : {KIND_RECURSIVE, KIND_NORMAL, KIND_ADAPTIVE, KIND_ERRORCHECK}) {
            if (scePthreadMutexattrSetkind(&attr, kind) != 0 || !HasKind(&attr, kind)) return false;
        }
        if (!LocksTwice(&attr, GUEST_EDEADLK)) return false;
        if (scePthreadMutexattrSetkind(&attr, KIND_RECURSIVE) != 0 || !LocksTwice(&attr, 0)) return false;
        for (const int kind : {0, 5, -1}) {
            if (!Throws<std::runtime_error>([&] { return scePthreadMutexattrSetkind(&attr, kind); }) || !HasKind(&attr, KIND_RECURSIVE)) return false;
        }
        if (pthread_mutexattr_destroy_nid_postfix(&attr) != 0) return false;
        return scePthreadMutexattrSetkind(&attr, KIND_NORMAL) == SCE_FAILED_CALL && scePthreadMutexattrSetkind(nullptr, KIND_NORMAL) == SCE_FAILED_CALL;
    });
    Case("scePthreadMutexattrGetkind", [] {
        PthreadMutexattr attr = nullptr;
        if (pthread_mutexattr_init_nid_postfix(&attr) != 0) return false;
        for (const int kind : {KIND_RECURSIVE, KIND_NORMAL, KIND_ADAPTIVE, KIND_ERRORCHECK}) {
            if (scePthreadMutexattrSetkind(&attr, kind) != 0 || scePthreadMutexattrGetkind(attr) != static_cast<int>(SCE_KERNEL_ERROR_BASE + static_cast<unsigned>(kind))) return false;
        }
        if (pthread_mutexattr_destroy_nid_postfix(&attr) != 0) return false;
        return scePthreadMutexattrGetkind(nullptr) == SCE_FAILED_CALL;
    });
}

void TestRwlockattr() {
    Case("pthread_rwlockattr_settype_np", [] {
        PthreadRwlockattr attr = nullptr;
        if (pthread_rwlockattr_init_nid_postfix(&attr) != 0) return false;
        for (const int type : {1, 2}) {
            if (pthread_rwlockattr_settype_np_nid_postfix(&attr, type) != 0 || scePthreadRwlockattrSettype(&attr, type) != 0) return false;
            PthreadRwlock rwlock = nullptr;
            if (pthread_rwlock_init_nid_postfix(&rwlock, &attr) != 0) return false;
            const bool usable = pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0 && pthread_rwlock_unlock_nid_postfix(&rwlock) == 0;
            if (!usable || pthread_rwlock_destroy_nid_postfix(&rwlock) != 0) return false;
        }
        for (const int type : {0, 3, -1, static_cast<int>(0x80000000u)}) {
            if (pthread_rwlockattr_settype_np_nid_postfix(&attr, type) != GUEST_EINVAL) return false;
        }
        if (pthread_rwlockattr_destroy_nid_postfix(&attr) != 0) return false;
        return pthread_rwlockattr_settype_np_nid_postfix(&attr, 1) == GUEST_EINVAL && pthread_rwlockattr_settype_np_nid_postfix(nullptr, 1) == GUEST_EINVAL;
    });
}

}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "rwlockattr") TestRwlockattr();
    else if (mode == "kind") TestKind();
    else return 2;
    if (failures == 0) std::printf("all cases passed\n");
    return failures == 0 ? 0 : 1;
}
