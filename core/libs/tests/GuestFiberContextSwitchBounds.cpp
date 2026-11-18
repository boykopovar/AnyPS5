#include "prx/libSceFiber/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <xmmintrin.h>

namespace {

constexpr std::int32_t SCE_OK = 0;
constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t FiberErrorAlignment = static_cast<std::int32_t>(0x80590002);
constexpr std::int32_t FiberErrorRange = static_cast<std::int32_t>(0x80590003);
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t FiberErrorPermission = static_cast<std::int32_t>(0x80590005);
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

alignas(16) std::uint8_t g_stackA[16384];
alignas(16) std::uint8_t g_stackB[16384];
alignas(16) std::uint8_t g_stackC[16384];
alignas(16) std::uint8_t g_dynStack[16384];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
alignas(8) std::uint8_t g_fiberStorageC[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);
FiberObject* const g_fiberC = reinterpret_cast<FiberObject*>(g_fiberStorageC);

void APS5_VABI DummyEntry(std::uint64_t, std::uint64_t) {
    Require(sceFiberReturnToThread(0, nullptr) == SCE_OK);
}

void APS5_VABI TargetValidationEntry(std::uint64_t, std::uint64_t) {
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    auto* uninit = reinterpret_cast<FiberObject*>(uninitStorage);
    Require(sceFiberSwitch(uninit, 0, nullptr) == FiberErrorInvalid);

    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorState);

    Require(_sceFiberAttachContextAndSwitch(nullptr, g_dynStack, sizeof(g_dynStack), 0, nullptr) == FiberErrorNull);
    Require(_sceFiberAttachContextAndSwitch(uninit, g_dynStack, sizeof(g_dynStack), 0, nullptr) == FiberErrorInvalid);
    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_dynStack, sizeof(g_dynStack), 0, nullptr) == FiberErrorState);

    Require(_sceFiberAttachContextAndSwitch(g_fiberB, nullptr, sizeof(g_dynStack), 0, nullptr) == FiberErrorInvalid);
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_dynStack + 1, sizeof(g_dynStack), 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_dynStack + 8, sizeof(g_dynStack), 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_dynStack, sizeof(g_dynStack) - 4, 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_dynStack, 256, 0, nullptr) == FiberErrorRange);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "with_ctx", DummyEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_dynStack, sizeof(g_dynStack), 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);

    alignas(8) std::uint8_t destroyedStorage[256] = {};
    auto* destroyed = reinterpret_cast<FiberObject*>(destroyedStorage);
    Require(_sceFiberInitializeImpl_nid_postfix(destroyed, "destroyed", DummyEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(destroyed) == SCE_OK);
    Require(sceFiberSwitch(destroyed, 0, nullptr) == FiberErrorInvalid);

    Require(sceFiberReturnToThread(777, nullptr) == SCE_OK);
}

void APS5_VABI PingPongEntryB(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 2000);
    Require(argOnRun == 100);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberA, 101, &received) == SCE_OK);
    Require(received == 102);

    Require(sceFiberSwitch(g_fiberA, 103, &received) == SCE_OK);
    Require(received == 104);

    Require(sceFiberSwitch(g_fiberA, 105, nullptr) == SCE_OK);

    Require(false);
}

void APS5_VABI PingPongEntryA(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 1000);
    Require(argOnRun == 50);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberB, 100, &received) == SCE_OK);
    Require(received == 101);

    Require(sceFiberSwitch(g_fiberB, 102, &received) == SCE_OK);
    Require(received == 103);

    Require(sceFiberSwitch(g_fiberB, 104, &received) == SCE_OK);
    Require(received == 105);

    Require(sceFiberReturnToThread(888, nullptr) == SCE_OK);
}

void APS5_VABI RingEntryC(std::uint64_t, std::uint64_t argOnRun) {
    Require(argOnRun == 1002);
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberC);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberA, 1003, &received) == SCE_OK);
    Require(received == 2002);

    Require(sceFiberSwitch(g_fiberA, 2003, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI RingEntryB(std::uint64_t, std::uint64_t argOnRun) {
    Require(argOnRun == 1001);
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberC, 1002, &received) == SCE_OK);
    Require(received == 2001);

    Require(sceFiberSwitch(g_fiberC, 2002, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI RingEntryA(std::uint64_t, std::uint64_t argOnRun) {
    Require(argOnRun == 1000);
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberB, 1001, &received) == SCE_OK);
    Require(received == 1003);

    Require(sceFiberSwitch(g_fiberB, 2001, &received) == SCE_OK);
    Require(received == 2003);

    Require(sceFiberReturnToThread(999, nullptr) == SCE_OK);
}

void APS5_VABI RegisterCanaryEntryB(std::uint64_t, std::uint64_t) {
    volatile std::uint64_t otherCanary = 0xabcdef0123456789ull;
    volatile double otherFloat = 9876.54321;
    (void)otherCanary;
    (void)otherFloat;

    const unsigned int currentMxcsr = _mm_getcsr();
    _mm_setcsr(currentMxcsr & ~0x8000u);

    Require(sceFiberSwitch(g_fiberA, 555, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI RegisterCanaryEntryA(std::uint64_t, std::uint64_t) {
    volatile std::uint64_t canary1 = 0x0123456789abcdefull;
    volatile std::uint64_t canary2 = 0xfedcba9876543210ull;
    volatile std::uint64_t canary3 = 0xa5a5a5a55a5a5a5aull;
    volatile double floatCanary = 3.141592653589793;

    const unsigned int initialMxcsr = _mm_getcsr();
    _mm_setcsr(initialMxcsr | 0x8000u);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberB, 444, &received) == SCE_OK);
    Require(received == 555);

    Require(canary1 == 0x0123456789abcdefull);
    Require(canary2 == 0xfedcba9876543210ull);
    Require(canary3 == 0xa5a5a5a55a5a5a5aull);
    Require(floatCanary == 3.141592653589793);
    Require((_mm_getcsr() & 0x8000u) != 0);

    _mm_setcsr(initialMxcsr);
    Require(sceFiberReturnToThread(111, nullptr) == SCE_OK);
}

void APS5_VABI DynAttachEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 3333);
    Require(argOnRun == 4444);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberB, &info) == SCE_OK);
    Require(info.addr_context == g_dynStack);
    Require(info.size_context == sizeof(g_dynStack));

    Require(sceFiberSwitch(g_fiberA, 5555, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI DynAttachCallerEntry(std::uint64_t, std::uint64_t) {
    std::uint64_t received = 0;
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_dynStack, sizeof(g_dynStack), 4444, &received) == SCE_OK);
    Require(received == 5555);

    Require(sceFiberReturnToThread(6666, nullptr) == SCE_OK);
}

void APS5_VABI MigrationEntryB(std::uint64_t, std::uint64_t argOnRun) {
    Require(argOnRun == 800);
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    Require(sceFiberSwitch(g_fiberA, 801, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI MigrationEntryA(std::uint64_t, std::uint64_t argOnRun) {
    Require(argOnRun == 700);

    std::uint64_t nextArg = 0;
    Require(sceFiberReturnToThread(701, &nextArg) == SCE_OK);
    Require(nextArg == 702);

    std::uint64_t fromB = 0;
    Require(sceFiberSwitch(g_fiberB, 800, &fromB) == SCE_OK);
    Require(fromB == 801);

    Require(sceFiberReturnToThread(703, nullptr) == SCE_OK);
}

void TestSwitchCallingContextValidation() {
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorPermission);
    Require(_sceFiberAttachContextAndSwitch(nullptr, g_dynStack, sizeof(g_dynStack), 0, nullptr) == FiberErrorNull);
    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_dynStack, sizeof(g_dynStack), 0, nullptr) == FiberErrorPermission);
    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission);

    Require(sceFiberGetSelf(nullptr) == FiberErrorNull);
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == FiberErrorPermission);

    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
}

void TestSwitchTargetValidation() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "target_val", TargetValidationEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 777);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestCooperativePingPongSwitch() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "ping_a", PingPongEntryA, 1000, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "ping_b", PingPongEntryB, 2000, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 50, &returned) == SCE_OK);
    Require(returned == 888);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

void TestMultiFiberRingSwitch() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "ring_a", RingEntryA, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "ring_b", RingEntryB, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberC, "ring_c", RingEntryC, 0, g_stackC, sizeof(g_stackC), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 1000, &returned) == SCE_OK);
    Require(returned == 999);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberFinalize(g_fiberC) == SCE_OK);
}

void TestRegisterCanaryAndFpuPreservation() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "canary_a", RegisterCanaryEntryA, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "canary_b", RegisterCanaryEntryB, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 111);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

void TestDynamicAttachAndSwitch() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "dyn_caller", DynAttachCallerEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "dyn_callee", DynAttachEntry, 3333, nullptr, 0, nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 6666);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

void TestCrossThreadMigrationAndSwitch() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "mig_a", MigrationEntryA, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "mig_b", MigrationEntryB, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 700, &returned) == SCE_OK);
    Require(returned == 701);

    std::thread workerThread([]() {
        std::uint64_t threadReturned = 0;
        Require(sceFiberRun_nid_postfix(g_fiberA, 702, &threadReturned) == SCE_OK);
        Require(threadReturned == 703);
    });
    workerThread.join();

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

} // namespace

int main() {
    TestSwitchCallingContextValidation();
    TestSwitchTargetValidation();
    TestCooperativePingPongSwitch();
    TestMultiFiberRingSwitch();
    TestRegisterCanaryAndFpuPreservation();
    TestDynamicAttachAndSwitch();
    TestCrossThreadMigrationAndSwitch();
    return 0;
}
