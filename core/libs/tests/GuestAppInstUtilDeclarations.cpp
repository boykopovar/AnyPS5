#include "prx/libSceAppInstUtil/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/LibcxxMutexForwards.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "assertion failed at %s:%d\n", location.file_name(), location.line());
    std::abort();
}

void RequireThrows(auto function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    std::fprintf(stderr, "expected a throw from %s:%d\n", location.file_name(), location.line());
    std::abort();
}

struct MockMutexState {
    int attrInitCalls = 0;
    int attrSettypeCalls = 0;
    int attrSettypeType = 0;
    int attrSettypeResult = 0;
    int attrDestroyCalls = 0;
    int initCalls = 0;
    int destroyCalls = 0;
    int lockCalls = 0;
    int trylockCalls = 0;
    int trylockResult = 0;
    int unlockCalls = 0;
};

MockMutexState mockState{};

void ResetMockState(int attrSettypeResult = 0, int trylockResult = 0) {
    mockState = {};
    mockState.attrSettypeResult = attrSettypeResult;
    mockState.trylockResult = trylockResult;
}

int TotalMockCalls() {
    return mockState.attrInitCalls + mockState.attrSettypeCalls + mockState.attrDestroyCalls +
           mockState.initCalls + mockState.destroyCalls + mockState.lockCalls +
           mockState.trylockCalls + mockState.unlockCalls;
}

int APS5_VABI MockMutexAttrInit(PthreadMutexattr* attr) {
    ++mockState.attrInitCalls;
    if (attr) *attr = reinterpret_cast<PthreadMutexattr>(std::uintptr_t{0x1001});
    return 0;
}

int APS5_VABI MockMutexAttrSettype(PthreadMutexattr* attr, int type) {
    ++mockState.attrSettypeCalls;
    static_cast<void>(attr);
    mockState.attrSettypeType = type;
    return mockState.attrSettypeResult;
}

int APS5_VABI MockMutexAttrDestroy(PthreadMutexattr* attr) {
    ++mockState.attrDestroyCalls;
    if (attr) *attr = nullptr;
    return 0;
}

int APS5_VABI MockMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    ++mockState.initCalls;
    Require(attr != nullptr && *attr != nullptr);
    if (mutex) *mutex = reinterpret_cast<PthreadMutex>(std::uintptr_t{0x2001});
    return 0;
}

int APS5_VABI MockMutexDestroy(PthreadMutex* mutex) {
    ++mockState.destroyCalls;
    if (mutex) *mutex = nullptr;
    return 0;
}

int APS5_VABI MockMutexLock(PthreadMutex* mutex) {
    ++mockState.lockCalls;
    static_cast<void>(mutex);
    return 0;
}

int APS5_VABI MockMutexTrylock(PthreadMutex* mutex) {
    ++mockState.trylockCalls;
    static_cast<void>(mutex);
    return mockState.trylockResult;
}

int APS5_VABI MockMutexUnlock(PthreadMutex* mutex) {
    ++mockState.unlockCalls;
    static_cast<void>(mutex);
    return 0;
}

const LibcxxMutexForwards_nid_no_patch CompleteMockForwards{
    MockMutexAttrInit,
    MockMutexAttrSettype,
    MockMutexAttrDestroy,
    MockMutexInit,
    MockMutexDestroy,
    MockMutexLock,
    MockMutexTrylock,
    MockMutexUnlock,
};

const LibcxxMutexForwards_nid_no_patch DefaultKernelForwards{
    pthread_mutexattr_init_nid_postfix,
    pthread_mutexattr_settype_nid_postfix,
    pthread_mutexattr_destroy_nid_postfix,
    pthread_mutex_init_nid_postfix,
    pthread_mutex_destroy_nid_postfix,
    pthread_mutex_lock_nid_postfix,
    pthread_mutex_trylock_nid_postfix,
    pthread_mutex_unlock_nid_postfix,
};

struct LibcxxMutexForwardsRestoreGuard {
    ~LibcxxMutexForwardsRestoreGuard() { RegisterLibcxxMutexForwards_nid_no_patch(&DefaultKernelForwards); }
};

void AssertAllRecursiveMutexCallsThrow(PthreadMutex* mutex) {
    ResetMockState();
    RequireThrows([&] { _ZNSt3__115recursive_mutexC1Ev_nid_postfix(mutex); });
    RequireThrows([&] { _ZNSt3__115recursive_mutex4lockEv_nid_postfix(mutex); });
    RequireThrows([&] { static_cast<void>(_ZNSt3__115recursive_mutex8try_lockEv_nid_postfix(mutex)); });
    RequireThrows([&] { _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(mutex); });
    RequireThrows([&] { _ZNSt3__115recursive_mutexD1Ev_nid_postfix(mutex); });
    Require(TotalMockCalls() == 0);
}

void TestLibcxxMutexForwardsValidation() {
    LibcxxMutexForwardsRestoreGuard restoreGuard;
    PthreadMutex mutex = nullptr;

    RegisterLibcxxMutexForwards_nid_no_patch(nullptr);
    AssertAllRecursiveMutexCallsThrow(&mutex);

    LibcxxMutexForwards_nid_no_patch partialForwards[] = {
        {nullptr, MockMutexAttrSettype, MockMutexAttrDestroy, MockMutexInit, MockMutexDestroy, MockMutexLock, MockMutexTrylock, MockMutexUnlock},
        {MockMutexAttrInit, nullptr, MockMutexAttrDestroy, MockMutexInit, MockMutexDestroy, MockMutexLock, MockMutexTrylock, MockMutexUnlock},
        {MockMutexAttrInit, MockMutexAttrSettype, nullptr, MockMutexInit, MockMutexDestroy, MockMutexLock, MockMutexTrylock, MockMutexUnlock},
        {MockMutexAttrInit, MockMutexAttrSettype, MockMutexAttrDestroy, nullptr, MockMutexDestroy, MockMutexLock, MockMutexTrylock, MockMutexUnlock},
        {MockMutexAttrInit, MockMutexAttrSettype, MockMutexAttrDestroy, MockMutexInit, nullptr, MockMutexLock, MockMutexTrylock, MockMutexUnlock},
        {MockMutexAttrInit, MockMutexAttrSettype, MockMutexAttrDestroy, MockMutexInit, MockMutexDestroy, nullptr, MockMutexTrylock, MockMutexUnlock},
        {MockMutexAttrInit, MockMutexAttrSettype, MockMutexAttrDestroy, MockMutexInit, MockMutexDestroy, MockMutexLock, nullptr, MockMutexUnlock},
        {MockMutexAttrInit, MockMutexAttrSettype, MockMutexAttrDestroy, MockMutexInit, MockMutexDestroy, MockMutexLock, MockMutexTrylock, nullptr},
    };
    for (const auto& forwards : partialForwards) {
        RegisterLibcxxMutexForwards_nid_no_patch(&forwards);
        AssertAllRecursiveMutexCallsThrow(&mutex);
    }

    RegisterLibcxxMutexForwards_nid_no_patch(&CompleteMockForwards);
    ResetMockState();
    _ZNSt3__115recursive_mutexC1Ev_nid_postfix(&mutex);
    Require(mockState.attrInitCalls == 1);
    Require(mockState.attrSettypeCalls == 1);
    Require(mockState.attrSettypeType == 2);
    Require(mockState.attrDestroyCalls == 1);
    Require(mockState.initCalls == 1);
    Require(mutex != nullptr);

    _ZNSt3__115recursive_mutex4lockEv_nid_postfix(&mutex);
    Require(mockState.lockCalls == 1);
    Require(_ZNSt3__115recursive_mutex8try_lockEv_nid_postfix(&mutex));
    mockState.trylockResult = 16;
    Require(!_ZNSt3__115recursive_mutex8try_lockEv_nid_postfix(&mutex));
    Require(mockState.trylockCalls == 2);
    _ZNSt3__115recursive_mutex6unlockEv_nid_postfix(&mutex);
    Require(mockState.unlockCalls == 1);
    _ZNSt3__115recursive_mutexD1Ev_nid_postfix(&mutex);
    Require(mockState.destroyCalls == 1);
    Require(mutex == nullptr);

    ResetMockState(22, 0);
    RequireThrows([&] { _ZNSt3__115recursive_mutexC1Ev_nid_postfix(&mutex); });
    Require(mockState.attrInitCalls == 1);
    Require(mockState.attrSettypeCalls == 1);
    Require(mockState.attrDestroyCalls == 1);
    Require(mockState.initCalls == 0);
}

}

int main() {
    RequireThrows([] { sceAppInstUtilAppPrepareOverwritePkg("package:/test/overwrite.pkg"); });
    RequireThrows([] { sceAppInstUtilAppPrepareOverwritePkg(nullptr); });
    RequireThrows([] { sceAppInstUtilAppRecoverApp("PPSA00000"); });
    RequireThrows([] { sceAppInstUtilAppRecoverApp(nullptr); });
    RequireThrows([] { sceAppInstUtilAppUnInstallTheme("PPSA00000-THME00001"); });
    RequireThrows([] { sceAppInstUtilAppUnInstallTheme(nullptr); });
    TestLibcxxMutexForwardsValidation();
    return 0;
}
