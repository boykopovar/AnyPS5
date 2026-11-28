#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;

constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000Eu);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);

constexpr int PrioNone = 0;
constexpr int PrioInherit = 1;
constexpr int PrioProtect = 2;

constexpr int DefaultCeiling = 767;
constexpr int MinCeiling = 256;
constexpr int MaxCeiling = 767;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

void TestProtocolValidationAndBounds() {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == 0);
    Require(attr != nullptr);

    int protocol = -1;
    Require(scePthreadMutexattrGetprotocol(&attr, &protocol) == 0);
    Require(protocol == PrioNone);

    const int validProtocols[] = {PrioNone, PrioInherit, PrioProtect};
    for (const int validProtocol : validProtocols) {
        Require(scePthreadMutexattrSetprotocol(&attr, validProtocol) == 0);
        protocol = -1;
        Require(scePthreadMutexattrGetprotocol(&attr, &protocol) == 0);
        Require(protocol == validProtocol);
    }

    const int invalidProtocols[] = {-1000, -50, -2, -1, 3, 4, 10, 100, 500};
    for (const int invalidProtocol : invalidProtocols) {
        Require(scePthreadMutexattrSetprotocol(&attr, invalidProtocol) == SceKernelErrorEinval);
    }

    protocol = -1;
    Require(scePthreadMutexattrGetprotocol(&attr, &protocol) == 0);
    Require(protocol == PrioProtect);

    Require(scePthreadMutexattrDestroy(&attr) == 0);
    Require(attr == nullptr);
}

void TestPriorityCeilingBoundsUnderProtect() {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == 0);
    Require(attr != nullptr);

    Require(scePthreadMutexattrSetprotocol(&attr, PrioProtect) == 0);

    int ceiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0);
    Require(ceiling == DefaultCeiling);

    const int validCeilings[] = {MinCeiling, 257, 300, 400, 512, 600, 700, 766, MaxCeiling};
    for (const int validCeiling : validCeilings) {
        Require(scePthreadMutexattrSetprioceiling(&attr, validCeiling) == 0);
        ceiling = -1;
        Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0);
        Require(ceiling == validCeiling);
    }

    const int invalidCeilings[] = {-1000, -100, -1, 0, 1, 100, 200, 254, 255, 768, 769, 800, 1000, 65535};
    for (const int invalidCeiling : invalidCeilings) {
        Require(scePthreadMutexattrSetprioceiling(&attr, invalidCeiling) == SceKernelErrorEinval);
    }

    ceiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0);
    Require(ceiling == MaxCeiling);

    Require(scePthreadMutexattrDestroy(&attr) == 0);
    Require(attr == nullptr);
}

void TestCeilingResetAndNonProtectRejection() {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == 0);
    Require(attr != nullptr);

    int ceiling = -1;
    Require(scePthreadMutexattrSetprioceiling(&attr, 500) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SceKernelErrorEinval);

    Require(scePthreadMutexattrSetprotocol(&attr, PrioInherit) == 0);
    Require(scePthreadMutexattrSetprioceiling(&attr, 500) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SceKernelErrorEinval);

    Require(scePthreadMutexattrSetprotocol(&attr, PrioProtect) == 0);
    ceiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0);
    Require(ceiling == DefaultCeiling);

    Require(scePthreadMutexattrSetprioceiling(&attr, 350) == 0);
    ceiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0);
    Require(ceiling == 350);

    Require(scePthreadMutexattrSetprotocol(&attr, PrioNone) == 0);
    Require(scePthreadMutexattrSetprioceiling(&attr, 350) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SceKernelErrorEinval);

    Require(scePthreadMutexattrSetprotocol(&attr, PrioProtect) == 0);
    ceiling = -1;
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == 0);
    Require(ceiling == DefaultCeiling);

    Require(scePthreadMutexattrDestroy(&attr) == 0);
    Require(attr == nullptr);
}

void TestNullAndUninitializedAttributeHandling() {
    int protocol = -1;
    int ceiling = -1;

    Require(scePthreadMutexattrSetprotocol(nullptr, PrioNone) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(nullptr, PrioProtect) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(nullptr, -1) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprotocol(nullptr, &protocol) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprioceiling(nullptr, 500) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(nullptr, &ceiling) == SceKernelErrorEinval);

    PthreadMutexattr uninit = nullptr;
    Require(scePthreadMutexattrSetprotocol(&uninit, PrioNone) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(&uninit, PrioProtect) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(&uninit, -1) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprotocol(&uninit, &protocol) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprioceiling(&uninit, 500) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&uninit, &ceiling) == SceKernelErrorEinval);
    Require(scePthreadMutexattrDestroy(&uninit) == SceKernelErrorEinval);

    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == 0);
    Require(attr != nullptr);
    Require(scePthreadMutexattrDestroy(&attr) == 0);
    Require(attr == nullptr);

    Require(scePthreadMutexattrSetprotocol(&attr, PrioNone) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprotocol(&attr, PrioProtect) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprotocol(&attr, &protocol) == SceKernelErrorEinval);
    Require(scePthreadMutexattrSetprioceiling(&attr, 500) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceiling) == SceKernelErrorEinval);
    Require(scePthreadMutexattrDestroy(&attr) == SceKernelErrorEinval);
}

void TestFaultPointerPrecedence() {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == 0);
    Require(attr != nullptr);

    Require(scePthreadMutexattrGetprotocol(&attr, nullptr) == SceKernelErrorEfault);

    Require(scePthreadMutexattrGetprioceiling(&attr, nullptr) == SceKernelErrorEinval);

    Require(scePthreadMutexattrSetprotocol(&attr, PrioInherit) == 0);
    Require(scePthreadMutexattrGetprioceiling(&attr, nullptr) == SceKernelErrorEinval);

    Require(scePthreadMutexattrSetprotocol(&attr, PrioProtect) == 0);
    Require(scePthreadMutexattrGetprioceiling(&attr, nullptr) == SceKernelErrorEfault);

    Require(scePthreadMutexattrGetprotocol(nullptr, nullptr) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(nullptr, nullptr) == SceKernelErrorEinval);

    PthreadMutexattr uninit = nullptr;
    Require(scePthreadMutexattrGetprotocol(&uninit, nullptr) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&uninit, nullptr) == SceKernelErrorEinval);

    Require(scePthreadMutexattrDestroy(&attr) == 0);
    Require(attr == nullptr);

    Require(scePthreadMutexattrGetprotocol(&attr, nullptr) == SceKernelErrorEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, nullptr) == SceKernelErrorEinval);
}

void TestPosixParityAndConsistency() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    int protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0);
    Require(protocol == PrioNone);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PrioProtect) == 0);
    protocol = -1;
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == 0);
    Require(protocol == PrioProtect);

    int ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
    Require(ceiling == DefaultCeiling);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 450) == 0);
    ceiling = -1;
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == 0);
    Require(ceiling == 450);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 255) == PosixEinval);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 768) == PosixEinval);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, 3) == PosixEinval);
    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, -1) == PosixEinval);

    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, nullptr) == PosixEfault);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PrioNone) == 0);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, nullptr) == PosixEinval);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 450) == PosixEinval);

    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == nullptr);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, PrioProtect) == PosixEinval);
    Require(pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol) == PosixEinval);
    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 450) == PosixEinval);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceiling) == PosixEinval);
}

} // namespace

int main() {
    TestProtocolValidationAndBounds();
    TestPriorityCeilingBoundsUnderProtect();
    TestCeilingResetAndNonProtectRejection();
    TestNullAndUninitializedAttributeHandling();
    TestFaultPointerPrecedence();
    TestPosixParityAndConsistency();
    return 0;
}
