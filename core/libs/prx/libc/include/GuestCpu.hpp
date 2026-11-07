#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTCPU_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTCPU_HPP

#include <array>
#include <cstdint>
#include <cstring>

#ifdef ANYPS5_GUEST_LIFTED

extern "C" {
std::uint32_t GuestCpuMxcsr_nid_no_patch(void);
void GuestCpuSetMxcsr_nid_no_patch(std::uint32_t value);
std::uint16_t GuestCpuX87Control_nid_no_patch(void);
void GuestCpuSetX87Control_nid_no_patch(std::uint16_t value);
std::uint16_t GuestCpuX87Status_nid_no_patch(void);
void GuestCpuSetX87Status_nid_no_patch(std::uint16_t value);
void GuestCpuX87Wait_nid_no_patch(void);
std::uint32_t GuestCpuMxcsrMask_nid_no_patch(void);
void GuestCpuUnwindStart_nid_no_patch(std::uint64_t* registers);
const char* GuestCpuImportName_nid_no_patch(std::uint64_t address);
void GuestCpuMakingWritable_nid_no_patch(std::uint64_t address, std::uint64_t length);
[[noreturn]] void GuestCpuUnwindResume_nid_no_patch(const std::uint64_t* registers);
std::uint64_t GuestCpuReadTsc_nid_no_patch(void);
void GuestCpuCpuid_nid_no_patch(std::uint32_t leaf, std::uint32_t subleaf, std::uint32_t* registers);
std::uint64_t GuestCpuXgetbv_nid_no_patch(std::uint32_t index);
void GuestCpuUseStack_nid_no_patch(std::uint64_t stackTop);
std::uint64_t GuestCpuFindImage_nid_no_patch(const char* path);
std::uint64_t GuestCpuImageSymbol_nid_no_patch(std::uint64_t image, const char* name);
std::uint64_t GuestCpuCall_nid_no_patch(std::uint64_t target, std::uint64_t first, std::uint64_t second, std::uint64_t third);
std::uint64_t GuestCpuHostStackSize_nid_no_patch(void);
void GuestCpuCallInterrupted_nid_no_patch(std::uint64_t target, std::uint64_t first, std::uint64_t second);
void GuestCpuInterruptedContext_nid_no_patch(std::uint64_t* registers, std::uint64_t* flags);
}

namespace GuestCpu {

inline std::uint32_t Mxcsr() { return GuestCpuMxcsr_nid_no_patch(); }
inline void SetMxcsr(const std::uint32_t value) { GuestCpuSetMxcsr_nid_no_patch(value); }
inline std::uint16_t X87Control() { return GuestCpuX87Control_nid_no_patch(); }
inline void SetX87Control(const std::uint16_t value) { GuestCpuSetX87Control_nid_no_patch(value); }
inline std::uint16_t X87Status() { return GuestCpuX87Status_nid_no_patch(); }
inline void SetX87Status(const std::uint16_t value) { GuestCpuSetX87Status_nid_no_patch(value); }
inline void X87ClearExceptions() { SetX87Status(static_cast<std::uint16_t>(X87Status() & 0x7f00)); }
inline void X87Wait() { GuestCpuX87Wait_nid_no_patch(); }
inline std::uint32_t MxcsrMask() { return GuestCpuMxcsrMask_nid_no_patch(); }
inline std::uint64_t ReadTsc() { return GuestCpuReadTsc_nid_no_patch(); }

inline std::array<std::uint32_t, 4> Cpuid(const std::uint32_t leaf, const std::uint32_t subleaf) {
    std::array<std::uint32_t, 4> registers;
    GuestCpuCpuid_nid_no_patch(leaf, subleaf, registers.data());
    return registers;
}

inline std::uint64_t Xgetbv(const std::uint32_t index) { return GuestCpuXgetbv_nid_no_patch(index); }

inline std::array<std::uint32_t, 7> X87Environment() {
    return {0xffff0000u | X87Control(), 0xffff0000u | X87Status(), 0xffffffffu, 0, 0, 0, 0xffff0000u};
}

inline void SetX87Environment(const std::array<std::uint32_t, 7>& environment) {
    SetX87Control(static_cast<std::uint16_t>(environment[0]));
    SetX87Status(static_cast<std::uint16_t>(environment[1]));
}

inline void RaiseInvalidForSignalingNan(const float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    if ((bits & 0x7fffffffu) > 0x7f800000u && (bits & 0x400000u) == 0) SetMxcsr(Mxcsr() | 1u);
}

}

#elif defined(__x86_64__)

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
#error "GuestCpu.hpp needs an x86-64 target or ANYPS5_GUEST_LIFTED"
#endif

#endif
