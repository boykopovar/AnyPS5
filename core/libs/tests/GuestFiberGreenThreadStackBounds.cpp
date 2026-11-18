#include "prx/libSceFiber/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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

alignas(16) std::uint8_t g_stack16K[16384];
alignas(16) std::uint8_t g_stack32K[32768];
alignas(16) std::uint8_t g_stack64K[65536];
alignas(16) std::uint8_t g_attachStack[16384];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
alignas(8) std::uint8_t g_fiberStorageC[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);
FiberObject* const g_fiberC = reinterpret_cast<FiberObject*>(g_fiberStorageC);

void APS5_VABI TrivialEntry(std::uint64_t, std::uint64_t) {
    Require(sceFiberReturnToThread(12345, nullptr) == SCE_OK);
}

void APS5_VABI Execution16KEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x1111);
    Require(argOnRun == 0x2222);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(nullptr) == FiberErrorNull);
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_stack16K));
    Require(info.entry == Execution16KEntry);

    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(sceFiberGetThreadFramePointerAddress(&fp) == SCE_OK);
    Require(fp != 0);

    std::uint64_t fp2 = 0;
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(&fp2) == SCE_OK);
    Require(fp2 == fp);

    Require(sceFiberRun_nid_postfix(g_fiberB, 0, nullptr) == FiberErrorPermission);
    Require(sceFiberFinalize(g_fiberA) == FiberErrorState);

    volatile std::uint8_t localBuf[256];
    for (std::size_t i = 0; i < sizeof(localBuf); ++i) {
        localBuf[i] = static_cast<std::uint8_t>(i & 0xff);
    }
    Require(localBuf[0] == 0);
    Require(localBuf[255] == 255);

    std::uint64_t nextArg = 0;
    Require(sceFiberReturnToThread(0x3333, &nextArg) == SCE_OK);
    Require(nextArg == 0x4444);

    Require(sceFiberReturnToThread(0x5555, nullptr) == SCE_OK);
}

void APS5_VABI Execution64KEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x6401);
    Require(argOnRun == 0x6402);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_stack64K));

    volatile std::uint64_t marker = 0xabcdef0123456789ull;
    const auto markerAddr = reinterpret_cast<std::uintptr_t>(&marker);
    const auto stackLow = reinterpret_cast<std::uintptr_t>(g_stack64K);
    const auto stackHigh = stackLow + sizeof(g_stack64K);
    Require(markerAddr >= stackLow && markerAddr < stackHigh);

    Require(sceFiberReturnToThread(0x6403, nullptr) == SCE_OK);
}

void APS5_VABI ContextlessEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x7701);
    Require(argOnRun == 0x7702);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == 0);
    Require(info.addr_context == nullptr);

    Require(sceFiberReturnToThread(0x7703, nullptr) == SCE_OK);
}

void APS5_VABI SwitchEntryA(std::uint64_t, std::uint64_t) {
    volatile std::uint64_t canaries[4] = {
        0x1111222233334444ull,
        0x5555666677778888ull,
        0x9999aaaabbbbccccull,
        0xddddeeeeffff0000ull
    };

    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorState);
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberSwitch(reinterpret_cast<FiberObject*>(uninitStorage), 0, nullptr) == FiberErrorInvalid);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberB, 101, &received) == SCE_OK);
    Require(received == 202);

    Require(canaries[0] == 0x1111222233334444ull);
    Require(canaries[1] == 0x5555666677778888ull);
    Require(canaries[2] == 0x9999aaaabbbbccccull);
    Require(canaries[3] == 0xddddeeeeffff0000ull);

    Require(sceFiberReturnToThread(303, nullptr) == SCE_OK);
}

void APS5_VABI SwitchEntryB(std::uint64_t, std::uint64_t argOnRun) {
    Require(argOnRun == 101);

    volatile std::uint64_t canaries[4] = {
        0xaaaa1111bbbb2222ull,
        0xcccc3333dddd4444ull,
        0xeeee5555ffff6666ull,
        0x0000777711118888ull
    };

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberC, 102, &received) == SCE_OK);
    Require(received == 201);

    Require(canaries[0] == 0xaaaa1111bbbb2222ull);
    Require(canaries[1] == 0xcccc3333dddd4444ull);
    Require(canaries[2] == 0xeeee5555ffff6666ull);
    Require(canaries[3] == 0x0000777711118888ull);

    Require(sceFiberSwitch(g_fiberA, 202, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI SwitchEntryC(std::uint64_t, std::uint64_t argOnRun) {
    Require(argOnRun == 102);
    Require(sceFiberSwitch(g_fiberB, 201, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI MarginConsumptionEntry(std::uint64_t, std::uint64_t) {
    volatile std::uint8_t buffer[2048];
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = static_cast<std::uint8_t>(i & 0xff);
    }
    Require(buffer[0] == 0);
    Require(buffer[2047] == (2047 & 0xff));

    Require(sceFiberReturnToThread(789, nullptr) == SCE_OK);
}

void APS5_VABI DynamicAttachedEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 501);
    Require(argOnRun == 601);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_attachStack));

    Require(sceFiberReturnToThread(701, nullptr) == SCE_OK);
}

void TestStackSizeAndAlignmentBounds() {
    alignas(16) std::uint8_t unalignedStorage[512];
    for (std::uintptr_t offset = 1; offset <= 7; ++offset) {
        Require(_sceFiberInitializeImpl_nid_postfix(reinterpret_cast<FiberObject*>(unalignedStorage + offset), "misaligned_obj", TrivialEntry, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == FiberErrorAlignment);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(nullptr, "test", TrivialEntry, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, nullptr, TrivialEntry, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "test", nullptr, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == FiberErrorNull);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "null_ctx", TrivialEntry, 0, nullptr, 512, nullptr, 0) == FiberErrorInvalid);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "null_ctx", TrivialEntry, 0, nullptr, 16384, nullptr, 0) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "zero_size", TrivialEntry, 0, g_stack16K, 0, nullptr, 0) == FiberErrorRange);

    for (std::uintptr_t offset = 1; offset <= 15; ++offset) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_addr", TrivialEntry, 0, g_stack16K + offset, 16384, nullptr, 0) == FiberErrorAlignment);
    }

    for (const std::uint64_t delta : {1u, 2u, 3u, 4u, 7u, 8u, 15u}) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_size", TrivialEntry, 0, g_stack16K, 16384 + delta, nullptr, 0) == FiberErrorAlignment);
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_size", TrivialEntry, 0, g_stack16K, 512 + delta, nullptr, 0) == FiberErrorAlignment);
    }

    for (const std::uint64_t undersized : {16u, 32u, 64u, 128u, 256u, 496u}) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "undersized", TrivialEntry, 0, g_stack16K, undersized, nullptr, 0) == FiberErrorRange);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "exact_min", TrivialEntry, 0, g_stack16K, 512, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "above_min", TrivialEntry, 0, g_stack16K, 528, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "contextless", TrivialEntry, 0, nullptr, 0, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestStackExecutionAcrossSizes() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "exec_16k", Execution16KEntry, 0x1111, g_stack16K, sizeof(g_stack16K), nullptr, 0) == SCE_OK);
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0x2222, &returned) == SCE_OK);
    Require(returned == 0x3333);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0x4444, &returned) == SCE_OK);
    Require(returned == 0x5555);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "exec_32k", TrivialEntry, 0, g_stack32K, sizeof(g_stack32K), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 12345);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "exec_64k", Execution64KEntry, 0x6401, g_stack64K, sizeof(g_stack64K), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0x6402, &returned) == SCE_OK);
    Require(returned == 0x6403);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "exec_contextless", ContextlessEntry, 0x7701, nullptr, 0, nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0x7702, &returned) == SCE_OK);
    Require(returned == 0x7703);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestCooperativeStackSwitchingAndRegisterIntegrity() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "switch_a", SwitchEntryA, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "switch_b", SwitchEntryB, 0, g_stack32K, sizeof(g_stack32K), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberC, "switch_c", SwitchEntryC, 0, g_stack64K, sizeof(g_stack64K), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 303);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberFinalize(g_fiberC) == SCE_OK);
}

void TestDynamicStackAttachmentBounds() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "dynamic_attach", DynamicAttachedEntry, 501, nullptr, 0, nullptr, 0) == SCE_OK);

    Require(_sceFiberAttachContextAndRun(nullptr, g_attachStack, sizeof(g_attachStack), 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(_sceFiberAttachContextAndRun(reinterpret_cast<FiberObject*>(uninitStorage), g_attachStack, sizeof(g_attachStack), 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberAttachContextAndRun(g_fiberA, nullptr, sizeof(g_attachStack), 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack + 1, sizeof(g_attachStack), 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack + 8, sizeof(g_attachStack), 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, sizeof(g_attachStack) - 7, 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, 496, 0, nullptr) == FiberErrorRange);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "has_context", TrivialEntry, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == SCE_OK);
    Require(_sceFiberAttachContextAndRun(g_fiberB, g_attachStack, sizeof(g_attachStack), 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);

    std::uint64_t returned = 0;
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, sizeof(g_attachStack), 601, &returned) == SCE_OK);
    Require(returned == 701);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestStackMarginAndGuardPageBounds() {
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);
    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid);
    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);
    Require(sceFiberStartContextSizeCheck(0) == FiberErrorState);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "margin_test", MarginConsumptionEntry, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin == sizeof(g_stack16K));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 789);

    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin > 0);
    Require(info.size_context_margin < sizeof(g_stack16K));
    Require(info.size_context_margin % 8 == 0);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);
}

void TestThreadFramePointerAndLifecycleBounds() {
    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission);
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorPermission);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == FiberErrorPermission);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "idle_lifecycle", TrivialEntry, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_after_finalize", TrivialEntry, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == SCE_OK);
    std::uint64_t ret = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &ret) == SCE_OK && ret == 12345);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_live_1", TrivialEntry, 0, g_stack16K, sizeof(g_stack16K), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_live_2", TrivialEntry, 0, g_stack32K, sizeof(g_stack32K), nullptr, 0) == SCE_OK);
    ret = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &ret) == SCE_OK && ret == 12345);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

} // namespace

int main() {
    TestStackSizeAndAlignmentBounds();
    TestStackExecutionAcrossSizes();
    TestCooperativeStackSwitchingAndRegisterIntegrity();
    TestDynamicStackAttachmentBounds();
    TestStackMarginAndGuardPageBounds();
    TestThreadFramePointerAndLifecycleBounds();
    return 0;
}
