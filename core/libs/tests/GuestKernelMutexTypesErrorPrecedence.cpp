#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <exception>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEdeadlk = 11;
constexpr int PosixEfault = 14;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;

constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000Eu);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

constexpr int MutexTypeErrorCheck = 1;
constexpr int MutexTypeRecursive = 2;
constexpr int MutexTypeNormal = 3;
constexpr int MutexTypeAdaptive = 4;

constexpr std::uint32_t UmutexUnowned = 0;
constexpr std::uint32_t UmutexContested = 0x80000000u;
constexpr std::uint32_t UmutexPrioProtect = 0x8;
constexpr std::uint32_t UmutexRobust = 0x10;

constexpr int UmtxOpMutexTrylock = 4;
constexpr int UmtxOpMutexLock = 5;
constexpr int UmtxOpMutexUnlock = 6;
constexpr int UmtxOpSetCeiling = 7;
constexpr int UmtxOpMutexWait = 17;
constexpr int UmtxOpMutexWake = 18;

struct alignas(4) UserMutex {
    std::uint32_t owner;
    std::uint32_t flags;
    std::uint32_t ceilings[2];
    std::uint32_t spare[4];
};

static_assert(sizeof(UserMutex) == 32);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void TestMutexattrTypesAndValidation() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    int type = -1;
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == MutexTypeErrorCheck);
    type = -1;
    Require(scePthreadMutexattrGettype(&attr, &type) == 0 && type == MutexTypeErrorCheck);

    const int validTypes[] = {
        MutexTypeErrorCheck,
        MutexTypeRecursive,
        MutexTypeNormal,
        MutexTypeAdaptive
    };

    for (const int validType : validTypes) {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, validType) == 0);
        type = -1;
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == validType);
        type = -1;
        Require(scePthreadMutexattrGettype(&attr, &type) == 0 && type == validType);
    }

    for (const int validType : validTypes) {
        Require(scePthreadMutexattrSettype(&attr, validType) == 0);
        type = -1;
        Require(scePthreadMutexattrGettype(&attr, &type) == 0 && type == validType);
        type = -1;
        Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == 0 && type == validType);
    }

    const int invalidTypes[] = {-500, -10, -2, -1, 0, 5, 6, 10, 100, 1000};
    for (const int invalidType : invalidTypes) {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, invalidType) == PosixEinval);
        Require(scePthreadMutexattrSettype(&attr, invalidType) == SceKernelErrorEinval);
    }

    Require(pthread_mutexattr_settype_nid_postfix(nullptr, MutexTypeErrorCheck) == PosixEinval);
    Require(pthread_mutexattr_settype_nid_postfix(nullptr, MutexTypeRecursive) == PosixEinval);
    Require(pthread_mutexattr_settype_nid_postfix(nullptr, MutexTypeNormal) == PosixEinval);
    Require(pthread_mutexattr_settype_nid_postfix(nullptr, -1) == PosixEinval);

    Require(scePthreadMutexattrSettype(nullptr, MutexTypeErrorCheck) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSettype(nullptr, MutexTypeRecursive) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSettype(nullptr, MutexTypeNormal) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSettype(nullptr, -1) == SceKernelErrorEinval);

    Require(pthread_mutexattr_gettype_nid_postfix(nullptr, &type) == PosixEinval);
    Require(scePthreadMutexattrGettype(nullptr, &type) == SceKernelErrorEinval);
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadMutexattrGettype(&attr, nullptr) == SceKernelErrorEfault);
    Require(pthread_mutexattr_gettype_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(scePthreadMutexattrGettype(nullptr, nullptr) == SceKernelErrorEinval);

    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeErrorCheck) == PosixEinval);
    Require(pthread_mutexattr_gettype_nid_postfix(&attr, &type) == PosixEinval);
    Require(scePthreadMutexattrSettype(&attr, MutexTypeErrorCheck) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGettype(&attr, &type) == SceKernelErrorEinval);
}

void TestMutexTypeErrorCheckSemantics() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeErrorCheck) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == PosixEdeadlk);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

void TestMutexTypeRecursiveSemantics() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeRecursive) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

void TestMutexTypeNormalSemantics() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeNormal) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
    Require(Throws([&] { pthread_mutex_lock_nid_postfix(&mutex); }));
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

void TestMutexTypeAdaptiveSemantics() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeAdaptive) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == PosixEdeadlk);
    Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

void TestRobustMutexRecoveryAndPrecedence() {
    *__error_nid_postfix() = 0;
    Require(_umtx_op_nid_postfix(nullptr, UmtxOpMutexTrylock, 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(_umtx_op_nid_postfix(nullptr, UmtxOpMutexLock, 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(_umtx_op_nid_postfix(nullptr, UmtxOpMutexUnlock, 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    UserMutex unalignedMutex{};
    unalignedMutex.flags = UmutexRobust;
    auto* unaligned = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&unalignedMutex) + 1);
    Require(Throws([&] { _umtx_op_nid_postfix(unaligned, UmtxOpMutexTrylock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(unaligned, UmtxOpMutexUnlock, 0, nullptr, nullptr); }));

    UserMutex robustPrioMutex{};
    robustPrioMutex.flags = UmutexRobust | UmutexPrioProtect;
    Require(Throws([&] { _umtx_op_nid_postfix(&robustPrioMutex, UmtxOpMutexTrylock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(&robustPrioMutex, UmtxOpMutexLock, 0, nullptr, nullptr); }));

    UserMutex unownedRobustMutex{};
    unownedRobustMutex.owner = 99999;
    unownedRobustMutex.flags = UmutexRobust;
    Require(Throws([&] { _umtx_op_nid_postfix(&unownedRobustMutex, UmtxOpMutexUnlock, 0, nullptr, nullptr); }));

    UserMutex recoveredMutex{};
    recoveredMutex.owner = UmutexUnowned;
    recoveredMutex.flags = 0;

    Require(_umtx_op_nid_postfix(&recoveredMutex, UmtxOpMutexTrylock, 0, nullptr, nullptr) == 0);
    Require(recoveredMutex.owner != UmutexUnowned);

    Require(_umtx_op_nid_postfix(&recoveredMutex, UmtxOpMutexTrylock, 0, nullptr, nullptr) == 0);

    Require(_umtx_op_nid_postfix(&recoveredMutex, UmtxOpMutexUnlock, 0, nullptr, nullptr) == 0);
    Require(recoveredMutex.owner == UmutexUnowned);

    recoveredMutex.owner = UmutexContested;
    Require(_umtx_op_nid_postfix(&recoveredMutex, UmtxOpMutexTrylock, 0, nullptr, nullptr) == 0);
    Require((recoveredMutex.owner & UmutexContested) != 0);

    Require(_umtx_op_nid_postfix(&recoveredMutex, UmtxOpMutexUnlock, 0, nullptr, nullptr) == 0);
    Require(recoveredMutex.owner == UmutexUnowned);
}

} // namespace

int main() {
    TestMutexattrTypesAndValidation();
    TestMutexTypeErrorCheckSemantics();
    TestMutexTypeRecursiveSemantics();
    TestMutexTypeNormalSemantics();
    TestMutexTypeAdaptiveSemantics();
    TestRobustMutexRecoveryAndPrecedence();
    return 0;
}
