#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
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

alignas(16) std::uint8_t g_stackA[16384];
alignas(16) std::uint8_t g_stackB[16384];
alignas(16) std::uint8_t g_minStack[512];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

void APS5_VABI ExecutionEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x12345678ull);
    Require(argOnRun == 0x87654321ull);
    Require(sceFiberReturnToThread(0xfeedfaceull, nullptr) == SCE_OK);
}

void APS5_VABI MinStackEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 42);
    Require(argOnRun == 84);
    Require(sceFiberReturnToThread(126, nullptr) == SCE_OK);
}

void APS5_VABI ReinitEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 500);
    Require(argOnRun == 600);
    Require(sceFiberReturnToThread(1100, nullptr) == SCE_OK);
}

void TestFiberObjectAndNameNullValidation() {
    Require(_sceFiberInitializeImpl_nid_postfix(nullptr, "test", ExecutionEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, nullptr, ExecutionEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "test", nullptr, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeImpl_nid_postfix(nullptr, nullptr, nullptr, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == FiberErrorNull);

    constexpr std::uint32_t kFlagNoUlobjmgr = 0x1;
    Require(_sceFiberInitializeWithInternalOptionImpl(nullptr, "test", ExecutionEntry, 0, g_stackA, sizeof(g_stackA), nullptr, kFlagNoUlobjmgr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, nullptr, ExecutionEntry, 0, g_stackA, sizeof(g_stackA), nullptr, kFlagNoUlobjmgr, 0) == FiberErrorNull);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "test", nullptr, 0, g_stackA, sizeof(g_stackA), nullptr, kFlagNoUlobjmgr, 0) == FiberErrorNull);
}

void TestFiberObjectAlignmentValidation() {
    alignas(16) std::uint8_t storage[512];
    for (std::uintptr_t offset = 1; offset <= 7; ++offset) {
        auto* unalignedObj = reinterpret_cast<FiberObject*>(storage + offset);
        Require(_sceFiberInitializeImpl_nid_postfix(unalignedObj, "unaligned", ExecutionEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0) == FiberErrorAlignment);
        Require(_sceFiberInitializeWithInternalOptionImpl(unalignedObj, "unaligned_opt", ExecutionEntry, 0, g_stackA, sizeof(g_stackA), nullptr, 0, 0) == FiberErrorAlignment);
    }
}

void TestContextAddressAlignmentBounds() {
    for (std::uintptr_t offset = 1; offset <= 15; ++offset) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "misaligned_addr", ExecutionEntry, 0, g_stackA + offset, 8192, nullptr, 0) == FiberErrorAlignment);
        Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "misaligned_addr_opt", ExecutionEntry, 0, g_stackA + offset, 8192, nullptr, 0, 0) == FiberErrorAlignment);
    }
}

void TestContextSizeAlignmentAndZeroRejection() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "zero_stack", ExecutionEntry, 0, g_stackA, 0, nullptr, 0) == FiberErrorRange);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "zero_stack_opt", ExecutionEntry, 0, g_stackA, 0, nullptr, 0, 0) == FiberErrorRange);

    const std::uint64_t unalignedSizes[] = {513, 514, 515, 518, 521, 527, 1025, 2049, 4097};
    for (const auto size : unalignedSizes) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "unaligned_size", ExecutionEntry, 0, g_stackA, size, nullptr, 0) == FiberErrorAlignment);
        Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "unaligned_size_opt", ExecutionEntry, 0, g_stackA, size, nullptr, 0, 0) == FiberErrorAlignment);
    }

    const std::uint64_t underMinSizes[] = {16, 32, 64, 128, 256, 384, 480, 496};
    for (const auto size : underMinSizes) {
        Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "under_min", ExecutionEntry, 0, g_stackA, size, nullptr, 0) == FiberErrorRange);
        Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "under_min_opt", ExecutionEntry, 0, g_stackA, size, nullptr, 0, 0) == FiberErrorRange);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "null_nonzero", ExecutionEntry, 0, nullptr, 4096, nullptr, 0) == FiberErrorInvalid);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "null_nonzero_opt", ExecutionEntry, 0, nullptr, 4096, nullptr, 0, 0) == FiberErrorInvalid);
}

void TestContextExecutionAndReinitialization() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "min_fiber", MinStackEntry, 42, g_minStack, sizeof(g_minStack), nullptr, 0) == SCE_OK);
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 84, &returned) == SCE_OK);
    Require(returned == 126);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "initial_stack", ExecutionEntry, 0x12345678ull, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0x87654321ull, &returned) == SCE_OK);
    Require(returned == 0xfeedfaceull);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_stack", ReinitEntry, 500, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 600, &returned) == SCE_OK);
    Require(returned == 1100);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    constexpr std::uint32_t kFlags = 0x101;
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberB, "opt_exec", ExecutionEntry, 0x12345678ull, g_stackB, sizeof(g_stackB), nullptr, kFlags, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberB, 0x87654321ull, &returned) == SCE_OK);
    Require(returned == 0xfeedfaceull);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

} // namespace

int main() {
    TestFiberObjectAndNameNullValidation();
    TestFiberObjectAlignmentValidation();
    TestContextAddressAlignmentBounds();
    TestContextSizeAlignmentAndZeroRejection();
    TestContextExecutionAndReinitialization();
    return 0;
}
