#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define Require(expr) \
    do { \
        if (!(expr)) { \
            std::fprintf(stderr, "%s:%d: Require(%s) failed\n", __FILE__, __LINE__, #expr); \
            std::abort(); \
        } \
    } while (false)

namespace {

constexpr std::int32_t SCE_OK = 0;
constexpr std::int32_t FiberErrorNull = static_cast<std::int32_t>(0x80590001);
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);
constexpr std::int32_t FiberErrorPermission = static_cast<std::int32_t>(0x80590005);
constexpr std::int32_t FiberErrorState = static_cast<std::int32_t>(0x80590006);

constexpr std::uint64_t CanaryPattern1 = 0xdeadbeefcafebabeull;
constexpr std::uint64_t CanaryPattern2 = 0xfeedfacec001d00dull;
constexpr std::uint64_t CanaryPattern3 = 0x0123456789abcdefull;
constexpr std::uint64_t CanaryPattern4 = 0xfedcba9876543210ull;

struct alignas(16) GuardedSmallStack {
    std::uint64_t headCanary[8];
    alignas(16) std::uint8_t stack[16384];
    std::uint64_t tailCanary[8];
};

struct alignas(16) GuardedLargeStack {
    std::uint64_t headCanary[8];
    alignas(16) std::uint8_t stack[65536];
    std::uint64_t tailCanary[8];
};

GuardedSmallStack g_stackA;
GuardedSmallStack g_stackB;
GuardedLargeStack g_stackC;

alignas(8) std::uint8_t g_fiberStorageA[256];
alignas(8) std::uint8_t g_fiberStorageB[256];
alignas(8) std::uint8_t g_fiberStorageC[256];
alignas(8) std::uint8_t g_fiberStorageDead[256];

FiberObject* const g_fiberA = reinterpret_cast<FiberObject*>(g_fiberStorageA);
FiberObject* const g_fiberB = reinterpret_cast<FiberObject*>(g_fiberStorageB);
FiberObject* const g_fiberC = reinterpret_cast<FiberObject*>(g_fiberStorageC);
FiberObject* const g_fiberDead = reinterpret_cast<FiberObject*>(g_fiberStorageDead);

void InitializeStackCanaries(GuardedSmallStack& s, std::uint64_t base) {
    for (std::size_t i = 0; i < 8; ++i) {
        s.headCanary[i] = base + i;
        s.tailCanary[i] = ~base - i;
    }
}

void InitializeLargeStackCanaries(GuardedLargeStack& s, std::uint64_t base) {
    for (std::size_t i = 0; i < 8; ++i) {
        s.headCanary[i] = base + i;
        s.tailCanary[i] = ~base - i;
    }
}

void VerifyStackCanaries(const GuardedSmallStack& s, std::uint64_t base) {
    for (std::size_t i = 0; i < 8; ++i) {
        Require(s.headCanary[i] == base + i);
        Require(s.tailCanary[i] == (~base - i));
    }
}

void VerifyLargeStackCanaries(const GuardedLargeStack& s, std::uint64_t base) {
    for (std::size_t i = 0; i < 8; ++i) {
        Require(s.headCanary[i] == base + i);
        Require(s.tailCanary[i] == (~base - i));
    }
}

void APS5_VABI DeadDummyEntry(std::uint64_t, std::uint64_t) {
    Require(sceFiberReturnToThread(1234, nullptr) == SCE_OK);
}

void APS5_VABI DummyEntry(std::uint64_t, std::uint64_t) {
    Require(sceFiberReturnToThread(0, nullptr) == SCE_OK);
}

void APS5_VABI SelfSwitchEntry(std::uint64_t, std::uint64_t) {
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorState);
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);

    alignas(8) std::uint8_t localUninit[256] = {};
    Require(sceFiberSwitch(reinterpret_cast<FiberObject*>(localUninit), 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberSwitch(g_fiberDead, 0, nullptr) == FiberErrorInvalid);

    Require(sceFiberReturnToThread(7777, nullptr) == SCE_OK);
}

void APS5_VABI PingPongFiberAEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 100);
    Require(argOnRun == 101);

    volatile std::uint64_t localCanaryHead = CanaryPattern1;
    volatile std::uint64_t localData[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    volatile std::uint64_t localCanaryTail = CanaryPattern2;

#if defined(__x86_64__) || defined(_M_X64)
    register std::uint64_t regRbx asm("rbx") = 0x1111222233334444ull;
    register std::uint64_t regR12 asm("r12") = 0x2222333344445555ull;
    register std::uint64_t regR13 asm("r13") = 0x3333444455556666ull;
    register std::uint64_t regR14 asm("r14") = 0x4444555566667777ull;
    register std::uint64_t regR15 asm("r15") = 0x5555666677778888ull;
    asm volatile("" : "+r"(regRbx), "+r"(regR12), "+r"(regR13), "+r"(regR14), "+r"(regR15));
#endif

    for (std::uint64_t cycle = 0; cycle < 5; ++cycle) {
        std::uint64_t received = 0;
        Require(sceFiberSwitch(g_fiberB, 1000 + cycle, &received) == SCE_OK);
        Require(received == 2000 + cycle);

#if defined(__x86_64__) || defined(_M_X64)
        asm volatile("" : "+r"(regRbx), "+r"(regR12), "+r"(regR13), "+r"(regR14), "+r"(regR15) : : "memory");
        Require(regRbx == 0x1111222233334444ull);
        Require(regR12 == 0x2222333344445555ull);
        Require(regR13 == 0x3333444455556666ull);
        Require(regR14 == 0x4444555566667777ull);
        Require(regR15 == 0x5555666677778888ull);
#endif
        Require(localCanaryHead == CanaryPattern1);
        Require(localCanaryTail == CanaryPattern2);
        for (std::size_t i = 0; i < 8; ++i) {
            Require(localData[i] == (i + 1));
        }
    }

    std::uint64_t fromHost = 0;
    Require(sceFiberReturnToThread(3000, &fromHost) == SCE_OK);
    Require(fromHost == 3001);

#if defined(__x86_64__) || defined(_M_X64)
    asm volatile("" : "+r"(regRbx), "+r"(regR12), "+r"(regR13), "+r"(regR14), "+r"(regR15) : : "memory");
    Require(regRbx == 0x1111222233334444ull);
    Require(regR12 == 0x2222333344445555ull);
    Require(regR13 == 0x3333444455556666ull);
    Require(regR14 == 0x4444555566667777ull);
    Require(regR15 == 0x5555666677778888ull);
#endif
    Require(localCanaryHead == CanaryPattern1);
    Require(localCanaryTail == CanaryPattern2);

    Require(sceFiberReturnToThread(3002, nullptr) == SCE_OK);
}

void APS5_VABI PingPongFiberBEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 200);
    std::uint64_t current = argOnRun;

    volatile std::uint64_t localCanaryHead = CanaryPattern3;
    volatile std::uint64_t localCanaryTail = CanaryPattern4;

#if defined(__x86_64__) || defined(_M_X64)
    register std::uint64_t regRbx asm("rbx") = 0xaaaaaaaa11111111ull;
    register std::uint64_t regR12 asm("r12") = 0xbbbbbbbb22222222ull;
    register std::uint64_t regR13 asm("r13") = 0xcccccccc33333333ull;
    register std::uint64_t regR14 asm("r14") = 0xdddddddd44444444ull;
    register std::uint64_t regR15 asm("r15") = 0xeeeeeeee55555555ull;
    asm volatile("" : "+r"(regRbx), "+r"(regR12), "+r"(regR13), "+r"(regR14), "+r"(regR15));
#endif

    for (std::uint64_t cycle = 0; cycle < 5; ++cycle) {
        Require(current == 1000 + cycle);

        std::uint64_t fromLarge = 0;
        Require(sceFiberSwitch(g_fiberC, 5000 + cycle, &fromLarge) == SCE_OK);
        Require(fromLarge == 6000 + cycle);

#if defined(__x86_64__) || defined(_M_X64)
        asm volatile("" : "+r"(regRbx), "+r"(regR12), "+r"(regR13), "+r"(regR14), "+r"(regR15) : : "memory");
        Require(regRbx == 0xaaaaaaaa11111111ull);
        Require(regR12 == 0xbbbbbbbb22222222ull);
        Require(regR13 == 0xcccccccc33333333ull);
        Require(regR14 == 0xdddddddd44444444ull);
        Require(regR15 == 0xeeeeeeee55555555ull);
#endif
        Require(localCanaryHead == CanaryPattern3);
        Require(localCanaryTail == CanaryPattern4);

        std::uint64_t nextInput = 0;
        Require(sceFiberSwitch(g_fiberA, 2000 + cycle, &nextInput) == SCE_OK);
        current = nextInput;
    }

    Require(false);
}

void APS5_VABI LargeStackFiberCEntry(std::uint64_t argOnInitialize, std::uint64_t argOnRun) {
    Require(argOnInitialize == 300);
    std::uint64_t current = argOnRun;

    volatile std::uint8_t deepStack[4096];
    for (std::size_t i = 0; i < sizeof(deepStack); ++i) {
        deepStack[i] = static_cast<std::uint8_t>(i & 0xff);
    }

    for (std::uint64_t cycle = 0; cycle < 5; ++cycle) {
        Require(current == 5000 + cycle);

        for (std::size_t i = 0; i < sizeof(deepStack); ++i) {
            Require(deepStack[i] == static_cast<std::uint8_t>(i & 0xff));
        }

        std::uint64_t nextInput = 0;
        Require(sceFiberSwitch(g_fiberB, 6000 + cycle, &nextInput) == SCE_OK);
        current = nextInput;
    }

    Require(false);
}

void TestCallingContextRestrictions() {
    Require(sceFiberSwitch(nullptr, 0, nullptr) == FiberErrorNull);
    alignas(8) std::uint8_t uninitStorage[256] = {};
    Require(sceFiberSwitch(reinterpret_cast<FiberObject*>(uninitStorage), 0, nullptr) == FiberErrorInvalid);
    Require(sceFiberReturnToThread(0, nullptr) == FiberErrorPermission);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "perm_check", DummyEntry, 0, g_stackA.stack, sizeof(g_stackA.stack), nullptr, 0) == SCE_OK);
    Require(sceFiberSwitch(g_fiberA, 0, nullptr) == FiberErrorPermission);
    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
}

void TestSwitchSelfAndInvalidRejection() {
    InitializeStackCanaries(g_stackA, 0x11110000);
    InitializeStackCanaries(g_stackB, 0x22220000);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberDead, "dead", DeadDummyEntry, 0, g_stackB.stack, sizeof(g_stackB.stack), nullptr, 0) == SCE_OK);
    Require(sceFiberFinalize(g_fiberDead) == SCE_OK);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "self_rej", SelfSwitchEntry, 0, g_stackA.stack, sizeof(g_stackA.stack), nullptr, 0) == SCE_OK);

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 0, &returned) == SCE_OK);
    Require(returned == 7777);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    VerifyStackCanaries(g_stackA, 0x11110000);
    VerifyStackCanaries(g_stackB, 0x22220000);
}

void TestRegisterAndCanaryPreservationMultiFiber() {
    InitializeStackCanaries(g_stackA, 0xaaaa0000);
    InitializeStackCanaries(g_stackB, 0xbbbb0000);
    InitializeLargeStackCanaries(g_stackC, 0xcccc0000);

    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberA, "fiber_a", PingPongFiberAEntry, 100, g_stackA.stack, sizeof(g_stackA.stack), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberB, "fiber_b", PingPongFiberBEntry, 200, g_stackB.stack, sizeof(g_stackB.stack), nullptr, 0) == SCE_OK);
    Require(_sceFiberInitializeImpl_nid_postfix(g_fiberC, "fiber_c", LargeStackFiberCEntry, 300, g_stackC.stack, sizeof(g_stackC.stack), nullptr, 0) == SCE_OK);

#if defined(__x86_64__) || defined(_M_X64)
    register std::uint64_t callerRbx asm("rbx") = 0x9999888877776666ull;
    register std::uint64_t callerR12 asm("r12") = 0x8888777766665555ull;
    register std::uint64_t callerR13 asm("r13") = 0x7777666655554444ull;
    register std::uint64_t callerR14 asm("r14") = 0x6666555544443333ull;
    register std::uint64_t callerR15 asm("r15") = 0x5555444433332222ull;
    asm volatile("" : "+r"(callerRbx), "+r"(callerR12), "+r"(callerR13), "+r"(callerR14), "+r"(callerR15));
#endif

    std::uint64_t returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 101, &returned) == SCE_OK);
    Require(returned == 3000);

#if defined(__x86_64__) || defined(_M_X64)
    asm volatile("" : "+r"(callerRbx), "+r"(callerR12), "+r"(callerR13), "+r"(callerR14), "+r"(callerR15) : : "memory");
    Require(callerRbx == 0x9999888877776666ull);
    Require(callerR12 == 0x8888777766665555ull);
    Require(callerR13 == 0x7777666655554444ull);
    Require(callerR14 == 0x6666555544443333ull);
    Require(callerR15 == 0x5555444433332222ull);
#endif

    returned = 0;
    Require(sceFiberRun_nid_postfix(g_fiberA, 3001, &returned) == SCE_OK);
    Require(returned == 3002);

#if defined(__x86_64__) || defined(_M_X64)
    asm volatile("" : "+r"(callerRbx), "+r"(callerR12), "+r"(callerR13), "+r"(callerR14), "+r"(callerR15) : : "memory");
    Require(callerRbx == 0x9999888877776666ull);
    Require(callerR12 == 0x8888777766665555ull);
    Require(callerR13 == 0x7777666655554444ull);
    Require(callerR14 == 0x6666555544443333ull);
    Require(callerR15 == 0x5555444433332222ull);
#endif

    VerifyStackCanaries(g_stackA, 0xaaaa0000);
    VerifyStackCanaries(g_stackB, 0xbbbb0000);
    VerifyLargeStackCanaries(g_stackC, 0xcccc0000);

    Require(sceFiberFinalize(g_fiberA) == SCE_OK);
    Require(sceFiberFinalize(g_fiberB) == SCE_OK);
    Require(sceFiberFinalize(g_fiberC) == SCE_OK);

    Require(sceFiberFinalize(g_fiberA) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberB) == FiberErrorInvalid);
    Require(sceFiberFinalize(g_fiberC) == FiberErrorInvalid);
}

} // namespace

int main() {
    TestCallingContextRestrictions();
    TestSwitchSelfAndInvalidRejection();
    TestRegisterAndCanaryPreservationMultiFiber();
    return 0;
}
