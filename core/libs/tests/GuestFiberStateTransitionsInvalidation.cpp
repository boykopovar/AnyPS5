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

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
alignas(8) std::uint8_t g_fiberStorageC[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);
FiberObject* const g_fiberC = reinterpret_cast<FiberObject*>(g_fiberStorageC);

bool g_reinitExecuted = false;

void APS5_VABI DummyEntry(std::uint64_t, std::uint64_t) {}

void APS5_VABI FiberAEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 111);
    Require(argOnRun == 222);

    Require(sceFiberGetSelf(nullptr) == FiberErrorNull);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    Require(sceFiberSwitch(g_fiberA, 10, nullptr) == FiberErrorState);
    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_stackA, sizeof(g_stackA), 10, nullptr) == FiberErrorState);
    Require(sceFiberFinalize(g_fiberA) == FiberErrorState);

    Require(sceFiberRun_nid_postfix(g_fiberB, 20, nullptr) == FiberErrorPermission);
    Require(_sceFiberAttachContextAndRun(g_fiberB, g_stackB, sizeof(g_stackB), 20, nullptr) == FiberErrorPermission);

    std::uint64_t transferred = 0;
    Require(sceFiberSwitch(g_fiberB, 100, &transferred) == SCE_OK);
    Require(transferred == 400);

    self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    Require(sceFiberReturnToThread(500, nullptr) == SCE_OK);
}

void APS5_VABI FiberBEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 333);
    Require(argOnRun == 100);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    Require(sceFiberSwitch(g_fiberB, 20, nullptr) == FiberErrorState);
    Require(_sceFiberAttachContextAndSwitch(g_fiberB, g_stackB, sizeof(g_stackB), 20, nullptr) == FiberErrorState);
    Require(sceFiberFinalize(g_fiberB) == FiberErrorState);

    std::uint64_t transferred = 0;
    Require(sceFiberSwitch(g_fiberC, 200, &transferred) == SCE_OK);
    Require(transferred == 300);

    self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberB);

    Require(sceFiberSwitch(g_fiberA, 400, nullptr) == SCE_OK);
}

void APS5_VABI FiberCEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 555);
    Require(argOnRun == 200);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberC);

    Require(sceFiberSwitch(g_fiberC, 30, nullptr) == FiberErrorState);
    Require(_sceFiberAttachContextAndSwitch(g_fiberC, g_stackC, sizeof(g_stackC), 30, nullptr) == FiberErrorState);
    Require(sceFiberFinalize(g_fiberC) == FiberErrorState);

    Require(sceFiberSwitch(g_fiberB, 300, nullptr) == SCE_OK);
}

void APS5_VABI ReinitEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 9001);
    Require(argOnRun == 9002);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self == g_fiberA);

    g_reinitExecuted = true;
    Require(sceFiberReturnToThread(9003, nullptr) == SCE_OK);
}

void TestOutsideFiberPermissions() {
    Require(sceFiberGetSelf(nullptr) == FiberErrorNull);

    FiberObject* self = reinterpret_cast<FiberObject*>(0x1234);
    Require(sceFiberGetSelf(&self) == FiberErrorPermission);
    Require(self == reinterpret_cast<FiberObject*>(0x1234));

    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission);

    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256];
    std::memset(uninitStorage, 0, sizeof(uninitStorage));
    auto* const uninitObj = reinterpret_cast<FiberObject*>(uninitStorage);
    Require(sceFiberSwitch(uninitObj, 0, nullptr) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "test_perm", DummyEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorPermission);
    Require(_sceFiberAttachContextAndSwitch(g_fiberA, g_stackA, sizeof(g_stackA), 0, nullptr) == FiberErrorPermission);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    std::uint64_t fp = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(&fp) == FiberErrorPermission);
}

void TestStateTransitionsAndSelfSwitchRejection() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "fiber_a", FiberAEntry, 111, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "fiber_b", FiberBEntry, 333, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberC, "fiber_c", FiberCEntry, 555, g_stackC, sizeof(g_stackC), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 222, &returned) == SCE_OK);
    Require(returned == 500);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == FiberErrorPermission);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberFinalize(g_fiberC) == SCE_OK);
}

void TestPostTerminationHandleInvalidation() {
    Require(sceFiberFinalize(nullptr) == FiberErrorNull);
    Require(sceFiberRun_nid_postfix(nullptr, 0, nullptr) == FiberErrorNull);
    Require(sceFiberRename(nullptr, "test") == FiberErrorNull);

    FiberInfo info{};
    Require(sceFiberGetInfo(nullptr, &info) == FiberErrorNull);

    FiberObject* const fibers[3] = {g_fiberA, g_fiberB, g_fiberC};
    for (auto* fiber : fibers) {
        Require(sceFiberFinalize(fiber) == FiberErrorInvalid);
        Require(sceFiberRun_nid_postfix(fiber, 0, nullptr) == FiberErrorInvalid);
        Require(sceFiberSwitch(fiber, 0, nullptr) == FiberErrorInvalid);

        info.size = sizeof(info);
        Require(sceFiberGetInfo(fiber, &info) == FiberErrorInvalid);
        Require(sceFiberRename(fiber, "invalidated") == FiberErrorInvalid);

        std::uint64_t dummyOut = 0;
        Require(_sceFiberAttachContextAndRun(fiber, g_stackA, sizeof(g_stackA), 0, &dummyOut) == FiberErrorInvalid);
        Require(_sceFiberAttachContextAndSwitch(fiber, g_stackA, sizeof(g_stackA), 0, &dummyOut) == FiberErrorInvalid);
    }
}

void TestReinitializationLifecycle() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "first_init", DummyEntry, 12, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strcmp(info.name, "first_init") == 0);
    Require(info.arg_on_initialize == 12);

    g_reinitExecuted = false;
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_fiber", ReinitEntry, 9001, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    info = {};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strcmp(info.name, "reinit_fiber") == 0);
    Require(info.arg_on_initialize == 9001);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 9002, &returned) == SCE_OK);
    Require(returned == 9003);
    Require(g_reinitExecuted);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);
}

} // namespace

int main() {
    TestOutsideFiberPermissions();
    TestStateTransitionsAndSelfSwitchRejection();
    TestPostTerminationHandleInvalidation();
    TestReinitializationLifecycle();
    return 0;
}
