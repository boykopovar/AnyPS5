#ifndef CODEGEN_X86_X64REGISTER_HPP
#define CODEGEN_X86_X64REGISTER_HPP

#include <cstdint>

namespace Codegen {

enum class X64Register : std::uint8_t { Ax, Cx, Dx, Bx, Sp, Bp, Si, Di, R8, R9, R10, R11, R12, R13, R14, R15 };

}

#endif
