#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::int32_t SCE_OK = 0;
constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t FiberErrorAlignment = static_cast<std::int32_t>(0x80590002);
constexpr std::int32_t FiberErrorRange = static_cast<std::int32_t>(0x80590003);
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t FiberErrorPermission = static_cast<std::int32_t>(0x80590005);
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

constexpr std::uint32_t FIBER_FLAG_NO_ULOBJMGR = 0x1;
constexpr std::uint32_t FIBER_FLAG_CONTEXT_SIZE_CHECK = 0x10;
constexpr std::uint32_t FIBER_FLAG_SET_FPU_REGISTERS = 0x100;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

alignas(16) std::uint8_t g_smallStack[16384];
alignas(16) std::uint8_t g_largeStack[65536];
alignas(16) std::uint8_t g_attachStack[16384];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

std::uint64_t g_observedFramePointer1 = 0;
std::uint64_t g_observedFramePointer2 = 0;

void APS5_VABI DummyEntry(std::uint64_t, std::uint64_t) {}

void APS5_VABI StackConsumerEntry(std::uint64_t, std::uint64_t) {
    volatile std::uint8_t buffer[1024];
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = static_cast<std::uint8_t>(i & 0xff);
    }
    Require(sceFiberReturnToThread(777, nullptr) == SCE_OK);
}

void APS5_VABI AttachRunEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 100);
    Require(argOnRun == 200);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    Require(sceFiberGetThreadFramePointerAddress(&g_observedFramePointer1) == SCE_OK);
    Require(_sceFiberGetThreadFramePointerAddress(&g_observedFramePointer2) == SCE_OK);
    Require(g_observedFramePointer1 != 0);
    Require(g_observedFramePointer1 == g_observedFramePointer2);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_attachStack));

    Require(sceFiberReturnToThread(300, nullptr) == SCE_OK);
}

void APS5_VABI FiberAEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 10);
    Require(argOnRun == 20);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    std::uint64_t received = 0;
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_attachStack, sizeof(g_attachStack), 555, &received) == SCE_OK);
    Require(received == 777);

    Require(sceFiberReturnToThread(888, nullptr) == SCE_OK);
}

void APS5_VABI FiberBEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 30);
    Require(argOnRun == 555);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    std::uint64_t fp1 = 0;
    std::uint64_t fp2 = 0;
    Require(sceFiberGetThreadFramePointerAddress(&fp1) == SCE_OK);
    Require(_sceFiberGetThreadFramePointerAddress(&fp2) == SCE_OK);
    Require(fp1 != 0);
    Require(fp1 == fp2);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberB, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_attachStack));

    Require(sceFiberSwitch(g_fiberA, 777, nullptr) == SCE_OK);
}

void TestOptParamInitializationBounds() {
    Require(sceFiberOptParamInitialize(nullptr) == FiberErrorNull);
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(static_cast<std::uintptr_t>(0x10))) == FiberErrorInvalid);
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(static_cast<std::uintptr_t>(0x1000))) == FiberErrorInvalid);

    struct alignas(8) GuardedOptParam {
        std::uint64_t headSentinels[4];
        FiberOptParam opt;
        std::uint8_t padding[0x80 - sizeof(FiberOptParam)];
        std::uint64_t tailSentinels[4];
    } guarded;

    for (std::size_t i = 0; i < 4; ++i) {
        guarded.headSentinels[i] = 0x1122334455667788ull + i;
        guarded.tailSentinels[i] = 0x8877665544332211ull + i;
    }
    std::memset(&guarded.opt, 0xff, 0x80);

    *__error_nid_postfix() = 77;
    Require(sceFiberOptParamInitialize(&guarded.opt) == SCE_OK);
    Require(*__error_nid_postfix() == 77);

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&guarded.opt);
    for (std::size_t i = 0; i < 0x80; ++i) {
        Require(bytes[i] == 0);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        Require(guarded.headSentinels[i] == 0x1122334455667788ull + i);
        Require(guarded.tailSentinels[i] == 0x8877665544332211ull + i);
    }
}

void TestInternalOptionFlagsAndValidation() {
    Require(_sceFiberInitializeWithInternalOptionImpl(nullptr, "test", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, 0, 0) == FiberErrorNull);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, nullptr, DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, 0, 0) == FiberErrorNull);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "test", nullptr, 0, g_smallStack, sizeof(g_smallStack), nullptr, 0, 0) == FiberErrorNull);

    alignas(16) std::uint8_t misalignedObj[256];
    for (std::uintptr_t offset = 1; offset <= 7; ++offset) {
        auto* const badObj = reinterpret_cast<FiberObject*>(misalignedObj + offset);
        Require(_sceFiberInitializeWithInternalOptionImpl(badObj, "test", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, 0, 0) == FiberErrorAlignment);
    }

    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "test", DummyEntry, 0, nullptr, 512, nullptr, 0, 0) == FiberErrorInvalid);

    for (std::uintptr_t offset = 1; offset <= 15; ++offset) {
        Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "test", DummyEntry, 0, g_smallStack + offset, 4096, nullptr, 0, 0) == FiberErrorAlignment);
    }

    const std::uint64_t unalignedSizes[] = {513, 514, 515, 517, 520, 1025, 2049, 4097};
    for (const auto size : unalignedSizes) {
        Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "test", DummyEntry, 0, g_smallStack, size, nullptr, 0, 0) == FiberErrorAlignment);
    }

    const std::uint64_t subMinSizes[] = {16, 32, 64, 128, 256, 384, 480, 496};
    for (const auto size : subMinSizes) {
        Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "test", DummyEntry, 0, g_smallStack, size, nullptr, 0, 0) == FiberErrorRange);
    }

    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "f_zero", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, 0, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "f_no_ulobjmgr", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, FIBER_FLAG_NO_ULOBJMGR, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "f_size_check", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, FIBER_FLAG_CONTEXT_SIZE_CHECK, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "f_fpu", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, FIBER_FLAG_SET_FPU_REGISTERS, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    const std::uint32_t validCombined = FIBER_FLAG_NO_ULOBJMGR | FIBER_FLAG_CONTEXT_SIZE_CHECK | FIBER_FLAG_SET_FPU_REGISTERS;
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "f_all", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, validCombined, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    FiberOptParam opt{};
    Require(sceFiberOptParamInitialize(&opt) == SCE_OK);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "f_opt", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), &opt, 0, 0x03500000) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    const std::uint32_t unsupportedFlags[] = {
        0x0002, 0x0004, 0x0008, 0x0020, 0x0040, 0x0080,
        0x0200, 0x0400, 0x0800, 0x1000, 0x80000000
    };
    for (const auto badFlag : unsupportedFlags) {
        bool threw = false;
        try {
            _sceFiberInitializeWithInternalOptionImpl(g_fiberA, "bad_flag", DummyEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, badFlag, 0);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }
}

void TestAttachContextAndRunBounds() {
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "ctxless_run", AttachRunEntry, 100, nullptr, 0, nullptr, 0, 0) == SCE_OK);

    Require(_sceFiberAttachContextAndRun(nullptr, g_attachStack, sizeof(g_attachStack), 200, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256];
    std::memset(uninitStorage, 0, sizeof(uninitStorage));
    auto* const uninitObj = reinterpret_cast<FiberObject*>(uninitStorage);
    Require(_sceFiberAttachContextAndRun(uninitObj, g_attachStack, sizeof(g_attachStack), 200, nullptr) == FiberErrorInvalid);

    Require(_sceFiberAttachContextAndRun(g_fiberA, nullptr, sizeof(g_attachStack), 200, nullptr) == FiberErrorInvalid);

    for (std::uintptr_t offset = 1; offset <= 15; ++offset) {
        Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack + offset, 4096, 200, nullptr) == FiberErrorAlignment);
    }

    const std::uint64_t unalignedSizes[] = {513, 514, 515, 517, 520, 1025, 2049, 4097};
    for (const auto size : unalignedSizes) {
        Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, size, 200, nullptr) == FiberErrorAlignment);
    }

    const std::uint64_t subMinSizes[] = {16, 32, 64, 128, 256, 384, 480, 496};
    for (const auto size : subMinSizes) {
        Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, size, 200, nullptr) == FiberErrorRange);
    }

    std::uint64_t returned = 0;
    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, sizeof(g_attachStack), 200, &returned) == SCE_OK);
    Require(returned == 300);

    Require(_sceFiberAttachContextAndRun(g_fiberA, g_attachStack, sizeof(g_attachStack), 200, &returned) == FiberErrorInvalid);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestAttachContextAndSwitchBounds() {
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "fiber_a", FiberAEntry, 10, g_smallStack, sizeof(g_smallStack), nullptr, 0, 0) == SCE_OK);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberB, "fiber_b", FiberBEntry, 30, nullptr, 0, nullptr, 0, 0) == SCE_OK);

    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_attachStack, sizeof(g_attachStack), 0, nullptr) == FiberErrorPermission);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 20, &returned) == SCE_OK);
    Require(returned == 888);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

void TestStackGuardMarginsAndHostStackThreshold() {
    struct alignas(16) GuardedSmallStack {
        std::uint64_t headSentinels[8];
        alignas(16) std::uint8_t stack[16384];
        std::uint64_t tailSentinels[8];
    } smallWrapper;

    for (std::size_t i = 0; i < 8; ++i) {
        smallWrapper.headSentinels[i] = 0xdeadbeefcafebabeull + i;
        smallWrapper.tailSentinels[i] = 0xcafebabedeadbeefull + i;
    }

    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "guard_small", StackConsumerEntry, 0, smallWrapper.stack, sizeof(smallWrapper.stack), nullptr, FIBER_FLAG_CONTEXT_SIZE_CHECK, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin == sizeof(smallWrapper.stack));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 777);

    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin > 0);
    Require(info.size_context_margin < sizeof(smallWrapper.stack));
    Require(info.size_context_margin % 8 == 0);

    for (std::size_t i = 0; i < 8; ++i) {
        Require(smallWrapper.headSentinels[i] == 0xdeadbeefcafebabeull + i);
        Require(smallWrapper.tailSentinels[i] == 0xcafebabedeadbeefull + i);
    }

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    struct alignas(16) GuardedLargeStack {
        std::uint64_t headSentinels[8];
        alignas(16) std::uint8_t stack[65536];
        std::uint64_t tailSentinels[8];
    } largeWrapper;

    for (std::size_t i = 0; i < 8; ++i) {
        largeWrapper.headSentinels[i] = 0x11223344aabbccddull + i;
        largeWrapper.tailSentinels[i] = 0xaabbccdd11223344ull + i;
    }

    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "guard_large", StackConsumerEntry, 0, largeWrapper.stack, sizeof(largeWrapper.stack), nullptr, FIBER_FLAG_NO_ULOBJMGR, 0) == SCE_OK);

    info = {};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.addr_context == largeWrapper.stack);
    Require(info.size_context == sizeof(largeWrapper.stack));

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 777);

    for (std::size_t i = 0; i < 8; ++i) {
        Require(largeWrapper.headSentinels[i] == 0x11223344aabbccddull + i);
        Require(largeWrapper.tailSentinels[i] == 0xaabbccdd11223344ull + i);
    }

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestThreadFramePointerBounds() {
    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
}

} // namespace

int main() {
    TestOptParamInitializationBounds();
    TestInternalOptionFlagsAndValidation();
    TestAttachContextAndRunBounds();
    TestAttachContextAndSwitchBounds();
    TestStackGuardMarginsAndHostStackThreshold();
    TestThreadFramePointerBounds();
    return 0;
}
