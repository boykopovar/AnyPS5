#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTCPU_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTCPU_HPP

#include <array>
#include <cstdint>

#if defined(__x86_64__)

#include <x86intrin.h>
#include <xmmintrin.h>

namespace GuestCpu {

inline std::uint64_t ReadTsc() { return __rdtsc(); }

inline std::array<std::uint32_t, 4> Cpuid(const std::uint32_t leaf, const std::uint32_t subleaf) {
    std::uint32_t eax;
    std::uint32_t ebx;
    std::uint32_t ecx;
    std::uint32_t edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "0"(leaf), "2"(subleaf));
    return {eax, ebx, ecx, edx};
}

inline std::uint64_t Xgetbv(const std::uint32_t index) {
    std::uint32_t low;
    std::uint32_t high;
    asm volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(index));
    return (static_cast<std::uint64_t>(high) << 32) | low;
}

inline std::uint32_t Mxcsr() { return _mm_getcsr(); }
inline void SetMxcsr(const std::uint32_t value) { _mm_setcsr(value); }

inline std::uint16_t X87Control() {
    std::uint16_t control;
    asm volatile("fnstcw %0" : "=m"(control));
    return control;
}

inline void SetX87Control(const std::uint16_t value) {
    asm volatile("fldcw %0" : : "m"(value));
}

inline std::uint16_t X87Status() {
    std::uint16_t status;
    asm volatile("fnstsw %0" : "=am"(status));
    return status;
}

inline void SetX87Status(const std::uint16_t value) {
    std::array<std::uint32_t, 7> environment;
    asm volatile("fnstenv %0" : "=m"(environment));
    environment[1] = (environment[1] & 0xffff0000u) | value;
    asm volatile("fldenv %0" : : "m"(environment));
}

inline void X87ClearExceptions() { asm volatile("fnclex"); }
inline void X87Wait() { asm volatile("fwait"); }

inline std::uint32_t MxcsrMask() {
    alignas(16) std::array<std::uint32_t, 128> state;
    asm volatile("fxsave %0" : "=m"(state));
    return state[7] != 0 ? state[7] : 0xffbfu;
}

inline std::array<std::uint32_t, 7> X87Environment() {
    std::array<std::uint32_t, 7> environment;
    asm volatile("fnstenv %0\n\tfldcw %0" : "+m"(environment));
    return environment;
}

inline void SetX87Environment(const std::array<std::uint32_t, 7>& environment) {
    asm volatile("fldenv %0" : : "m"(environment) : "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)");
}

inline void RaiseInvalidForSignalingNan(const float value) {
    volatile int unordered = _mm_ucomieq_ss(_mm_set_ss(value), _mm_setzero_ps());
    (void)unordered;
}

}

#else
#error "GuestCpu.hpp supports only x86-64 targets"
#endif

#endif
