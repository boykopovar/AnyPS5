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

alignas(16) std::uint8_t g_validStack[65536];
alignas(8) std::uint8_t g_fiberStorage[256];
FiberObject* const g_fiber = reinterpret_cast<FiberObject*>(g_fiberStorage);

void APS5_VABI DummyEntry(std::uint64_t, std::uint64_t) {}

void APS5_VABI LifecycleEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 123);
    Require(argOnRun == 456);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiber);

    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(sceFiberGetThreadFramePointerAddress(&fp) == SCE_OK);
    Require(fp != 0);

    fp = 0;
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(&fp) == SCE_OK);
    Require(fp != 0);

    Require(sceFiberSwitch(g_fiber, 0, nullptr) == FiberErrorState);

    std::uint64_t nextRun = 0;
    Require(sceFiberReturnToThread(789, &nextRun) == SCE_OK);
    Require(nextRun == 999);

    Require(sceFiberReturnToThread(1000, nullptr) == SCE_OK);
}

void TestOptParamInitializationBounds() {
    Require(sceFiberOptParamInitialize(nullptr) == FiberErrorNull);
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(static_cast<std::uintptr_t>(0x10))) == FiberErrorInvalid);
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(static_cast<std::uintptr_t>(0x1000))) == FiberErrorInvalid);

    struct alignas(8) ParamContainer {
        std::uint8_t prefix[32];
        std::uint8_t param[0x80];
        std::uint8_t suffix[32];
    } container;

    std::memset(container.prefix, 0x5a, sizeof(container.prefix));
    std::memset(container.param, 0xff, sizeof(container.param));
    std::memset(container.suffix, 0xa5, sizeof(container.suffix));

    *__error_nid_postfix() = 42;
    const std::int32_t result = sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(container.param));
    Require(result == SCE_OK);
    Require(*__error_nid_postfix() == 42);

    for (std::size_t i = 0; i < sizeof(container.param); ++i) {
        Require(container.param[i] == 0);
    }
    for (std::size_t i = 0; i < sizeof(container.prefix); ++i) {
        Require(container.prefix[i] == 0x5a);
    }
    for (std::size_t i = 0; i < sizeof(container.suffix); ++i) {
        Require(container.suffix[i] == 0xa5);
    }
}

void TestContextAndGuardPageSizeBounds() {
    Require(_sceFiberInitializeImpl_nid_postfix(nullptr, "test", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, nullptr, DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "test", nullptr, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorNull);

    alignas(16) std::uint8_t misalignedStorage[256];
    auto* const badObject1 = reinterpret_cast<FiberObject*>(misalignedStorage + 1);
    auto* const badObject2 = reinterpret_cast<FiberObject*>(misalignedStorage + 3);
    auto* const badObject3 = reinterpret_cast<FiberObject*>(misalignedStorage + 7);
    Require(_sceFiberInitializeImpl_nid_postfix(badObject1, "test", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorAlignment);
    Require(_sceFiberInitializeImpl_nid_postfix(badObject2, "test", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorAlignment);
    Require(_sceFiberInitializeImpl_nid_postfix(badObject3, "test", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == FiberErrorAlignment);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "test", DummyEntry, 0, nullptr, 512, nullptr, 0) == FiberErrorInvalid);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "test", DummyEntry, 0, nullptr, 1024, nullptr, 0) == FiberErrorInvalid);

    auto* const misalignedAddr1 = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(g_validStack) + 1);
    auto* const misalignedAddr2 = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(g_validStack) + 4);
    auto* const misalignedAddr3 = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(g_validStack) + 8);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "test", DummyEntry, 0, misalignedAddr1, 512, nullptr, 0) == FiberErrorAlignment);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "test", DummyEntry, 0, misalignedAddr2, 512, nullptr, 0) == FiberErrorAlignment);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "test", DummyEntry, 0, misalignedAddr3, 512, nullptr, 0) == FiberErrorAlignment);

    const std::uint64_t unalignedSizes[] = { 513, 515, 518, 523, 1025, 4097 };
    for (const std::uint64_t size : unalignedSizes) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "test", DummyEntry, 0, g_validStack, size, nullptr, 0) == FiberErrorAlignment);
    }

    const std::uint64_t smallSizes[] = { 0, 16, 32, 64, 128, 256, 496 };
    for (const std::uint64_t size : smallSizes) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "test", DummyEntry, 0, g_validStack, size, nullptr, 0) == FiberErrorRange);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "min_context", DummyEntry, 0, g_validStack, 512, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiber) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "kb_context", DummyEntry, 0, g_validStack, 1024, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiber) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "page_context", DummyEntry, 0, g_validStack, 16384, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiber) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "contextless", DummyEntry, 0, nullptr, 0, nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiber) == SCE_OK);
}

void TestAttachContextBounds() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "has_context", DummyEntry, 0, g_validStack, 512, nullptr, 0) == SCE_OK);
    alignas(16) std::uint8_t attachStack[1024];
    Require(_sceFiberAttachContextAndRun(g_fiber, attachStack, sizeof(attachStack), 0, nullptr) == FiberErrorInvalid);
    Require(_sceFiberAttachContextAndSwitch(g_fiber, attachStack, sizeof(attachStack), 0, nullptr) == FiberErrorPermission);
    Require(sceFiberFinalize(g_fiber) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "no_context", DummyEntry, 0, nullptr, 0, nullptr, 0) == SCE_OK);
    Require(_sceFiberAttachContextAndRun(g_fiber, nullptr, 512, 0, nullptr) == FiberErrorInvalid);

    auto* const misalignedAttach = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(attachStack) + 1);
    Require(_sceFiberAttachContextAndRun(g_fiber, misalignedAttach, 512, 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndRun(g_fiber, attachStack, 513, 0, nullptr) == FiberErrorAlignment);
    Require(_sceFiberAttachContextAndRun(g_fiber, attachStack, 256, 0, nullptr) == FiberErrorRange);

    Require(sceFiberFinalize(g_fiber) == SCE_OK);
}

void TestRunErrorBounds() {
    Require(sceFiberRun_nid_postfix(nullptr, 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256];
    std::memset(uninitStorage, 0, sizeof(uninitStorage));
    auto* const uninitObject = reinterpret_cast<FiberObject*>(uninitStorage);
    Require(sceFiberRun_nid_postfix(uninitObject, 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "finalize_test", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiber) == SCE_OK);
    Require(sceFiberRun_nid_postfix(g_fiber, 0, nullptr) == FiberErrorInvalid);

    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);
    Require(sceFiberSwitch(g_fiber, 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(nullptr) == FiberErrorNull);
    Require(sceFiberGetSelf(&self) == FiberErrorPermission);

    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
}

void TestExecutionLifecycleBounds() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "lifecycle", LifecycleEntry, 123, g_validStack, sizeof(g_validStack), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber, 456, &returned) == SCE_OK);
    Require(returned == 789);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber, 999, &returned) == SCE_OK);
    Require(returned == 1000);

    Require(sceFiberFinalize(g_fiber) == SCE_OK);
    Require(sceFiberFinalize(g_fiber) == FiberErrorInvalid);
}

void TestInfoAndRenameBounds() {
    alignas(8) std::uint8_t uninitStorage[256];
    std::memset(uninitStorage, 0, sizeof(uninitStorage));
    auto* const uninitObject = reinterpret_cast<FiberObject*>(uninitStorage);

    Require(sceFiberRename(nullptr, "test") == FiberErrorNull);
    Require(sceFiberRename(g_fiber, nullptr) == FiberErrorNull);
    Require(sceFiberRename(uninitObject, "test") == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber, "init_name", DummyEntry, 0, g_validStack, sizeof(g_validStack), nullptr, 0) == SCE_OK);
    Require(sceFiberRename(g_fiber, "abcdefghijklmnopqrstuvwxyz0123456789") == SCE_OK);

    FiberInfo info{};
    Require(sceFiberGetInfo(nullptr, &info) == FiberErrorNull);
    Require(sceFiberGetInfo(g_fiber, nullptr) == FiberErrorNull);
    Require(sceFiberGetInfo(uninitObject, &info) == FiberErrorInvalid);

    info.size = 0;
    Require(sceFiberGetInfo(g_fiber, &info) == FiberErrorInvalid);
    info.size = sizeof(info) - 8;
    Require(sceFiberGetInfo(g_fiber, &info) == FiberErrorInvalid);

    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiber, &info) == SCE_OK);
    Require(std::strlen(info.name) == FIBER_MAX_NAME_LENGTH);
    Require(std::strncmp(info.name, "abcdefghijklmnopqrstuvwxyz01234", FIBER_MAX_NAME_LENGTH) == 0);
    Require(info.entry == DummyEntry);
    Require(info.size_context == sizeof(g_validStack));

    Require(sceFiberFinalize(g_fiber) == SCE_OK);
}

void TestContextSizeCheckBounds() {
    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid);
    Require(sceFiberStartContextSizeCheck(2) == FiberErrorInvalid);

    Require(sceFiberStopContextSizeCheck() == FiberErrorState);

    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);
    Require(sceFiberStartContextSizeCheck(0) == FiberErrorState);

    Require(sceFiberStopContextSizeCheck() == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);
}

} // namespace

int main() {
    TestOptParamInitializationBounds();
    TestContextAndGuardPageSizeBounds();
    TestAttachContextBounds();
    TestRunErrorBounds();
    TestExecutionLifecycleBounds();
    TestInfoAndRenameBounds();
    TestContextSizeCheckBounds();
    return 0;
}
