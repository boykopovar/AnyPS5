#include "prx/libSceFiber/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <xmmintrin.h>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Check failed: %s\n", message);
        std::abort();
    }
}

namespace {

constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t FiberErrorAlignment = static_cast<std::int32_t>(0x80590002);
constexpr std::int32_t FiberErrorRange = static_cast<std::int32_t>(0x80590003);
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t FiberErrorPermission = static_cast<std::int32_t>(0x80590005);
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);
constexpr std::int32_t FiberErrorInvalidArgument = static_cast<std::int32_t>(0x80A60001);
constexpr std::int32_t FiberErrorOutOfMemory = static_cast<std::int32_t>(0x80A60002);

struct alignas(8) FiberStorage {
    unsigned char bytes[0x100];
};

alignas(16) std::array<unsigned char, 16 * 1024> g_stack16Kb;
alignas(16) std::array<unsigned char, 32 * 1024> g_stack32Kb;
alignas(16) std::array<unsigned char, 64 * 1024> g_stack64Kb;

FiberStorage g_storage16;
FiberStorage g_storage32;
FiberStorage g_storage64;

auto* const g_fiber16 = reinterpret_cast<FiberObject*>(&g_storage16);
auto* const g_fiber32 = reinterpret_cast<FiberObject*>(&g_storage32);
auto* const g_fiber64 = reinterpret_cast<FiberObject*>(&g_storage64);

static bool IsUndersizedError(std::int32_t code) {
    return code == FiberErrorRange || code == FiberErrorInvalidArgument || code == FiberErrorInvalid;
}

void APS5_VABI Entry16Kb(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x1000, "valid initial argument in 16KB fiber");
    Require(argOnRun == 0x2000, "valid run argument in 16KB fiber");

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == 0 && self == g_fiber16, "self matches 16KB fiber object");

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiber16, &info) == 0, "get info on 16KB fiber");
    Require(info.size_context == g_stack16Kb.size(), "context size is 16KB");

    std::uint64_t received = 0;
    Require(sceFiberReturnToThread(0x3000, &received) == 0, "16KB fiber return to thread");
    Require(received == 0x4000, "16KB fiber resumed with expected argument");

    for (;;) {
        sceFiberReturnToThread(0x5000, &received);
    }
}

void APS5_VABI Entry32Kb(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x3201, "32KB initial argument");
    Require(argOnRun == 0x3202, "32KB run argument");

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiber64, 0x6401, &received) == 0, "switch from 32KB to 64KB fiber");
    Require(received == 0x6402, "switch back to 32KB fiber");

    for (;;) {
        sceFiberReturnToThread(0x3203, &received);
    }
}

void APS5_VABI Entry64Kb(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 0x6400, "64KB initial argument");
    Require(argOnRun == 0x6401, "64KB run argument from switch");

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_fiber64, &info) == 0, "get info on 64KB fiber");
    Require(info.size_context == g_stack64Kb.size(), "context size is 64KB");

    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_fiber32, 0x6402, &received) == 0, "switch back from 64KB to 32KB");

    for (;;) {
        sceFiberReturnToThread(0x6403, &received);
    }
}

static void TestValidStack16KbExecution() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber16, "fiber_16kb", Entry16Kb, 0x1000, g_stack16Kb.data(), g_stack16Kb.size(), nullptr, 0) == 0, "initialize 16KB fiber");

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber16, 0x2000, &returned) == 0, "run 16KB fiber");
    Require(returned == 0x3000, "returned value matches 16KB fiber first yield");

    Require(sceFiberRun_nid_postfix(g_fiber16, 0x4000, &returned) == 0, "resume 16KB fiber");
    Require(returned == 0x5000, "returned value matches 16KB fiber second yield");

    Require(sceFiberFinalize(g_fiber16) == 0, "finalize 16KB fiber");
}

static void TestValidStack32KbAnd64KbExecution() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber32, "fiber_32kb", Entry32Kb, 0x3201, g_stack32Kb.data(), g_stack32Kb.size(), nullptr, 0) == 0, "initialize 32KB fiber");
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiber64, "fiber_64kb", Entry64Kb, 0x6400, g_stack64Kb.data(), g_stack64Kb.size(), nullptr, 0) == 0, "initialize 64KB fiber");

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiber32, 0x3202, &returned) == 0, "run 32KB fiber and perform cooperative switches");
    Require(returned == 0x3203, "returned value from 32KB fiber");

    Require(sceFiberFinalize(g_fiber32) == 0, "finalize 32KB fiber");
    Require(sceFiberFinalize(g_fiber64) == 0, "finalize 64KB fiber");
}

static void TestUndersizedStackBounds() {
    FiberStorage storage{};
    auto* fiber = reinterpret_cast<FiberObject*>(&storage);
    alignas(16) std::array<unsigned char, 1024> buffer{};

    const std::uint64_t undersizedSizes[] = {0, 64, 128, 256, 480, 500};
    for (const auto size : undersizedSizes) {
        if (size == 0) {
            continue;
        }
        const auto err = _sceFiberInitializeImpl_nid_postfix(fiber, "undersized", Entry16Kb, 0, buffer.data(), size, nullptr, 0);
        if ((size & 15) != 0) {
            Require(err == FiberErrorAlignment, "unaligned undersized size rejected with alignment error");
        } else {
            Require(IsUndersizedError(err), "undersized stack size rejected with range or invalid argument");
        }
    }

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "valid_size", Entry16Kb, 0, buffer.data(), 512, nullptr, 0) == 0, "512 bytes minimum context accepted");
    Require(sceFiberFinalize(fiber) == 0, "finalize 512-byte fiber");

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "attach_undersized", Entry16Kb, 0, nullptr, 0, nullptr, 0) == 0, "initialize attach fiber");
    const auto undersizedErr = _sceFiberAttachContextAndRun(fiber, buffer.data(), 256, 0, nullptr);
    Require(IsUndersizedError(undersizedErr), "attach undersized context rejected");
    const auto zeroErr = _sceFiberAttachContextAndRun(fiber, buffer.data(), 0, 0, nullptr);
    Require(zeroErr == FiberErrorRange || zeroErr == FiberErrorInvalid || IsUndersizedError(zeroErr), "attach zero size context rejected");
    Require(sceFiberFinalize(fiber) == 0, "finalize attach fiber");
}

static void TestMisalignedStackAddressAndSize() {
    FiberStorage storage{};
    auto* fiber = reinterpret_cast<FiberObject*>(&storage);
    alignas(16) std::array<unsigned char, 16 * 1024 + 32> buffer{};

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "misaligned_addr", Entry16Kb, 0, buffer.data() + 1, 16 * 1024, nullptr, 0) == FiberErrorAlignment, "misaligned context address +1 rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "misaligned_addr_8", Entry16Kb, 0, buffer.data() + 8, 16 * 1024, nullptr, 0) == FiberErrorAlignment, "misaligned context address +8 rejected");

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "misaligned_size_1", Entry16Kb, 0, buffer.data(), 16 * 1024 + 1, nullptr, 0) == FiberErrorAlignment, "misaligned context size +1 rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "misaligned_size_8", Entry16Kb, 0, buffer.data(), 16 * 1024 + 8, nullptr, 0) == FiberErrorAlignment, "misaligned context size +8 rejected");
}

static void TestNullPointersAndInvalidDescriptors() {
    FiberStorage storage{};
    auto* fiber = reinterpret_cast<FiberObject*>(&storage);

    Require(_sceFiberInitializeImpl_nid_postfix(nullptr, "test", Entry16Kb, 0, g_stack16Kb.data(), g_stack16Kb.size(), nullptr, 0) == FiberErrorNull, "null object initialize rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, nullptr, Entry16Kb, 0, g_stack16Kb.data(), g_stack16Kb.size(), nullptr, 0) == FiberErrorNull, "null name initialize rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", nullptr, 0, g_stack16Kb.data(), g_stack16Kb.size(), nullptr, 0) == FiberErrorNull, "null entry initialize rejected");

    alignas(16) unsigned char unalignedObject[0x100 + 8]{};
    auto* unalignedFiber = reinterpret_cast<FiberObject*>(unalignedObject + 1);
    Require(_sceFiberInitializeImpl_nid_postfix(unalignedFiber, "test", Entry16Kb, 0, g_stack16Kb.data(), g_stack16Kb.size(), nullptr, 0) == FiberErrorAlignment, "unaligned object initialize rejected");

    Require(sceFiberRun_nid_postfix(nullptr, 0, nullptr) == FiberErrorNull, "null fiber run rejected");
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull, "null fiber switch rejected");
    Require(sceFiberFinalize(nullptr) == FiberErrorNull, "null fiber finalize rejected");
    Require(sceFiberGetInfo(nullptr, nullptr) == FiberErrorNull, "null fiber and null info rejected");

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(nullptr, &info) == FiberErrorNull, "null fiber getInfo rejected");
    Require(sceFiberRename(nullptr, "name") == FiberErrorNull, "null fiber rename rejected");
    Require(sceFiberRename(fiber, nullptr) == FiberErrorNull, "null name rename rejected");

    alignas(8) unsigned char invalidMemory[0x100]{};
    auto* invalidFiber = reinterpret_cast<FiberObject*>(invalidMemory);
    Require(sceFiberRun_nid_postfix(invalidFiber, 0, nullptr) == FiberErrorInvalid, "uninitialized fiber run rejected");
    Require(sceFiberSwitch(invalidFiber, 0, nullptr) == FiberErrorInvalid, "uninitialized fiber switch rejected");
    Require(sceFiberFinalize(invalidFiber) == FiberErrorInvalid, "uninitialized fiber finalize rejected");
    Require(sceFiberGetInfo(invalidFiber, &info) == FiberErrorInvalid, "uninitialized fiber getInfo rejected");
    Require(sceFiberRename(invalidFiber, "new_name") == FiberErrorInvalid, "uninitialized fiber rename rejected");

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(nullptr) == FiberErrorNull, "null self pointer rejected");
    Require(sceFiberGetSelf(&self) == FiberErrorPermission, "getSelf outside fiber rejected");
    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission, "returnToThread outside fiber rejected");

    std::uint64_t framePointer = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull, "null frame pointer rejected");
    Require(sceFiberGetThreadFramePointerAddress(&framePointer) == FiberErrorPermission, "get frame pointer outside fiber rejected");
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull, "null underscore frame pointer rejected");
    Require(_sceFiberGetThreadFramePointerAddress(&framePointer) == FiberErrorPermission, "get underscore frame pointer outside fiber rejected");

    Require(_sceFiberAttachContextAndRun(nullptr, g_stack16Kb.data(), g_stack16Kb.size(), 0, nullptr) == FiberErrorNull, "attach to null fiber rejected");
    Require(_sceFiberAttachContextAndRun(fiber, nullptr, g_stack16Kb.size(), 0, nullptr) == FiberErrorInvalid, "attach null context with non-zero size rejected");
}

static void TestOptParamBounds() {
    Require(sceFiberOptParamInitialize(nullptr) == FiberErrorNull, "null opt param rejected");
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(0x8)) == FiberErrorInvalid, "invalid opt param pointer 0x8 rejected");
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(0x10)) == FiberErrorInvalid, "invalid opt param pointer 0x10 rejected");

    FiberOptParam optParam{};
    std::memset(&optParam, 0xef, sizeof(optParam));
    Require(sceFiberOptParamInitialize(&optParam) == 0, "valid opt param initialized");

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&optParam);
    bool allZero = true;
    for (std::size_t i = 0; i < sizeof(optParam); ++i) {
        if (bytes[i] != 0) {
            allZero = false;
            break;
        }
    }
    Require(allZero, "opt param memory cleared to zero");
}

void APS5_VABI MarginCheckEntry(std::uint64_t, std::uint64_t) {
    volatile unsigned char stackConsumption[2048];
    for (auto& b : stackConsumption) {
        b = 0x7c;
    }

    std::uint64_t received = 0;
    for (;;) {
        sceFiberReturnToThread(0x9001, &received);
    }
}

static void TestGuardPageStackMargin() {
    FiberStorage storage{};
    auto* fiber = reinterpret_cast<FiberObject*>(&storage);
    alignas(16) std::array<unsigned char, 16 * 1024> marginContext;

    Require(sceFiberStopContextSizeCheck() == FiberErrorState, "stop size check before start rejected");
    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid, "start size check with invalid flag rejected");
    Require(sceFiberStartContextSizeCheck(0) == 0, "start size check succeeded");
    Require(sceFiberStartContextSizeCheck(0) == FiberErrorState, "start size check already active rejected");

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "margin_fiber", MarginCheckEntry, 0, marginContext.data(), marginContext.size(), nullptr, 0) == 0, "init margin fiber");

    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(fiber, &info) == 0, "get info on margin fiber before run");
    Require(info.size_context_margin == marginContext.size(), "untouched context margin equals total context size");

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(fiber, 0, &returned) == 0, "run margin fiber");
    Require(returned == 0x9001, "margin fiber yielded value");

    Require(sceFiberGetInfo(fiber, &info) == 0, "get info on margin fiber after run");
    Require(info.size_context_margin > 0 && info.size_context_margin < marginContext.size(), "margin decreased after stack consumption");
    Require((info.size_context_margin % 8) == 0, "margin aligned to word boundary");

    Require(sceFiberFinalize(fiber) == 0, "finalize margin fiber");
    Require(sceFiberStopContextSizeCheck() == 0, "stop size check succeeded");
    Require(sceFiberStopContextSizeCheck() == FiberErrorState, "stop size check when inactive rejected");
}

static void TestStackStateTransitions() {
    FiberStorage storage{};
    auto* fiber = reinterpret_cast<FiberObject*>(&storage);
    alignas(16) std::array<unsigned char, 16 * 1024> context;

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "trans_fiber", Entry16Kb, 0x1000, context.data(), context.size(), nullptr, 0) == 0, "init transition fiber");
    Require(sceFiberFinalize(fiber) == 0, "finalize idle fiber succeeds");
    Require(sceFiberFinalize(fiber) == FiberErrorInvalid, "finalize already finalized fiber rejected");
    Require(sceFiberRun_nid_postfix(fiber, 0x2000, nullptr) == FiberErrorInvalid, "run finalized fiber rejected");

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "reinit_fiber", Entry16Kb, 0x1000, context.data(), context.size(), nullptr, 0) == 0, "reinit after finalize succeeds");
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(fiber, 0x2000, &returned) == 0 && returned == 0x3000, "run reinitialized fiber succeeds");
    Require(sceFiberFinalize(fiber) == 0, "finalize reinitialized fiber succeeds");
}

} // namespace

int main() {
    TestValidStack16KbExecution();
    TestValidStack32KbAnd64KbExecution();
    TestUndersizedStackBounds();
    TestMisalignedStackAddressAndSize();
    TestNullPointersAndInvalidDescriptors();
    TestOptParamBounds();
    TestGuardPageStackMargin();
    TestStackStateTransitions();
    return 0;
}
