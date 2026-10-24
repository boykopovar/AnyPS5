#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "GcnOpcodeTables.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

std::atomic<bool> g_gcnGuestShaders{false};

constexpr std::uint32_t LiteralSource = 0xffu;
constexpr std::uint32_t VccSdst = 106u;
constexpr std::uint32_t NullSoffset = 0x7du;
constexpr std::uint32_t LinkedCallSopk = 0x16u;
constexpr std::uint32_t CodeEndSopp = 0x1fu;
constexpr std::uint32_t SetRegImmSopk = 0x15u;

template <std::size_t N>
std::optional<std::uint32_t> mapOpcode(const std::array<std::pair<std::uint16_t, std::uint16_t>, N>& table, std::uint32_t opcode) {
    const auto found = std::lower_bound(table.begin(), table.end(), opcode, [](const auto& entry, std::uint32_t value) { return entry.first < value; });
    if (found == table.end() || found->first != opcode) return std::nullopt;
    return found->second;
}

[[noreturn]] void unsupported(const char* family, std::uint32_t opcode, std::uint32_t programCounter) {
    throw UnsupportedInstructionError(std::string("GCN ") + family + " opcode " + std::to_string(opcode) + " at program counter " + std::to_string(programCounter) + " is not implemented");
}

std::uint32_t replaceBits(std::uint32_t word, std::uint32_t shift, std::uint32_t width, std::uint32_t value) {
    const std::uint32_t mask = ((1u << width) - 1u) << shift;
    return (word & ~mask) | ((value << shift) & mask);
}

bool hasLiteral(std::uint32_t source) {
    return source == LiteralSource;
}

struct Encoded {
    std::array<std::uint32_t, 4> words{};
    std::uint32_t rdnaWords = 0;
    std::uint32_t gcnWords = 0;
};

std::uint32_t literalAt(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    if (index >= code.size()) throw std::out_of_range("GCN literal constant at program counter " + std::to_string(programCounter) + " is past the end of the code");
    return code[index];
}

Encoded encodeScalar(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    const auto word = code[index];
    Encoded out;
    out.gcnWords = 1;
    std::uint32_t opcode = 0;
    std::optional<std::uint32_t> mapped;
    bool literal = false;
    const auto prefix9 = word >> 23u;
    if (prefix9 == 0x17du) {
        opcode = (word >> 8u) & 0xffu;
        mapped = mapOpcode(GcnTables::Sop1, opcode);
        if (!mapped) unsupported("SOP1", opcode, programCounter);
        out.words[0] = replaceBits(word, 8, 8, *mapped);
        literal = hasLiteral(word & 0xffu);
    } else if (prefix9 == 0x17eu) {
        opcode = (word >> 16u) & 0x7fu;
        mapped = mapOpcode(GcnTables::Sopc, opcode);
        if (!mapped) unsupported("SOPC", opcode, programCounter);
        out.words[0] = replaceBits(word, 16, 7, *mapped);
        literal = hasLiteral(word & 0xffu) || hasLiteral((word >> 8u) & 0xffu);
    } else if (prefix9 == 0x17fu) {
        opcode = (word >> 16u) & 0x7fu;
        mapped = opcode == CodeEndSopp ? std::optional<std::uint32_t>(CodeEndSopp) : mapOpcode(GcnTables::Sopp, opcode);
        if (!mapped) unsupported("SOPP", opcode, programCounter);
        out.words[0] = replaceBits(word, 16, 7, *mapped);
    } else if ((word >> 28u) == 0xbu) {
        opcode = (word >> 23u) & 0x1fu;
        mapped = opcode == LinkedCallSopk ? std::optional<std::uint32_t>(LinkedCallSopk) : mapOpcode(GcnTables::Sopk, opcode);
        if (!mapped) unsupported("SOPK", opcode, programCounter);
        out.words[0] = replaceBits(word, 23, 5, *mapped);
        literal = opcode == SetRegImmSopk;
    } else {
        opcode = (word >> 23u) & 0x7fu;
        mapped = mapOpcode(GcnTables::Sop2, opcode);
        if (!mapped) unsupported("SOP2", opcode, programCounter);
        out.words[0] = replaceBits(word, 23, 7, *mapped);
        literal = hasLiteral(word & 0xffu) || hasLiteral((word >> 8u) & 0xffu);
    }
    out.rdnaWords = 1;
    if (literal) {
        out.words[1] = literalAt(code, index + 1, programCounter);
        out.rdnaWords = out.gcnWords = 2;
    }
    return out;
}

Encoded encodeSmrd(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    const auto word = code[index];
    const auto opcode = (word >> 22u) & 0x1fu;
    const auto mapped = mapOpcode(GcnTables::Smrd, opcode);
    if (!mapped) unsupported("SMRD", opcode, programCounter);
    const auto sdst = (word >> 15u) & 0x7fu;
    const auto sbase = (word >> 9u) & 0x3fu;
    const bool immediate = ((word >> 8u) & 1u) != 0;
    const auto offset = word & 0xffu;
    if (!immediate && offset == LiteralSource) unsupported("SMRD literal-offset", opcode, programCounter);
    Encoded out;
    out.gcnWords = 1;
    out.rdnaWords = 2;
    out.words[0] = (0x3du << 26u) | (*mapped << 18u) | (sdst << 6u) | sbase;
    out.words[1] = immediate ? (NullSoffset << 25u) | (offset * 4u) : (offset << 25u);
    return out;
}

Encoded encodeVop3Words(std::uint32_t word0, std::uint32_t word1, std::uint32_t rdnaOpcode, bool carryOut) {
    Encoded out;
    out.gcnWords = 2;
    out.rdnaWords = 2;
    const auto vdst = word0 & 0xffu;
    std::uint32_t encoded = (0x35u << 26u) | (rdnaOpcode << 16u) | vdst;
    if (carryOut) {
        encoded |= ((word0 >> 8u) & 0x7fu) << 8u;
    } else {
        encoded |= ((word0 >> 8u) & 0x7u) << 8u;
        encoded |= ((word0 >> 11u) & 1u) << 15u;
    }
    out.words[0] = encoded;
    out.words[1] = word1;
    return out;
}

std::optional<std::uint32_t> reversedShift(std::uint32_t gcnVop3Opcode) {
    switch (gcnVop3Opcode) {
        case 0x115u: return 0x116u;
        case 0x117u: return 0x118u;
        case 0x119u: return 0x11au;
        case 0x161u: return 0x2ffu;
        case 0x162u: return 0x300u;
        case 0x163u: return 0x301u;
        default: return std::nullopt;
    }
}

bool compareWritesExec(std::uint32_t gcnVop3Opcode) {
    return gcnVop3Opcode < 0x100u && (gcnVop3Opcode & 0x10u) != 0;
}

std::uint32_t swapSources(std::uint32_t word1) {
    const auto src0 = word1 & 0x1ffu;
    const auto src1 = (word1 >> 9u) & 0x1ffu;
    const auto neg0 = (word1 >> 29u) & 1u;
    const auto neg1 = (word1 >> 30u) & 1u;
    word1 = (word1 & ~0x3ffffu & ~(3u << 29u)) | src1 | (src0 << 9u) | (neg1 << 29u) | (neg0 << 30u);
    return word1;
}

std::uint32_t swapAbsolute(std::uint32_t word0) {
    const auto abs0 = (word0 >> 8u) & 1u;
    const auto abs1 = (word0 >> 9u) & 1u;
    return (word0 & ~(3u << 8u)) | (abs1 << 8u) | (abs0 << 9u);
}

bool gcnVop3bCarryOut(std::uint32_t opcode) {
    switch (opcode) {
        case 0x125u: case 0x126u: case 0x127u: case 0x128u: case 0x129u: case 0x12au: case 0x16du: case 0x16eu: case 0x176u: case 0x177u: return true;
        default: return false;
    }
}

Encoded encodeVop3(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    if (index + 1 >= code.size()) throw std::out_of_range("truncated GCN VOP3 instruction");
    const auto word0 = code[index];
    const auto opcode = (word0 >> 17u) & 0x1ffu;
    if (compareWritesExec(opcode)) unsupported("VOP3 v_cmpx (writes both the scalar destination and EXEC)", opcode, programCounter);
    if (const auto shift = reversedShift(opcode)) return encodeVop3Words(swapAbsolute(word0), swapSources(code[index + 1]), *shift, false);
    const auto mapped = mapOpcode(GcnTables::Vop3, opcode);
    if (!mapped) unsupported("VOP3", opcode, programCounter);
    return encodeVop3Words(word0, code[index + 1], *mapped, gcnVop3bCarryOut(opcode));
}

Encoded promoteToVop3(std::uint32_t gcnVop3Opcode, std::uint32_t vdst, std::uint32_t src0, std::uint32_t src1, std::uint32_t src2, bool carryOut, std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    const auto shift = reversedShift(gcnVop3Opcode);
    const auto mapped = shift ? shift : mapOpcode(GcnTables::Vop3, gcnVop3Opcode);
    if (!mapped) unsupported("VOP3-promoted", gcnVop3Opcode, programCounter);
    const std::uint32_t word0 = carryOut ? (vdst | (VccSdst << 8u)) : vdst;
    const std::uint32_t word1 = shift ? (src1 | (src0 << 9u) | (src2 << 18u)) : (src0 | (src1 << 9u) | (src2 << 18u));
    auto out = encodeVop3Words(word0, word1, *mapped, carryOut);
    out.gcnWords = 1;
    if (hasLiteral(src0)) {
        out.words[2] = literalAt(code, index + 1, programCounter);
        out.rdnaWords = 3;
        out.gcnWords = 2;
    }
    return out;
}

Encoded encodeVop2(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    const auto word = code[index];
    const auto opcode = (word >> 25u) & 0x3fu;
    const auto src0 = word & 0x1ffu;
    if (const auto mapped = mapOpcode(GcnTables::Vop2, opcode)) {
        Encoded out;
        out.words[0] = replaceBits(word, 25, 6, *mapped);
        out.rdnaWords = out.gcnWords = 1;
        const bool constantK = opcode == 0x20u || opcode == 0x21u;
        if (hasLiteral(src0) || constantK) {
            out.words[1] = literalAt(code, index + 1, programCounter);
            out.rdnaWords = out.gcnWords = 2;
        }
        return out;
    }
    const auto vdst = (word >> 17u) & 0xffu;
    const auto vsrc1 = (word >> 9u) & 0xffu;
    const bool laneSelect = opcode == 0x01u || opcode == 0x02u;
    const bool carryOut = opcode == 0x25u || opcode == 0x26u || opcode == 0x27u;
    return promoteToVop3(0x100u + opcode, vdst, src0, laneSelect ? vsrc1 : 0x100u + vsrc1, 0, carryOut, code, index, programCounter);
}

Encoded encodeVop1(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    const auto word = code[index];
    const auto opcode = (word >> 9u) & 0xffu;
    const auto src0 = word & 0x1ffu;
    if (const auto mapped = mapOpcode(GcnTables::Vop1, opcode)) {
        Encoded out;
        out.words[0] = replaceBits(word, 9, 8, *mapped);
        out.rdnaWords = out.gcnWords = 1;
        if (hasLiteral(src0)) {
            out.words[1] = literalAt(code, index + 1, programCounter);
            out.rdnaWords = out.gcnWords = 2;
        }
        return out;
    }
    return promoteToVop3(0x180u + opcode, (word >> 17u) & 0xffu, src0, 0, 0, false, code, index, programCounter);
}

Encoded encodeVopc(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    const auto word = code[index];
    const auto opcode = (word >> 17u) & 0xffu;
    if (compareWritesExec(opcode)) unsupported("VOPC v_cmpx (writes both VCC and EXEC)", opcode, programCounter);
    const auto mapped = mapOpcode(GcnTables::Vopc, opcode);
    if (!mapped) unsupported("VOPC", opcode, programCounter);
    Encoded out;
    out.words[0] = replaceBits(word, 17, 8, *mapped);
    out.rdnaWords = out.gcnWords = 1;
    if (hasLiteral(word & 0x1ffu)) {
        out.words[1] = literalAt(code, index + 1, programCounter);
        out.rdnaWords = out.gcnWords = 2;
    }
    return out;
}

Encoded encodeDs(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    if (index + 1 >= code.size()) throw std::out_of_range("truncated GCN DS instruction");
    const auto word0 = code[index];
    const auto opcode = (word0 >> 18u) & 0xffu;
    const auto mapped = mapOpcode(GcnTables::Ds, opcode);
    if (!mapped) unsupported("DS", opcode, programCounter);
    Encoded out;
    out.words[0] = replaceBits(word0, 18, 8, *mapped);
    out.words[1] = code[index + 1];
    out.rdnaWords = out.gcnWords = 2;
    return out;
}

Encoded encodeMubuf(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    if (index + 1 >= code.size()) throw std::out_of_range("truncated GCN MUBUF instruction");
    const auto word0 = code[index];
    const auto opcode = (word0 >> 18u) & 0x7fu;
    const auto mapped = mapOpcode(GcnTables::Mubuf, opcode);
    if (!mapped) unsupported("MUBUF", opcode, programCounter);
    if (((word0 >> 15u) & 1u) != 0) unsupported("MUBUF addr64", opcode, programCounter);
    Encoded out;
    out.words[0] = replaceBits(replaceBits(word0, 18, 7, *mapped & 0x7fu), 25, 1, *mapped >> 7u);
    out.words[1] = code[index + 1];
    out.rdnaWords = out.gcnWords = 2;
    return out;
}

Encoded encodeMimg(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    if (index + 1 >= code.size()) throw std::out_of_range("truncated GCN MIMG instruction");
    const auto word0 = code[index];
    const auto opcode = (word0 >> 18u) & 0x7fu;
    const auto mapped = mapOpcode(GcnTables::Mimg, opcode);
    if (!mapped) unsupported("MIMG", opcode, programCounter);
    const bool array = ((word0 >> 14u) & 1u) != 0;
    Encoded out;
    out.words[0] = replaceBits(replaceBits(replaceBits(word0, 18, 7, *mapped & 0x7fu), 14, 1, 0), 0, 8, (*mapped >> 7u) | ((array ? 5u : 1u) << 3u));
    out.words[1] = code[index + 1] & 0x03ffffffu;
    out.rdnaWords = out.gcnWords = 2;
    return out;
}

Encoded encodePassThrough(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t words, const char* family) {
    if (index + words > code.size()) throw std::out_of_range(std::string("truncated GCN ") + family + " instruction");
    Encoded out;
    for (std::uint32_t i = 0; i < words; ++i) out.words[i] = code[index + i];
    out.rdnaWords = out.gcnWords = words;
    return out;
}

Encoded encodeGcn(std::span<const std::uint32_t> code, std::uint32_t index, std::uint32_t programCounter) {
    const auto word = code[index];
    if ((word >> 31u) == 0) {
        switch ((word >> 25u) & 0x3fu) {
            case 0x3eu: return encodeVopc(code, index, programCounter);
            case 0x3fu: return encodeVop1(code, index, programCounter);
            default: return encodeVop2(code, index, programCounter);
        }
    }
    if ((word >> 30u) == 2u) return encodeScalar(code, index, programCounter);
    switch (word >> 26u) {
        case 0x30u: case 0x31u: return encodeSmrd(code, index, programCounter);
        case 0x32u: return encodePassThrough(code, index, 1, "VINTRP");
        case 0x34u: return encodeVop3(code, index, programCounter);
        case 0x36u: return encodeDs(code, index, programCounter);
        case 0x38u: return encodeMubuf(code, index, programCounter);
        case 0x3eu: return encodePassThrough(code, index, 2, "EXP");
        case 0x37u: unsupported("FLAT", (word >> 18u) & 0x7fu, programCounter);
        case 0x3au: unsupported("MTBUF", (word >> 16u) & 7u, programCounter);
        case 0x3cu: return encodeMimg(code, index, programCounter);
        default: break;
    }
    throw std::invalid_argument("unknown GCN instruction family at program counter " + std::to_string(programCounter) + " raw word " + std::to_string(word));
}

}

void SetGcnGuestShaders(bool enabled) {
    g_gcnGuestShaders.store(enabled, std::memory_order_release);
}

bool GcnGuestShaders() {
    return g_gcnGuestShaders.load(std::memory_order_acquire);
}

RdnaInstruction DecodeGcnInstruction(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    if (wordIndex >= code.size()) throw std::out_of_range("word index is out of the code span bounds");
    const auto encoded = encodeGcn(code, wordIndex, programCounter);
    auto instruction = DecodeRdnaEncodedInstruction(programCounter, std::span<const std::uint32_t>(encoded.words.data(), encoded.rdnaWords), 0);
    instruction.wordCount = encoded.gcnWords;
    return instruction;
}

}
