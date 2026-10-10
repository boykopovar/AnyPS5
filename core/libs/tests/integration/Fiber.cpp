#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <xmmintrin.h>

using Entry = void (APS5_VABI*)(std::uint64_t, std::uint64_t);

extern "C" {
std::int32_t APS5_VABI _sceFiberInitializeImpl_nid_postfix(FiberObject*, const char*, Entry, std::uint64_t, void*, std::uint64_t, const void*, std::uint32_t);
std::int32_t APS5_VABI sceFiberFinalize(FiberObject*);
std::int32_t APS5_VABI sceFiberRun_nid_postfix(FiberObject*, std::uint64_t, std::uint64_t*);
std::int32_t APS5_VABI sceFiberSwitch(FiberObject*, std::uint64_t, std::uint64_t*);
std::int32_t APS5_VABI sceFiberReturnToThread(std::uint64_t, std::uint64_t*);
std::int32_t APS5_VABI sceFiberGetSelf(FiberObject**);
std::int32_t APS5_VABI sceFiberGetInfo(FiberObject*, FiberInfo*);
std::int32_t APS5_VABI sceFiberGetThreadFramePointerAddress(std::uint64_t*);
std::int32_t APS5_VABI sceFiberStartContextSizeCheck(std::uint32_t);
std::int32_t APS5_VABI sceFiberStopContextSizeCheck(void);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t FiberErrorPermission = static_cast<std::int32_t>(0x80590005);
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

alignas(16) std::array<unsigned char, 64 * 1024> g_firstContext;
alignas(16) std::array<unsigned char, 64 * 1024> g_secondContext;
struct alignas(8) FiberStorage {
    unsigned char bytes[0x100];
};

FiberStorage g_firstStorage;
FiberStorage g_checkedStorage;
alignas(16) std::array<unsigned char, 16 * 1024> g_checkedContext;
std::uint64_t g_threadFramePointer = 0;
FiberStorage g_secondStorage;
auto* const g_first = reinterpret_cast<FiberObject*>(&g_firstStorage);
auto* const g_second = reinterpret_cast<FiberObject*>(&g_secondStorage);
auto* const g_checked = reinterpret_cast<FiberObject*>(&g_checkedStorage);

std::vector<std::string>& FiberFailures() {
    static std::vector<std::string> failures;
    return failures;
}

void Check(bool condition, const char* what) {
    if (!condition) FiberFailures().emplace_back(what);
}

void RequireNoFiberFailures() {
    if (FiberFailures().empty()) return;
    std::string joined;
    for (const auto& failure : FiberFailures()) joined += (joined.empty() ? "" : "; ") + failure;
    Testing::Fail("checks failed inside the fibers: " + joined);
}

[[noreturn]] void ParkInThread() {
    std::uint64_t received = 0;
    for (;;) sceFiberReturnToThread(0, &received);
}

void APS5_VABI FirstEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Check(argOnInitialize == 11 && argOnRun == 100, "first fiber arguments");
    FiberObject* self = nullptr;
    Check(sceFiberGetSelf(&self) == 0 && self == g_first, "first fiber self");
    std::uint64_t framePointer = 0;
    Check(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull, "frame pointer null");
    Check(sceFiberGetThreadFramePointerAddress(&framePointer) == 0 && framePointer == g_threadFramePointer, "thread frame pointer");
    volatile double carried = 1.5;
    std::uint64_t received = 0;
    Check(sceFiberSwitch(g_second, 200, &received) == 0, "switch to second fiber");
    Check(received == 300 && carried == 1.5, "switch back to first fiber");
    for (;;) {
        Check(sceFiberReturnToThread(received + 1, &received) == 0, "first fiber return");
    }
}

void APS5_VABI SecondEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Check(argOnInitialize == 22 && argOnRun == 200, "second fiber arguments");
    Check(sceFiberSwitch(g_second, 0, nullptr) == FiberErrorState, "switch to self");
    _mm_setcsr(_mm_getcsr() | 0x8000u);
    std::uint64_t received = 0;
    Check(sceFiberReturnToThread(250, &received) == 0 && received == 260, "second fiber resumed on another thread");
    Check((_mm_getcsr() & 0x8000u) != 0, "second fiber keeps its MXCSR");
    Check(sceFiberSwitch(g_first, 300, nullptr) == 0, "switch to first fiber");
    Check(false, "second fiber resumed after its last switch");
    ParkInThread();
}

void APS5_VABI CheckedEntry(std::uint64_t, std::uint64_t) {
    volatile unsigned char used[4096];
    for (auto& byte : used) byte = 1;
    ParkInThread();
}

[[gnu::noinline]] std::int32_t RunFirst(std::uint64_t* returned) {
    g_threadFramePointer = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0));
    const std::int32_t result = sceFiberRun_nid_postfix(g_first, 100, returned);
    asm volatile("" ::: "memory");
    return result;
}

class FiberPair {
public:
    FiberPair() {
        FiberFailures().clear();
        RequireEqual(_sceFiberInitializeImpl_nid_postfix(g_first, "first", FirstEntry, 11, g_firstContext.data(), g_firstContext.size(), nullptr, 0), 0,
                     "initialize first");
        RequireEqual(_sceFiberInitializeImpl_nid_postfix(g_second, "second", SecondEntry, 22, g_secondContext.data(), g_secondContext.size(), nullptr, 0), 0,
                     "initialize second");
    }

    ~FiberPair() {
        sceFiberFinalize(g_first);
        sceFiberFinalize(g_second);
    }

    FiberPair(const FiberPair&) = delete;
    FiberPair& operator=(const FiberPair&) = delete;

    void RunFirstUntilSecondReturns() {
        std::uint64_t returned = 0;
        RequireEqual(RunFirst(&returned), 0, "run first until second returns");
        RequireEqual(returned, std::uint64_t{250}, "value returned by the second fiber");
        RequireNoFiberFailures();
    }

    void ResumeSecondOnAnotherThread() {
        std::int32_t result = 0;
        std::uint64_t returned = 0;
        std::thread([&] { result = sceFiberRun_nid_postfix(g_second, 260, &returned); }).join();
        RequireEqual(result, 0, "resume second on another thread");
        RequireEqual(returned, std::uint64_t{301}, "value returned by the first fiber after the second switched to it");
        RequireNoFiberFailures();
    }
};

class CheckedFiber {
public:
    explicit CheckedFiber(const char* name) {
        RequireEqual(_sceFiberInitializeImpl_nid_postfix(g_checked, name, CheckedEntry, 0, g_checkedContext.data(), g_checkedContext.size(), nullptr, 0), 0,
                     std::string("initialize ") + name);
    }

    ~CheckedFiber() {
        sceFiberFinalize(g_checked);
    }

    CheckedFiber(const CheckedFiber&) = delete;
    CheckedFiber& operator=(const CheckedFiber&) = delete;

    FiberInfo Info() {
        FiberInfo info{};
        info.size = sizeof(info);
        RequireEqual(sceFiberGetInfo(g_checked, &info), 0, "checked info");
        return info;
    }
};

class ContextSizeCheck {
public:
    ContextSizeCheck() {
        RequireEqual(sceFiberStartContextSizeCheck(0), 0, "start size check");
    }

    ~ContextSizeCheck() {
        sceFiberStopContextSizeCheck();
    }

    ContextSizeCheck(const ContextSizeCheck&) = delete;
    ContextSizeCheck& operator=(const ContextSizeCheck&) = delete;
};

const Case getSelfOutsideFiber{"FiberGetSelf_PlainThread_ReturnsPermissionError", [] {
    FiberObject* self = nullptr;
    RequireEqual(sceFiberGetSelf(&self), FiberErrorPermission, "no fiber on the thread");
}};

const Case framePointerOutsideFiber{"FiberGetThreadFramePointerAddress_PlainThread_ReturnsPermissionError", [] {
    const FiberPair fibers;
    std::uint64_t framePointer = 0;
    RequireEqual(sceFiberGetThreadFramePointerAddress(&framePointer), FiberErrorPermission, "frame pointer outside a fiber");
}};

const Case runAndSwitch{"FiberRun_FirstSwitchesToSecond_ReturnsValueFromSecondFiber", [] {
    FiberPair fibers;
    fibers.RunFirstUntilSecondReturns();
}};

const Case runRestoresMxcsr{"FiberRun_FiberChangesMxcsr_RestoresThreadMxcsr", [] {
    FiberPair fibers;
    const auto threadCsr = _mm_getcsr();
    fibers.RunFirstUntilSecondReturns();
    RequireEqual(_mm_getcsr(), threadCsr, "thread MXCSR restored after the fibers");
}};

const Case resumeOnAnotherThread{"FiberRun_SuspendedFiberOnAnotherThread_KeepsItsMxcsrAndSwitchesBack", [] {
    FiberPair fibers;
    fibers.RunFirstUntilSecondReturns();
    fibers.ResumeSecondOnAnotherThread();
}};

const Case finalizeSuspended{"FiberFinalize_SuspendedFibers_ReturnsOk", [] {
    FiberPair fibers;
    fibers.RunFirstUntilSecondReturns();
    fibers.ResumeSecondOnAnotherThread();
    RequireEqual(sceFiberFinalize(g_first), 0, "finalize first");
    RequireEqual(sceFiberFinalize(g_second), 0, "finalize second");
}};

const Case stopBeforeStart{"FiberStopContextSizeCheck_NotStarted_ReturnsStateError", [] {
    RequireEqual(sceFiberStopContextSizeCheck(), FiberErrorState, "stop before start");
}};

const Case startWithFlags{"FiberStartContextSizeCheck_NonZeroFlags_ReturnsInvalid", [] {
    RequireEqual(sceFiberStartContextSizeCheck(1), FiberErrorInvalid, "start with flags");
}};

const Case infoWithoutSizeCheck{"FiberGetInfo_WithoutSizeCheck_ReportsNoMargin", [] {
    CheckedFiber fiber("unchecked");
    RequireEqual(fiber.Info().size_context_margin, ~0ull, "no margin without size check");
    RequireEqual(sceFiberFinalize(g_checked), 0, "finalize unchecked");
}};

const Case startTwice{"FiberStartContextSizeCheck_AlreadyStarted_ReturnsStateError", [] {
    const ContextSizeCheck sizeCheck;
    RequireEqual(sceFiberStartContextSizeCheck(0), FiberErrorState, "start twice");
}};

const Case untouchedMargin{"FiberGetInfo_CheckedFiberNotRun_ReportsWholeContextAsMargin", [] {
    const ContextSizeCheck sizeCheck;
    CheckedFiber fiber("checked");
    RequireEqual(fiber.Info().size_context_margin, std::uint64_t{g_checkedContext.size()}, "untouched context is all margin");
}};

const Case usedMargin{"FiberGetInfo_CheckedFiberAfterRun_ReportsWordAlignedMarginBelowUsedStack", [] {
    const ContextSizeCheck sizeCheck;
    CheckedFiber fiber("checked");
    RequireEqual(sceFiberRun_nid_postfix(g_checked, 0, nullptr), 0, "run checked");
    const auto margin = fiber.Info().size_context_margin;
    Require(margin > 0 && margin < g_checkedContext.size() - 4096,
            "margin below the used stack: " + std::to_string(margin) + " not in (0, " + std::to_string(g_checkedContext.size() - 4096) + ")");
    RequireEqual(margin % 8, std::uint64_t{0}, "margin in whole words");
}};

const Case stopAfterStart{"FiberStopContextSizeCheck_Started_ReturnsOk", [] {
    const ContextSizeCheck sizeCheck;
    RequireEqual(sceFiberStopContextSizeCheck(), 0, "stop size check");
}};

} // namespace
