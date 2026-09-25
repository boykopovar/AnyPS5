#pragma once
// Emulation of the AMD SSE4a bit-field instructions EXTRQ and INSERTQ on a thread CONTEXT.
//
// The PS5's Zen 2 CPU has SSE4a and titles use it (Demon's Souls packs bytes with vpinsrb + insertq),
// but Intel hosts raise STATUS_ILLEGAL_INSTRUCTION. The crash reporter's vectored handler decodes the
// faulting bytes with this header, performs the operation on the CONTEXT's XMM registers and resumes.
// Kept header-only and free of process state so a host test can drive it with crafted CONTEXTs.
//
// Encodings (AMD64 Architecture Programmer's Manual vol. 4); only register-direct ModRM forms exist:
//   EXTRQ   xmm1, imm8(length), imm8(index)         66 [REX] 0F 78 /0 ib ib
//   EXTRQ   xmm1, xmm2                              66 [REX] 0F 79 /r   (length = xmm2[5:0], index = xmm2[13:8])
//   INSERTQ xmm1, xmm2, imm8(length), imm8(index)   F2 [REX] 0F 78 /r ib ib
//   INSERTQ xmm1, xmm2                              F2 [REX] 0F 79 /r   (length = xmm2[69:64], index = xmm2[77:72])
// A length of 0 means 64. EXTRQ zero-extends the extracted field into xmm1[63:0]; INSERTQ writes the
// low `length` bits of xmm2[63:0] into xmm1[63:0] at bit `index`, keeping the bits outside the field.
// xmm1[127:64] is undefined after both and is left untouched here.
#ifdef _WIN32
#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace sse4a {

enum class Op : std::uint8_t { Extrq, Insertq };

struct Instruction {
    Op op = Op::Extrq;
    bool registerForm = false; // 0F 79: length and index come from the source register
    unsigned destination = 0;  // xmm register number 0-15 (xmm1 in the manual)
    unsigned source = 0;       // xmm register number 0-15 (xmm2; equals destination for EXTRQ imm)
    std::uint8_t length = 0;   // raw imm8 (immediate forms only)
    std::uint8_t index = 0;    // raw imm8 (immediate forms only)
    std::size_t size = 0;      // encoded length in bytes (4 to 7)
};

// The effective field after masking and the length-0-means-64 rule.
struct Field {
    unsigned length = 0;
    unsigned index = 0;
};

constexpr std::size_t kMaxInstructionSize = 7; // prefix, REX, 0F, 78, ModRM, imm8, imm8

inline bool Decode(const std::uint8_t* bytes, std::size_t available, Instruction& out) {
    std::size_t cursor = 0;
    if (available < 4) return false;
    const std::uint8_t prefix = bytes[cursor++];
    if (prefix != 0x66 && prefix != 0xf2) return false;
    std::uint8_t rex = 0;
    if ((bytes[cursor] & 0xf0) == 0x40) rex = bytes[cursor++];
    if (cursor + 3 > available) return false;
    if (bytes[cursor++] != 0x0f) return false;
    const std::uint8_t opcode = bytes[cursor++];
    if (opcode != 0x78 && opcode != 0x79) return false;
    const std::uint8_t modrm = bytes[cursor++];
    if ((modrm >> 6) != 3) return false; // register-direct only
    const unsigned regField = (modrm >> 3) & 7;
    const unsigned reg = regField | ((rex & 0x4) ? 8u : 0u);
    const unsigned rm = (modrm & 7) | ((rex & 0x1) ? 8u : 0u);
    Instruction result;
    result.op = prefix == 0x66 ? Op::Extrq : Op::Insertq;
    result.registerForm = opcode == 0x79;
    if (opcode == 0x78) {
        if (cursor + 2 > available) return false;
        result.length = bytes[cursor++];
        result.index = bytes[cursor++];
        if (result.op == Op::Extrq) {
            if (regField != 0) return false; // /0: the reg field is an opcode extension
            result.destination = result.source = rm;
        } else {
            result.destination = reg;
            result.source = rm;
        }
    } else {
        result.destination = reg;
        result.source = rm;
    }
    result.size = cursor;
    out = result;
    return true;
}

inline M128A& Register(CONTEXT& context, unsigned number) {
    return context.FltSave.XmmRegisters[number & 15];
}

inline Field Resolve(const Instruction& instruction, const CONTEXT& context) {
    Field field;
    if (instruction.registerForm) {
        const M128A& source = context.FltSave.XmmRegisters[instruction.source & 15];
        const auto control = instruction.op == Op::Extrq ? static_cast<std::uint64_t>(source.Low) : static_cast<std::uint64_t>(source.High);
        field.length = static_cast<unsigned>(control & 0x3f);
        field.index = static_cast<unsigned>((control >> 8) & 0x3f);
    } else {
        field.length = instruction.length & 0x3f;
        field.index = instruction.index & 0x3f;
    }
    if (field.length == 0) field.length = 64;
    return field;
}

inline std::uint64_t FieldMask(unsigned length) {
    return length >= 64 ? ~std::uint64_t{0} : ((std::uint64_t{1} << length) - 1);
}

// Applies the decoded instruction to the CONTEXT's XMM registers (Rip is not touched).
inline Field Execute(const Instruction& instruction, CONTEXT& context) {
    const Field field = Resolve(instruction, context);
    const std::uint64_t mask = FieldMask(field.length);
    M128A& destination = Register(context, instruction.destination);
    const M128A& source = Register(context, instruction.source);
    const auto low = static_cast<std::uint64_t>(destination.Low);
    if (instruction.op == Op::Extrq) {
        destination.Low = (low >> field.index) & mask;
    } else {
        const std::uint64_t hole = mask << field.index;
        const std::uint64_t bits = (static_cast<std::uint64_t>(source.Low) & mask) << field.index;
        destination.Low = (low & ~hole) | bits;
    }
    return field;
}

// Emulates the EXTRQ / INSERTQ at `bytes` on `context` and advances Rip past it. Returns false, leaving
// the context untouched, when the bytes are not one of those instructions.
inline bool Emulate(const std::uint8_t* bytes, std::size_t available, CONTEXT& context, Instruction* decoded = nullptr, Field* field = nullptr) {
    Instruction instruction;
    if (!Decode(bytes, available, instruction)) return false;
    const Field resolved = Execute(instruction, context);
    context.Rip += instruction.size;
    if (decoded) *decoded = instruction;
    if (field) *field = resolved;
    return true;
}

}
#endif
