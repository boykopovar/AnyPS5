#pragma once
// Emulation of the AMD-only CLZERO instruction on a thread CONTEXT.
//
// The PS5's Zen 2 CPU has CLZERO and titles use it in bulk-zero loops, but Intel hosts raise
// STATUS_ILLEGAL_INSTRUCTION. The static conversion cannot always help: --to-intel rewrites fail
// when a branch target enters the instruction window a trampoline would move, and code generated
// at run time is out of scope for conversion entirely. The crash reporter's vectored handler
// decodes the faulting bytes with this header, performs the operation on the CONTEXT and resumes.
// Kept header-only and free of process state so a host test can drive it with crafted CONTEXTs.
//
// CLZERO (AMD64 Architecture Programmer's Manual) is encoded 0F 01 FC and zeroes the cache line
// (64 bytes, aligned down) containing the linear address in rax. Zeroing memory is its complete
// memory-visible effect, so a plain fill of the line is the exact emulation.
#ifdef _WIN32
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace clzero {

constexpr std::size_t kInstructionSize = 3;
constexpr std::uintptr_t kLineMask = 0x3f;

inline bool Decode(const std::uint8_t* bytes, std::size_t available) {
    return available >= kInstructionSize && bytes[0] == 0x0f && bytes[1] == 0x01 && bytes[2] == 0xfc;
}

// Zeroes the cache line containing the address in the context's rax and advances Rip past the
// instruction. Returns false, leaving the context untouched, when the bytes are not CLZERO.
inline bool Emulate(const std::uint8_t* bytes, std::size_t available, CONTEXT& context) {
    if (!Decode(bytes, available)) return false;
    std::memset(reinterpret_cast<void*>(static_cast<std::uintptr_t>(context.Rax) & ~kLineMask), 0, kLineMask + 1);
    context.Rip += kInstructionSize;
    return true;
}

}
#endif
