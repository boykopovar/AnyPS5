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

alignas(16) std::uint8_t g_stackA[16384];
alignas(16) std::uint8_t g_stackB[16384];
alignas(16) std::uint8_t g_stackC[16384];
alignas(16) std::uint8_t g_largeStack[65536];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

std::uint64_t g_executionRunCount = 0;

void APS5_VABI DummyEntry(std::uint64_t, std::uint64_t) {}

void APS5_VABI SimpleReturnEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 10);
    Require(argOnRun == 20);
    ++g_executionRunCount;
    Require(sceFiberReturnToThread(30, nullptr) == SCE_OK);
}

void APS5_VABI InitializedWithContextEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 100);
    Require(argOnRun == 200);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_stackB, sizeof(g_stackB), 0, nullptr) == FiberErrorState);

    Require(sceFiberReturnToThread(300, nullptr) == SCE_OK);
}

void APS5_VABI SwitchTestFiberAEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 1);
    Require(argOnRun == 2);

    std::uint64_t received = 0;
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_stackB, sizeof(g_stackB), 50, &received) == SCE_OK);
    Require(received == 60);

    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_stackC, sizeof(g_stackC), 70, nullptr) == FiberErrorInvalid);

    Require(sceFiberReturnToThread(80, nullptr) == SCE_OK);
}

void APS5_VABI SwitchTestFiberBEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 3);
    Require(argOnRun == 50);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberB, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_stackB));

    Require(sceFiberSwitch(g_fiberA, 60, nullptr) == SCE_OK);
}

void APS5_VABI ReinitLargeEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 777);
    Require(argOnRun == 888);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.addr_context == g_largeStack);
    Require(info.size_context == sizeof(g_largeStack));

    Require(sceFiberReturnToThread(999, nullptr) == SCE_OK);
}

void TestAttachContextParameterValidation() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "ctxless_val", DummyEntry, 0, nullptr, 0, nullptr, 0) == SCE_OK);

    Require(_sceFiberAttachContextAndRun(nullptr, g_stackA, sizeof(g_stackA), 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256];
    std::memset(uninitStorage, 0, sizeof(uninitStorage));
    auto* const uninitObj = reinterpret_cast<FiberObject*>(uninitStorage);
    Require(_sceFiberAttachContextAndRun(uninitObj, g_stackA, sizeof(g_stackA), 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberAttachContextAndRun(g_fiberA, nullptr, sizeof(g_stackA), 0, nullptr) == FiberErrorInvalid);

    for (std::uintptr_t offset = 1; offset <= 15; ++offset) {
        Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackA + offset, 4096, 0, nullptr) == FiberErrorAlignment);
    }

    const std::uint64_t unalignedSizes[] = {513, 514, 515, 517, 520, 1025, 2049, 4097};
    for (const auto size : unalignedSizes) {
        Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackA, size, 0, nullptr) == FiberErrorAlignment);
    }

    const std::uint64_t subMinSizes[] = {16, 32, 64, 128, 256, 384, 480, 496};
    for (const auto size : subMinSizes) {
        Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackA, size, 0, nullptr) == FiberErrorRange);
    }

    g_executionRunCount = 0;
    std::uint64_t returned = 0;
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "min_exec", SimpleReturnEntry, 10, nullptr, 0, nullptr, 0) == SCE_OK);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackA, 512, 20, &returned) == SCE_OK);
    Require(returned == 30);
    Require(g_executionRunCount == 1);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestMultipleContextAttachmentRejection() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "init_ctx", InitializedWithContextEntry, 100, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackB, sizeof(g_stackB), 0, nullptr) == FiberErrorInvalid);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 200, &returned) == SCE_OK);
    Require(returned == 300);

    Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackB, sizeof(g_stackB), 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    g_executionRunCount = 0;
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "attach_once", SimpleReturnEntry, 10, nullptr, 0, nullptr, 0) == SCE_OK);
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackA, sizeof(g_stackA), 20, &returned) == SCE_OK);
    Require(returned == 30);
    Require(g_executionRunCount == 1);

    Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackB, sizeof(g_stackB), 20, &returned) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "switch_a", SwitchTestFiberAEntry, 1, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "switch_b", SwitchTestFiberBEntry, 3, nullptr, 0, nullptr, 0) == SCE_OK);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 2, &returned) == SCE_OK);
    Require(returned == 80);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

void TestDetachViaReinitializationLifecycle() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "small_ctx", DummyEntry, 50, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_stackA));

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "large_ctx", ReinitLargeEntry, 777, g_largeStack, sizeof(g_largeStack), nullptr, 0) == SCE_OK);

    info = {};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.addr_context == g_largeStack);
    Require(info.size_context == sizeof(g_largeStack));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 888, &returned) == SCE_OK);
    Require(returned == 999);

    g_executionRunCount = 0;
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_ctxless", SimpleReturnEntry, 10, nullptr, 0, nullptr, 0) == SCE_OK);

    info = {};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.addr_context == nullptr);
    Require(info.size_context == 0);

    returned = 0;
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackA, sizeof(g_stackA), 20, &returned) == SCE_OK);
    Require(returned == 30);
    Require(g_executionRunCount == 1);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestPostDetachPostFinalizeStateChecks() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "finalize_detach", DummyEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorInvalid);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    Require(sceFiberRename(g_fiberA, "post_detach") == FiberErrorInvalid);

    std::uint64_t dummyOut = 0;
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_stackA, sizeof(g_stackA), 0, &dummyOut) == FiberErrorInvalid);
    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_stackA, sizeof(g_stackA), 0, &dummyOut) == FiberErrorInvalid);

    g_executionRunCount = 0;
    std::uint64_t returned = 0;
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "revived", SimpleReturnEntry, 10, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(sceFiberRun_nid_postfix(g_fiberA, 20, &returned) == SCE_OK);
    Require(returned == 30);
    Require(g_executionRunCount == 1);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

} // namespace

int main() {
    TestAttachContextParameterValidation();
    TestMultipleContextAttachmentRejection();
    TestDetachViaReinitializationLifecycle();
    TestPostDetachPostFinalizeStateChecks();
    return 0;
}
