#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <barrier>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <xmmintrin.h>

extern "C" {
int* APS5_VABI __error_nid_postfix();
void APS5_VABI _Feraise_nid_postfix(int);
int APS5_VABI fegetround_nid_postfix();
int APS5_VABI fesetround_nid_postfix(int);
int APS5_VABI fetestexcept_nid_postfix(int);
int APS5_VABI feclearexcept_nid_postfix(int);
int APS5_VABI fegetexceptflag_nid_postfix(std::uint16_t*, int);
int APS5_VABI fesetexceptflag_nid_postfix(const std::uint16_t*, int);
int APS5_VABI feenableexcept_nid_postfix(int);
int APS5_VABI fedisableexcept_nid_postfix(int);
int APS5_VABI __feenableexcept_nid_postfix(int);
int APS5_VABI __fedisableexcept_nid_postfix(int);
int APS5_VABI fegettrapenable_nid_postfix();
int APS5_VABI fesettrapenable_nid_postfix(int);
}

namespace {

struct Environment {
    std::array<std::uint32_t, 7> x87;
    std::uint32_t sse;
};

Environment ReadEnvironment() {
    Environment state;
    asm volatile("fnstenv %0\n\tfldenv %0" : "=m"(state.x87));
    state.sse = _mm_getcsr();
    return state;
}

void WriteEnvironment(const Environment& state) {
    asm volatile("fldenv %0" : : "m"(state.x87));
    _mm_setcsr(state.sse);
}

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "floating-point environment: %s\n", message);
        std::abort();
    }
}

Environment SeedFlags(unsigned x87, unsigned sse) {
    auto state = ReadEnvironment();
    state.x87[0] |= 0x3f;
    state.x87[1] = (state.x87[1] & ~0xffu) | x87;
    state.sse = (state.sse & ~0x3fu) | (0x3fu << 7) | sse;
    WriteEnvironment(state);
    return ReadEnvironment();
}

void CheckRounding() {
    const auto seeded = SeedFlags(0x02, 0x10);
    constexpr int modes[] = {0, 0x400, 0x800, 0xc00};
    constexpr int positive[] = {2, 1, 2, 1};
    constexpr int negative[] = {-2, -2, -1, -1};
    for (unsigned index = 0; index < 4; ++index) {
        const auto before = ReadEnvironment();
        Require(fesetround_nid_postfix(modes[index]) == 0, "set rounding mode");
        Require(fegetround_nid_postfix() == modes[index], "query rounding mode");
        const auto state = ReadEnvironment();
        Require((state.x87[0] & 0xc00) == static_cast<unsigned>(modes[index]), "x87 rounding bits");
        Require((state.sse & 0x6000) == static_cast<unsigned>(modes[index] << 3), "SSE rounding bits");
        Require((state.x87[0] & ~0xc00u) == (seeded.x87[0] & ~0xc00u), "preserve x87 control bits");
        Require((state.sse & ~0x603fu) == (seeded.sse & ~0x603fu), "preserve SSE control bits");
        Require(state.x87[1] == before.x87[1] && (state.sse & 0x3f) == (before.sse & 0x3f), "rounding preserves exceptions");
        for (const double input : {1.75, -1.75}) {
            const int expected = input > 0 ? positive[index] : negative[index];
            int x87;
            asm volatile("fldl %1\n\tfistpl %0" : "=m"(x87) : "m"(input) : "st");
            Require(x87 == expected, "x87 arithmetic observes rounding");
            int sse;
            asm volatile("cvtsd2si %1, %0" : "=r"(sse) : "x"(input));
            Require(sse == expected, "SSE arithmetic observes rounding");
        }
    }
    for (const int invalid : {-1, 1, 0x200, 0x1000, 0x7fffffff}) {
        const auto before = ReadEnvironment();
        Require(fesetround_nid_postfix(invalid) == -1, "reject invalid rounding mode");
        const auto after = ReadEnvironment();
        Require(before.x87[0] == after.x87[0] && before.sse == after.sse, "invalid mode leaves state unchanged");
    }
    auto distinct = ReadEnvironment();
    distinct.x87[0] = (distinct.x87[0] & ~0xc00u) | 0x400;
    distinct.sse = (distinct.sse & ~0x6000u) | 0x4000;
    WriteEnvironment(distinct);
    Require(fegetround_nid_postfix() == 0x400, "query uses the x87 rounding mode");
}

void CheckFeraise() {
    for (unsigned requested = 0; requested <= 0x3f; ++requested) {
        const auto seeded = SeedFlags(0, 0);
        *__error_nid_postfix() = 0;
        _Feraise_nid_postfix(static_cast<int>(requested));
        const unsigned raised = (requested & 0x18) != 0 ? requested | 0x20 : requested;
        const auto state = ReadEnvironment();
        Require((state.x87[1] & 0x3f) == raised && (state.sse & 0x3f) == raised, "_Feraise raises the flags on both units");
        Require(state.x87[0] == seeded.x87[0] && (state.sse & ~0x3fu) == (seeded.sse & ~0x3fu), "_Feraise preserves control bits");
        const int error = (raised & 0x01) != 0 || (raised & 0x1c) == 0 ? 33 : 34;
        Require(*__error_nid_postfix() == error, "_Feraise sets EDOM or ERANGE");
    }
    SeedFlags(0, 0);
}

void CheckFlags() {
    const auto initial = SeedFlags(0x03, 0x24);
    Require(fetestexcept_nid_postfix(0) == 0, "empty exception query");
    Require(fetestexcept_nid_postfix(0x3f) == 0x27, "combine x87 and SSE flags, including denormals");
    Require(fetestexcept_nid_postfix(0x0c) == 0x04, "query only requested flags");
    struct { std::uint16_t before, value, after; } saved{0x1234, 0xffff, 0x5678};
    Require(fegetexceptflag_nid_postfix(&saved.value, 0x07) == 0 && saved.value == 0x07, "save selected flags");
    Require(saved.before == 0x1234 && saved.after == 0x5678, "fexcept_t writes exactly two bytes");
    auto state = ReadEnvironment();
    Require(state.x87[1] == initial.x87[1] && state.sse == initial.sse, "queries do not change flags");

    const std::uint16_t flags = 0x22;
    Require(fesetexceptflag_nid_postfix(&flags, 0x0a) == 0, "restore selected flags");
    state = ReadEnvironment();
    Require((state.x87[1] & 0x3f) == 0x03 && (state.sse & 0x3f) == 0x26, "restore only selected flags on both units");
    Require(fesetexceptflag_nid_postfix(&flags, 0) == 0, "empty restore");
    Require(feclearexcept_nid_postfix(0x03) == 0, "clear selected flags");
    state = ReadEnvironment();
    Require((state.x87[1] & 0x3f) == 0 && (state.sse & 0x3f) == 0x24, "clear flags on both units");
    Require(state.x87[0] == initial.x87[0], "flag operations preserve x87 control");
    Require((state.sse & ~0x3fu) == (initial.sse & ~0x3fu), "flag operations preserve SSE control");
    Require(feclearexcept_nid_postfix(0) == 0 && fetestexcept_nid_postfix(0x3f) == 0x24, "empty clear");
    SeedFlags(0x3f, 0x3f);
    Require(feclearexcept_nid_postfix(0x3f) == 0 && fetestexcept_nid_postfix(0x3f) == 0, "clear every exception");
    Require(fegetexceptflag_nid_postfix(&saved.value, 0) == 0 && saved.value == 0, "save empty mask");
    auto traps = ReadEnvironment();
    traps.x87[0] &= ~0x08u;
    traps.sse &= ~(0x08u << 7);
    WriteEnvironment(traps);
    traps = ReadEnvironment();
    const std::uint16_t none = 0;
    Require(fesetexceptflag_nid_postfix(&none, 0x01) == 0, "restore with an unmasked exception");
    Require(feclearexcept_nid_postfix(0x02) == 0, "clear with an unmasked exception");
    state = ReadEnvironment();
    Require(state.x87[0] == traps.x87[0] && state.sse == traps.sse, "flag updates preserve exception masks");
    SeedFlags(0, 0);
    for (const bool get : {false, true}) {
        bool threw = false;
        try {
            if (get) fegetexceptflag_nid_postfix(nullptr, 0x3f);
            else fesetexceptflag_nid_postfix(nullptr, 0x3f);
        } catch (const std::invalid_argument&) { threw = true; }
        Require(threw, "reject null exception storage");
    }
    CheckFeraise();
}

void CheckThreads() {
    Require(fesetround_nid_postfix(0) == 0, "reset main rounding mode");
    Require(feclearexcept_nid_postfix(0x3f) == 0, "reset main exceptions");
    std::barrier ready(2);
    const auto worker = [&](int rounding, std::uint16_t flags) {
        Require(fesetround_nid_postfix(rounding) == 0, "thread rounding mode");
        Require(fesetexceptflag_nid_postfix(&flags, 0x3f) == 0, "thread exceptions");
        ready.arrive_and_wait();
        Require(fegetround_nid_postfix() == rounding, "thread-local rounding mode");
        Require(fetestexcept_nid_postfix(0x3f) == flags, "thread-local exception flags");
    };
    std::thread first(worker, 0x400, 0x01);
    std::thread second(worker, 0x800, 0x04);
    first.join();
    second.join();
    Require(fegetround_nid_postfix() == 0 && fetestexcept_nid_postfix(0x3f) == 0, "worker state does not change main");
}

}

unsigned X87Masks() {
    return ReadEnvironment().x87[0] & 0x3fu;
}

unsigned SseMasks() {
    return (ReadEnvironment().sse >> 7) & 0x3fu;
}

bool HostKeepsSseTraps() {
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved & ~(0x01u << 7));
    const bool kept = (_mm_getcsr() & (0x01u << 7)) == 0;
    _mm_setcsr(saved);
    return kept;
}

void CheckTraps() {
    const auto seeded = SeedFlags(0, 0);
    const bool sseKept = HostKeepsSseTraps();
    const auto sse = [&](unsigned masks) { return sseKept ? masks : 0x3fu; };
    const auto both = [&](unsigned traps) { return static_cast<int>(0xffffffc0u | (sseKept ? traps : 0u)); };
    Require(feenableexcept_nid_postfix(0x49) == static_cast<int>(0xffffffc0u), "enable reports no previous trap as the console does");
    Require(X87Masks() == 0x36 && fegettrapenable_nid_postfix() == 0x09, "enable unmasks the x87 exceptions");
    Require(SseMasks() == sse(0x36), "enable unmasks the SSE exceptions");
    Require(fedisableexcept_nid_postfix(0x41) == both(0x09), "disable returns the traps enabled on both units");
    Require(X87Masks() == 0x37 && fegettrapenable_nid_postfix() == 0x08, "disable masks the x87 exception");
    Require(SseMasks() == sse(0x37), "disable masks the SSE exception");
    Require(__feenableexcept_nid_postfix(0x02) == both(0x08) && X87Masks() == 0x35, "the internal enable differs");
    Require(__fedisableexcept_nid_postfix(0x3f) == both(0x0a) && X87Masks() == 0x3f && fegettrapenable_nid_postfix() == 0, "the internal disable differs");
    Require(fesettrapenable_nid_postfix(0x40) == 1 && X87Masks() == 0x3f, "an out-of-range trap set was accepted");
    Require(fesettrapenable_nid_postfix(0x10004) == 0 && fegettrapenable_nid_postfix() == 0x04 && SseMasks() == sse(0x3b), "a trap set did not use the low 16 bits");
    Require(fesettrapenable_nid_postfix(0x01) == 0 && fegettrapenable_nid_postfix() == 0x05 && SseMasks() == sse(0x3a), "a trap set masked enabled traps");
    Require(fesettrapenable_nid_postfix(0) == 0 && fegettrapenable_nid_postfix() == 0x05 && SseMasks() == sse(0x3a), "an empty trap set masked enabled traps");
    auto mixed = ReadEnvironment();
    mixed.x87[0] = (mixed.x87[0] & ~0x3fu) | 0x36;
    mixed.sse = (mixed.sse & ~(0x3fu << 7)) | (0x3au << 7);
    WriteEnvironment(mixed);
    Require(feenableexcept_nid_postfix(0) == both(0x01), "enable reports only the traps enabled on both units");
    Require(fegettrapenable_nid_postfix() == 0x09, "the trap query reads only the x87 control word");
    WriteEnvironment(seeded);
}

int main() {
    const auto original = ReadEnvironment();
    CheckRounding();
    CheckFlags();
    CheckTraps();
    CheckThreads();
    WriteEnvironment(original);
    std::puts("floating-point environment checks passed");
}
