#ifndef CODEGEN_X86_SCALARPREFIXES_HPP
#define CODEGEN_X86_SCALARPREFIXES_HPP

#include <codegen/x86/ScalarLowering.hpp>
#include <cstdint>
#include <vector>

namespace Codegen::ScalarLowering {

struct LegacyEncoding {
    std::vector<std::uint8_t> Segment;
    bool OperandSize = false;
    bool Repeat = false;
    bool RepeatNot = false;
    std::uint8_t Rex = 0;
    std::size_t Opcode = 0;
};

inline LegacyEncoding ReadLegacyEncoding(const std::uint8_t* data, const std::size_t length) {
    LegacyEncoding encoding;
    std::size_t index = 0;
    for (; index < length; ++index) {
        const auto value = data[index];
        if (value == 0x66)
            encoding.OperandSize = true;
        else if (value == 0xF3)
            encoding.Repeat = true;
        else if (value == 0xF2)
            encoding.RepeatNot = true;
        else if (LegacyPrefix(value))
            encoding.Segment.push_back(value);
        else
            break;
    }
    if (index < length && (data[index] & 0xF0) == 0x40)
        encoding.Rex = data[index++];
    encoding.Opcode = index;
    return encoding;
}

inline void SizedStackSlot(StubBodyBuilder& body, const std::uint8_t opcode, const std::uint8_t width, const std::uint8_t reg, const std::int32_t displacement) {
    auto bytes = StackBytes(opcode, width == 64, reg, displacement);
    if (width == 16) bytes.insert(bytes.begin(), 0x66);
    body.Raw(bytes);
}

inline void SizedRegister(StubBodyBuilder& body, const std::uint8_t opcode, const std::uint8_t width, const std::uint8_t reg, const std::uint8_t rm) {
    auto bytes = RegisterBytes(opcode, width == 64, reg, rm);
    if (width == 16) bytes.insert(bytes.begin(), 0x66);
    body.Raw(bytes);
}

inline void SizedMemory(StubBodyBuilder& body, const std::uint8_t opcode, const std::uint8_t width, MemoryOperand memory, const std::uint8_t reg, const char* instruction, const std::int32_t frame) {
    if (width == 16) memory.Prefixes.insert(memory.Prefixes.begin(), 0x66);
    Memory(body, opcode, width == 64, memory, reg, instruction, frame);
}

inline void MoveImmediate32(StubBodyBuilder& body, const std::uint8_t reg, const std::uint32_t value) {
    std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(0x40 | ((reg & 8) >> 3)), static_cast<std::uint8_t>(0xB8 | (reg & 7))};
    Append32(bytes, static_cast<std::int32_t>(value));
    body.Raw(bytes);
}

inline void TwoByteRegister(StubBodyBuilder& body, const std::uint8_t opcode, const bool wide, const std::uint8_t reg, const std::uint8_t rm) {
    body.Raw(std::vector<std::uint8_t>{Rex(wide, reg, rm), 0x0F, opcode, static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7))});
}

inline void TwoByteStackSlot(StubBodyBuilder& body, const std::uint8_t opcode, const std::uint8_t reg, const std::int32_t displacement) {
    std::vector<std::uint8_t> bytes{Rex(false, reg, Stack), 0x0F, opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24};
    Append32(bytes, displacement);
    body.Raw(bytes);
}

inline void ByteSwap(StubBodyBuilder& body, const bool wide, const std::uint8_t reg) {
    body.Raw(std::vector<std::uint8_t>{Rex(wide, 0, reg), 0x0F, static_cast<std::uint8_t>(0xC8 | (reg & 7))});
}

}

#endif
