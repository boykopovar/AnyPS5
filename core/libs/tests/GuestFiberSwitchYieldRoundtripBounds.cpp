#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

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
alignas(16) std::uint8_t g_largeStack[65536];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
alignas(8) std::uint8_t g_fiberStorageC[256];
alignas(8) std::uint8_t g_fiberStorageDead[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);
FiberObject* const g_fiberC = reinterpret_cast<FiberObject*>(g_fiberStorageC);
FiberObject* const g_fiberDead = reinterpret_cast<FiberObject*>(g_fiberStorageDead);

struct MigrationConfig {
    FiberObject* fiber;
    std::uint64_t contextSize;
    const void* callerBuffer;
    bool expectCallerBuffer;
    std::uint64_t initTag;
};

void VerifyActiveFiberSelfAndBounds(const MigrationConfig& cfg, const volatile void* stackVar, void*& recordedAddrContext) {
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == cfg.fiber);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(cfg.fiber, &info) == SCE_OK);
    Require(info.size_context == cfg.contextSize);
    if (cfg.expectCallerBuffer) {
        Require(info.addr_context == cfg.callerBuffer);
    } else {
        Require(info.addr_context != cfg.callerBuffer);
    }

    const auto varAddr = reinterpret_cast<std::uintptr_t>(stackVar);
    const auto low = reinterpret_cast<std::uintptr_t>(info.addr_context);
    const auto high = low + info.size_context;
    Require(varAddr >= low && varAddr < high);

    if (recordedAddrContext == nullptr) {
        recordedAddrContext = info.addr_context;
    } else {
        Require(info.addr_context == recordedAddrContext);
    }
}

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

void APS5_VABI UnwritableSwitchTargetEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 2001);
    Require(argOnRun == 3001);

    std::uint64_t reply = 0;
    Require(sceFiberSwitch(g_fiberA, 4001, &reply) == SCE_OK);
    Require(reply == 5001);

    Require(sceFiberSwitch(g_fiberA, 6001, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI UnwritableAttachSwitchEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 7001);
    Require(argOnRun == 8001);

    std::uint64_t reply = 0;
    Require(sceFiberSwitch(g_fiberA, 9001, &reply) == SCE_OK);
    Require(reply == 9101);

    Require(sceFiberSwitch(g_fiberA, 9201, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI UnwritableMainEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 1001);
    Require(argOnRun == 1002);

    auto* const unwritableOut = reinterpret_cast<std::uint64_t*>(0x10);

    Require(sceFiberSwitch(g_fiberB, 3001, unwritableOut) == FiberErrorInvalid);
    std::uint64_t switchOut = 0;
    Require(sceFiberSwitch(g_fiberB, 5001, &switchOut) == SCE_OK);
    Require(switchOut == 6001);

    Require(_sceFiberAttachContextAndSwitch(g_fiberC, g_stackC, sizeof(g_stackC), 8001, unwritableOut) == FiberErrorInvalid);
    switchOut = 0;
    Require(sceFiberSwitch(g_fiberC, 9101, &switchOut) == SCE_OK);
    Require(switchOut == 9201);

    std::uint64_t nextRunArg = 0;
    Require(sceFiberReturnToThread(1111, &nextRunArg) == SCE_OK);
    Require(nextRunArg == 1212);

    Require(sceFiberReturnToThread(1313, unwritableOut) == FiberErrorInvalid);

    nextRunArg = 0;
    Require(sceFiberReturnToThread(1515, &nextRunArg) == SCE_OK);
    Require(nextRunArg == 1616);

    Require(sceFiberReturnToThread(1717, nullptr) == SCE_OK);
}

void APS5_VABI UnwritableAttachRunEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 2002);
    Require(argOnRun == 3002);

    std::uint64_t nextArg = 0;
    Require(sceFiberReturnToThread(4002, &nextArg) == SCE_OK);
    Require(nextArg == 5002);
    Require(sceFiberReturnToThread(6002, nullptr) == SCE_OK);
}

void APS5_VABI CrossThreadMigrationEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    const auto* cfg = reinterpret_cast<const MigrationConfig*>(static_cast<std::uintptr_t>(argOnInitialize));
    Require(cfg != nullptr);
    Require(argOnRun == cfg->initTag + 1);

    volatile std::uint64_t stackVarThread1 = 0x1122334455667788ull;
    volatile double preservedFloat = 1.6180339887;
    void* recordedAddrContext = nullptr;
    VerifyActiveFiberSelfAndBounds(*cfg, &stackVarThread1, recordedAddrContext);
    Require(stackVarThread1 == 0x1122334455667788ull);

    std::uint64_t resumedArg = 0;
    Require(sceFiberReturnToThread(cfg->initTag + 2, &resumedArg) == SCE_OK);
    Require(resumedArg == cfg->initTag + 3);
    Require(stackVarThread1 == 0x1122334455667788ull);
    Require(preservedFloat == 1.6180339887);

    volatile std::uint64_t stackVarThread2 = 0x99aabbccddeeff00ull;
    VerifyActiveFiberSelfAndBounds(*cfg, &stackVarThread2, recordedAddrContext);
    Require(stackVarThread2 == 0x99aabbccddeeff00ull);

    std::uint64_t finalArg = 0;
    Require(sceFiberReturnToThread(cfg->initTag + 4, &finalArg) == SCE_OK);
    Require(finalArg == cfg->initTag + 5);
    Require(sceFiberReturnToThread(cfg->initTag + 6, nullptr) == SCE_OK);
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

void TestUnwritableOutputPointerRejection() {
    auto* const unwritableOut = reinterpret_cast<std::uint64_t*>(0x10);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "unwritable_main", UnwritableMainEntry, 1001, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "unwritable_sw", UnwritableSwitchTargetEntry, 2001, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberC, "unwritable_asw", UnwritableAttachSwitchEntry, 7001, nullptr, 0, nullptr, 0) == SCE_OK);

    Require(sceFiberRun_nid_postfix(g_fiberA, 1002, unwritableOut) == FiberErrorInvalid);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 1212, &returned) == SCE_OK);
    Require(returned == 1313);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 1414, &returned) == SCE_OK);
    Require(returned == 1515);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 1616, &returned) == SCE_OK);
    Require(returned == 1717);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberFinalize(g_fiberC) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "unwritable_arun", UnwritableAttachRunEntry, 2002, nullptr, 0, nullptr, 0) == SCE_OK);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackA, sizeof(g_stackA), 3002, unwritableOut) == FiberErrorInvalid);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 5002, &returned) == SCE_OK);
    Require(returned == 6002);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void RunCrossThreadMigrationCase(void* context, std::uint64_t contextSize, bool expectCallerBuffer, std::uint64_t initTag) {
    const MigrationConfig cfg{
        g_fiberA,
        contextSize,
        context,
        expectCallerBuffer,
        initTag,
    };

    Require(_sceFiberInitializeImpl_nid_postfix(
        g_fiberA,
        "thread_migrate",
        CrossThreadMigrationEntry,
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&cfg)),
        context,
        contextSize,
        nullptr,
        0) == SCE_OK);

    std::thread worker1([&] {
        FiberObject* self = nullptr;
        Require(sceFiberGetSelf(&self) == FiberErrorPermission);
        std::uint64_t returned = 0;
        Require(sceFiberRun_nid_postfix(g_fiberA, initTag + 1, &returned) == SCE_OK);
        Require(returned == initTag + 2);
        Require(sceFiberGetSelf(&self) == FiberErrorPermission);
    });
    worker1.join();

    std::thread worker2([&] {
        FiberObject* self = nullptr;
        Require(sceFiberGetSelf(&self) == FiberErrorPermission);
        std::uint64_t returned = 0;
        Require(sceFiberRun_nid_postfix(g_fiberA, initTag + 3, &returned) == SCE_OK);
        Require(returned == initTag + 4);
        returned = 0;
        Require(sceFiberRun_nid_postfix(g_fiberA, initTag + 5, &returned) == SCE_OK);
        Require(returned == initTag + 6);
        Require(sceFiberGetSelf(&self) == FiberErrorPermission);
    });
    worker2.join();

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);
}

void TestCrossThreadMigrationYieldRoundtrip() {
    RunCrossThreadMigrationCase(g_stackA, sizeof(g_stackA), false, 10000);
    RunCrossThreadMigrationCase(g_largeStack, sizeof(g_largeStack), true, 20000);
}

} // namespace

int main() {
    TestHostThreadCallingRestrictions();
    TestSwitchSelfAndInvalidFiberRejection();
    TestMultiFiberSwitchChainAndArgumentRoundtrips();
    TestYieldLoopAndNullOutPointers();
    TestUnwritableOutputPointerRejection();
    TestCrossThreadMigrationYieldRoundtrip();
    return 0;
}
