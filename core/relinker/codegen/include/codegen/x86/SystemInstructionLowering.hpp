#ifndef CODEGEN_X86_SYSTEMINSTRUCTIONLOWERING_HPP
#define CODEGEN_X86_SYSTEMINSTRUCTIONLOWERING_HPP

#include <codegen/x86/Sse4aLowering.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Codegen {

enum class SystemInstruction {
    Monitorx,
    Mwaitx,
    Clzero,
    Clzero32,
    Mcommit
};

class SystemInstructionLowering {
public:
    [[nodiscard]] std::vector<std::uint8_t> LowerInPlace(SystemInstruction instruction, std::size_t originalLength) const;
    [[nodiscard]] LoweredBody LowerOutOfLine(std::span<const SystemInstruction> sequence, std::span<const std::uint8_t> trailing) const;
};

}

#endif
