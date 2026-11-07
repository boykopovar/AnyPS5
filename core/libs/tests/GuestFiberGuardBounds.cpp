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
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

struct alignas(8) FiberStorage {
    unsigned char bytes[0x100];
};

alignas(8) FiberStorage g_smallGuardStorage;
auto* const g_smallGuardFiber = reinterpret_cast<FiberObject*>(&g_smallGuardStorage);
alignas(16) std::array<unsigned char, 16 * 1024> g_smallGuardContext;

#ifdef _WIN32
static void CheckFiberWindowsBounds(const FiberInfo& info) {
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto stackLimit = reinterpret_cast<std::uintptr_t>(tib->StackLimit);
    const auto stackBase = reinterpret_cast<std::uintptr_t>(tib->StackBase);
    volatile std::uint64_t marker = 0;
    const auto currentSp = reinterpret_cast<std::uintptr_t>(&marker);
    Require(currentSp > stackLimit && currentSp < stackBase, "current SP within TEB stack bounds");
    Require(stackBase > stackLimit, "TEB StackBase above StackLimit");
    const auto* teb = reinterpret_cast<const std::uint8_t*>(tib);
    const auto deallocStack = reinterpret_cast<std::uintptr_t>(*reinterpret_cast<void* const*>(teb + 0x1478));
    Require(deallocStack <= stackLimit, "TEB DeallocationStack at or below StackLimit");

    MEMORY_BASIC_INFORMATION mbi{};
    Require(VirtualQuery(info.addr_context, &mbi, sizeof(mbi)) != 0, "VirtualQuery context");
    Require(mbi.AllocationBase != nullptr, "valid AllocationBase");

    MEMORY_BASIC_INFORMATION guardMbi{};
    Require(VirtualQuery(mbi.AllocationBase, &guardMbi, sizeof(guardMbi)) != 0, "VirtualQuery guard page");
    Require(guardMbi.Protect == PAGE_NOACCESS, "guard page has PAGE_NOACCESS");
    Require(guardMbi.State == MEM_COMMIT, "guard page is committed");
    Require(guardMbi.RegionSize >= 4096, "guard page at least one page");

    const auto* usable = static_cast<const std::uint8_t*>(mbi.AllocationBase) + guardMbi.RegionSize;
    MEMORY_BASIC_INFORMATION usableMbi{};
    Require(VirtualQuery(usable, &usableMbi, sizeof(usableMbi)) != 0, "VirtualQuery usable stack");
    Require(usableMbi.Protect == PAGE_READWRITE, "usable stack has PAGE_READWRITE");
}
#endif

void APS5_VABI SmallGuardEntry(std::uint64_t, std::uint64_t) {
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_smallGuardFiber, &info) == 0, "get info inside small guard fiber");
    Require(info.size_context == g_smallGuardContext.size(), "context size reported accurately");

#ifdef _WIN32
    CheckFiberWindowsBounds(info);
#endif

    std::uint64_t runArg = 0;
    for (;;) {
        sceFiberReturnToThread(100, &runArg);
    }
}

static void TestSmallContextGuardPage() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_smallGuardFiber, "small_guard", SmallGuardEntry, 0, g_smallGuardContext.data(), g_smallGuardContext.size(), nullptr, 0) == 0, "init small guard fiber");

#ifdef _WIN32
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto origBase = tib->StackBase;
    const auto origLimit = tib->StackLimit;
#endif

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_smallGuardFiber, 0, &returned) == 0 && returned == 100, "run small guard fiber");

#ifdef _WIN32
    Require(tib->StackBase == origBase && tib->StackLimit == origLimit, "thread stack bounds restored after fiber execution");
#endif

    Require(sceFiberFinalize(g_smallGuardFiber) == 0, "finalize small guard fiber");
}

alignas(8) FiberStorage g_largeGuardStorage;
auto* const g_largeGuardFiber = reinterpret_cast<FiberObject*>(&g_largeGuardStorage);
constexpr std::size_t LargeSize = 64 * 1024;
alignas(16) std::array<unsigned char, LargeSize> g_largeGuardContext;

void APS5_VABI LargeGuardEntry(std::uint64_t, std::uint64_t) {
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_largeGuardFiber, &info) == 0, "get info inside large guard fiber");
    Require(info.addr_context == g_largeGuardContext.data(), "large fiber runs directly on user context buffer");
    Require(info.size_context == g_largeGuardContext.size(), "context size matches large user buffer");

#ifdef _WIN32
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    Require(tib->StackBase == g_largeGuardContext.data() + g_largeGuardContext.size(), "large fiber StackBase equals buffer top");
    Require(tib->StackLimit == g_largeGuardContext.data(), "large fiber StackLimit equals buffer base");
#endif

    std::uint64_t runArg = 0;
    for (;;) {
        sceFiberReturnToThread(200, &runArg);
    }
}

static void TestLargeContextExecution() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_largeGuardFiber, "large_guard", LargeGuardEntry, 0, g_largeGuardContext.data(), g_largeGuardContext.size(), nullptr, 0) == 0, "init large guard fiber");
    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_largeGuardFiber, 0, &returned) == 0 && returned == 200, "run large guard fiber");
    Require(sceFiberFinalize(g_largeGuardFiber) == 0, "finalize large guard fiber");
}

alignas(8) FiberStorage g_marginStorage;
auto* const g_marginFiber = reinterpret_cast<FiberObject*>(&g_marginStorage);
alignas(16) std::array<unsigned char, 32 * 1024> g_marginContext;

[[gnu::noinline]] static void DeepStackConsumer(std::size_t remaining) {
    volatile unsigned char buffer[1024];
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = static_cast<unsigned char>(i ^ 0x5a);
    }
    if (remaining > sizeof(buffer)) {
        DeepStackConsumer(remaining - sizeof(buffer));
    }
    asm volatile("" ::: "memory");
}

void APS5_VABI MarginEntry(std::uint64_t, std::uint64_t) {
    DeepStackConsumer(8 * 1024);
    std::uint64_t runArg = 0;
    for (;;) {
        sceFiberReturnToThread(500, &runArg);
    }
}

static void TestStackOverflowDetectionAndMargin() {
    Require(_sceFiberInitializeWithInternalOptionImpl(g_marginFiber, "margin", MarginEntry, 0, g_marginContext.data(), g_marginContext.size(), nullptr, 0x10, 0) == 0, "initialize margin fiber with context size check");
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberGetInfo(g_marginFiber, &info) == 0, "get info before run");
    Require(info.size_context_margin == g_marginContext.size(), "initial margin covers entire context");

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_marginFiber, 0, &returned) == 0 && returned == 500, "run margin fiber");
    Require(sceFiberGetInfo(g_marginFiber, &info) == 0, "get info after deep stack consumption");
    Require(info.size_context_margin < g_marginContext.size() - 8 * 1024, "margin correctly reflects stack descent");
    Require(info.size_context_margin % 8 == 0, "margin aligned to word boundary");

    const auto* words = static_cast<const std::uint64_t*>(info.addr_context);
    const auto marginWords = info.size_context_margin / sizeof(std::uint64_t);
    for (std::size_t i = 0; i < marginWords; ++i) {
        Require(words[i] == 0xdeadbeefdeadbeefull, "margin area retains deadbeef fill pattern");
    }

    Require(sceFiberFinalize(g_marginFiber) == 0, "finalize margin fiber");
}

alignas(8) FiberStorage g_regStorage;
auto* const g_regFiber = reinterpret_cast<FiberObject*>(&g_regStorage);
alignas(16) std::array<unsigned char, 32 * 1024> g_regContext;

volatile std::uint32_t g_fiberObservedMxcsr = 0;

void APS5_VABI RegEntry(std::uint64_t, std::uint64_t) {
    g_fiberObservedMxcsr = _mm_getcsr();

    register std::uint64_t clobberRbx asm("rbx") = 0xbeef0001ull;
    register std::uint64_t clobberR12 asm("r12") = 0xbeef0002ull;
    register std::uint64_t clobberR13 asm("r13") = 0xbeef0003ull;
    register std::uint64_t clobberR14 asm("r14") = 0xbeef0004ull;
    register std::uint64_t clobberR15 asm("r15") = 0xbeef0005ull;
    asm volatile("" : "+r"(clobberRbx), "+r"(clobberR12), "+r"(clobberR13), "+r"(clobberR14), "+r"(clobberR15));
    _mm_setcsr(g_fiberObservedMxcsr ^ 0x40u);

    std::uint64_t runArg = 0;
    Require(sceFiberReturnToThread(1234, &runArg) == 0, "fiber return to thread");
    Require(runArg == 5678, "fiber resumed with expected argument");

    register std::uint64_t resumeRbx asm("rbx");
    register std::uint64_t resumeR12 asm("r12");
    register std::uint64_t resumeR13 asm("r13");
    register std::uint64_t resumeR14 asm("r14");
    register std::uint64_t resumeR15 asm("r15");
    asm volatile("" : "=r"(resumeRbx), "=r"(resumeR12), "=r"(resumeR13), "=r"(resumeR14), "=r"(resumeR15));
    Require(resumeRbx == 0xbeef0001ull, "fiber RBX preserved across suspension");
    Require(resumeR12 == 0xbeef0002ull, "fiber R12 preserved across suspension");
    Require(resumeR13 == 0xbeef0003ull, "fiber R13 preserved across suspension");
    Require(resumeR14 == 0xbeef0004ull, "fiber R14 preserved across suspension");
    Require(resumeR15 == 0xbeef0005ull, "fiber R15 preserved across suspension");
    Require(_mm_getcsr() == (g_fiberObservedMxcsr ^ 0x40u), "fiber MXCSR preserved across suspension");

    for (;;) {
        sceFiberReturnToThread(9999, &runArg);
    }
}

[[gnu::noinline]] static void TestCallerRegisterIntegrity() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_regFiber, "reg_fiber", RegEntry, 0, g_regContext.data(), g_regContext.size(), nullptr, 0) == 0, "init reg fiber");

    register std::uint64_t rbx asm("rbx") = 0x1111222233334444ull;
    register std::uint64_t r12 asm("r12") = 0x2222333344445555ull;
    register std::uint64_t r13 asm("r13") = 0x3333444455556666ull;
    register std::uint64_t r14 asm("r14") = 0x4444555566667777ull;
    register std::uint64_t r15 asm("r15") = 0x5555666677778888ull;
    asm volatile("" : "+r"(rbx), "+r"(r12), "+r"(r13), "+r"(r14), "+r"(r15));

    const auto callerMxcsr = _mm_getcsr();
    std::uint64_t returned = 0;
    const auto ret = sceFiberRun_nid_postfix(g_regFiber, 0, &returned);
    asm volatile("" : "+r"(rbx), "+r"(r12), "+r"(r13), "+r"(r14), "+r"(r15) : : "memory");

    Require(ret == 0, "run reg fiber");
    Require(returned == 1234, "returned value from reg fiber");
    Require(rbx == 0x1111222233334444ull, "caller RBX intact");
    Require(r12 == 0x2222333344445555ull, "caller R12 intact");
    Require(r13 == 0x3333444455556666ull, "caller R13 intact");
    Require(r14 == 0x4444555566667777ull, "caller R14 intact");
    Require(r15 == 0x5555666677778888ull, "caller R15 intact");
    Require(_mm_getcsr() == callerMxcsr, "caller MXCSR intact");

    const auto retResume = sceFiberRun_nid_postfix(g_regFiber, 5678, &returned);
    asm volatile("" : "+r"(rbx), "+r"(r12), "+r"(r13), "+r"(r14), "+r"(r15) : : "memory");

    Require(retResume == 0, "resume reg fiber");
    Require(returned == 9999, "returned value from resumed reg fiber");
    Require(rbx == 0x1111222233334444ull, "caller RBX intact after resume");
    Require(r12 == 0x2222333344445555ull, "caller R12 intact after resume");
    Require(r13 == 0x3333444455556666ull, "caller R13 intact after resume");
    Require(r14 == 0x4444555566667777ull, "caller R14 intact after resume");
    Require(r15 == 0x5555666677778888ull, "caller R15 intact after resume");
    Require(_mm_getcsr() == callerMxcsr, "caller MXCSR intact after resume");

    Require(sceFiberFinalize(g_regFiber) == 0, "finalize reg fiber");
}

alignas(8) FiberStorage g_switchAStorage;
alignas(8) FiberStorage g_switchBStorage;
auto* const g_switchA = reinterpret_cast<FiberObject*>(&g_switchAStorage);
auto* const g_switchB = reinterpret_cast<FiberObject*>(&g_switchBStorage);
alignas(16) std::array<unsigned char, 32 * 1024> g_switchAContext;
alignas(16) std::array<unsigned char, 32 * 1024> g_switchBContext;

void APS5_VABI SwitchAEntry(std::uint64_t, std::uint64_t) {
    register std::uint64_t rbx asm("rbx") = 0xaaaa0001ull;
    register std::uint64_t r13 asm("r13") = 0xaaaa0003ull;
    register std::uint64_t r14 asm("r14") = 0xaaaa0004ull;
    register std::uint64_t r15 asm("r15") = 0xaaaa0005ull;
    asm volatile("" : "+r"(rbx), "+r"(r13), "+r"(r14), "+r"(r15));

    std::uint64_t runOut = 0;
    Require(sceFiberSwitch(g_switchB, 777, &runOut) == 0, "switch from A to B");
    asm volatile("" : "+r"(rbx), "+r"(r13), "+r"(r14), "+r"(r15) : : "memory");

    Require(runOut == 888, "switch back run argument");
    Require(rbx == 0xaaaa0001ull, "Fiber A RBX preserved after switch");
    Require(r13 == 0xaaaa0003ull, "Fiber A R13 preserved after switch");
    Require(r14 == 0xaaaa0004ull, "Fiber A R14 preserved after switch");
    Require(r15 == 0xaaaa0005ull, "Fiber A R15 preserved after switch");

    for (;;) {
        sceFiberReturnToThread(1000, &runOut);
    }
}

void APS5_VABI SwitchBEntry(std::uint64_t, std::uint64_t run) {
    Require(run == 777, "Fiber B received argument from A");
    register std::uint64_t rbx asm("rbx") = 0xbbbb0001ull;
    register std::uint64_t r13 asm("r13") = 0xbbbb0003ull;
    register std::uint64_t r14 asm("r14") = 0xbbbb0004ull;
    register std::uint64_t r15 asm("r15") = 0xbbbb0005ull;
    asm volatile("" : "+r"(rbx), "+r"(r13), "+r"(r14), "+r"(r15));

    std::uint64_t runOut = 0;
    Require(sceFiberSwitch(g_switchA, 888, &runOut) == 0, "switch from B to A");
    asm volatile("" : "+r"(rbx), "+r"(r13), "+r"(r14), "+r"(r15) : : "memory");

    Require(false, "Fiber B resumed unexpectedly");
}

static void TestCooperativeFiberSwitchRegisters() {
    Require(_sceFiberInitializeImpl_nid_postfix(g_switchA, "switch_a", SwitchAEntry, 0, g_switchAContext.data(), g_switchAContext.size(), nullptr, 0) == 0, "init switch A");
    Require(_sceFiberInitializeImpl_nid_postfix(g_switchB, "switch_b", SwitchBEntry, 0, g_switchBContext.data(), g_switchBContext.size(), nullptr, 0) == 0, "init switch B");

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_switchA, 0, &returned) == 0 && returned == 1000, "run switch A until return");

    Require(sceFiberFinalize(g_switchB) == 0, "finalize switch B");
    Require(sceFiberFinalize(g_switchA) == 0, "finalize switch A");
}

static void TestStackBoundsAndInvalidSizes() {
    FiberStorage storage{};
    auto* fiber = reinterpret_cast<FiberObject*>(&storage);
    alignas(16) std::array<unsigned char, 16 * 1024> validContext;

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 0, nullptr, 0) == FiberErrorRange, "zero context size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 16, nullptr, 0) == FiberErrorRange, "16 byte context size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 256, nullptr, 0) == FiberErrorRange, "256 byte context size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 496, nullptr, 0) == FiberErrorRange, "496 byte context size rejected");

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 255, nullptr, 0) == FiberErrorAlignment, "unaligned 255 byte size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 511, nullptr, 0) == FiberErrorAlignment, "unaligned 511 byte size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 513, nullptr, 0) == FiberErrorAlignment, "unaligned 513 byte size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 1023, nullptr, 0) == FiberErrorAlignment, "unaligned 1023 byte size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 4095, nullptr, 0) == FiberErrorAlignment, "unaligned 4095 byte size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 16383, nullptr, 0) == FiberErrorAlignment, "unaligned 16383 byte size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data(), 16385, nullptr, 0) == FiberErrorAlignment, "unaligned 16385 byte size rejected");

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data() + 1, validContext.size(), nullptr, 0) == FiberErrorAlignment, "unaligned context address +1 rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, validContext.data() + 7, validContext.size(), nullptr, 0) == FiberErrorAlignment, "unaligned context address +7 rejected");

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, nullptr, 512, nullptr, 0) == FiberErrorInvalid, "null context pointer with 512 size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, nullptr, 4096, nullptr, 0) == FiberErrorInvalid, "null context pointer with 4096 size rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, nullptr, 16384, nullptr, 0) == FiberErrorInvalid, "null context pointer with 16384 size rejected");

    Require(_sceFiberInitializeWithInternalOptionImpl(fiber, "test", SmallGuardEntry, 0, validContext.data(), 256, nullptr, 0, 0) == FiberErrorRange, "internal option size < 512 rejected");
    Require(_sceFiberInitializeWithInternalOptionImpl(fiber, "test", SmallGuardEntry, 0, validContext.data(), 16383, nullptr, 0, 0) == FiberErrorAlignment, "internal option unaligned size rejected");
    Require(_sceFiberInitializeWithInternalOptionImpl(fiber, "test", SmallGuardEntry, 0, nullptr, 16384, nullptr, 0, 0) == FiberErrorInvalid, "internal option null buffer with non-zero size rejected");

    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", SmallGuardEntry, 0, nullptr, 0, nullptr, 0) == 0, "init contextless fiber");
    Require(_sceFiberAttachContextAndRun(fiber, validContext.data(), 256, 0, nullptr) == FiberErrorRange, "attach context size < 512 rejected");
    Require(_sceFiberAttachContextAndRun(fiber, validContext.data(), 16383, 0, nullptr) == FiberErrorAlignment, "attach context unaligned size rejected");
    Require(_sceFiberAttachContextAndRun(fiber, validContext.data() + 1, validContext.size(), 0, nullptr) == FiberErrorAlignment, "attach context unaligned address rejected");
    Require(_sceFiberAttachContextAndRun(fiber, nullptr, 16384, 0, nullptr) == FiberErrorInvalid, "attach null context with non-zero size rejected");
    Require(_sceFiberAttachContextAndSwitch(fiber, validContext.data(), validContext.size(), 0, nullptr) == FiberErrorPermission, "attach and switch outside fiber rejected");
    Require(sceFiberFinalize(fiber) == 0, "finalize test fiber");
}

static void TestParameterPointerFaultRejection() {
    FiberStorage storage{};
    auto* fiber = reinterpret_cast<FiberObject*>(&storage);
    alignas(16) std::array<unsigned char, 4096> validContext;

    Require(sceFiberOptParamInitialize(nullptr) == FiberErrorNull, "opt param null pointer rejected");
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(0x8)) == FiberErrorInvalid, "opt param invalid pointer 0x8 rejected");
    Require(sceFiberOptParamInitialize(reinterpret_cast<FiberOptParam*>(0x10)) == FiberErrorInvalid, "opt param invalid pointer 0x10 rejected");

    FiberOptParam optParam{};
    optParam.magic = 0xabcdef01;
    Require(sceFiberOptParamInitialize(&optParam) == 0, "valid opt param initialized");
    Require(optParam.magic == 0, "opt param memory cleared");

    Require(_sceFiberInitializeImpl_nid_postfix(nullptr, "test", SmallGuardEntry, 0, validContext.data(), validContext.size(), nullptr, 0) == FiberErrorNull, "null object initialize rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, nullptr, SmallGuardEntry, 0, validContext.data(), validContext.size(), nullptr, 0) == FiberErrorNull, "null name initialize rejected");
    Require(_sceFiberInitializeImpl_nid_postfix(fiber, "test", nullptr, 0, validContext.data(), validContext.size(), nullptr, 0) == FiberErrorNull, "null entry initialize rejected");

    alignas(16) unsigned char unalignedStorage[0x100 + 8];
    auto* unalignedFiber = reinterpret_cast<FiberObject*>(unalignedStorage + 1);
    Require(_sceFiberInitializeImpl_nid_postfix(unalignedFiber, "test", SmallGuardEntry, 0, validContext.data(), validContext.size(), nullptr, 0) == FiberErrorAlignment, "unaligned fiber object rejected");

    Require(sceFiberRun_nid_postfix(nullptr, 0, nullptr) == FiberErrorNull, "null fiber run rejected");
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull, "null fiber switch rejected");
    Require(sceFiberFinalize(nullptr) == FiberErrorNull, "null fiber finalize rejected");
    Require(sceFiberGetInfo(nullptr, nullptr) == FiberErrorNull, "null fiber and info getInfo rejected");
    Require(sceFiberRename(nullptr, nullptr) == FiberErrorNull, "null fiber and name rename rejected");

    alignas(8) unsigned char invalidMemory[0x100] = {};
    auto* invalidFiber = reinterpret_cast<FiberObject*>(invalidMemory);
    FiberInfo info{};
    info.size = sizeof(info);
    Require(sceFiberRun_nid_postfix(invalidFiber, 0, nullptr) == FiberErrorInvalid, "invalid magic run rejected");
    Require(sceFiberSwitch(invalidFiber, 0, nullptr) == FiberErrorInvalid, "invalid magic switch rejected");
    Require(sceFiberFinalize(invalidFiber) == FiberErrorInvalid, "invalid magic finalize rejected");
    Require(sceFiberGetInfo(invalidFiber, &info) == FiberErrorInvalid, "invalid magic get info rejected");
    Require(sceFiberRename(invalidFiber, "new_name") == FiberErrorInvalid, "invalid magic rename rejected");

    FiberObject* self = nullptr;
    Require(sceFiberGetSelf(nullptr) == FiberErrorNull, "null self pointer rejected");
    Require(sceFiberGetSelf(&self) == FiberErrorPermission, "get self outside fiber rejected");
    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission, "return to thread outside fiber rejected");
    Require(sceFiberSwitch(invalidFiber, 0, nullptr) == FiberErrorPermission, "switch outside fiber rejected");

    Require(sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull, "null frame pointer address rejected");
    std::uint64_t framePointer = 0;
    Require(sceFiberGetThreadFramePointerAddress(&framePointer) == FiberErrorPermission, "frame pointer outside fiber rejected");
    Require(_sceFiberGetThreadFramePointerAddress(nullptr) == FiberErrorNull, "null underscore frame pointer address rejected");
    Require(_sceFiberGetThreadFramePointerAddress(&framePointer) == FiberErrorPermission, "underscore frame pointer outside fiber rejected");

    Require(sceFiberStartContextSizeCheck(2) == FiberErrorInvalid, "start context check with invalid flags rejected");
}

}

int main() {
    TestSmallContextGuardPage();
    TestLargeContextExecution();
    TestStackOverflowDetectionAndMargin();
    TestCallerRegisterIntegrity();
    TestCooperativeFiberSwitchRegisters();
    TestStackBoundsAndInvalidSizes();
    TestParameterPointerFaultRejection();
    return 0;
}
