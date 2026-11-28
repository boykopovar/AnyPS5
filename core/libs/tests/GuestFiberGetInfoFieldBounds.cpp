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

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

alignas(16) std::uint8_t g_smallStack[16384];
alignas(16) std::uint8_t g_largeStack[65536];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

void APS5_VABI SmallEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 12345);
    Require(argOnRun == 54321);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_smallStack));

    volatile std::uint64_t marker = 0xa1b2c3d4e5f60718ull;
    const auto markerAddr = reinterpret_cast<std::uintptr_t>(&marker);
    const auto low = reinterpret_cast<std::uintptr_t>(info.addr_context);
    const auto high = low + info.size_context;
    Require(markerAddr >= low && markerAddr < high);

    Require(sceFiberReturnToThread(77777, nullptr) == SCE_OK);
}

void APS5_VABI LargeEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 98765);
    Require(argOnRun == 56789);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_largeStack));
    Require(info.addr_context == g_largeStack);

    volatile std::uint64_t marker = 0x1807f6e5d4c3b2a1ull;
    const auto markerAddr = reinterpret_cast<std::uintptr_t>(&marker);
    const auto low = reinterpret_cast<std::uintptr_t>(g_largeStack);
    const auto high = low + sizeof(g_largeStack);
    Require(markerAddr >= low && markerAddr < high);

    Require(sceFiberReturnToThread(88888, nullptr) == SCE_OK);
}

void APS5_VABI MarginCheckEntry(std::uint64_t, std::uint64_t) {
    volatile std::uint8_t frame[1024];
    for (std::size_t i = 0; i < sizeof(frame); ++i) {
        frame[i] = static_cast<std::uint8_t>(i & 0xff);
    }
    Require(sceFiberReturnToThread(99999, nullptr) == SCE_OK);
}

void TestNullAndInvalidFiberHandleRejection() {
    Require(sceFiberGetInfo(nullptr, nullptr) == FiberErrorNull);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(nullptr, &info) == FiberErrorNull);
    Require(sceFiberGetInfo(g_fiberA, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberGetInfo(reinterpret_cast<FiberObject*>(uninitStorage), &info) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "temp", SmallEntry, 12345, g_smallStack, sizeof(g_smallStack), nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
}

void TestInfoStructureSizeValidation() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "size_val", SmallEntry, 12345, g_smallStack, sizeof(g_smallStack), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = 0;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);

    info.size = sizeof(FiberInfo) - 1;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);

    info.size = sizeof(FiberInfo) + 1;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);

    info.size = sizeof(FiberInfo) - 8;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);

    info.size = sizeof(FiberInfo) + 8;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);

    info.size = sizeof(FiberInfo);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestFieldVerificationSmallStackAndRenaming() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "initial_name", SmallEntry, 12345, g_smallStack, sizeof(g_smallStack), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.entry == SmallEntry);
    Require(info.arg_on_initialize == 12345);
    Require(info.size_context == sizeof(g_smallStack));
    Require(info.size_context_margin == static_cast<std::uint64_t>(-1));
    Require(std::strcmp(info.name, "initial_name") == 0);

    Require(sceFiberRename(g_fiberA, "renamed_fiber") == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strcmp(info.name, "renamed_fiber") == 0);

    const char* exact31 = "1234567890123456789012345678901";
    Require(sceFiberRename(g_fiberA, exact31) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strlen(info.name) == 31);
    Require(std::strcmp(info.name, exact31) == 0);

    const char* overlong = "abcdefghijklmnopqrstuvwxyz0123456789_extra_tail";
    Require(sceFiberRename(g_fiberA, overlong) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strlen(info.name) == 31);
    Require(std::strncmp(info.name, "abcdefghijklmnopqrstuvwxyz01234", 31) == 0);
    Require(info.name[31] == '\0');

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 54321, &returned) == SCE_OK);
    Require(returned == 77777);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestFieldVerificationLargeStack() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "large_fiber", LargeEntry, 98765, g_largeStack, sizeof(g_largeStack), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.entry == LargeEntry);
    Require(info.arg_on_initialize == 98765);
    Require(info.size_context == sizeof(g_largeStack));
    Require(info.addr_context == g_largeStack);
    Require(info.size_context_margin == static_cast<std::uint64_t>(-1));
    Require(std::strcmp(info.name, "large_fiber") == 0);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 56789, &returned) == SCE_OK);
    Require(returned == 88888);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestContextSizeMarginReportingUnderCheck() {
    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "margin_fiber", MarginCheckEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberB, &info) == SCE_OK);
    Require(info.size_context_margin == sizeof(g_smallStack));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberB, 0, &returned) == SCE_OK);
    Require(returned == 99999);

    Require(sceFiberGetInfo(g_fiberB, &info) == SCE_OK);
    Require(info.size_context_margin > 0);
    Require(info.size_context_margin < sizeof(g_smallStack));
    Require(info.size_context_margin % 8 == 0);

    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == SCE_OK);
}

} // namespace

int main() {
    TestNullAndInvalidFiberHandleRejection();
    TestInfoStructureSizeValidation();
    TestFieldVerificationSmallStackAndRenaming();
    TestFieldVerificationLargeStack();
    TestContextSizeMarginReportingUnderCheck();
    return 0;
}
