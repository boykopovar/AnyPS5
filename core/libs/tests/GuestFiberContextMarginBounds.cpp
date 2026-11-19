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
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

alignas(16) std::uint8_t g_stackA[16384];
alignas(16) std::uint8_t g_stackB[16384];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

void APS5_VABI ShallowEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 100);
    Require(argOnRun == 200);
    Require(sceFiberReturnToThread(300, nullptr) == SCE_OK);
}

void APS5_VABI DeepStackEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 400);
    Require(argOnRun == 500);

    volatile std::uint8_t largeFrame[4096];
    for (std::size_t i = 0; i < sizeof(largeFrame); ++i) {
        largeFrame[i] = static_cast<std::uint8_t>(i & 0xff);
    }

    Require(sceFiberReturnToThread(600, nullptr) == SCE_OK);
}

void TestContextSizeCheckControlBounds() {
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);

    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid);
    Require(sceFiberStartContextSizeCheck(2) == FiberErrorInvalid);
    Require(sceFiberStartContextSizeCheck(0x10) == FiberErrorInvalid);
    Require(sceFiberStartContextSizeCheck(0xFFFFFFFFu) == FiberErrorInvalid);

    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);
    Require(sceFiberStartContextSizeCheck(0) == FiberErrorState);
    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid);

    Require(sceFiberStopContextSizeCheck() == SCE_OK);
    Require(sceFiberStopContextSizeCheck() == FiberErrorState);
}

void TestInactiveCheckMarginAndZeroSizeRejection() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "zero_ctx", ShallowEntry, 0, g_stackA, 0, nullptr, 0) == FiberErrorRange);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "inactive_fiber", ShallowEntry, 100, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin == static_cast<std::uint64_t>(-1));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 200, &returned) == SCE_OK);
    Require(returned == 300);

    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin == static_cast<std::uint64_t>(-1));

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "contextless", ShallowEntry, 100, nullptr, 0, nullptr, 0) == SCE_OK);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin == static_cast<std::uint64_t>(-1));
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestActiveCheckMarginsAndFrameConsumption() {
    Require(sceFiberStartContextSizeCheck(0) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "shallow_fiber", ShallowEntry, 100, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    FiberInfo shallowInfo{};
    shallowInfo.size = sizeof(shallowInfo);
    Require(sceFiberGetInfo(g_fiberA, &shallowInfo) == SCE_OK);
    Require(shallowInfo.size_context_margin == sizeof(g_stackA));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 200, &returned) == SCE_OK);
    Require(returned == 300);

    Require(sceFiberGetInfo(g_fiberA, &shallowInfo) == SCE_OK);
    const auto shallowMargin = shallowInfo.size_context_margin;
    Require(shallowMargin > 0);
    Require(shallowMargin < sizeof(g_stackA));
    Require(shallowMargin % 8 == 0);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "deep_fiber", DeepStackEntry, 400, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);

    FiberInfo deepInfo{};
    deepInfo.size = sizeof(deepInfo);
    Require(sceFiberGetInfo(g_fiberB, &deepInfo) == SCE_OK);
    Require(deepInfo.size_context_margin == sizeof(g_stackB));

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberB, 500, &returned) == SCE_OK);
    Require(returned == 600);

    Require(sceFiberGetInfo(g_fiberB, &deepInfo) == SCE_OK);
    const auto deepMargin = deepInfo.size_context_margin;
    Require(deepMargin > 0);
    Require(deepMargin < shallowMargin);
    Require(deepMargin % 8 == 0);

    Require(sceFiberFinalize(g_fiberB) == SCE_OK);

    Require(sceFiberStopContextSizeCheck() == SCE_OK);
}

void TestInternalOptionFlagMarginsAndInfoValidation() {
    constexpr std::uint32_t kFlagContextSizeCheck = 0x10;
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "internal_opt", ShallowEntry, 100, g_stackA, sizeof(g_stackA), nullptr, kFlagContextSizeCheck, 0) == SCE_OK);

    FiberInfo info{};
    Require(sceFiberGetInfo(nullptr, &info) == FiberErrorNull);
    Require(sceFiberGetInfo(g_fiberA, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberGetInfo(reinterpret_cast<FiberObject*>(uninitStorage), &info) == FiberErrorInvalid);

    info.size = 0;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    info.size = sizeof(info) - 8;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);
    info.size = sizeof(info) + 8;
    Require(sceFiberGetInfo(g_fiberA, &info) == FiberErrorInvalid);

    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin == sizeof(g_stackA));

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 200, &returned) == SCE_OK);
    Require(returned == 300);

    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(info.size_context_margin > 0);
    Require(info.size_context_margin < sizeof(g_stackA));
    Require(info.size_context_margin % 8 == 0);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

} // namespace

int main() {
    TestContextSizeCheckControlBounds();
    TestInactiveCheckMarginAndZeroSizeRejection();
    TestActiveCheckMarginsAndFrameConsumption();
    TestInternalOptionFlagMarginsAndInfoValidation();
    return 0;
}
