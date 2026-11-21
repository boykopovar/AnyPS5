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

} // namespace

int main() {
    TestStackBoundaryLimits();
    TestRenameStringBounds();
    TestSwitchErrorConditionsAndTransitions();
    return 0;
}
