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
constexpr std::int32_t FiberErrorRange = static_cast<std::int32_t>(0x80590003);
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t FiberErrorPermission = static_cast<std::int32_t>(0x80590005);
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

alignas(16) std::uint8_t g_smallContext[16384];
alignas(16) std::uint8_t g_largeContext[65536];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
alignas(8) std::uint8_t g_fiberStorageC[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);
FiberObject* const g_fiberC = reinterpret_cast<FiberObject*>(g_fiberStorageC);

FiberInfo QueryFiberInfo(FiberObject* fiber) {
    FiberInfo info{};
    std::memset(&info, 0xA5, sizeof(info));
    info.size = sizeof(info);
    Require(sceFiberGetInfo(fiber, &info) == SCE_OK);
    return info;
}

void AssertFiberInfoPreserved(FiberObject* fiber, const char* expectedName, GuestFiberEntry expectedEntry, std::uint64_t expectedArg, void* expectedContext, std::uint64_t expectedSize) {
    const FiberInfo info = QueryFiberInfo(fiber);
    Require(std::strcmp(info.name, expectedName) == 0);
    Require(info.entry == expectedEntry);
    Require(info.arg_on_initialize == expectedArg);
    Require(info.addr_context == expectedContext);
    Require(info.size_context == expectedSize);
}

void AssertThreadFramePointerParityInsideFiber(std::uint64_t* outFp) {
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);

    std::uint64_t fpPrimary = 0;
    std::uint64_t fpAlias = 0;
    const std::int32_t rcPrimary = sceFiberGetThreadFramePointerAddress(&fpPrimary);
    const std::int32_t rcAlias = _sceFiberGetThreadFramePointerAddress(&fpAlias);
    Require(rcPrimary == SCE_OK);
    Require(rcAlias == rcPrimary);
    Require(fpPrimary != 0);
    Require(fpAlias == fpPrimary);
    *outFp = fpPrimary;
}

void APS5_VABI SmallStackEntry(std::uint64_t, std::uint64_t) {
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_smallContext));

    volatile std::uint64_t marker = 0x1122334455667788ull;
    const auto markerAddr = reinterpret_cast<std::uintptr_t>(&marker);
    const auto low = reinterpret_cast<std::uintptr_t>(info.addr_context);
    const auto high = low + info.size_context;
    Require(markerAddr >= low && markerAddr < high);

    Require(sceFiberReturnToThread(111, nullptr) == SCE_OK);
}

void APS5_VABI LargeStackEntry(std::uint64_t, std::uint64_t) {
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_largeContext));
    Require(info.addr_context == g_largeContext);

    volatile std::uint64_t marker = 0x8877665544332211ull;
    const auto markerAddr = reinterpret_cast<std::uintptr_t>(&marker);
    const auto low = reinterpret_cast<std::uintptr_t>(g_largeContext);
    const auto high = low + sizeof(g_largeContext);
    Require(markerAddr >= low && markerAddr < high);

    Require(sceFiberReturnToThread(222, nullptr) == SCE_OK);
}

void APS5_VABI FramePointerProbeEntry(std::uint64_t, std::uint64_t) {
    std::uint64_t fp = 0;
    AssertThreadFramePointerParityInsideFiber(&fp);
    Require(sceFiberReturnToThread(fp, nullptr) == SCE_OK);
}

void APS5_VABI ContextSizeCheckEntry(std::uint64_t, std::uint64_t) {
    volatile std::uint8_t localScratch[512];
    for (std::size_t i = 0; i < sizeof(localScratch); ++i) {
        localScratch[i] = static_cast<std::uint8_t>(i & 0xffu);
    }
    Require(sceFiberReturnToThread(333, nullptr) == SCE_OK);
}

void APS5_VABI InFlightRenameFiberAEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x1122334455667788ull);
    Require(argOnRun == 1);

    Require(sceFiberRename(g_fiberA, "running_phase_1") == SCE_OK);
    AssertFiberInfoPreserved(g_fiberA, "running_phase_1", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    std::uint64_t fromB = 0;
    Require(sceFiberSwitch(g_fiberB, 42, &fromB) == SCE_OK);
    Require(fromB == 84);
    AssertFiberInfoPreserved(g_fiberA, "suspended_via_switch", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    std::uint64_t fromHost = 0;
    Require(sceFiberReturnToThread(101, &fromHost) == SCE_OK);
    Require(fromHost == 202);
    AssertFiberInfoPreserved(g_fiberA, "suspended_on_host", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    Require(sceFiberRename(g_fiberA, "running_phase_2") == SCE_OK);
    AssertFiberInfoPreserved(g_fiberA, "running_phase_2", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    Require(sceFiberReturnToThread(303, nullptr) == SCE_OK);
}

void APS5_VABI InFlightRenameFiberBEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x9988776655443322ull);
    Require(argOnRun == 42);

    AssertFiberInfoPreserved(g_fiberA, "running_phase_1", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));
    Require(sceFiberRename(g_fiberA, "suspended_via_switch") == SCE_OK);
    AssertFiberInfoPreserved(g_fiberA, "suspended_via_switch", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    const FiberInfo beforeB = QueryFiberInfo(g_fiberB);
    Require(std::strcmp(beforeB.name, "helper_fiber") == 0);
    Require(beforeB.entry == InFlightRenameFiberBEntry);
    Require(beforeB.arg_on_initialize == 0x9988776655443322ull);
    Require(beforeB.size_context == sizeof(g_smallContext));

    Require(sceFiberRename(g_fiberB, "helper_running") == SCE_OK);
    AssertFiberInfoPreserved(g_fiberB, "helper_running", InFlightRenameFiberBEntry, 0x9988776655443322ull, beforeB.addr_context, sizeof(g_smallContext));

    Require(sceFiberSwitch(g_fiberA, 84, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI FiberAEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 10);
    Require(argOnRun == 100);

    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorState);
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberSwitch(reinterpret_cast<FiberObject*>(uninitStorage), 0, nullptr) == FiberErrorInvalid);

    volatile double preservedFloat = 3.1415926535;
    volatile std::uint64_t preservedInt = 0xdeadbeefcafebabeull;

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberB, 200, &received) == SCE_OK);
    Require(received == 400);
    Require(preservedFloat == 3.1415926535);
    Require(preservedInt == 0xdeadbeefcafebabeull);

    Require(sceFiberReturnToThread(500, &received) == SCE_OK);
    Require(received == 600);
    Require(sceFiberReturnToThread(700, nullptr) == SCE_OK);
}

void APS5_VABI FiberBEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 20);
    Require(argOnRun == 200);

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiberC, 300, &received) == SCE_OK);
    Require(false);
}

void APS5_VABI FiberCEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 30);
    Require(argOnRun == 300);

    Require(sceFiberSwitch(g_fiberA, 400, nullptr) == SCE_OK);
    Require(false);
}

void TestStackBoundaryLimits() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "small_stack", SmallStackEntry, 0, g_smallContext, sizeof(g_smallContext), nullptr, 0) == SCE_OK);
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 111);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "large_stack", LargeStackEntry, 0, g_largeContext, sizeof(g_largeContext), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 222);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestRenameStringBounds() {
    Require(sceFiberRename(nullptr, "test") == FiberErrorNull);
    Require(sceFiberRename(g_fiberA, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberRename(reinterpret_cast<FiberObject*>(uninitStorage), "test") == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "initial", SmallStackEntry, 0, g_smallContext, sizeof(g_smallContext), nullptr, 0) == SCE_OK);

    Require(sceFiberRename(g_fiberA, "") == SCE_OK);
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strlen(info.name) == 0);

    const char* exactMaxName = "1234567890123456789012345678901";
    Require(sceFiberRename(g_fiberA, exactMaxName) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strlen(info.name) == 31);
    Require(std::strcmp(info.name, exactMaxName) == 0);

    const char* longName = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    Require(sceFiberRename(g_fiberA, longName) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strlen(info.name) == 31);
    Require(std::strncmp(info.name, longName, 31) == 0);
    Require(info.name[31] == '\0');

    Require(sceFiberRename(g_fiberA, "v2_name") == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strcmp(info.name, "v2_name") == 0);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestSwitchErrorConditionsAndTransitions() {
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission);

    alignas(16) std::uint8_t stack1[16384];
    alignas(16) std::uint8_t stack2[16384];
    alignas(16) std::uint8_t stack3[16384];

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "fiber_a", FiberAEntry, 10, stack1, sizeof(stack1), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "fiber_b", FiberBEntry, 20, stack2, sizeof(stack2), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberC, "fiber_c", FiberCEntry, 30, stack3, sizeof(stack3), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 100, &returned) == SCE_OK);
    Require(returned == 500);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 600, &returned) == SCE_OK);
    Require(returned == 700);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberFinalize(g_fiberC) == SCE_OK);

    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberB) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberC) == FiberErrorInvalid);
}

void TestThreadFramePointerAddressParity() {
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull);

    std::uint64_t hostFpPrimary = 0xA5A5A5A5A5A5A5A5ull;
    std::uint64_t hostFpAlias = 0x5A5A5A5A5A5A5A5Aull;
    const std::int32_t rcHostPrimary = sceFiberGetThreadFramePointerAddress(&hostFpPrimary);
    const std::int32_t rcHostAlias = _sceFiberGetThreadFramePointerAddress(&hostFpAlias);
    Require(rcHostPrimary == FiberErrorPermission);
    Require(rcHostAlias == rcHostPrimary);
    Require(hostFpPrimary == 0xA5A5A5A5A5A5A5A5ull);
    Require(hostFpAlias == 0x5A5A5A5A5A5A5A5Aull);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "fp_small", FramePointerProbeEntry, 0, g_smallContext, sizeof(g_smallContext), nullptr, 0) == SCE_OK);
    std::uint64_t fpSmall = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &fpSmall) == SCE_OK);
    Require(fpSmall != 0);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "fp_large", FramePointerProbeEntry, 0, g_largeContext, sizeof(g_largeContext), nullptr, 0) == SCE_OK);
    std::uint64_t fpLarge = 0;
    Require(sceFiberRun_nid_postfix(g_fiberB, 0, &fpLarge) == SCE_OK);
    Require(fpLarge != 0);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);

    Require(sceFiberGetThreadFramePointerAddress(&hostFpPrimary) == FiberErrorPermission);
    Require(_sceFiberGetThreadFramePointerAddress(&hostFpAlias) == FiberErrorPermission);
    Require(hostFpPrimary == 0xA5A5A5A5A5A5A5A5ull);
    Require(hostFpAlias == 0x5A5A5A5A5A5A5A5Aull);
}

void TestContextSizeCheckLifecycle() {
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);
    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid);
    Require(sceFiberStartContextSizeCheck(0x10) == FiberErrorInvalid);
    Require(sceFiberStartContextSizeCheck(0xffffffffu) == FiberErrorInvalid);
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);

    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);
    Require(sceFiberStartContextSizeCheck(0) == FiberErrorState);
    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "ctx_size_chk_on", ContextSizeCheckEntry, 0, g_smallContext, sizeof(g_smallContext), nullptr, 0) == SCE_OK);
    FiberInfo info = QueryFiberInfo(g_fiberA);
    Require(info.size_context_margin == sizeof(g_smallContext));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 333);

    info = QueryFiberInfo(g_fiberA);
    Require(info.size_context_margin != static_cast<std::uint64_t>(-1));
    Require(info.size_context_margin > 0);
    Require(info.size_context_margin < sizeof(g_smallContext));
    Require(info.size_context_margin % sizeof(std::uint64_t) == 0);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(sceFiberStopContextSizeCheck() == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "ctx_size_chk_off", ContextSizeCheckEntry, 0, g_smallContext, sizeof(g_smallContext), nullptr, 0) == SCE_OK);
    info = QueryFiberInfo(g_fiberA);
    Require(info.size_context_margin == static_cast<std::uint64_t>(-1));

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 333);

    info = QueryFiberInfo(g_fiberA);
    Require(info.size_context_margin == static_cast<std::uint64_t>(-1));
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == SCE_OK);
}

void TestInFlightRenamePreservesInfo() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "inflight_init", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "helper_fiber", InFlightRenameFiberBEntry, 0x9988776655443322ull, g_smallContext, sizeof(g_smallContext), nullptr, 0) == SCE_OK);

    AssertFiberInfoPreserved(g_fiberA, "inflight_init", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 1, &returned) == SCE_OK);
    Require(returned == 101);
    AssertFiberInfoPreserved(g_fiberA, "suspended_via_switch", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    Require(sceFiberRename(g_fiberA, "suspended_on_host") == SCE_OK);
    AssertFiberInfoPreserved(g_fiberA, "suspended_on_host", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 202, &returned) == SCE_OK);
    Require(returned == 303);
    AssertFiberInfoPreserved(g_fiberA, "running_phase_2", InFlightRenameFiberAEntry, 0x1122334455667788ull, g_largeContext, sizeof(g_largeContext));

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

} // namespace

int main() {
    TestStackBoundaryLimits();
    TestRenameStringBounds();
    TestSwitchErrorConditionsAndTransitions();
    TestThreadFramePointerAddressParity();
    TestContextSizeCheckLifecycle();
    TestInFlightRenamePreservesInfo();
    return 0;
}
