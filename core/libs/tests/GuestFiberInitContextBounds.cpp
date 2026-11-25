#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
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
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

alignas(16) std::uint8_t g_stackA[16384];
alignas(16) std::uint8_t g_stackB[16384];
alignas(16) std::uint8_t g_minStack[512];

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);

std::uint32_t g_contextlessRunCount = 0;
std::uintptr_t g_callerStackAnchor = 0;
std::uint64_t g_lastContextlessArgOnRun = 0;

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

void APS5_VABI ContextlessRestartEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x55aa1122ull);
    ++g_contextlessRunCount;
    g_lastContextlessArgOnRun = argOnRun;

    volatile std::uint64_t entryMarker = 0;
    const auto entryAddr = reinterpret_cast<std::uintptr_t>(&entryMarker);
    Require(g_callerStackAnchor != 0);
    Require(entryAddr < g_callerStackAnchor && (g_callerStackAnchor - entryAddr) < 65536);

    const auto stackALow = reinterpret_cast<std::uintptr_t>(g_stackA);
    const auto stackAHigh = stackALow + sizeof(g_stackA);
    Require(entryAddr < stackALow || entryAddr >= stackAHigh);

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == SCE_OK);
    Require(self != nullptr);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(self, &info) == SCE_OK);
    Require(info.addr_context == nullptr);
    Require(info.size_context == 0);

    std::uint64_t localStage = 1;
    Require(localStage == 1);
    localStage = 2;
    Require(sceFiberReturnToThread(0x1000ull + g_contextlessRunCount + argOnRun, nullptr) == SCE_OK);
    Require(false);
}

void APS5_VABI ActiveReinitEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 700);
    Require(argOnRun == 701);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_run", ExecutionEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == FiberErrorState);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "reinit_run_opt", ExecutionEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0, 0) == FiberErrorState);
    Require(sceFiberFinalize(g_fiberA) == FiberErrorState);

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiberA, &info) == SCE_OK);
    Require(std::strcmp(info.name, "active_reinit") == 0);
    Require(info.arg_on_initialize == 700);

    std::uint64_t resumedArg = 0;
    Require(sceFiberReturnToThread(702, &resumedArg) == SCE_OK);
    Require(resumedArg == 703);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_run2", ExecutionEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0) == FiberErrorState);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "reinit_run2_opt", ExecutionEntry, 0, g_stackB, sizeof(g_stackB), nullptr, 0, 0) == FiberErrorState);

    std::uint64_t thirdArg = 0;
    Require(sceFiberReturnToThread(704, &thirdArg) == SCE_OK);
    Require(thirdArg == 705);
    Require(sceFiberReturnToThread(706, nullptr) == SCE_OK);
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

void TestContextlessCallerStackRestart() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "ctxless_bad_sz", ContextlessRestartEntry, 0, nullptr, 512, nullptr, 0) == FiberErrorInvalid);
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberA, "ctxless_bad_sz_opt", ContextlessRestartEntry, 0, nullptr, 16, nullptr, 0, 0) == FiberErrorInvalid);

    volatile std::uint64_t callerMarker = 0;
    g_callerStackAnchor = reinterpret_cast<std::uintptr_t>(&callerMarker);

    g_contextlessRunCount = 0;
    g_lastContextlessArgOnRun = 0;
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "ctxless_restart", ContextlessRestartEntry, 0x55aa1122ull, nullptr, 0, nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 10, &returned) == SCE_OK);
    Require(g_contextlessRunCount == 1);
    Require(g_lastContextlessArgOnRun == 10);
    Require(returned == 0x1000ull + 1 + 10);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 25, &returned) == SCE_OK);
    Require(g_contextlessRunCount == 2);
    Require(g_lastContextlessArgOnRun == 25);
    Require(returned == 0x1000ull + 2 + 25);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    g_contextlessRunCount = 0;
    g_lastContextlessArgOnRun = 0;
    constexpr std::uint32_t kFlagSetFpuRegisters = 0x100;
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberB, "ctxless_opt", ContextlessRestartEntry, 0x55aa1122ull, nullptr, 0, nullptr, kFlagSetFpuRegisters, 0x03500001u) == SCE_OK);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberB, 31, &returned) == SCE_OK);
    Require(g_contextlessRunCount == 1);
    Require(g_lastContextlessArgOnRun == 31);
    Require(returned == 0x1000ull + 1 + 31);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberB, 42, &returned) == SCE_OK);
    Require(g_contextlessRunCount == 2);
    Require(g_lastContextlessArgOnRun == 42);
    Require(returned == 0x1000ull + 2 + 42);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);

    g_callerStackAnchor = 0;
}

void TestOptParamInitializeAndBuildVersion() {
    Require(sceFiberOptParamInitialize(nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t optStorage[136];
    std::memset(optStorage, 0xA5, sizeof(optStorage));
    for (std::uintptr_t offset = 1; offset <= 7; ++offset) {
        Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(optStorage + offset)) == FiberErrorAlignment);
    }
    for (std::size_t i = 0; i < sizeof(optStorage); ++i) {
        Require(optStorage[i] == 0xA5);
    }

    errno = 71;
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(0x10)) == FiberErrorInvalid);
    Require(errno == 71);

    constexpr std::size_t kOptParamSize = 0x80;
    constexpr std::uint32_t kOptParamMagic = 0xBB40E64Du;
    auto* optParam = reinterpret_cast<FiberOptParam*>(optStorage);
    errno = 71;
    Require(sceFiberOptParamInitialize(optParam) == SCE_OK);
    Require(errno == 71);

    std::uint32_t magic = 0;
    std::memcpy(&magic, optStorage, sizeof(magic));
    Require(magic == kOptParamMagic);
    for (std::size_t i = sizeof(magic); i < kOptParamSize; ++i) {
        Require(optStorage[i] == 0);
    }
    for (std::size_t i = kOptParamSize; i < sizeof(optStorage); ++i) {
        Require(optStorage[i] == 0xA5);
    }

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "opt_impl", ExecutionEntry, 0x12345678ull, g_stackA, sizeof(g_stackA), optParam, 0x03500001u) == SCE_OK);
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0x87654321ull, &returned) == SCE_OK);
    Require(returned == 0xfeedfaceull);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    constexpr std::uint32_t kFlags = 0x101;
    Require(_sceFiberInitializeWithInternalOptionImpl(g_fiberB, "opt_internal", ExecutionEntry, 0x12345678ull, g_stackB, sizeof(g_stackB), optParam, kFlags, 0x03500001u) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberB, 0x87654321ull, &returned) == SCE_OK);
    Require(returned == 0xfeedfaceull);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
}

void TestActiveAndSuspendedReinitRejection() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "active_reinit", ActiveReinitEntry, 700, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 701, &returned) == SCE_OK);
    Require(returned == 702);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 703, &returned) == SCE_OK);
    Require(returned == 704);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "reinit_susp", ReinitEntry, 500, g_stackB, sizeof(g_stackB), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 600, &returned) == SCE_OK);
    Require(returned == 1100);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "active_reinit", ActiveReinitEntry, 700, g_stackA, sizeof(g_stackA), nullptr, 0) == SCE_OK);
    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 701, &returned) == SCE_OK);
    Require(returned == 702);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 703, &returned) == SCE_OK);
    Require(returned == 704);

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 705, &returned) == SCE_OK);
    Require(returned == 706);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

} // namespace

int main() {
    TestFiberObjectAndNameNullValidation();
    TestFiberObjectAlignmentValidation();
    TestContextAddressAlignmentBounds();
    TestContextSizeAlignmentAndZeroRejection();
    TestContextExecutionAndReinitialization();
    TestContextlessCallerStackRestart();
    TestOptParamInitializeAndBuildVersion();
    TestActiveAndSuspendedReinitRejection();
    return 0;
}
