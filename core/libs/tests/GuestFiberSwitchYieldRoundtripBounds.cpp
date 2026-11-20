#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::int32_t SCE_OK = 0;
constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
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

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
alignas(8) std::uint8_t g_fiberStorageC[256];
alignas(8) std::uint8_t g_fiberStorageDead[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);
FiberObject* const g_fiberC = reinterpret_cast<FiberObject*>(g_fiberStorageC);
FiberObject* const g_fiberDead = reinterpret_cast<FiberObject*>(g_fiberStorageDead);

void APS5_VABI DeadDummyEntry(std::uint64_t, std::uint64_t) {
    Require(sceFiberReturnToThread(999, nullptr) == SCE_OK);
}

void APS5_VABI SelfSwitchTestEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 11);
    Require(argOnRun == 22);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorState);
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberSwitch(reinterpret_cast<FiberObject*>(uninitStorage), 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberSwitch(g_fiberDead, 0, nullptr) == FiberErrorInvalid);

    Require(sceFiberReturnToThread(777, nullptr) == SCE_OK);
}

void APS5_VABI ChainFiberAEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 100);
    Require(argOnRun == 101);

    volatile double preservedFloat = 2.718281828;
    volatile std::uint64_t preservedInt = 0xabcdef0123456789ull;

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberB, 202, &received) == SCE_OK);
    Require(received == 404);
    Require(preservedFloat == 2.718281828);
    Require(preservedInt == 0xabcdef0123456789ull);

    std::uint64_t fromHost = 0;
    Require(sceFiberReturnToThread(505, &fromHost) == SCE_OK);
    Require(fromHost == 606);

    Require(sceFiberReturnToThread(707, nullptr) == SCE_OK);
}

void APS5_VABI ChainFiberBEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 200);
    Require(argOnRun == 202);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberC, 303, &received) == SCE_OK);
    Require(false);
}

void APS5_VABI ChainFiberCEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 300);
    Require(argOnRun == 303);

    Require(sceFiberSwitch(g_fiberA, 404, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI YieldLoopEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 10);
    std::uint64_t current = argOnRun;

    for (std::uint64_t i = 0; i < 5; ++i) {
        Require(current == 100 + i);
        std::uint64_t nextInput = 0;
        Require(sceFiberReturnToThread(200 + i, &nextInput) == SCE_OK);
        current = nextInput;
    }

    Require(current == 9999);
    Require(sceFiberReturnToThread(10000, nullptr) == SCE_OK);
}

void TestHostThreadCallingRestrictions() {
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorInvalid);
    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_stackA, sizeof(g_stackA), 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "perm_check", DeadDummyEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorPermission);
    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_stackA, sizeof(g_stackA), 0, nullptr) == FiberErrorPermission);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission);
}

void TestSwitchSelfAndInvalidFiberRejection() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberDead, "dead_fiber", DeadDummyEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberDead) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "self_switch", SelfSwitchTestEntry, 11, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 22, &returned) == SCE_OK);
    Require(returned == 777);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestMultiFiberSwitchChainAndArgumentRoundtrips() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "fiber_a", ChainFiberAEntry, 100, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "fiber_b", ChainFiberBEntry, 200, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberC, "fiber_c", ChainFiberCEntry, 300, g_stackC, sizeof(g_stackC), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 101, &returned) == SCE_OK);
    Require(returned == 505);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 606, &returned) == SCE_OK);
    Require(returned == 707);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberFinalize(g_fiberC) == SCE_OK);

    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberB) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberC) == FiberErrorInvalid);
}

void TestYieldLoopAndNullOutPointers() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "yield_loop", YieldLoopEntry, 10, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    for (std::uint64_t i = 0; i < 5; ++i) {
        std::uint64_t returned = 0;
        Require(sceFiberRun_nid_postfix(g_fiberA, 100 + i, &returned) == SCE_OK);
        Require(returned == 200 + i);
    }

    Require(sceFiberRun_nid_postfix(g_fiberA, 9999, nullptr) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);
}

} // namespace

int main() {
    TestHostThreadCallingRestrictions();
    TestSwitchSelfAndInvalidFiberRejection();
    TestMultiFiberSwitchChainAndArgumentRoundtrips();
    TestYieldLoopAndNullOutPointers();
    return 0;
}
