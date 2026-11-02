#include <elfpatcher/windows/WindowsGuestGs.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <bit>
#include <iterator>

namespace Elfpatcher::Windows {

bool WindowsGuestGs::TryAppend(PeSection& section, const std::uint32_t rva, const std::uint8_t* bytes, const Codegen::DecodedInstructionInfo& info) {
    const auto encodedOpcode = bytes[info.OpcodeOffset];
    if (encodedOpcode == 0xc4 || encodedOpcode == 0xc5) {
        const auto prefixSize = encodedOpcode == 0xc4 ? 3u : 2u;
        const auto opcodeOffset = info.OpcodeOffset + prefixSize;
        if (opcodeOffset + 1 >= info.Length || !info.HasModRm || info.ModRmOffset != opcodeOffset + 1 || (info.ModRmByte >> 6) == 3) return false;
        const auto fields = bytes[opcodeOffset - 1];
        const auto map = encodedOpcode == 0xc4 ? bytes[info.OpcodeOffset + 1] & 31 : 1;
        const auto opcode = bytes[opcodeOffset];
        const auto pp = fields & 3;
        const bool wide = (fields & 4) != 0;
        const bool packed = ((opcode == 0x10 || opcode == 0x11 || opcode == 0x28 || opcode == 0x29) && pp <= 1) ||
            ((opcode == 0x6f || opcode == 0x7f) && (pp == 1 || pp == 2));
        const bool scalar = !wide && (((opcode == 0x10 || opcode == 0x11) && pp >= 2) ||
            ((opcode == 0x6e || opcode == 0x7e || opcode == 0xd6) && pp == 1) || (opcode == 0x7e && pp == 2));
        if (map != 1 || (fields & 0x78) != 0x78 || (!packed && !scalar)) return false;
        std::vector<std::uint8_t> legacy;
        for (std::size_t index = 0; index < info.OpcodeOffset; ++index) {
            if (bytes[index] != 0x65 && bytes[index] != 0x67) return false;
            legacy.push_back(bytes[index]);
        }
        if (pp != 0) legacy.push_back(pp == 1 ? 0x66 : pp == 2 ? 0xf3 : 0xf2);
        const auto address = bytes[info.OpcodeOffset + 1];
        const auto rex = static_cast<std::uint8_t>(0x40 | ((~address & 0x80) >> 5) |
            (encodedOpcode == 0xc4 ? ((~address & 0x60) >> 5) | ((fields & 0x80) >> 4) : 0));
        if (rex != 0x40) legacy.push_back(rex);
        legacy.push_back(0x0f);
        legacy.insert(legacy.end(), bytes + opcodeOffset, bytes + info.Length);
        if (legacy.size() > 15) return false;
        const auto decoded = Codegen::X64InstructionDecoder().DecodeInstruction(legacy.data(), legacy.size());
        if (decoded.Length != legacy.size() || !TryAppend(section, rva, legacy.data(), decoded)) return false;
        const auto record = section.Data.size() - 64;
        std::fill(section.Data.begin() + record + 31, section.Data.end(), 0);
        auto output = record + 31;
        for (std::size_t index = info.OpcodeOffset; index < opcodeOffset; ++index)
            section.Data[output++] = bytes[index] | (encodedOpcode == 0xc4 && index == info.OpcodeOffset + 1 ? 0x60 : 0);
        section.Data[output++] = opcode;
        section.Data[output++] = static_cast<std::uint8_t>((info.ModRmByte & 0x38) | section.Data[record + 12]);
        section.Data[record + 13] = static_cast<std::uint8_t>(output - record - 31);
        section.Data[output++] = 0x0f;
        section.Data[output] = 0x0b;
        Io::WriteU32(section.Data, record + 4, CheckedRva(static_cast<std::uint64_t>(rva) + info.Length));
        return true;
    }
    if (encodedOpcode == 0x62 ||
        (encodedOpcode == 0x8f && info.OpcodeOffset + 1 < info.Length && (bytes[info.OpcodeOffset + 1] & 0x1f) >= 8)) return false;
    const auto opcode = info.Opcode;
    const auto group = info.ModRmRegField;
    std::uint8_t mandatory = 0;
    bool multipleMandatory = false;
    for (std::size_t index = 0; index < info.OpcodeOffset; ++index) {
        const auto prefix = bytes[index];
        if (prefix != 0x66 && prefix != 0xf2 && prefix != 0xf3) continue;
        multipleMandatory = multipleMandatory || mandatory != 0;
        mandatory = prefix;
    }
    const bool vectorMove = info.IsTwoByteOpcode && !multipleMandatory &&
        ((opcode == 0x10 || opcode == 0x11) ||
         ((opcode == 0x28 || opcode == 0x29 || opcode == 0x12 || opcode == 0x13 || opcode == 0x16 || opcode == 0x17) && (mandatory == 0 || mandatory == 0x66)) ||
         ((opcode == 0x6f || opcode == 0x7f) && (mandatory == 0x66 || mandatory == 0xf3)) ||
         ((opcode == 0x6e || opcode == 0x7e || opcode == 0xd6) && mandatory == 0x66) ||
         (opcode == 0x7e && mandatory == 0xf3));
    const bool moffs = !info.IsTwoByteOpcode && opcode >= 0xa0 && opcode <= 0xa3;
    const bool scalar = !info.IsTwoByteOpcode && ((opcode <= 0x3b && (opcode & 7) <= 3) ||
        (opcode >= 0x80 && opcode <= 0x8b && opcode != 0x82) || opcode == 0xc0 || opcode == 0xc1 ||
        opcode == 0xc6 || opcode == 0xc7 || (opcode >= 0xd0 && opcode <= 0xd3) || opcode == 0xf6 || opcode == 0xf7 ||
        ((opcode == 0xfe || opcode == 0xff) && group <= 1));
    const bool extended = info.IsTwoByteOpcode && ((opcode >= 0x40 && opcode <= 0x4f) ||
        (opcode >= 0x90 && opcode <= 0x9f) || opcode == 0xaf || opcode == 0xb0 || opcode == 0xb1 ||
        opcode == 0xb6 || opcode == 0xb7 || opcode == 0xbe || opcode == 0xbf || opcode == 0xc0 || opcode == 0xc1);
    if ((!moffs && (!info.HasModRm || (info.ModRmByte >> 6) == 3 || (!scalar && !extended && !vectorMove))) || info.Length > 15)
        return false;
    std::vector<std::uint8_t> replay;
    for (std::size_t index = 0; index < info.OpcodeOffset; ++index) {
        const auto prefix = bytes[index];
        if (prefix == 0x65 || prefix == 0x67) continue;
        if (prefix == 0x66 || (prefix == 0xf0 && !vectorMove) || (vectorMove && (prefix == 0xf2 || prefix == 0xf3))) replay.push_back(prefix);
        else if (prefix >= 0x40 && prefix <= 0x4f && index + 1 == info.OpcodeOffset)
            replay.push_back(static_cast<std::uint8_t>(prefix & ~(moffs ? 7 : 3)));
        else return false;
    }
    const bool explicitRegister = !moffs && !vectorMove && (extended || (opcode <= 0x3b && (opcode & 7) <= 3) || (opcode >= 0x84 && opcode <= 0x8b));
    const bool byteRegister = info.IsTwoByteOpcode ? opcode == 0xb0 || opcode == 0xc0 :
        (opcode <= 0x3b && (opcode & 1) == 0) || opcode == 0x84 || opcode == 0x86 || opcode == 0x88 || opcode == 0x8a;
    if (explicitRegister && group == 4 && (info.RexPrefix & 4) == 0 && (!byteRegister || info.RexPrefix != 0) && !(info.IsTwoByteOpcode && opcode >= 0x90 && opcode <= 0x9f)) return false;
    const auto reg = moffs || vectorMove ? 0 : static_cast<unsigned>(group | ((info.RexPrefix & 4) << 1));
    const std::uint8_t scratch = reg == 3 || (!vectorMove && info.RexPrefix == 0 && group == 7) ? 6 : 3;
    std::uint8_t base = 0xff;
    std::uint8_t index = 0xff;
    std::uint8_t scale = 0;
    std::uint64_t displacement = 0;
    if (moffs) {
        const auto addressBytes = info.AddressSize32 ? 4u : 8u;
        if (info.Length - info.OpcodeOffset != 1 + addressBytes) return false;
        for (unsigned byte = 0; byte < addressBytes; ++byte)
            displacement |= static_cast<std::uint64_t>(bytes[info.OpcodeOffset + 1 + byte]) << (byte * 8);
        replay.push_back(static_cast<std::uint8_t>(0x88 + ((opcode & 2) == 0 ? 2 : 0) + (opcode & 1)));
        replay.push_back(scratch);
    } else {
        const auto mod = info.ModRmByte >> 6;
        const auto rm = info.ModRmByte & 7;
        auto position = info.ModRmOffset + 1;
        base = static_cast<std::uint8_t>(rm | ((info.RexPrefix & 1) << 3));
        bool disp32 = mod == 2;
        if (rm == 4) {
            const auto sib = bytes[position++];
            base = static_cast<std::uint8_t>((sib & 7) | ((info.RexPrefix & 1) << 3));
            index = static_cast<std::uint8_t>(((sib >> 3) & 7) | ((info.RexPrefix & 2) << 2));
            if (index == 4) index = 0xff;
            scale = sib >> 6;
            if (mod == 0 && (sib & 7) == 5) { base = 0xff; disp32 = true; }
        } else if (mod == 0 && rm == 5) {
            base = 0xfe;
            disp32 = true;
        }
        if (mod == 1) displacement = static_cast<std::uint64_t>(static_cast<std::int64_t>(std::bit_cast<std::int8_t>(bytes[position++])));
        if (disp32) {
            if (position + 4 > info.Length) return false;
            std::uint32_t value = 0;
            for (unsigned byte = 0; byte < 4; ++byte) value |= static_cast<std::uint32_t>(bytes[position++]) << (byte * 8);
            displacement = static_cast<std::uint64_t>(static_cast<std::int64_t>(std::bit_cast<std::int32_t>(value)));
        }
        replay.insert(replay.end(), bytes + info.OpcodeOffset, bytes + info.ModRmOffset);
        replay.push_back(static_cast<std::uint8_t>((info.ModRmByte & 0x38) | scratch));
        replay.insert(replay.end(), bytes + position, bytes + info.Length);
    }
    if (replay.empty() || replay.size() > 15) return false;
    if (section.Data.empty()) {
        section.Data = {'A', 'P', 'S', '5', 'G', 'G', 'S', '1'};
        section.Data.resize(16);
        Io::WriteU32(section.Data, 12, 64);
    }
    const auto offset = section.Data.size();
    section.Data.resize(offset + 64);
    Io::WriteU32(section.Data, 8, CheckedRva((section.Data.size() - 16) / 64));
    Io::WriteU32(section.Data, offset, rva);
    Io::WriteU32(section.Data, offset + 4, CheckedRva(static_cast<std::uint64_t>(rva) + info.Length));
    section.Data[offset + 8] = base;
    section.Data[offset + 9] = index;
    section.Data[offset + 10] = scale;
    section.Data[offset + 11] = info.AddressSize32;
    section.Data[offset + 12] = scratch;
    section.Data[offset + 13] = static_cast<std::uint8_t>(replay.size());
    Io::WriteU64(section.Data, offset + 16, displacement);
    const std::uint8_t enter[] = {0x48, 0x8d, 0x64, 0x24, 0x80, 0x0f, 0x0b};
    std::copy(std::begin(enter), std::end(enter), section.Data.begin() + offset + 24);
    std::copy(replay.begin(), replay.end(), section.Data.begin() + offset + 31);
    section.Data[offset + 31 + replay.size()] = 0x0f;
    section.Data[offset + 32 + replay.size()] = 0x0b;
    return true;
}

}
