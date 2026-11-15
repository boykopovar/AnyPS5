#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <exception>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEfault = 14;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;

constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000Eu);
constexpr int SceKernelErrorEbusy = static_cast<int>(0x80020010u);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

constexpr int PrioNone = 0;
constexpr int PrioInherit = 1;
constexpr int PrioProtect = 2;
constexpr int SchedRr = 3;

constexpr std::uint32_t UmutexPrioProtect = 0x8;
constexpr std::uint32_t UmutexRobust = 0x10;

constexpr int UmtxOpMutexTrylock = 4;
constexpr int UmtxOpMutexLock = 5;
constexpr int UmtxOpMutexUnlock = 6;
constexpr int UmtxOpSetCeiling = 7;
constexpr int UmtxOpMutexWait = 17;
constexpr int UmtxOpMutexWake = 18;
constexpr int UmtxOpRobustLists = 22;

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

void TestMutexAttributeProtocols() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    int protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == PrioNone);
    protocol = -1;
    Require(scePthreadMutexattrGetprotocol(&attr, &protocol) == 0 && protocol == PrioNone);

    const int validProtocols[] = {PrioNone, PrioInherit, PrioProtect};
    for (const int validProtocol : validProtocols) {
        Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, validProtocol) == 0);
        protocol = -1;
        Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == validProtocol);
        protocol = -1;
        Require(scePthreadMutexattrGetprotocol(&attr, &protocol) == 0 && protocol == validProtocol);
    }

    for (const int validProtocol : validProtocols) {
        Require(scePthreadMutexattrSetprotocol(&attr, validProtocol) == 0);
        protocol = -1;
        Require(scePthreadMutexattrGetprotocol(&attr, &protocol) == 0 && protocol == validProtocol);
        protocol = -1;
        Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0 && protocol == validProtocol);
    }

    const int invalidProtocols[] = {-1000, -50, -2, -1, 3, 4, 10, 100, 500};
    for (const int invalidProtocol : invalidProtocols) {
        Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, invalidProtocol) == PosixEinval);
        Require(scePthreadMutexattrSetprotocol(&attr, invalidProtocol) == SceKernelErrorEinval);
    }

    Require(pthread_mutexattr_setprotocol_nid_postfix(nullptr, PrioNone) == PosixEinval);
    Require(pthread_mutexattr_setprotocol_nid_postfix(nullptr, PrioInherit) == PosixEinval);
    Require(pthread_mutexattr_setprotocol_nid_postfix(nullptr, PrioProtect) == PosixEinval);
    Require(pthread_mutexattr_setprotocol_nid_postfix(nullptr, -1) == PosixEinval);
    Require(pthread_mutexattr_setprotocol_nid_postfix(nullptr, 3) == PosixEinval);

    Require(scePthreadMutexattrSetprotocol(nullptr, PrioNone) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(nullptr, PrioInherit) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(nullptr, PrioProtect) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(nullptr, -1) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(nullptr, 3) == SceKernelErrorEinval);

    Require(pthread_mutexattr_getprotocol_nid_postfix(nullptr, &protocol) == PosixEinval);
    Require(scePthreadMutexattrGetprotocol(nullptr, &protocol) == SceKernelErrorEinval);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadMutexattrGetprotocol(&attr, nullptr) == SceKernelErrorEfault);
    Require(pthread_mutexattr_getprotocol_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(scePthreadMutexattrGetprotocol(nullptr, nullptr) == SceKernelErrorEinval);

    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PrioNone) == PosixEinval);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == PosixEinval);
    Require(scePthreadMutexattrSetprotocol(&attr, PrioNone) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprotocol(&attr, &protocol) == SceKernelErrorEinval);
}

void TestPriorityCeilingAttributeLimits() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);

    int ceiling = -1;
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == PosixEinval);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == PosixEinval);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SceKernelErrorEinval);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PrioInherit) == 0);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == PosixEinval);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == PosixEinval);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SceKernelErrorEinval);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PrioProtect) == 0);
    ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == 767);
    ceiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0 && ceiling == 767);

    const int belowMinCeilings[] = {-500, -100, -1, 0, 1, 50, 100, 200, 254, 255};
    for (const int invalidCeiling : belowMinCeilings) {
        Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, invalidCeiling) == PosixEinval);
        Require(scePthreadMutexattrSetprioceiling(&attr, invalidCeiling) == SceKernelErrorEinval);
    }

    const int aboveMaxCeilings[] = {768, 769, 800, 1000, 5000, 65535, 100000};
    for (const int invalidCeiling : aboveMaxCeilings) {
        Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, invalidCeiling) == PosixEinval);
        Require(scePthreadMutexattrSetprioceiling(&attr, invalidCeiling) == SceKernelErrorEinval);
    }

    const int validCeilings[] = {256, 257, 300, 450, 512, 600, 700, 766, 767};
    for (const int validCeiling : validCeilings) {
        Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, validCeiling) == 0);
        ceiling = -1;
        Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == validCeiling);
        ceiling = -1;
        Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0 && ceiling == validCeiling);
    }

    Require(scePthreadMutexattrSetprioceiling(&attr, 256) == 0);
    ceiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0 && ceiling == 256);
    Require(scePthreadMutexattrSetprioceiling(&attr, 767) == 0);
    ceiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0 && ceiling == 767);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 400) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PrioProtect) == 0);
    ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0 && ceiling == 767);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(nullptr, 300) == PosixEinval);
    Require(scePthreadMutexattrSetprioceiling(nullptr, 300) == SceKernelErrorEinval);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(nullptr, &ceiling) == PosixEinval);
    Require(scePthreadMutexattrGetprioceiling(nullptr, &ceiling) == SceKernelErrorEinval);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadMutexattrGetprioceiling(&attr, nullptr) == SceKernelErrorEfault);

    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == PosixEinval);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == PosixEinval);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SceKernelErrorEinval);
}

void TestPriorityCeilingMutexLimits() {
    PthreadMutex plainMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&plainMutex, nullptr) == 0);

    int ceiling = -1;
    int oldCeiling = -1;
    Require(pthread_mutex_getprioceiling_nid_postfix(&plainMutex, &ceiling) == PosixEinval && ceiling == -1);
    Require(scePthreadMutexGetprioceiling(&plainMutex, &ceiling) == SceKernelErrorEinval && ceiling == -1);
    Require(pthread_mutex_setprioceiling_nid_postfix(&plainMutex, 300, &oldCeiling) == PosixEinval && oldCeiling == -1);
    Require(scePthreadMutexSetprioceiling(&plainMutex, 300, &oldCeiling) == SceKernelErrorEinval && oldCeiling == -1);
    Require(pthread_mutex_destroy_nid_postfix(&plainMutex) == 0);

    PthreadMutexattr prioAttr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&prioAttr) == 0);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&prioAttr, PrioProtect) == 0);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&prioAttr, 350) == 0);

    PthreadMutex prioMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&prioMutex, &prioAttr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&prioAttr) == 0);

    ceiling = -1;
    Require(pthread_mutex_getprioceiling_nid_postfix(&prioMutex, &ceiling) == 0 && ceiling == 350);
    ceiling = -1;
    Require(scePthreadMutexGetprioceiling(&prioMutex, &ceiling) == 0 && ceiling == 350);

    oldCeiling = -1;
    Require(pthread_mutex_setprioceiling_nid_postfix(&prioMutex, 500, &oldCeiling) == 0 && oldCeiling == 350);
    ceiling = -1;
    Require(pthread_mutex_getprioceiling_nid_postfix(&prioMutex, &ceiling) == 0 && ceiling == 500);

    Require(pthread_mutex_setprioceiling_nid_postfix(&prioMutex, 600, nullptr) == 0);
    ceiling = -1;
    Require(pthread_mutex_getprioceiling_nid_postfix(&prioMutex, &ceiling) == 0 && ceiling == 600);

    oldCeiling = -1;
    Require(scePthreadMutexSetprioceiling(&prioMutex, 700, &oldCeiling) == 0 && oldCeiling == 600);
    ceiling = -1;
    Require(scePthreadMutexGetprioceiling(&prioMutex, &ceiling) == 0 && ceiling == 700);

    Require(Throws([&] { pthread_mutex_setprioceiling_nid_postfix(&prioMutex, 255, &oldCeiling); }));
    Require(Throws([&] { pthread_mutex_setprioceiling_nid_postfix(&prioMutex, 768, &oldCeiling); }));
    Require(Throws([&] { pthread_mutex_setprioceiling_nid_postfix(&prioMutex, -1, &oldCeiling); }));
    Require(Throws([&] { pthread_mutex_setprioceiling_nid_postfix(&prioMutex, 1000, &oldCeiling); }));
    Require(Throws([&] { scePthreadMutexSetprioceiling(&prioMutex, 255, &oldCeiling); }));
    Require(Throws([&] { scePthreadMutexSetprioceiling(&prioMutex, 768, &oldCeiling); }));

    Require(Throws([&] { pthread_mutex_getprioceiling_nid_postfix(&prioMutex, nullptr); }));
    Require(Throws([&] { scePthreadMutexGetprioceiling(&prioMutex, nullptr); }));
    Require(Throws([&] { pthread_mutex_getprioceiling_nid_postfix(nullptr, &ceiling); }));
    Require(Throws([&] { pthread_mutex_setprioceiling_nid_postfix(nullptr, 300, &oldCeiling); }));
    Require(Throws([&] { scePthreadMutexGetprioceiling(nullptr, &ceiling); }));
    Require(Throws([&] { scePthreadMutexSetprioceiling(nullptr, 300, &oldCeiling); }));

    Require(pthread_mutex_lock_nid_postfix(&prioMutex) == 0);
    Require(pthread_mutex_setprioceiling_nid_postfix(&prioMutex, 256, &oldCeiling) == 0 && oldCeiling == 700);
    ceiling = -1;
    Require(pthread_mutex_getprioceiling_nid_postfix(&prioMutex, &ceiling) == 0 && ceiling == 256);
    Require(pthread_mutex_unlock_nid_postfix(&prioMutex) == 0);

    Require(pthread_mutex_destroy_nid_postfix(&prioMutex) == 0);
    Require(pthread_mutex_getprioceiling_nid_postfix(&prioMutex, &ceiling) == PosixEinval);
    Require(scePthreadMutexGetprioceiling(&prioMutex, &ceiling) == SceKernelErrorEinval);
}

void TestRobustMutexAndUmtxOperations() {
    UserMutex robustMutex{};
    robustMutex.flags = UmutexRobust;

    Require(Throws([&] { _umtx_op_nid_postfix(&robustMutex, UmtxOpMutexTrylock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(&robustMutex, UmtxOpMutexLock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(&robustMutex, UmtxOpMutexWait, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(&robustMutex, UmtxOpMutexUnlock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(&robustMutex, UmtxOpMutexWake, 0, nullptr, nullptr); }));

    UserMutex robustPrioMutex{};
    robustPrioMutex.flags = UmutexRobust | UmutexPrioProtect;
    Require(Throws([&] { _umtx_op_nid_postfix(&robustPrioMutex, UmtxOpMutexTrylock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(&robustPrioMutex, UmtxOpMutexLock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(&robustPrioMutex, UmtxOpMutexUnlock, 0, nullptr, nullptr); }));

    const int robustListOperations[] = {UmtxOpRobustLists, 23, 24, 25, 26};
    for (const int operation : robustListOperations) {
        Require(Throws([&] { _umtx_op_nid_postfix(&robustMutex, operation, 0, nullptr, nullptr); }));
        Require(Throws([&] { _umtx_op_nid_postfix(nullptr, operation, 0, nullptr, nullptr); }));
    }

    UserMutex prioUmutex{};
    prioUmutex.flags = UmutexPrioProtect;
    Require(Throws([&] { _umtx_op_nid_postfix(&prioUmutex, UmtxOpMutexUnlock, 0, nullptr, nullptr); }));

    *__error_nid_postfix() = 0;
    Require(_umtx_op_nid_postfix(&prioUmutex, UmtxOpMutexTrylock, 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(_umtx_op_nid_postfix(&prioUmutex, UmtxOpMutexLock, 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(_umtx_op_nid_postfix(nullptr, UmtxOpSetCeiling, 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    UserMutex plainUmutex{};
    plainUmutex.flags = 0;
    *__error_nid_postfix() = 0;
    Require(_umtx_op_nid_postfix(&plainUmutex, UmtxOpSetCeiling, 0, nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    Require(Throws([&] { _umtx_op_nid_postfix(&prioUmutex, UmtxOpSetCeiling, 0, nullptr, nullptr); }));

    UserMutex misalignedUmutex{};
    misalignedUmutex.flags = UmutexRobust;
    auto* unaligned = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(&misalignedUmutex) + 1);
    Require(Throws([&] { _umtx_op_nid_postfix(unaligned, UmtxOpMutexTrylock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(unaligned, UmtxOpMutexLock, 0, nullptr, nullptr); }));
    Require(Throws([&] { _umtx_op_nid_postfix(unaligned, UmtxOpMutexUnlock, 0, nullptr, nullptr); }));
}

void TestMutexDestructionAndBusyStates() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(scePthreadMutexDestroy(&mutex) == 0);

    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEinval);
    Require(scePthreadMutexDestroy(&mutex) == SceKernelErrorEinval);

    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEbusy);
    Require(scePthreadMutexDestroy(&mutex) == SceKernelErrorEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);

    PthreadMutex sceMutex = nullptr;
    Require(scePthreadMutexInit(&sceMutex, nullptr, nullptr) == 0);
    Require(scePthreadMutexLock(&sceMutex) == 0);
    Require(scePthreadMutexDestroy(&sceMutex) == SceKernelErrorEbusy);
    Require(pthread_mutex_destroy_nid_postfix(&sceMutex) == PosixEbusy);
    Require(scePthreadMutexUnlock(&sceMutex) == 0);
    Require(scePthreadMutexDestroy(&sceMutex) == 0);
}

} // namespace

int main() {
    TestMutexAttributeProtocols();
    TestPriorityCeilingAttributeLimits();
    TestPriorityCeilingMutexLimits();
    TestRobustMutexAndUmtxOperations();
    TestMutexDestructionAndBusyStates();
    return 0;
}
