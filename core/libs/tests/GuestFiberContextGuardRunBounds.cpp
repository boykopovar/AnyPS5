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

alignas(16) std::uint8_t g_validStack[16384];
alignas(16) std::uint8_t g_attachStack[16384];
alignas(16) std::uint8_t g_switchStack[16384];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

void APS5_VABI DummyEntry(std::uint64_t, std::uint64_t) {
    Require(sceFiberReturnToThread(444, nullptr) == SCE_OK);
}

void APS5_VABI DynamicAttachRunEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 111);
    Require(argOnRun == 222);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_attachStack));

    Require(sceFiberReturnToThread(333, nullptr) == SCE_OK);
}

void APS5_VABI FiberSwitchInitiatorEntry(std::uint64_t, std::uint64_t) {
    std::uint64_t received = 0;
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_switchStack, sizeof(g_switchStack), 555, &received) == SCE_OK);
    Require(received == 777);
    Require(sceFiberReturnToThread(888, nullptr) == SCE_OK);
}

void APS5_VABI FiberSwitchTargetEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 99);
    Require(argOnRun == 555);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberB, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_switchStack));

    Require(sceFiberSwitch(g_fiberA, 777, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI LifecycleGuardEntry(std::uint64_t, std::uint64_t) {
    Require(sceFiberRun_nid_postfix(g_fiberB, 0, nullptr) == FiberErrorPermission);
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, nullptr) == FiberErrorPermission);
    Require(_sceFiberAttachContextAndRun(g_fiberB, g_validStack, sizeof(g_validStack), 0, nullptr) == FiberErrorPermission);

    Require(sceFiberFinalize(g_fiberA) == FiberErrorState);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(nullptr) == FiberErrorNull);
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(sceFiberGetThreadFramePointerAddress(&fp) == SCE_OK);
    Require(fp != 0);

    fp = 0;
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(&fp) == SCE_OK);
    Require(fp != 0);

    Require(sceFiberReturnToThread(666, nullptr) == SCE_OK);
}

void APS5_VABI GuardCheckEntry(std::uint64_t, std::uint64_t) {
    volatile std::uint8_t buffer[1024];
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = static_cast<std::uint8_t>(i & 0xff);
    }
    Require(sceFiberReturnToThread(1234, nullptr) == SCE_OK);
}

void TestContextParametersAndAlignmentBounds() {
    alignas(16) std::uint8_t unalignedStorage[512];
    for (std::uintptr_t offset = 1; offset <= 7; ++offset) {
        Require(_sceFiberInitializeImpl_nid_postfix(reinterpret_cast<FiberObject*>(unalignedStorage + offset), "misaligned_obj", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorAlignment);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(nullptr, "null_obj", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, nullptr, DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "null_entry", nullptr, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorNull);

    for (std::uintptr_t offset = 1; offset <= 15; ++offset) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_ctx", DummyEntry, 0, g_validStack + offset, 16384, nullptr, 0) == FiberErrorAlignment);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_sz1", DummyEntry, 0, g_validStack, 513, nullptr, 0) == FiberErrorAlignment);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_sz2", DummyEntry, 0, g_validStack, 514, nullptr, 0) == FiberErrorAlignment);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_sz3", DummyEntry, 0, g_validStack, 520, nullptr, 0) == FiberErrorAlignment);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_sz4", DummyEntry, 0, g_validStack, 1025, nullptr, 0) == FiberErrorAlignment);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "range_0", DummyEntry, 0, g_validStack, 0, nullptr, 0) == FiberErrorRange);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "range_16", DummyEntry, 0, g_validStack, 16, nullptr, 0) == FiberErrorRange);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "range_64", DummyEntry, 0, g_validStack, 64, nullptr, 0) == FiberErrorRange);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "range_128", DummyEntry, 0, g_validStack, 128, nullptr, 0) == FiberErrorRange);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "range_256", DummyEntry, 0, g_validStack, 256, nullptr, 0) == FiberErrorRange);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "range_496", DummyEntry, 0, g_validStack, 496, nullptr, 0) == FiberErrorRange);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "valid_512", DummyEntry, 0, g_validStack, 512, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "null_ctx_nonzero", DummyEntry, 0, nullptr, 16384, nullptr, 0) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "contextless", DummyEntry, 0, nullptr, 0, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(sceFiberOptParamInitialize(nullptr) == FiberErrorNull);
    alignas(8) std::uint8_t optBuffer[128] = {};
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(optBuffer)) == SCE_OK);
}

void TestContextlessDynamicAttachAndGreenThreading() {
    Require(_sceFiberAttachContextAndRun(nullptr, g_validStack, sizeof(g_validStack), 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(_sceFiberAttachContextAndRun(reinterpret_cast<FiberObject*>(uninitStorage), g_validStack, sizeof(g_validStack), 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "with_ctx", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == SCE_OK);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, sizeof(g_attachStack), 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "dynamic_run", DynamicAttachRunEntry, 111, nullptr, 0, nullptr, 0) == SCE_OK);

    Require(_sceFiberAttachContextAndRun(g_fiberA, nullptr, sizeof(g_attachStack), 0, nullptr) == FiberErrorInvalid);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack + 1, sizeof(g_attachStack), 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, 16385, 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, 256, 0, nullptr) == FiberErrorRange);

    std::uint64_t returned = 0;
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, sizeof(g_attachStack), 222, &returned) == SCE_OK);
    Require(returned == 333);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_switchStack, sizeof(g_switchStack), 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "switch_init", FiberSwitchInitiatorEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "switch_target", FiberSwitchTargetEntry, 99, nullptr, 0, nullptr, 0) == SCE_OK);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 888);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

void TestRunErrorBoundsAndLifecycleGuards() {
    Require(sceFiberRun_nid_postfix(nullptr, 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberRun_nid_postfix(reinterpret_cast<FiberObject*>(uninitStorage), 0, nullptr) == FiberErrorInvalid);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == FiberErrorPermission);

    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
    Require(_sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "lifecycle_test", LifecycleGuardEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "idle_target", DummyEntry, 0, g_attachStack, sizeof(g_attachStack), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 666);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

void TestStackGuardPagesAndContextSizeCheck() {
    struct alignas(16) GuardedStack {
        std::uint64_t guardHead[8];
        alignas(16) std::uint8_t stack[16384];
        std::uint64_t guardTail[8];
    } guarded;

    for (std::size_t i = 0; i < 8; ++i) {
        guarded.guardHead[i] = 0xaaaaaaaaaaaaaaaaull + i;
        guarded.guardTail[i] = 0xbbbbbbbbbbbbbbbbull + i;
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "guard_test", GuardCheckEntry, 0, guarded.stack, sizeof(guarded.stack), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 1234);

    for (std::size_t i = 0; i < 8; ++i) {
        Require(guarded.guardHead[i] == 0xaaaaaaaaaaaaaaaaull + i);
        Require(guarded.guardTail[i] == 0xbbbbbbbbbbbbbbbbull + i);
    }

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid);
    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);
    Require(sceFiberStartContextSizeCheck(0) == FiberErrorState);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "check_test", GuardCheckEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin == sizeof(g_validStack));

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 1234);

    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin > 0);
    Require(info.size_context_margin < sizeof(g_validStack));
    Require(info.size_context_margin % 8 == 0);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);
}

void TestFiberInfoAndInternalOptions() {
    Require(sceFiberGetInfo(nullptr, nullptr) == FiberErrorNull);
    FiberInfo info{};
    Require(sceFiberGetInfo(nullptr, &info) == FiberErrorNull);
    Require(sceFiberGetInfo(g_fiberA, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberGetInfo(reinterpret_cast<FiberObject*>(uninitStorage), &info) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "info_test", DummyEntry, 12345, g_validStack, sizeof(g_validStack), nullptr, 0) == SCE_OK);

    info.size = 0;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    info.size = sizeof(FiberInfo) - 1;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    info.size = sizeof(FiberInfo) + 1;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);

    info.size = sizeof(FiberInfo);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.entry == DummyEntry);
    Require(info.arg_on_initialize == 12345);
    Require(info.size_context == sizeof(g_validStack));
    Require(std::strcmp(info.name, "info_test") == 0);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    constexpr std::uint32_t kFlagNoUlobjmgr = 0x1;
    constexpr std::uint32_t kFlagSetFpu = 0x100;
    constexpr std::uint32_t kFlagsCombined = kFlagNoUlobjmgr | kFlagSetFpu;
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "internal_opts", DummyEntry, 999, g_validStack, sizeof(g_validStack), nullptr, kFlagsCombined, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 100, &returned) == SCE_OK);
    Require(returned == 444);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

} // namespace

int main() {
    TestContextParametersAndAlignmentBounds();
    TestContextlessDynamicAttachAndGreenThreading();
    TestRunErrorBoundsAndLifecycleGuards();
    TestStackGuardPagesAndContextSizeCheck();
    TestFiberInfoAndInternalOptions();
    return 0;
}
