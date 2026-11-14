#ifndef CODEGEN_X86_VEXENCODING_HPP
#define CODEGEN_X86_VEXENCODING_HPP

#include <codegen/x86/ScalarLowering.hpp>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Codegen::VexEncoding {

inline constexpr std::uint8_t Map0F = 1;
inline constexpr std::uint8_t Map0F38 = 2;
inline constexpr std::uint8_t Map0F3A = 3;
inline constexpr std::uint8_t Prefix66 = 1;
inline constexpr std::uint8_t PrefixF3 = 2;
inline constexpr std::uint8_t PrefixF2 = 3;

struct Form {
    std::uint8_t Map;
    std::uint8_t Prefix;
    bool Wide;
    bool Vector256;
    std::uint8_t Opcode;
};

inline std::vector<std::uint8_t> Header(const Form& form, const std::uint8_t reg, const std::uint8_t vvvv, const bool indexExtended, const bool baseExtended) {
    return {0xC4,
        static_cast<std::uint8_t>(((reg & 8) != 0 ? 0 : 0x80) | (indexExtended ? 0 : 0x40) | (baseExtended ? 0 : 0x20) | form.Map),
        static_cast<std::uint8_t>((form.Wide ? 0x80 : 0) | ((~vvvv & 15) << 3) | (form.Vector256 ? 4 : 0) | form.Prefix),
        form.Opcode};
}

inline void Register(StubBodyBuilder& body, const Form& form, const std::uint8_t reg, const std::uint8_t vvvv, const std::uint8_t rm, const std::optional<std::uint8_t> immediate = std::nullopt) {
    auto bytes = Header(form, reg, vvvv, false, (rm & 8) != 0);
    bytes.push_back(static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7)));
    if (immediate) bytes.push_back(*immediate);
    body.Raw(bytes);
}

inline void StackOperand(StubBodyBuilder& body, const Form& form, const std::uint8_t reg, const std::uint8_t vvvv, const std::int32_t displacement) {
    auto bytes = Header(form, reg, vvvv, false, false);
    bytes.insert(bytes.end(), {static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24});
    ScalarLowering::Append32(bytes, displacement);
    body.Raw(bytes);
}

inline void Memory(StubBodyBuilder& body, const Form& form, const std::uint8_t reg, const std::uint8_t vvvv, const MemoryOperand& memory, const std::int32_t frame, const char* instruction, const std::optional<std::uint8_t> immediate = std::nullopt) {
    auto displacement = static_cast<std::int64_t>(memory.Displacement);
    if (memory.StackBase) displacement += frame;
    if (displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException(std::string(instruction) + " stack displacement does not fit after spilling");
    const auto mod = memory.StackBase ? 2 : memory.Mod;
    auto bytes = memory.Prefixes;
    const auto header = Header(form, reg, vvvv, (memory.RexIndexBase & 2) != 0, (memory.RexIndexBase & 1) != 0);
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | ((reg & 7) << 3) | memory.Rm));
    if (memory.Rm == 4) bytes.push_back(memory.Sib);
    const auto offset = bytes.size();
    if (mod == 1) bytes.push_back(static_cast<std::uint8_t>(displacement));
    if (mod == 2 || (mod == 0 && (memory.RipRelative || (memory.Rm == 4 && (memory.Sib & 7) == 5))))
        ScalarLowering::Append32(bytes, static_cast<std::int32_t>(displacement));
    if (immediate) bytes.push_back(*immediate);
    if (memory.RipRelative)
        body.RipRelative(bytes, offset, static_cast<std::int64_t>(memory.InstructionLength) + memory.Displacement);
    else
        body.Raw(bytes);
}

inline constexpr Form MoveUnaligned256Load{Map0F, PrefixF3, false, true, 0x6F};
inline constexpr Form MoveUnaligned256Store{Map0F, PrefixF3, false, true, 0x7F};
inline constexpr Form MoveUnaligned128Load{Map0F, PrefixF3, false, false, 0x6F};
inline constexpr Form InsertHigh{Map0F3A, Prefix66, false, true, 0x18};
inline constexpr Form ExtractHigh{Map0F3A, Prefix66, false, true, 0x19};

inline void ExtractUpper(StubBodyBuilder& body, const std::uint8_t destination, const std::uint8_t source) {
    Register(body, ExtractHigh, source, 0, destination, std::uint8_t{1});
}

inline void InsertUpper(StubBodyBuilder& body, const std::uint8_t destination, const std::uint8_t low, const std::uint8_t high) {
    Register(body, InsertHigh, destination, low, high, std::uint8_t{1});
}

struct Instruction {
    std::uint8_t Map;
    std::uint8_t Prefix;
    bool Wide;
    bool Vector256;
    std::uint8_t Opcode;
    std::uint8_t Reg;
    std::uint8_t Vvvv;
    std::uint8_t Rm;
    std::optional<MemoryOperand> Memory;
    std::size_t End;
};

inline std::optional<Instruction> Decode(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    std::vector<std::uint8_t> prefixes;
    while (vex < length && ScalarLowering::LegacyPrefix(data[vex])) prefixes.push_back(data[vex++]);
    if (vex + 4 > length || (data[vex] != 0xC4 && data[vex] != 0xC5)) return std::nullopt;
    const bool threeByte = data[vex] == 0xC4;
    if (threeByte && vex + 5 > length) return std::nullopt;
    const auto payload = threeByte ? data[vex + 2] : data[vex + 1];
    const auto extensions = threeByte ? static_cast<std::uint8_t>(~data[vex + 1]) : static_cast<std::uint8_t>(~data[vex + 1] & 0x80);
    const auto opcode = vex + (threeByte ? 3 : 2);
    const auto modrm = data[opcode + 1];
    Instruction instruction{
        threeByte ? static_cast<std::uint8_t>(data[vex + 1] & 0x1F) : Map0F,
        static_cast<std::uint8_t>(payload & 3), threeByte && (payload & 0x80) != 0, (payload & 4) != 0, data[opcode],
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~payload >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        std::nullopt, opcode + 2};
    if ((modrm >> 6) != 3) {
        instruction.Memory = DecodeMemoryOperand(data, length, opcode + 1, static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5)), prefixes);
        instruction.End = opcode + 1 + instruction.Memory->EncodedSize;
    }
    return instruction;
}

}

#endif
