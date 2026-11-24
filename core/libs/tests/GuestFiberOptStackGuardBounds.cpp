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

alignas(16) std::uint8_t g_smallStack[16384];
alignas(16) std::uint8_t g_largeStack[65536];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

void APS5_VABI MinValidExecutionEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 1111);
    Require(argOnRun == 2222);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == 512);

    Require(sceFiberReturnToThread(3333, nullptr) == SCE_OK);
}

void APS5_VABI SmallExecutionEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 1111);
    Require(argOnRun == 2222);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_smallStack));

    Require(sceFiberReturnToThread(3333, nullptr) == SCE_OK);
}

void APS5_VABI LargeExecutionEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 4444);
    Require(argOnRun == 5555);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context == sizeof(g_largeStack));
    Require(info.addr_context == g_largeStack);

    Require(sceFiberReturnToThread(6666, nullptr) == SCE_OK);
}

void APS5_VABI GuardCheckEntry(std::uint64_t, std::uint64_t) {
    volatile std::uint8_t fill[512];
    for (std::size_t i = 0; i < sizeof(fill); ++i) {
        fill[i] = static_cast<std::uint8_t>(i & 0xff);
    }
    Require(sceFiberReturnToThread(7777, nullptr) == SCE_OK);
}

void TestOptParamBoundsAndZeroing() {
    Require(sceFiberOptParamInitialize(nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t misalignedParam[sizeof(FiberOptParam) + 8]{};
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(misalignedParam + 1)) == FiberErrorAlignment);

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

    *__error_nid_postfix() = 123;
    Require(sceFiberOptParamInitialize(&guarded.opt) == SCE_OK);
    Require(*__error_nid_postfix() == 123);

    const auto* paramBytes = reinterpret_cast<const std::uint8_t*>(&guarded.opt);
    for (std::size_t i = 0; i < 0x80; ++i) {
        Require(paramBytes[i] == 0);
    }

    for (std::size_t i = 0; i < 4; ++i) {
        Require(guarded.headSentinels[i] == 0x1122334455667788ull + i);
        Require(guarded.tailSentinels[i] == 0x8877665544332211ull + i);
    }
}

void TestStackRangeBoundsAndAlignment() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "zero_sz", SmallExecutionEntry, 0, g_smallStack, 0, nullptr, 0) == FiberErrorRange);

    for (std::uintptr_t offset = 1; offset <= 15; ++offset) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_addr", SmallExecutionEntry, 0, g_smallStack + offset, 8192, nullptr, 0) == FiberErrorAlignment);
    }

    const std::uint64_t unalignedSizes[] = {513, 514, 515, 517, 520, 1025, 2049, 4097};
    for (const auto size : unalignedSizes) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_sz", SmallExecutionEntry, 0, g_smallStack, size, nullptr, 0) == FiberErrorAlignment);
    }

    const std::uint64_t subMinSizes[] = {16, 32, 64, 128, 256, 384, 480, 496};
    for (const auto size : subMinSizes) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "sub_min", SmallExecutionEntry, 0, g_smallStack, size, nullptr, 0) == FiberErrorRange);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "min_valid", MinValidExecutionEntry, 1111, g_smallStack, 512, nullptr, 0) == SCE_OK);
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 2222, &returned) == SCE_OK);
    Require(returned == 3333);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestStackGuardPagesAndHostMargins() {
    struct alignas(16) StackWrapper {
        std::uint64_t headGuard[8];
        alignas(16) std::uint8_t stack[16384];
        std::uint64_t tailGuard[8];
    } wrapper;

    for (std::size_t i = 0; i < 8; ++i) {
        wrapper.headGuard[i] = 0xdeadbeefcafebabeull + i;
        wrapper.tailGuard[i] = 0xcafebabedeadbeefull + i;
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "guard_check", GuardCheckEntry, 0, wrapper.stack, sizeof(wrapper.stack), nullptr, 0) == SCE_OK);
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 7777);

    for (std::size_t i = 0; i < 8; ++i) {
        Require(wrapper.headGuard[i] == 0xdeadbeefcafebabeull + i);
        Require(wrapper.tailGuard[i] == 0xcafebabedeadbeefull + i);
    }

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestStackInfoAndContextMarginValidation() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "small_exec", SmallExecutionEntry, 1111, g_smallStack, sizeof(g_smallStack), nullptr, 0) == SCE_OK);
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 2222, &returned) == SCE_OK);
    Require(returned == 3333);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "large_exec", LargeExecutionEntry, 4444, g_largeStack, sizeof(g_largeStack), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 5555, &returned) == SCE_OK);
    Require(returned == 6666);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "margin_test", GuardCheckEntry, 0, g_smallStack, sizeof(g_smallStack), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin <= sizeof(g_smallStack));

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 7777);

    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin <= sizeof(g_smallStack));

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == SCE_OK);
}

} // namespace

int main() {
    TestOptParamBoundsAndZeroing();
    TestStackRangeBoundsAndAlignment();
    TestStackGuardPagesAndHostMargins();
    TestStackInfoAndContextMarginValidation();
    return 0;
}
