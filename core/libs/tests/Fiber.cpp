#include "prx/libSceFiber/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <xmmintrin.h>
#ifdef _WIN32
#include <windows.h>
#endif

using Entry = void (APS5_VABI*)(std::uint64_t, std::uint64_t);

static void Require(bool value, const char* what) {
    if (!value) { std::fprintf(stderr, "Fiber check failed: %s\n", what); std::abort(); }
}

namespace {

constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t FiberErrorAlignment = static_cast<std::int32_t>(0x80590002);
constexpr std::int32_t FiberErrorRange = static_cast<std::int32_t>(0x80590003);
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

[[gnu::no_sanitize("address")]] void CheckActiveContext(FiberObject* fiber, std::uint64_t expectedSize) {
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(fiber, &info) == 0, "active fiber context info");
    Require(info.size_context == expectedSize, "reported context keeps the requested size");
    volatile std::uint64_t marker = 0;
    const auto address = reinterpret_cast<std::uintptr_t>(&marker);
    const auto low = reinterpret_cast<std::uintptr_t>(info.addr_context);
    const auto high = low + info.size_context;
    Require(address >= low && address < high, "reported fiber context contains the executing stack");
    for (auto cursor = address; cursor < high; cursor += sizeof(std::uint64_t)) {
        const volatile auto word = *reinterpret_cast<const volatile std::uint64_t*>(cursor);
        (void)word;
    }
}

void APS5_VABI FirstEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 11 && argOnRun == 100, "first fiber arguments");
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == 0 && self == g_first, "first fiber self");
    CheckActiveContext(g_first, g_firstContext.size());
    std::uint64_t framePointer = 0;
    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull, "frame pointer null");
    Require(sceFiberGetThreadFramePointerAddress(&framePointer) == 0 && framePointer == g_threadFramePointer, "thread frame pointer");
    framePointer = 0;
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull, "underscore frame pointer null");
    Require(_sceFiberGetThreadFramePointerAddress(&framePointer) == 0 && framePointer == g_threadFramePointer, "underscore thread frame pointer");
    volatile double carried = 1.5;
    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_second, 200, &received) == 0, "switch to second fiber");
    Require(received == 300 && carried == 1.5, "switch back to first fiber");
    CheckActiveContext(g_first, g_firstContext.size());
    for (;;) {
        Require(sceFiberReturnToThread(received + 1, &received) == 0, "first fiber return");
    }
}

void APS5_VABI SecondEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 22 && argOnRun == 200, "second fiber arguments");
    CheckActiveContext(g_second, g_secondContext.size());
    Require(sceFiberSwitch(g_second, 0, nullptr) == FiberErrorState, "switch to self");
    _mm_setcsr(_mm_getcsr() | 0x8000u);
    std::uint64_t received = 0;
    Require(sceFiberReturnToThread(250, &received) == 0 && received == 260, "second fiber resumed on another thread");
    CheckActiveContext(g_second, g_secondContext.size());
    Require((_mm_getcsr() & 0x8000u) != 0, "second fiber keeps its MXCSR");
    Require(sceFiberSwitch(g_first, 300, nullptr) == 0, "switch to first fiber");
    Require(false, "second fiber resumed after its last switch");
}

void APS5_VABI CheckedEntry(std::uint64_t, std::uint64_t) {
    volatile unsigned char used[4096];
    for (auto& byte : used) byte = 1;
    std::uint64_t received = 0;
    for (;;) sceFiberReturnToThread(0, &received);
}

constexpr std::size_t SmallContextSize = 2048;
constexpr unsigned char SmallGuardFill = 0xa5;
alignas(16) std::array<unsigned char, 64 * 1024> g_smallArea;
FiberStorage g_smallStorage;

void APS5_VABI SmallEntry(std::uint64_t, std::uint64_t) {
    std::uint64_t received = 0;
    for (;;) sceFiberReturnToThread(0, &received);
}

void RunSmallContextFirst() {
    auto* small = reinterpret_cast<FiberObject*>(&g_smallStorage);
    auto* context = g_smallArea.data() + g_smallArea.size() - SmallContextSize;
    g_smallArea.fill(SmallGuardFill);
    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission, "return to thread outside a fiber");
    Require(_sceFiberInitializeImpl_nid_postfix(small, "small", SmallEntry, 0, context, SmallContextSize, nullptr, 0) == 0, "initialize small");
    Require(sceFiberRun_nid_postfix(small, 0, nullptr) == 0, "first fiber run of the process on a 2 KiB context");
    Require(sceFiberRun_nid_postfix(small, 0, nullptr) == 0, "resume on a 2 KiB context");
    Require(sceFiberFinalize(small) == 0, "finalize small");
    for (auto* byte = g_smallArea.data(); byte != context; ++byte) {
        Require(*byte == SmallGuardFill, "nothing is written below a 2 KiB context");
    }
}

constexpr std::size_t HostCallContextSize = 16 * 1024;
constexpr std::size_t HostCallStack = 256 * 1024;
constexpr std::size_t HostCallFrameSize = 4096;
constexpr unsigned char HostCallGuardFill = 0x5c;
alignas(16) std::array<unsigned char, HostCallStack + 64 * 1024> g_hostCallArea;
FiberStorage g_hostCallStorage;

std::size_t DescendHostStack(std::size_t remaining);
std::size_t (*volatile g_descendHostStack)(std::size_t) = DescendHostStack;

std::size_t DescendHostStack(std::size_t remaining) {
    volatile unsigned char frame[HostCallFrameSize];
    frame[0] = 0x11;
    frame[HostCallFrameSize - 1] = 0x22;
    const std::size_t frames = remaining > HostCallFrameSize ? g_descendHostStack(remaining - HostCallFrameSize) : 0;
    Require(frame[0] == 0x11 && frame[HostCallFrameSize - 1] == 0x22, "host call frame intact");
    return frames + 1;
}

#ifdef _WIN32
void CheckWindowsHostStack() {
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto low = reinterpret_cast<std::uintptr_t>(tib->StackLimit);
    const auto high = reinterpret_cast<std::uintptr_t>(tib->StackBase);
    const volatile unsigned char marker = 0;
    const auto frame = reinterpret_cast<std::uintptr_t>(&marker);
    Require(frame > low && frame < high && high - low >= HostCallContextSize + HostCallStack, "the thread's stack bounds are the fiber's host stack");
}
#endif

void APS5_VABI HostCallEntry(std::uint64_t, std::uint64_t) {
    CheckActiveContext(reinterpret_cast<FiberObject*>(&g_hostCallStorage), HostCallContextSize);
#ifdef _WIN32
    CheckWindowsHostStack();
#endif
    Require(g_descendHostStack(HostCallStack) == HostCallStack / HostCallFrameSize, "host call depth");
    std::uint64_t received = 0;
    for (;;) sceFiberReturnToThread(0, &received);
}

alignas(16) std::array<unsigned char, 64 * 1024> g_chainContext;
FiberStorage g_chainStorage;
bool g_chainEnded = false;

void APS5_VABI FrameChainEntry(std::uint64_t, std::uint64_t) {
    auto* frame = static_cast<std::uintptr_t*>(__builtin_frame_address(0));
    const auto low = reinterpret_cast<std::uintptr_t>(frame);
    const auto high = low + (1u << 20);
    for (int depth = 0; depth < 100; ++depth) {
        const auto address = reinterpret_cast<std::uintptr_t>(frame);
        Require(address >= low && address + 2 * sizeof(std::uintptr_t) <= high, "frame chain stays on the fiber's stack");
        if (frame[1] == 0) {
            g_chainEnded = frame[0] == 0;
            break;
        }
        auto* next = reinterpret_cast<std::uintptr_t*>(frame[0]);
        Require(reinterpret_cast<std::uintptr_t>(next) > address, "frame chain climbs the stack to a frame whose return address is 0");
        frame = next;
    }
    std::uint64_t received = 0;
    for (;;) sceFiberReturnToThread(0, &received);
}

void RunHostCallOnSmallContext() {
    auto* fiber = reinterpret_cast<FiberObject*>(&g_hostCallStorage);
    auto* context = g_hostCallArea.data() + g_hostCallArea.size() - HostCallContextSize;
    g_hostCallArea.fill(HostCallGuardFill);
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "host call", HostCallEntry, 0, context, HostCallContextSize, nullptr, 0) == 0, "initialize host call fiber");
    Require(sceFiberRun_nid_postfix(fiber, 0, nullptr) == 0, "run a 256 KiB host call from a 16 KiB fiber");
    Require(sceFiberFinalize(fiber) == 0, "finalize host call fiber");
    for (auto* byte = g_hostCallArea.data(); byte != context; ++byte) {
        Require(*byte == HostCallGuardFill, "a host call from a 16 KiB fiber writes nothing below its context");
    }
}

void RunFrameChain() {
    auto* fiber = reinterpret_cast<FiberObject*>(&g_chainStorage);
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "chain", FrameChainEntry, 0, g_chainContext.data(), g_chainContext.size(), nullptr, 0) == 0, "initialize chain");
    Require(sceFiberRun_nid_postfix(fiber, 0, nullptr) == 0, "run chain");
    Require(g_chainEnded, "frame chain of a fiber ends at a frame whose saved frame pointer and return address are 0");
    Require(sceFiberFinalize(fiber) == 0, "finalize chain");
}

constexpr std::size_t LargeContextSize = 512 * 1024;
alignas(16) std::array<unsigned char, LargeContextSize> g_largeContext;
FiberStorage g_largeStorage;
bool g_largeOnContext = false;

void APS5_VABI LargeContextEntry(std::uint64_t, std::uint64_t) {
    volatile unsigned char local = 0;
    const auto* address = const_cast<const unsigned char*>(&local);
    g_largeOnContext = address >= g_largeContext.data() && address < g_largeContext.data() + g_largeContext.size();
    sceFiberReturnToThread(0, nullptr);

}

void RunOnLargeContext() {
    auto* fiber = reinterpret_cast<FiberObject*>(&g_largeStorage);
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "large", LargeContextEntry, 0, g_largeContext.data(), g_largeContext.size(), nullptr, 0) == 0, "initialize large fiber");
    Require(sceFiberRun_nid_postfix(fiber, 0, nullptr) == 0, "run large fiber");
    Require(g_largeOnContext, "a fiber with a 512 KiB context runs on its context");
    Require(sceFiberFinalize(fiber) == 0, "finalize large fiber");
}

alignas(16) std::array<unsigned char, 64 * 1024> g_attachedContext;
alignas(16) std::array<unsigned char, 4 * 1024> g_attachedSmallContext;
FiberStorage g_attachedStorage;
FiberStorage g_attachedTargetStorage;
auto* const g_attached = reinterpret_cast<FiberObject*>(&g_attachedStorage);
auto* const g_attachedTarget = reinterpret_cast<FiberObject*>(&g_attachedTargetStorage);

void APS5_VABI AttachedTargetEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 44 && argOnRun == 500, "attached switch target arguments");
    CheckActiveContext(g_attachedTarget, g_attachedSmallContext.size());
    std::uint64_t received = 0;
    Require(sceFiberSwitch(g_attached, 600, &received) == 0, "switch back from the attached target");
    Require(false, "attached switch target resumed after its last switch");
}

void APS5_VABI AttachedEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 33 && argOnRun == 400, "attached fiber arguments");
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == 0 && self == g_attached, "attached fiber self");
    CheckActiveContext(g_attached, g_attachedContext.size());
    Require(_sceFiberAttachContextAndRun(g_attachedTarget, g_attachedSmallContext.data(), g_attachedSmallContext.size(), 0, nullptr) == FiberErrorPermission, "attach and run inside a fiber");
    std::uint64_t received = 0;
    Require(_sceFiberAttachContextAndSwitch(g_attached, g_attachedContext.data(), g_attachedContext.size(), 0, nullptr) == FiberErrorState, "attach and switch to self");
    Require(_sceFiberAttachContextAndSwitch(g_attachedTarget, g_attachedSmallContext.data() + 8, g_attachedSmallContext.size() - 16, 500, nullptr) == FiberErrorAlignment, "attach and switch with a misaligned context");
    Require(_sceFiberAttachContextAndSwitch(g_attachedTarget, g_attachedSmallContext.data(), g_attachedSmallContext.size(), 500, &received) == 0 && received == 600, "attach and switch");
    Require(sceFiberReturnToThread(700, &received) == 0 && received == 800, "attached fiber resumed");
    for (;;) sceFiberReturnToThread(received + 1, &received);
}

void APS5_VABI DefaultFpuEntry(std::uint64_t, std::uint64_t) {
    std::uint16_t control = 0;
    asm volatile("fnstcw %0" : "=m"(control));
    std::uint64_t received = 0;
    for (;;) sceFiberReturnToThread((static_cast<std::uint64_t>(control) << 32) | _mm_getcsr(), &received);
}

void RunAttachedContext() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_attached, "attached", AttachedEntry, 33, nullptr, 0, nullptr, 0) == 0, "initialize without a context");
    Require(_sceFiberInitializeImpl_nid_postfix(g_attachedTarget, "attached target", AttachedTargetEntry, 44, nullptr, 0, nullptr, 0) == 0, "initialize switch target without a context");
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_attached, &info) == 0 && info.addr_context == nullptr && info.size_context == 0, "fiber without a context reports none");
    Require(_sceFiberAttachContextAndRun(nullptr, g_attachedContext.data(), g_attachedContext.size(), 0, nullptr) == FiberErrorNull, "attach and run a null fiber");
    Require(_sceFiberAttachContextAndRun(g_attached, nullptr, g_attachedContext.size(), 0, nullptr) == FiberErrorInvalid, "attach a null context with a size");
    Require(_sceFiberAttachContextAndRun(g_attached, g_attachedContext.data(), 0, 0, nullptr) == FiberErrorRange, "attach a context without a size");
    Require(_sceFiberAttachContextAndRun(g_attached, g_attachedContext.data(), 256, 0, nullptr) == FiberErrorRange, "attach a context below the minimum size");
    Require(_sceFiberAttachContextAndRun(g_attached, g_attachedContext.data(), 1000, 0, nullptr) == FiberErrorAlignment, "attach a context of an unaligned size");
    Require(_sceFiberAttachContextAndSwitch(g_attached, g_attachedContext.data(), g_attachedContext.size(), 0, nullptr) == FiberErrorPermission, "attach and switch outside a fiber");
    std::uint64_t returned = 0;
    Require(_sceFiberAttachContextAndRun(g_attached, g_attachedContext.data(), g_attachedContext.size(), 400, &returned) == 0 && returned == 700, "attach and run");
    Require(_sceFiberAttachContextAndRun(g_attached, g_attachedContext.data(), g_attachedContext.size(), 800, &returned) == FiberErrorInvalid, "attach to a fiber that has a context");
    Require(_sceFiberAttachContextAndRun(g_attached, nullptr, 0, 800, &returned) == 0 && returned == 801, "run an attached fiber without a new context");
    Require(sceFiberRun_nid_postfix(g_attached, 802, &returned) == 0 && returned == 803, "run an attached fiber");
    Require(sceFiberFinalize(g_attachedTarget) == 0, "finalize a suspended attached target");
    Require(sceFiberFinalize(g_attached) == 0, "finalize attached fiber");
    Require(_sceFiberInitializeImpl_nid_postfix(g_attachedTarget, "attached target", AttachedTargetEntry, 44, nullptr, 0, nullptr, 0) == 0, "reinitialize attached target");
    Require(sceFiberFinalize(g_attachedTarget) == 0, "finalize fiber without a context");
}

void RunInternalOptions() {
    auto* fiber = reinterpret_cast<FiberObject*>(&g_checkedStorage);
    Require(_sceFiberInitializeWithInternalOptionImpl(nullptr, "options", CheckedEntry, 0, g_checkedContext.data(), g_checkedContext.size(), nullptr, 0, 0) == FiberErrorNull, "internal option null fiber");
    Require(_sceFiberInitializeWithInternalOptionImpl(fiber, "options", CheckedEntry, 0, g_checkedContext.data(), 256, nullptr, 0, 0) == FiberErrorRange, "internal option small context");
    Require(_sceFiberInitializeWithInternalOptionImpl(fiber, "options", CheckedEntry, 0, g_checkedContext.data(), g_checkedContext.size(), nullptr, 0x1, 0) == 0, "internal option without object registration");
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(fiber, &info) == 0 && info.size_context_margin == ~0ull, "no margin without the size check flag");
    Require(sceFiberFinalize(fiber) == 0, "finalize internal option fiber");
    Require(_sceFiberInitializeWithInternalOptionImpl(fiber, "options", CheckedEntry, 0, g_checkedContext.data(), g_checkedContext.size(), nullptr, 0x10, 0) == 0, "internal option size check");
    Require(sceFiberGetInfo(fiber, &info) == 0 && info.size_context_margin == g_checkedContext.size(), "size check flag fills the context");
    Require(sceFiberRun_nid_postfix(fiber, 0, nullptr) == 0, "run size checked fiber");
    Require(sceFiberGetInfo(fiber, &info) == 0 && info.size_context_margin < g_checkedContext.size() - 4096, "size check flag measures the margin");
    Require(sceFiberFinalize(fiber) == 0, "finalize size checked fiber");

    Require(_sceFiberInitializeWithInternalOptionImpl(fiber, "fpu", DefaultFpuEntry, 0, nullptr, 0, nullptr, 0x100, 0) == 0, "internal option fpu defaults without a context");
    const auto threadCsr = _mm_getcsr();
    _mm_setcsr((threadCsr & ~0x8040u) | 0x6000u);
    std::uint64_t returned = 0;
    Require(_sceFiberAttachContextAndRun(fiber, g_checkedContext.data(), g_checkedContext.size(), 0, &returned) == 0, "run fpu defaults fiber");
    _mm_setcsr(threadCsr);
    Require((returned & 0xffffffffu) == 0x9fc0u, "fpu defaults flag starts with the default MXCSR");
    Require((returned >> 32) == 0x037fu, "fpu defaults flag starts with the default x87 control word");
    Require(sceFiberFinalize(fiber) == 0, "finalize fpu defaults fiber");
}

[[gnu::noinline]] std::int32_t RunFirst(std::uint64_t* returned) {
    g_threadFramePointer = reinterpret_cast<std::uint64_t>(__builtin_frame_address(0));
    const std::int32_t result = sceFiberRun_nid_postfix(g_first, 100, returned);
    asm volatile("" ::: "memory");
    return result;
}

}

int main() {
    RunSmallContextFirst();
    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(&self) == FiberErrorPermission, "no fiber on the thread");
    Require(_sceFiberInitializeImpl_nid_postfix(g_first, "first", FirstEntry, 11, g_firstContext.data(), g_firstContext.size(), nullptr, 0) == 0, "initialize first");
    Require(_sceFiberInitializeImpl_nid_postfix(g_second, "second", SecondEntry, 22, g_secondContext.data(), g_secondContext.size(), nullptr, 0) == 0, "initialize second");
    const auto threadCsr = _mm_getcsr();
    std::uint64_t returned = 0;
    std::uint64_t framePointer = 0;
    Require(sceFiberGetThreadFramePointerAddress(&framePointer) == FiberErrorPermission, "frame pointer outside a fiber");
    Require(_sceFiberGetThreadFramePointerAddress(&framePointer) == FiberErrorPermission, "underscore frame pointer outside a fiber");
    Require(RunFirst(&returned) == 0 && returned == 250, "run first until second returns");
    Require(_mm_getcsr() == threadCsr, "thread MXCSR restored after the fibers");
    std::thread([&] {
        Require(sceFiberRun_nid_postfix(g_second, 260, &returned) == 0 && returned == 301, "resume second on another thread");
    }).join();
    Require(sceFiberFinalize(g_first) == 0, "finalize first");
    Require(sceFiberFinalize(g_second) == 0, "finalize second");

    auto* checked = reinterpret_cast<FiberObject*>(&g_checkedStorage);
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberStopContextSizeCheck() == FiberErrorState, "stop before start");
    Require(sceFiberStartContextSizeCheck(1) == FiberErrorInvalid, "start with flags");
    Require(_sceFiberInitializeImpl_nid_postfix(checked, "unchecked", CheckedEntry, 0, g_checkedContext.data(), g_checkedContext.size(), nullptr, 0) == 0, "initialize unchecked");
    Require(sceFiberGetInfo(checked, &info) == 0 && info.size_context_margin == ~0ull, "no margin without size check");
    Require(info.entry == CheckedEntry, "info preserves the guest entry callback type");
    Require(sceFiberFinalize(checked) == 0, "finalize unchecked");
    Require(sceFiberStartContextSizeCheck(0) == 0, "start size check");
    Require(sceFiberStartContextSizeCheck(0) == FiberErrorState, "start twice");
    Require(_sceFiberInitializeImpl_nid_postfix(checked, "checked", CheckedEntry, 0, g_checkedContext.data(), g_checkedContext.size(), nullptr, 0) == 0, "initialize checked");
    Require(sceFiberGetInfo(checked, &info) == 0 && info.size_context_margin == g_checkedContext.size(), "untouched context is all margin");
    Require(sceFiberRun_nid_postfix(checked, 0, nullptr) == 0, "run checked");
    Require(sceFiberGetInfo(checked, &info) == 0, "checked info");
    Require(info.size_context_margin > 0 && info.size_context_margin < g_checkedContext.size() - 4096, "margin below the used stack");
    Require(info.size_context_margin % 8 == 0, "margin in whole words");
    Require(sceFiberStopContextSizeCheck() == 0, "stop size check");

    RunHostCallOnSmallContext();
    RunFrameChain();
    RunOnLargeContext();
    RunAttachedContext();
    RunInternalOptions();
}
