#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace {

constexpr int GUEST_EDEADLK = 11;
constexpr int GUEST_EINVAL = 22;
constexpr int GUEST_EFAULT = 14;
constexpr int GUEST_ERANGE = 34;
constexpr int SCE_FAILED_CALL = static_cast<int>(0x8001ffffu);
constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
constexpr unsigned SCE_KERNEL_ERROR_BASE = 0x80020000u;
constexpr int KIND_ERRORCHECK = 1;
constexpr int KIND_RECURSIVE = 2;
constexpr int KIND_NORMAL = 3;
constexpr int KIND_ADAPTIVE = 4;

using MutexInit = int (APS5_VABI*)(PthreadMutex*, const PthreadMutexattr*);

int failures = 0;

struct StackQueryState {
    std::atomic<bool> queried{false};
    std::atomic<bool> release{false};
    void* address = nullptr;
    std::size_t size = 0;
};

void* APS5_VABI QueryStack(void* argument) {
    auto* state = static_cast<StackQueryState*>(argument);
    pthread_getstack_np_nid_postfix(&state->address, &state->size);
    state->queried.store(true, std::memory_order_release);
    while (!state->release.load(std::memory_order_acquire)) scePthreadYield();
    return argument;
}

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

bool HasKind(const PthreadMutexattr* attr, int kind) {
    int type = -1;
    return pthread_mutexattr_gettype_nid_postfix(attr, &type) == 0 && type == kind;
}

bool LocksTwice(const PthreadMutexattr* attr, int secondLock, MutexInit init = pthread_mutex_init_nid_postfix) {
    PthreadMutex mutex = nullptr;
    if (init(&mutex, attr) != 0 || mutex == nullptr) return false;
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
        for (const auto [kind, stored] : {std::pair{0, 0}, std::pair{5, 5}, std::pair{-1, -1}, std::pair{0x8000, -32768}, std::pair{0x12345, 0x2345}}) {
            if (scePthreadMutexattrSetkind(&attr, kind) != 0 || pthread_mutexattr_getkind_np_nid_postfix(attr) != stored) return false;
            PthreadMutex mutex = nullptr;
            if (scePthreadMutexInit(&mutex, &attr, nullptr) != SCE_KERNEL_ERROR_EINVAL || mutex != nullptr) return false;
        }
        if (scePthreadMutexattrSetkind(&attr, 0x10001) != 0 || !HasKind(&attr, KIND_ERRORCHECK) || !LocksTwice(&attr, GUEST_EDEADLK)) return false;
        if (pthread_mutexattr_destroy_nid_postfix(&attr) != 0) return false;
        for (PthreadMutexattr* target : {&attr, static_cast<PthreadMutexattr*>(nullptr)}) {
            for (const int kind : {KIND_NORMAL, 5}) {
                *__error_nid_postfix() = 0;
                if (scePthreadMutexattrSetkind(target, kind) != SCE_FAILED_CALL || *__error_nid_postfix() != GUEST_EINVAL) return false;
            }
        }
        return true;
    });
    Case("scePthreadMutexattrGetkind", [] {
        PthreadMutexattr attr = nullptr;
        if (pthread_mutexattr_init_nid_postfix(&attr) != 0) return false;
        for (const int kind : {KIND_RECURSIVE, KIND_NORMAL, KIND_ADAPTIVE, KIND_ERRORCHECK}) {
            if (scePthreadMutexattrSetkind(&attr, kind) != 0 || scePthreadMutexattrGetkind(attr) != static_cast<int>(SCE_KERNEL_ERROR_BASE + static_cast<unsigned>(kind))) return false;
        }
        for (const auto [kind, result] : {std::pair{0, 0}, std::pair{5, static_cast<int>(SCE_KERNEL_ERROR_BASE + 5u)}, std::pair{0x12345, static_cast<int>(SCE_KERNEL_ERROR_BASE + 0x2345u)}}) {
            if (scePthreadMutexattrSetkind(&attr, kind) != 0 || scePthreadMutexattrGetkind(attr) != result) return false;
        }
        if (pthread_mutexattr_destroy_nid_postfix(&attr) != 0) return false;
        return scePthreadMutexattrGetkind(nullptr) == SCE_FAILED_CALL;
    });
    Case("scePthreadMutexattrInitForInternalLibc", [] {
        PthreadMutexattr attr = nullptr;
        if (scePthreadMutexattrInitForInternalLibc(&attr) != 0 || !HasKind(&attr, KIND_ERRORCHECK)) return false;
        if (!LocksTwice(&attr, GUEST_EDEADLK, scePthreadMutexInitForInternalLibc)) return false;
        return pthread_mutexattr_destroy_nid_postfix(&attr) == 0;
    });
    Case("scePthreadMutexInitForInternalLibc", [] {
        PthreadMutexattr attr = nullptr;
        if (!LocksTwice(nullptr, GUEST_EDEADLK, scePthreadMutexInitForInternalLibc) || !LocksTwice(&attr, GUEST_EDEADLK, scePthreadMutexInitForInternalLibc)) return false;
        if (pthread_mutexattr_init_nid_postfix(&attr) != 0) return false;
        if (scePthreadMutexattrSetkind(&attr, KIND_RECURSIVE) != 0 || !LocksTwice(&attr, 0, scePthreadMutexInitForInternalLibc)) return false;
        PthreadMutex mutex = nullptr;
        if (scePthreadMutexattrSetkind(&attr, 5) != 0 || scePthreadMutexInitForInternalLibc(&mutex, &attr) != SCE_KERNEL_ERROR_EINVAL || mutex != nullptr) return false;
        return pthread_mutexattr_destroy_nid_postfix(&attr) == 0;
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

void TestStack() {
    Case("pthread_getstack_np", [] {
        void* address = nullptr;
        std::size_t size = 0;
        pthread_getstack_np_nid_postfix(&address, &size);
        if (!address || size < 16384) return false;
        if (pthread_getstack_np_nid_postfix(nullptr, &size) != GUEST_EFAULT) return false;
        address = nullptr;
        if (pthread_getstack_np_nid_postfix(&address, nullptr) != GUEST_EFAULT || !address) return false;
        PthreadAttr attr = nullptr;
        if (pthread_attr_init_nid_postfix(&attr) != 0) return false;
        constexpr std::size_t ChildStackSize = 256 * 1024;
        if (pthread_attr_setstacksize_nid_postfix(&attr, ChildStackSize) != 0) return false;
        StackQueryState state;
        Pthread child = nullptr;
        if (pthread_create_nid_postfix(&child, &attr, QueryStack, &state) != 0) return false;
        while (!state.queried.load(std::memory_order_acquire)) scePthreadYield();
        if (!state.address || state.size != ChildStackSize) return false;
        state.release.store(true, std::memory_order_release);
        void* result = nullptr;
        if (pthread_join_nid_postfix(child, &result) != 0 || result != &state) return false;
        return pthread_attr_destroy_nid_postfix(&attr) == 0;
    });
}

void TestAttr() {
    Case("pthread_attr_setstackaddr", [] {
        PthreadAttr attr = nullptr;
        void* dummy = reinterpret_cast<void*>(0x1000000);
        if (pthread_attr_setstackaddr_nid_postfix(&attr, dummy) != GUEST_EINVAL) return false;
        if (pthread_attr_setstackaddr_nid_postfix(nullptr, dummy) != GUEST_EINVAL) return false;
        if (pthread_attr_init_nid_postfix(&attr) != 0 || attr == nullptr) return false;
        if (pthread_attr_setstackaddr_nid_postfix(&attr, nullptr) != GUEST_EINVAL) return false;
        if (pthread_attr_setstackaddr_nid_postfix(&attr, dummy) != 0) return false;
        void* retrieved = nullptr;
        if (pthread_attr_getstackaddr_nid_postfix(&attr, &retrieved) != 0 || retrieved != dummy) return false;
        return pthread_attr_destroy_nid_postfix(&attr) == 0;
    });
    Case("pthread_attr_getsolosched_np", [] {
        PthreadAttr attr = nullptr;
        int solosched = -1;
        if (pthread_attr_getsolosched_np_nid_postfix(&attr, &solosched) != GUEST_EINVAL) return false;
        if (pthread_attr_getsolosched_np_nid_postfix(nullptr, &solosched) != GUEST_EINVAL) return false;
        if (pthread_attr_init_nid_postfix(&attr) != 0 || attr == nullptr) return false;
        if (pthread_attr_getsolosched_np_nid_postfix(&attr, nullptr) != GUEST_EINVAL) return false;
        if (pthread_attr_getsolosched_np_nid_postfix(&attr, &solosched) != 0 || solosched != 0) return false;
        if (pthread_attr_setsolosched_np_nid_postfix(&attr, 16) != 0) return false;
        if (pthread_attr_getsolosched_np_nid_postfix(&attr, &solosched) != 0 || solosched != 16) return false;
        return pthread_attr_destroy_nid_postfix(&attr) == 0;
    });
    Case("pthread_attr_affinity_np", [] {
        PthreadAttr attr = nullptr;
        KernelCpumask mask = 0;
        if (pthread_attr_getaffinity_np_nid_postfix(&attr, sizeof(mask), &mask) != GUEST_EINVAL) return false;
        if (pthread_attr_getaffinity_np_nid_postfix(nullptr, sizeof(mask), &mask) != GUEST_EINVAL) return false;
        if (pthread_attr_setaffinity_np_nid_postfix(&attr, sizeof(mask), &mask) != GUEST_EINVAL) return false;
        if (pthread_attr_setaffinity_np_nid_postfix(nullptr, sizeof(mask), &mask) != GUEST_EINVAL) return false;
        if (pthread_attr_init_nid_postfix(&attr) != 0 || attr == nullptr) return false;
        if (pthread_attr_getaffinity_np_nid_postfix(&attr, sizeof(mask), nullptr) != GUEST_EFAULT) return false;
        if (pthread_attr_getaffinity_np_nid_postfix(&attr, 0, &mask) != GUEST_ERANGE) return false;
        if (pthread_attr_getaffinity_np_nid_postfix(&attr, 4, &mask) != GUEST_ERANGE) return false;
        if (pthread_attr_getaffinity_np_nid_postfix(&attr, 7, &mask) != GUEST_ERANGE) return false;
        unsigned char buffer[32] = {0xFF};
        if (pthread_attr_getaffinity_np_nid_postfix(&attr, sizeof(buffer), buffer) != 0) return false;
        for (std::size_t i = 8; i < sizeof(buffer); ++i) {
            if (buffer[i] != 0) return false;
        }
        if (pthread_attr_setaffinity_np_nid_postfix(&attr, 0, nullptr) != 0) return false;
        if (pthread_attr_setaffinity_np_nid_postfix(&attr, 4, &mask) != GUEST_ERANGE) return false;
        if (pthread_attr_setaffinity_np_nid_postfix(&attr, sizeof(mask), nullptr) != GUEST_EFAULT) return false;
        unsigned char badBuffer[16] = {0};
        badBuffer[8] = 1;
        if (pthread_attr_setaffinity_np_nid_postfix(&attr, sizeof(badBuffer), badBuffer) != GUEST_EINVAL) return false;
        KernelCpumask targetMask = 0x3;
        if (pthread_attr_setaffinity_np_nid_postfix(&attr, sizeof(targetMask), &targetMask) != 0) return false;
        KernelCpumask readBack = 0;
        if (pthread_attr_getaffinity_np_nid_postfix(&attr, sizeof(readBack), &readBack) != 0 || readBack != targetMask) return false;
        return pthread_attr_destroy_nid_postfix(&attr) == 0;
    });
}

}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "rwlockattr") TestRwlockattr();
    else if (mode == "kind") TestKind();
    else if (mode == "stack") TestStack();
    else if (mode == "attr") TestAttr();
    else return 2;
    if (failures == 0) std::printf("all cases passed\n");
    return failures == 0 ? 0 : 1;
}
