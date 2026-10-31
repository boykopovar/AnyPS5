#ifndef CODEGEN_X86_SCALARLOWERING_HPP
#define CODEGEN_X86_SCALARLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Codegen::ScalarLowering {

inline constexpr std::int32_t FrameSize = 144;
inline constexpr std::uint8_t Stack = 4;

inline bool LegacyPrefix(std::uint8_t value) {
    return value == 0x2E || value == 0x36 || value == 0x3E || value == 0x26 || value == 0x64 || value == 0x65 || value == 0x67;
}

inline void Append32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
}

inline std::uint8_t Rex(bool wide, std::uint8_t reg, std::uint8_t rm) {
    return static_cast<std::uint8_t>(0x40 | (wide ? 8 : 0) | ((reg & 8) >> 1) | ((rm & 8) >> 3));
}

inline std::vector<std::uint8_t> RegisterBytes(std::uint8_t opcode, bool wide, std::uint8_t reg, std::uint8_t rm) {
    return {Rex(wide, reg, rm), opcode, static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7))};
}

inline std::vector<std::uint8_t> StackBytes(std::uint8_t opcode, bool wide, std::uint8_t reg, std::int32_t displacement) {
    std::vector<std::uint8_t> bytes{Rex(wide, reg, Stack), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24};
    Append32(bytes, displacement);
    return bytes;
}

inline void Register(StubBodyBuilder& body, std::uint8_t opcode, bool wide, std::uint8_t reg, std::uint8_t rm) {
    body.Raw(RegisterBytes(opcode, wide, reg, rm));
}

inline void StackSlot(StubBodyBuilder& body, std::uint8_t opcode, bool wide, std::uint8_t reg, std::int32_t displacement) {
    body.Raw(StackBytes(opcode, wide, reg, displacement));
}

inline std::array<bool, 16> UsedRegisters(std::initializer_list<std::uint8_t> registers, const std::optional<MemoryOperand>& memory) {
    std::array<bool, 16> used{};
    used[Stack] = true;
    for (const auto reg : registers) {
        if (reg >= used.size()) throw CodegenException("register index " + std::to_string(reg) + " is out of range");
        used[reg] = true;
    }
    if (!memory) return used;
    if (memory->Rm == 4) {
        const auto base = static_cast<std::uint8_t>((memory->Sib & 7) | ((memory->RexIndexBase & 1) << 3));
        const auto index = static_cast<std::uint8_t>(((memory->Sib >> 3) & 7) | ((memory->RexIndexBase & 2) << 2));
        if (memory->Mod != 0 || (memory->Sib & 7) != 5) used[base] = true;
        if (index != Stack) used[index] = true;
    } else if (!memory->RipRelative) {
        used[memory->Rm | ((memory->RexIndexBase & 1) << 3)] = true;
    }
    return used;
}

inline std::uint8_t Scratch(std::array<bool, 16>& used, const char* instruction) {
    for (const auto reg : {11, 10, 9, 8, 0, 1, 2, 3, 5, 6, 7, 12, 13, 14, 15}) {
        if (used[reg]) continue;
        used[reg] = true;
        return static_cast<std::uint8_t>(reg);
    }
    throw CodegenException(std::string(instruction) + " has no scratch register");
}

inline void Memory(StubBodyBuilder& body, std::uint8_t opcode, bool wide, const MemoryOperand& memory, std::uint8_t reg, const char* instruction, std::int32_t frame = FrameSize) {
    auto displacement = static_cast<std::int64_t>(memory.Displacement);
    if (memory.StackBase) displacement += frame;
    const bool address32 = std::find(memory.Prefixes.begin(), memory.Prefixes.end(), 0x67) != memory.Prefixes.end();
    if (!address32 && displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException(std::string(instruction) + " stack displacement does not fit after spilling");
    const auto mod = memory.StackBase ? 2 : memory.Mod;
    auto bytes = memory.Prefixes;
    bytes.push_back(static_cast<std::uint8_t>(Rex(wide, reg, 0) | memory.RexIndexBase));
    bytes.push_back(opcode);
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | ((reg & 7) << 3) | memory.Rm));
    if (memory.Rm == 4) bytes.push_back(memory.Sib);
    const auto offset = bytes.size();
    if (mod == 1) bytes.push_back(static_cast<std::uint8_t>(displacement));
    if (mod == 2 || (mod == 0 && (memory.RipRelative || (memory.Rm == 4 && (memory.Sib & 7) == 5))))
        Append32(bytes, static_cast<std::int32_t>(displacement));
    if (memory.RipRelative)
        body.RipRelative(bytes, offset, static_cast<std::int64_t>(memory.InstructionLength) + memory.Displacement);
    else
        body.Raw(bytes);
}

}

#endif
