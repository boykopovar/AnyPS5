#ifndef CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_GUESTCPU_HPP
#define CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_GUESTCPU_HPP

#include <cstddef>
#include <cstdint>

namespace WasmRuntime {

inline constexpr std::uint32_t AbiVersion = 1;
inline constexpr std::uint64_t StubStride = 16;

enum Register : unsigned {
    Rax = 0,
    Rcx = 1,
    Rdx = 2,
    Rbx = 3,
    Rsp = 4,
    Rbp = 5,
    Rsi = 6,
    Rdi = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15,
};

inline constexpr Register IntegerArgumentRegisters[6] = {Rdi, Rsi, Rdx, Rcx, R8, R9};
inline constexpr unsigned FloatArgumentRegisters = 8;

struct alignas(64) GuestCpu {
    std::uint64_t gpr[16];
    std::uint64_t rip;
    std::uint64_t rflags;
    std::uint64_t fsBase;
    std::uint64_t gsBase;
    std::uint64_t stopRip;
    std::uint32_t mxcsr;
    std::uint8_t reserved0[0x14];
    alignas(32) std::uint8_t ymm[16][32];
    double x87St[8];
    std::uint32_t x87Top;
    std::uint16_t x87Fcw;
    std::uint16_t x87Fsw;
    std::uint16_t x87Tags;
    std::uint8_t reserved1[0xF6];
};

static_assert(offsetof(GuestCpu, gpr) == 0x000);
static_assert(offsetof(GuestCpu, rip) == 0x080);
static_assert(offsetof(GuestCpu, rflags) == 0x088);
static_assert(offsetof(GuestCpu, fsBase) == 0x090);
static_assert(offsetof(GuestCpu, gsBase) == 0x098);
static_assert(offsetof(GuestCpu, stopRip) == 0x0A0);
static_assert(offsetof(GuestCpu, mxcsr) == 0x0A8);
static_assert(offsetof(GuestCpu, ymm) == 0x0C0);
static_assert(offsetof(GuestCpu, x87St) == 0x2C0);
static_assert(offsetof(GuestCpu, x87Top) == 0x300);
static_assert(offsetof(GuestCpu, x87Fcw) == 0x304);
static_assert(offsetof(GuestCpu, x87Fsw) == 0x306);
static_assert(offsetof(GuestCpu, x87Tags) == 0x308);
static_assert(sizeof(GuestCpu) == 0x400);

inline constexpr std::uint32_t InitialMxcsr = 0x1F80;
inline constexpr std::uint16_t InitialFcw = 0x037F;

}  // namespace WasmRuntime

#endif
