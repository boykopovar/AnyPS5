#include <codegen/x86/RedZoneFramer.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>

namespace Codegen {

namespace {

constexpr std::uint8_t kRegRsp = 4;
constexpr std::uint8_t kRegRbp = 5;
constexpr std::size_t kJumpBytes = 5;
constexpr std::int64_t kRedZoneBytes = 0x80;
constexpr std::int64_t kStackAlignment = 16;
constexpr std::uint8_t kJumpBack[] = {0xe9, 0x00, 0x00, 0x00, 0x00};

std::vector<std::uint8_t> lowerRsp(const std::int64_t shift) {
    if (shift <= -static_cast<std::int64_t>(std::numeric_limits<std::int8_t>::min())) return {0x48, 0x8d, 0x64, 0x24, static_cast<std::uint8_t>(-shift)};
    const auto value = static_cast<std::uint32_t>(static_cast<std::int32_t>(-shift));
    return {0x48, 0x8d, 0xa4, 0x24, static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24)};
}

std::vector<std::uint8_t> raiseRsp(const std::int64_t shift) {
    const auto value = static_cast<std::uint32_t>(shift);
    return {0x48, 0x8d, 0xa4, 0x24, static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24)};
}

struct Insn {
    std::size_t Offset;
    std::size_t Length;
    DecodedInstructionInfo Info;
};

std::optional<std::uint8_t> pushedRegister(const std::uint8_t* code, const Insn& insn) {
    if (insn.Length == 1 && code[0] >= 0x50 && code[0] <= 0x57 && code[0] != 0x50 + kRegRsp) return static_cast<std::uint8_t>(code[0] - 0x50);
    if (insn.Length == 2 && code[0] == 0x41 && code[1] >= 0x50 && code[1] <= 0x57) return static_cast<std::uint8_t>(code[1] - 0x50 + 8);
    return std::nullopt;
}

std::optional<std::uint8_t> poppedRegister(const std::uint8_t* code, const Insn& insn) {
    if (insn.Length == 1 && code[0] >= 0x58 && code[0] <= 0x5f && code[0] != 0x58 + kRegRsp) return static_cast<std::uint8_t>(code[0] - 0x58);
    if (insn.Length == 2 && code[0] == 0x41 && code[1] >= 0x58 && code[1] <= 0x5f) return static_cast<std::uint8_t>(code[1] - 0x58 + 8);
    return std::nullopt;
}

bool pushesOrPops(const std::uint8_t* code, const Insn& insn) {
    if (pushedRegister(code, insn) || poppedRegister(code, insn)) return true;
    const auto& info = insn.Info;
    const auto lead = code[info.OpcodeOffset];
    if (info.RexPrefix == 0 && (lead == 0xc4 || lead == 0xc5 || lead == 0x62 || lead == 0x8f) && info.Opcode != 0x8f) return false;
    if (info.IsTwoByteOpcode) return info.Opcode == 0xa0 || info.Opcode == 0xa1 || info.Opcode == 0xa8 || info.Opcode == 0xa9;
    const auto reg = static_cast<std::uint8_t>((info.ModRmByte >> 3) & 7);
    return (info.Opcode >= 0x50 && info.Opcode <= 0x5f) || info.Opcode == 0x68 || info.Opcode == 0x6a || info.Opcode == 0x9c || info.Opcode == 0x9d || (info.Opcode == 0x8f && info.HasModRm && reg == 0) || (info.Opcode == 0xff && info.HasModRm && reg == 6);
}

bool pushOpcode(const Insn& insn) {
    const auto& info = insn.Info;
    const auto reg = static_cast<std::uint8_t>((info.ModRmByte >> 3) & 7);
    if (info.IsTwoByteOpcode) return info.Opcode == 0xa0 || info.Opcode == 0xa8;
    return (info.Opcode >= 0x50 && info.Opcode <= 0x57) || info.Opcode == 0x68 || info.Opcode == 0x6a || info.Opcode == 0x9c || (info.Opcode == 0xff && info.HasModRm && reg == 6);
}

std::optional<std::int64_t> rspAdjustment(const std::uint8_t* code, const Insn& insn) {
    std::int32_t value = 0;
    if (insn.Length == 4 && code[0] == 0x48 && code[1] == 0x83 && (code[2] == 0xec || code[2] == 0xc4)) {
        const std::int64_t amount = static_cast<std::int8_t>(code[3]);
        return code[2] == 0xec ? -amount : amount;
    }
    if (insn.Length == 7 && code[0] == 0x48 && code[1] == 0x81 && (code[2] == 0xec || code[2] == 0xc4)) {
        std::memcpy(&value, code + 3, 4);
        return code[2] == 0xec ? -std::int64_t{value} : std::int64_t{value};
    }
    if (insn.Length == 5 && std::memcmp(code, "\x48\x8d\x64\x24", 4) == 0) return static_cast<std::int8_t>(code[4]);
    if (insn.Length == 8 && std::memcmp(code, "\x48\x8d\xa4\x24", 4) == 0) {
        std::memcpy(&value, code + 4, 4);
        return value;
    }
    return std::nullopt;
}

struct MemoryUse {
    bool UsesRsp = false;
    bool RegisterRsp = false;
    bool RbpBased = false;
    std::int64_t RbpDisp = 0;
    bool RbpIndexed = false;
    bool RspBased = false;
    std::int64_t RspDisp = 0;
    std::uint8_t Mod = 0;
    std::size_t DispOffset = 0;
    bool Indexed = false;
    bool Address = false;
};

bool xopPrefixed(const std::uint8_t* code, const Insn& insn) {
    const auto at = insn.Info.OpcodeOffset;
    return code[at] == 0x8f && at + 1 < insn.Length && (code[at + 1] & 0x1f) >= 8;
}

void extensionBits(const std::uint8_t* code, const Insn& insn, bool& r, bool& x, bool& b) {
    const auto& info = insn.Info;
    r = x = b = false;
    if (info.RexPrefix != 0) {
        r = (info.RexPrefix & 4) != 0;
        x = (info.RexPrefix & 2) != 0;
        b = (info.RexPrefix & 1) != 0;
        return;
    }
    const auto lead = code[info.OpcodeOffset];
    if ((lead == 0xc4 || lead == 0x62 || xopPrefixed(code, insn)) && info.OpcodeOffset + 1 < insn.Length) {
        r = (code[info.OpcodeOffset + 1] & 0x80) == 0;
        x = (code[info.OpcodeOffset + 1] & 0x40) == 0;
        b = (code[info.OpcodeOffset + 1] & 0x20) == 0;
    } else if (lead == 0xc5 && info.OpcodeOffset + 1 < insn.Length) {
        r = (code[info.OpcodeOffset + 1] & 0x80) == 0;
    }
}

std::uint8_t legacyPp(const std::uint8_t* code, const Insn& insn) {
    std::uint8_t repeat = 0;
    bool operandSize = false;
    for (std::size_t index = 0; index < insn.Info.OpcodeOffset; ++index) {
        if (code[index] == 0x66) operandSize = true;
        else if (code[index] == 0xf3) repeat = 2;
        else if (code[index] == 0xf2) repeat = 3;
    }
    return repeat != 0 ? repeat : (operandSize ? std::uint8_t{1} : std::uint8_t{0});
}

bool twoByteScalarRegisterIsGeneral(const std::uint8_t op) {
    return op == 0x02 || op == 0x03 || (op >= 0x40 && op <= 0x4f) || op == 0xa3 || op == 0xa4 || op == 0xa5 || op == 0xab || op == 0xac || op == 0xad || op == 0xaf || op == 0xb0 || op == 0xb1 || op == 0xb3 || op == 0xb6 || op == 0xb7 || op == 0xb8 || (op >= 0xbb && op <= 0xbf) || op == 0xc0 || op == 0xc1 || op == 0xc3;
}

bool oneByteRegisterIsByte(const std::uint8_t op) {
    return (op <= 0x3f && (op & 7) <= 3 && (op & 1) == 0) || op == 0x84 || op == 0x86 || op == 0x88 || op == 0x8a;
}

bool oneByteRmIsByte(const std::uint8_t op) {
    return oneByteRegisterIsByte(op) || op == 0x80 || op == 0x82 || op == 0xc0 || op == 0xc6 || op == 0xd0 || op == 0xd2 || op == 0xf6 || op == 0xfe;
}

bool twoByteRegisterIsByte(const std::uint8_t op) {
    return op == 0xb0 || op == 0xc0;
}

bool twoByteRmIsByte(const std::uint8_t op) {
    return op == 0xb0 || op == 0xc0 || op == 0xb6 || op == 0xbe;
}

bool map1RegisterIsGeneral(const std::uint8_t op, const std::uint8_t pp, const std::uint8_t mod, const bool vex, const bool evex) {
    if (!vex && !evex && twoByteScalarRegisterIsGeneral(op)) return true;
    if ((op == 0x2c || op == 0x2d) && (pp == 2 || pp == 3)) return true;
    if (op == 0x50 && !evex && mod == 3 && (pp == 0 || pp == 1)) return true;
    return (op == 0xc5 || (op == 0xd7 && !evex)) && mod == 3 && (pp == 1 || (!vex && !evex && pp == 0));
}

bool map1RmIsGeneral(const std::uint8_t op, const std::uint8_t pp, const std::uint8_t reg, const std::uint8_t rex, const bool vex, const bool evex) {
    if (!vex && !evex) {
        if (twoByteScalarRegisterIsGeneral(op) || op == 0xba) return true;
        if ((op == 0x00 && reg <= 1) || (op == 0x01 && reg == 4)) return true;
        if (op >= 0x90 && op <= 0x9f && pp == 0 && rex != 0) return true;
        if (op == 0xae && pp == 2 && reg <= 3) return true;
        if (op == 0xc7 && reg >= 6 && (pp <= 1 || (pp == 2 && reg == 7))) return true;
    }
    if (op == 0x2a && (pp == 2 || pp == 3)) return true;
    return (op == 0x6e || op == 0x7e || op == 0xc4) && (pp == 1 || (!vex && !evex && pp == 0));
}

bool extendedOpcodeUsesRsp(const std::uint8_t* code, const Insn& insn, const std::uint8_t mod, const std::uint8_t reg, const std::uint8_t rm, const bool r, const bool b) {
    const auto& info = insn.Info;
    const auto at = info.OpcodeOffset;
    const auto lead = code[at];
    const bool regRsp = reg == kRegRsp && !r;
    const bool rmRsp = mod == 3 && rm == kRegRsp && !b;
    if (info.RexPrefix == 0 && xopPrefixed(code, insn)) {
        if (at + 3 >= insn.Length) return false;
        const auto map = static_cast<std::uint8_t>(code[at + 1] & 0x1f);
        const auto vvvv = static_cast<std::uint8_t>((~code[at + 2] >> 3) & 0xf);
        const auto op = code[at + 3];
        if (map == 9 && (op == 0x01 || op == 0x02)) return rmRsp || vvvv == kRegRsp;
        if (map == 10 && op == 0x10) return regRsp || rmRsp;
        return false;
    }
    std::uint8_t map = 0;
    std::uint8_t pp = 0;
    std::uint8_t op = 0;
    std::uint8_t vvvv = 0;
    bool isVex = false;
    bool isEvex = false;
    if (info.RexPrefix == 0 && (lead == 0xc4 || lead == 0xc5)) {
        if (at + (lead == 0xc5 ? 2u : 3u) >= insn.Length) return false;
        isVex = true;
        map = lead == 0xc5 ? std::uint8_t{1} : static_cast<std::uint8_t>(code[at + 1] & 0x1f);
        const auto second = code[lead == 0xc5 ? at + 1 : at + 2];
        pp = static_cast<std::uint8_t>(second & 3);
        vvvv = static_cast<std::uint8_t>((~second >> 3) & 0xf);
        op = code[lead == 0xc5 ? at + 2 : at + 3];
    } else if (info.RexPrefix == 0 && lead == 0x62) {
        if (at + 4 >= insn.Length) return false;
        isEvex = true;
        map = static_cast<std::uint8_t>(code[at + 1] & 0x07);
        pp = static_cast<std::uint8_t>(code[at + 2] & 3);
        op = code[at + 4];
    } else if (info.IsTwoByteOpcode) {
        pp = legacyPp(code, insn);
        if (info.Opcode == 0x38 || info.Opcode == 0x3a) {
            if (at + 2 >= insn.Length) return false;
            map = info.Opcode == 0x38 ? std::uint8_t{2} : std::uint8_t{3};
            op = code[at + 2];
        } else {
            map = 1;
            op = info.Opcode;
        }
    } else {
        return false;
    }
    if (map == 1) {
        const bool byteWithoutRex = !isVex && !isEvex && info.RexPrefix == 0;
        return (regRsp && !(byteWithoutRex && twoByteRegisterIsByte(op)) && map1RegisterIsGeneral(op, pp, mod, isVex, isEvex)) || (rmRsp && !(byteWithoutRex && twoByteRmIsByte(op)) && map1RmIsGeneral(op, pp, reg, info.RexPrefix, isVex, isEvex));
    }
    if (map == 2) {
        if (isVex) return (op == 0xf2 || op == 0xf3 || op == 0xf5 || op == 0xf6 || op == 0xf7) && ((op != 0xf3 && regRsp) || rmRsp || vvvv == kRegRsp);
        if (!isEvex) {
            const bool regGen = ((op == 0xf0 || op == 0xf1) && (pp <= 1 || pp == 3)) || (op == 0xf6 && (pp == 1 || pp == 2)) || (op == 0xf8 && pp == 1) || (op == 0xf9 && pp == 0);
            const bool rmGen = (op == 0xf0 && pp == 3 && info.RexPrefix != 0) || (op == 0xf1 && pp == 3) || (op == 0xf6 && (pp == 1 || pp == 2));
            return (regRsp && regGen) || (rmRsp && rmGen);
        }
        return false;
    }
    if (map == 3) {
        if (isVex && op == 0xf0 && pp == 3) return regRsp || rmRsp;
        return rmRsp && pp == 1 && ((op >= 0x14 && op <= 0x17) || op == 0x20 || op == 0x22);
    }
    return false;
}

MemoryUse memoryUse(const std::uint8_t* code, const Insn& insn) {
    MemoryUse use;
    const auto& info = insn.Info;
    if (!info.HasModRm) {
        const auto op = info.Opcode;
        if ((info.RexPrefix & 1) == 0
            && ((!info.IsTwoByteOpcode && (op == 0x54 || op == 0x5c || op == 0x94 || op == 0xbc || (op == 0xb4 && info.RexPrefix != 0)))
                || (info.IsTwoByteOpcode && op == 0xcc)))
            use.UsesRsp = use.RegisterRsp = true;
        return use;
    }
    if (info.ModRmOffset == 0 || info.ModRmOffset >= insn.Length) return use;
    bool r = false;
    bool x = false;
    bool b = false;
    extensionBits(code, insn, r, x, b);
    const auto modrm = info.ModRmByte;
    const auto mod = static_cast<std::uint8_t>(modrm >> 6);
    const auto reg = static_cast<std::uint8_t>((modrm >> 3) & 7);
    const auto rm = static_cast<std::uint8_t>(modrm & 7);
    const bool vex = info.RexPrefix == 0 && (code[info.OpcodeOffset] == 0xc4 || code[info.OpcodeOffset] == 0xc5 || code[info.OpcodeOffset] == 0x62 || xopPrefixed(code, insn));
    const bool rex = info.RexPrefix != 0;
    if (!vex && !info.IsTwoByteOpcode && reg == kRegRsp && !r && (rex || !oneByteRegisterIsByte(info.Opcode))) {
        const auto op = info.Opcode;
        if ((op <= 0x3f && (op & 7) <= 3) || op == 0x63 || op == 0x69 || op == 0x6b || (op >= 0x84 && op <= 0x8b) || op == 0x8d) use.UsesRsp = use.RegisterRsp = true;
    }
    if ((vex || info.IsTwoByteOpcode) && extendedOpcodeUsesRsp(code, insn, mod, reg, rm, r, b)) use.UsesRsp = use.RegisterRsp = true;
    if (mod == 3) {
        const bool generalPurpose = !vex && !info.IsTwoByteOpcode && !(info.Opcode >= 0xd8 && info.Opcode <= 0xdf);
        if (generalPurpose && rm == kRegRsp && !b && (rex || !oneByteRmIsByte(info.Opcode))) use.UsesRsp = use.RegisterRsp = true;
        return use;
    }
    std::size_t at = info.ModRmOffset + 1;
    std::uint8_t base = rm;
    bool baseExtended = b;
    bool indexed = false;
    if (rm == 4) {
        if (at >= insn.Length) return use;
        const auto sib = code[at++];
        base = sib & 7;
        indexed = ((sib >> 3) & 7) != 4 || x;
        if (mod == 0 && base == 5) return use;
    } else if (mod == 0 && rm == 5) {
        return use;
    }
    std::int64_t disp = 0;
    if (mod == 1 && at < insn.Length) disp = static_cast<std::int8_t>(code[at]);
    if (mod == 2 && at + 4 <= insn.Length) {
        std::int32_t value = 0;
        std::memcpy(&value, code + at, 4);
        disp = value;
    }
    if (base == kRegRsp && !baseExtended) {
        use.UsesRsp = true;
        use.RspBased = true;
        use.RspDisp = disp;
        use.Mod = mod;
        use.DispOffset = at;
        use.Indexed = indexed;
        use.Address = !vex && !info.IsTwoByteOpcode && info.Opcode == 0x8d;
    }
    if (base == kRegRbp && !baseExtended && mod != 0) {
        use.RbpBased = true;
        use.RbpDisp = disp;
        use.RbpIndexed = indexed;
    }
    return use;
}

std::optional<std::vector<std::uint8_t>> movedDisplacement(const MemoryUse& use, const std::int64_t shift) {
    const auto moved = use.RspDisp + shift;
    if (use.Mod == 1 && moved <= std::numeric_limits<std::int8_t>::max()) return std::vector<std::uint8_t>{static_cast<std::uint8_t>(moved)};
    if (use.Mod == 2 && moved <= std::numeric_limits<std::int32_t>::max()) {
        const auto value = static_cast<std::uint32_t>(static_cast<std::int32_t>(moved));
        return std::vector<std::uint8_t>{static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24)};
    }
    return std::nullopt;
}

std::optional<std::vector<std::uint8_t>> withDisplacement(const std::uint8_t* code, const Insn& insn, const MemoryUse& use, const std::int64_t disp) {
    if (disp < std::numeric_limits<std::int32_t>::min() || disp > std::numeric_limits<std::int32_t>::max()) return std::nullopt;
    const std::size_t oldBytes = use.Mod == 1 ? 1 : use.Mod == 2 ? 4 : 0;
    if (use.DispOffset + oldBytes > insn.Length) return std::nullopt;
    const bool narrow = disp >= std::numeric_limits<std::int8_t>::min() && disp <= std::numeric_limits<std::int8_t>::max();
    std::vector<std::uint8_t> bytes(code, code + use.DispOffset);
    bytes[insn.Info.ModRmOffset] = static_cast<std::uint8_t>((bytes[insn.Info.ModRmOffset] & 0x3f) | (narrow ? 0x40 : 0x80));
    const auto value = static_cast<std::uint32_t>(static_cast<std::int32_t>(disp));
    for (std::size_t index = 0; index < (narrow ? 1u : 4u); ++index) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * index)));
    bytes.insert(bytes.end(), code + use.DispOffset + oldBytes, code + insn.Length);
    return bytes;
}

struct OperandStub {
    std::size_t First;
    std::size_t Last;
    std::size_t Begin;
    std::size_t End;
    std::vector<std::uint8_t> Body;
    std::size_t Operands = 0;
};

struct ExitSite {
    std::size_t First;
    std::size_t Tail;
    std::size_t Begin;
    std::size_t TailBegin;
    std::size_t End;
    std::vector<std::uint8_t> Lead;
    std::size_t Operands = 0;
};

std::vector<std::uint8_t> leaWithDisplacement(const std::uint8_t destination, const std::uint8_t base, const std::int32_t disp) {
    std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(0x48 | ((destination & 8) != 0 ? 4 : 0) | ((base & 8) != 0 ? 1 : 0)), 0x8d, static_cast<std::uint8_t>(0x80 | ((destination & 7) << 3) | (base & 7))};
    if ((base & 7) == kRegRsp) bytes.push_back(0x24);
    const auto value = static_cast<std::uint32_t>(disp);
    for (std::size_t index = 0; index < 4; ++index) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * index)));
    return bytes;
}

struct RegisterMove {
    std::uint8_t Destination;
    std::uint8_t Source;
};

std::optional<RegisterMove> quadRegisterMove(const std::uint8_t* code, const Insn& insn) {
    const auto& info = insn.Info;
    if (info.IsTwoByteOpcode || !info.HasModRm || (info.RexPrefix & 8) == 0 || (info.ModRmByte >> 6) != 3) return std::nullopt;
    if (info.Opcode != 0x89 && info.Opcode != 0x8b) return std::nullopt;
    if (code[info.OpcodeOffset] != info.Opcode) return std::nullopt;
    const auto reg = static_cast<std::uint8_t>(((info.ModRmByte >> 3) & 7) | ((info.RexPrefix & 4) != 0 ? 8 : 0));
    const auto rm = static_cast<std::uint8_t>((info.ModRmByte & 7) | ((info.RexPrefix & 1) != 0 ? 8 : 0));
    return info.Opcode == 0x89 ? RegisterMove{rm, reg} : RegisterMove{reg, rm};
}

std::optional<TrampolineSite> relocateLeaf(
    const std::vector<std::uint8_t>& seg,
    const Domain::ProgramHeader& ph,
    const std::size_t start,
    const std::vector<Insn>& body,
    const std::set<std::size_t>& targets,
    const std::function<bool(std::size_t, std::size_t)>& overlapsExisting,
    const char*& refusal,
    std::size_t& operands
) {
    refusal = nullptr;
    operands = 0;
    if (body.empty()) {
        refusal = "empty body";
        return std::nullopt;
    }
    const auto end = body.back().Offset + body.back().Length;
    std::map<std::size_t, std::size_t> index;
    for (std::size_t k = 0; k < body.size(); ++k) index.emplace(body[k].Offset, k);
    std::set<std::size_t> internal;
    for (const auto& insn : body) {
        if (!insn.Info.HasBranchTarget || insn.Info.FlowKind == ControlFlowKind::Call || insn.Info.HasRipRelativeDisp) continue;
        const auto target = static_cast<std::int64_t>(insn.Offset + insn.Length) + insn.Info.BranchDisp;
        if (target < static_cast<std::int64_t>(start) || target >= static_cast<std::int64_t>(end)) continue;
        if (!index.contains(static_cast<std::size_t>(target))) {
            refusal = "branch into an instruction";
            return std::nullopt;
        }
        internal.insert(static_cast<std::size_t>(target));
    }
    for (auto it = targets.upper_bound(start); it != targets.end() && *it < end; ++it) {
        if (!internal.contains(*it)) {
            refusal = "branch into the function from outside";
            return std::nullopt;
        }
    }
    if (overlapsExisting(start, end)) {
        refusal = "overlaps another stub";
        return std::nullopt;
    }
    std::size_t entryEnd = start;
    for (const auto& insn : body) {
        if (entryEnd - start >= kJumpBytes) break;
        entryEnd = insn.Offset + insn.Length;
    }
    if (entryEnd - start < kJumpBytes) {
        refusal = "function shorter than a jump";
        return std::nullopt;
    }

    for (const auto& insn : body) {
        const auto& info = insn.Info;
        if ((info.FlowKind == ControlFlowKind::UnconditionalJump || info.FlowKind == ControlFlowKind::ConditionalBranch) && info.HasRipRelativeDisp) {
            refusal = "rip-relative indirect branch";
            return std::nullopt;
        }
    }

    std::int64_t deepest = 0;
    std::set<std::size_t> indirectExits;
    {
        std::vector<std::optional<std::int64_t>> depthAt(body.size());
        std::vector<std::optional<std::int64_t>> frameAt(body.size());
        std::vector<std::int64_t> pushedAt(body.size(), 0);
        std::set<std::int64_t> poppedSlots;
        std::vector<std::pair<std::int64_t, bool>> upperOperands;
        std::vector<std::size_t> work{0};
        depthAt[0] = 0;
        const auto reach = [&](const std::size_t k, const std::int64_t depth, const std::optional<std::int64_t> frame, const std::int64_t pushed) {
            if (!depthAt[k]) {
                depthAt[k] = depth;
                frameAt[k] = frame;
                pushedAt[k] = pushed;
                work.push_back(k);
                return true;
            }
            if (*depthAt[k] != depth || frameAt[k] != frame) return false;
            if (pushedAt[k] != pushed && pushedAt[k] != -1) {
                pushedAt[k] = -1;
                work.push_back(k);
            }
            return true;
        };
        while (!work.empty()) {
            const auto k = work.back();
            work.pop_back();
            const auto& insn = body[k];
            const auto* code = seg.data() + insn.Offset;
            const auto& info = insn.Info;
            auto depth = *depthAt[k];
            auto frame = frameAt[k];
            auto pushed = pushedAt[k];
            const auto use = memoryUse(code, insn);
            if (use.RspBased && (use.Indexed || use.RspDisp >= 0)) upperOperands.emplace_back(use.RspDisp - depth, use.Indexed);
            if (use.RbpBased && frame && (use.RbpIndexed || use.RbpDisp - *frame >= -depth)) upperOperands.emplace_back(use.RbpDisp - *frame, use.RbpIndexed);
            const auto reg = static_cast<std::uint8_t>((info.ModRmByte >> 3) & 7);
            const bool push = pushOpcode(insn);
            const bool pop = !info.IsTwoByteOpcode ? ((info.Opcode >= 0x58 && info.Opcode <= 0x5f) || info.Opcode == 0x9d || (info.Opcode == 0x8f && info.HasModRm && reg == 0)) : (info.Opcode == 0xa1 || info.Opcode == 0xa9);
            const auto move = quadRegisterMove(code, insn);
            if (pushesOrPops(code, insn) && (push || pop)) {
                if (pop && depth < 8) {
                    refusal = "pops a value its caller pushed";
                    return std::nullopt;
                }
                if (pop) poppedSlots.insert(-depth);
                depth += push ? 8 : -8;
                if (pushed != -1) pushed = push ? pushed + 8 : pushed >= 8 ? pushed - 8 : -1;
            } else if (const auto adjustment = rspAdjustment(code, insn)) {
                depth -= *adjustment;
                if (depth < 0) {
                    refusal = "frees stack its caller allocated";
                    return std::nullopt;
                }
                if (pushed > depth) pushed = -1;
            } else if (move && move->Destination == kRegRbp && move->Source == kRegRsp) {
                frame = depth;
            } else if (move && move->Destination == kRegRsp && move->Source == kRegRbp) {
                if (!frame) {
                    refusal = "rsp restored from rbp without a frame";
                    return std::nullopt;
                }
                depth = *frame;
                pushed = depth == 0 ? 0 : -1;
            } else if (!info.IsTwoByteOpcode && info.Opcode == 0xc9 && insn.Length == 1) {
                if (!frame || *frame < 8) {
                    refusal = "leave without a frame";
                    return std::nullopt;
                }
                poppedSlots.insert(-*frame);
                depth = *frame - 8;
                frame.reset();
                pushed = depth == 0 ? 0 : -1;
            }
            deepest = std::max(deepest, depth);
            const auto flow = info.FlowKind;
            if (flow == ControlFlowKind::Return) {
                if (depth != 0) {
                    refusal = "returns at another stack depth";
                    return std::nullopt;
                }
                continue;
            }
            if (flow == ControlFlowKind::IndirectJump) continue;
            std::optional<std::int64_t> target;
            if ((flow == ControlFlowKind::UnconditionalJump || flow == ControlFlowKind::ConditionalBranch) && info.HasBranchTarget) {
                const auto at = static_cast<std::int64_t>(insn.Offset + insn.Length) + info.BranchDisp;
                if (at >= static_cast<std::int64_t>(start) && at < static_cast<std::int64_t>(end)) target = at;
                else if (depth != 0) {
                    refusal = "leaves the function at another stack depth";
                    return std::nullopt;
                }
            }
            if (target && !reach(index.at(static_cast<std::size_t>(*target)), depth, frame, pushed)) {
                refusal = "inconsistent stack depth";
                return std::nullopt;
            }
            if (flow != ControlFlowKind::UnconditionalJump && k + 1 < body.size() && !reach(k + 1, depth, frame, pushed)) {
                refusal = "inconsistent stack depth";
                return std::nullopt;
            }
        }
        for (std::size_t k = 0; k < body.size(); ++k) {
            if (body[k].Info.FlowKind != ControlFlowKind::IndirectJump || !depthAt[k]) continue;
            const auto depth = *depthAt[k];
            const auto pushed = pushedAt[k];
            if (depth != 0 && pushed != 0 && pushed != depth) {
                refusal = "indirect jump at a stack depth of pushes and allocations";
                return std::nullopt;
            }
            indirectExits.insert(k);
        }
        for (const auto& [offset, indexed] : upperOperands) {
            const auto slot = poppedSlots.upper_bound(offset);
            if ((indexed && !poppedSlots.empty()) || (slot != poppedSlots.begin() && offset < *std::prev(slot) + 8)) {
                refusal = "stack operand reaching a pushed register";
                return std::nullopt;
            }
        }
    }

    const std::int64_t shift = (kRedZoneBytes + deepest + kStackAlignment - 1) / kStackAlignment * kStackAlignment;
    std::vector<std::uint8_t> out = lowerRsp(shift);
    std::vector<StubRelocation> relocations;
    struct Fixup {
        std::size_t Field;
        std::size_t End;
        std::size_t Target;
    };
    std::vector<Fixup> fixups;
    std::map<std::size_t, std::size_t> placed;
    const auto raise = raiseRsp(shift);
    const auto shiftField = static_cast<std::uint32_t>(shift);
    const std::vector<std::uint8_t> storePushed{0xff, 0x34, 0x24, 0x8f, 0x84, 0x24, static_cast<std::uint8_t>(shiftField), static_cast<std::uint8_t>(shiftField >> 8), static_cast<std::uint8_t>(shiftField >> 16), static_cast<std::uint8_t>(shiftField >> 24)};
    const auto emitOutsideJump = [&](const std::int64_t target) {
        out.insert(out.end(), raise.begin(), raise.end());
        out.push_back(0xe9);
        const auto field = out.size();
        out.insert(out.end(), 4, 0);
        relocations.push_back({field, out.size(), target - static_cast<std::int64_t>(start), false});
    };
    static const char* traceText = std::getenv("APS5_RELINK_RELOCATE_TRACE");
    const bool trace = traceText != nullptr && std::strtoull(traceText, nullptr, 0) == ph.MappedAddress + start;
    std::size_t traced = 0;
    const auto traceStep = [&](const Insn& insn) {
        if (!trace) return;
        std::fprintf(stderr, "reloc +0x%04zx ->", insn.Offset - start);
        for (std::size_t index = 0; index < insn.Length; ++index) std::fprintf(stderr, " %02x", seg[insn.Offset + index]);
        std::fprintf(stderr, " | out +0x%04zx:", traced);
        for (std::size_t index = traced; index < out.size(); ++index) std::fprintf(stderr, " %02x", out[index]);
        std::fprintf(stderr, "\n");
    };
    for (const auto& insn : body) {
        if (trace && !placed.empty()) traceStep(body[placed.size() - 1]);
        traced = out.size();
        placed.emplace(insn.Offset, out.size());
        const auto* code = seg.data() + insn.Offset;
        const auto& info = insn.Info;
        const auto flow = info.FlowKind;
        const auto lead = code[info.OpcodeOffset];
        const bool pushes = pushesOrPops(code, insn) && pushOpcode(insn);
        if (flow == ControlFlowKind::Call || flow == ControlFlowKind::IndirectCall) {
            refusal = "call in a leaf";
            return std::nullopt;
        }
        if (flow == ControlFlowKind::IndirectJump) {
            if (info.HasRipRelativeDisp) {
                refusal = "rip-relative indirect branch";
                return std::nullopt;
            }
            if (!indirectExits.contains(static_cast<std::size_t>(&insn - body.data()))) {
                out.insert(out.end(), code, code + insn.Length);
                continue;
            }
            out.insert(out.end(), raise.begin(), raise.end());
            out.insert(out.end(), code, code + insn.Length);
            continue;
        }
        if (info.AddressSize32 && info.HasRipRelativeDisp) {
            refusal = "eip-relative operand";
            return std::nullopt;
        }
        if (info.SegmentPrefix != 0) {
            refusal = "segment-prefixed operand, which a later pass rewrites in the original code";
            return std::nullopt;
        }
        const bool assertTrap = !info.IsTwoByteOpcode && info.Opcode == 0xcd && insn.Length == 2 && code[0] == 0xcd && code[1] == 0x41;
        if ((info.IsTwoByteOpcode && (info.Opcode == 0x05 || info.Opcode == 0x34)) || (!info.IsTwoByteOpcode && ((info.Opcode == 0xcd && !assertTrap) || info.Opcode == 0xce || info.Opcode == 0xf1))) {
            refusal = "system call or interrupt";
            return std::nullopt;
        }
        if (!info.IsTwoByteOpcode && (info.Opcode == 0xc8 || (info.Opcode >= 0xe0 && info.Opcode <= 0xe3))) {
            refusal = info.Opcode == 0xc8 ? "enter" : "loop or jrcxz";
            return std::nullopt;
        }
        if (flow == ControlFlowKind::Return) {
            out.insert(out.end(), raise.begin(), raise.end());
            out.insert(out.end(), code, code + insn.Length);
            continue;
        }
        if ((flow == ControlFlowKind::UnconditionalJump || flow == ControlFlowKind::ConditionalBranch) && info.HasBranchTarget) {
            const auto target = static_cast<std::int64_t>(insn.Offset + insn.Length) + info.BranchDisp;
            const bool inside = target >= static_cast<std::int64_t>(start) && target < static_cast<std::int64_t>(end);
            if (flow == ControlFlowKind::UnconditionalJump) {
                if (!inside) {
                    emitOutsideJump(target);
                    continue;
                }
                out.push_back(0xe9);
            } else {
                const auto condition = static_cast<std::uint8_t>(info.Opcode & 0x0f);
                if (!inside) {
                    out.push_back(static_cast<std::uint8_t>(0x70 | (condition ^ 1)));
                    out.push_back(static_cast<std::uint8_t>(raise.size() + 5));
                    emitOutsideJump(target);
                    continue;
                }
                out.push_back(0x0f);
                out.push_back(static_cast<std::uint8_t>(0x80 | condition));
            }
            const auto field = out.size();
            out.insert(out.end(), 4, 0);
            fixups.push_back({field, out.size(), static_cast<std::size_t>(target)});
            continue;
        }
        if (!info.IsTwoByteOpcode && info.Opcode == 0xc9 && lead == 0xc9 && insn.Length == 1) {
            const auto lea = leaWithDisplacement(kRegRsp, kRegRbp, static_cast<std::int32_t>(-shift));
            out.insert(out.end(), lea.begin(), lea.end());
            out.push_back(0x5d);
            continue;
        }
        const auto use = memoryUse(code, insn);
        if (use.RegisterRsp) {
            if (const auto move = quadRegisterMove(code, insn)) {
                if (move->Source == kRegRsp && move->Destination != kRegRsp) {
                    const auto lea = leaWithDisplacement(move->Destination, kRegRsp, static_cast<std::int32_t>(shift));
                    out.insert(out.end(), lea.begin(), lea.end());
                    continue;
                }
                if (move->Destination == kRegRsp && move->Source != kRegRsp) {
                    const auto lea = leaWithDisplacement(kRegRsp, move->Source, static_cast<std::int32_t>(-shift));
                    out.insert(out.end(), lea.begin(), lea.end());
                    continue;
                }
            }
            const bool leaIntoRsp = !info.IsTwoByteOpcode && info.Opcode == 0x8d && use.RspBased && ((info.ModRmByte >> 3) & 7) == kRegRsp && (info.RexPrefix & 4) == 0;
            if (!rspAdjustment(code, insn) && !leaIntoRsp) {
                refusal = "register use of rsp";
                return std::nullopt;
            }
            out.insert(out.end(), code, code + insn.Length);
            continue;
        }
        if (use.RspBased) {
            if (lead == 0x62) {
                refusal = "EVEX rsp operand";
                return std::nullopt;
            }
            const auto bytes = withDisplacement(code, insn, use, use.RspDisp + shift);
            if (!bytes) {
                refusal = "rsp operand that cannot move";
                return std::nullopt;
            }
            out.insert(out.end(), bytes->begin(), bytes->end());
            if (pushes) out.insert(out.end(), storePushed.begin(), storePushed.end());
            ++operands;
            continue;
        }
        if (info.HasRipRelativeDisp) {
            if (info.RipRelativeDispOffset + 4 > insn.Length) {
                refusal = "rip-relative operand outside its instruction";
                return std::nullopt;
            }
            std::int32_t disp = 0;
            std::memcpy(&disp, code + info.RipRelativeDispOffset, 4);
            const auto field = out.size() + info.RipRelativeDispOffset;
            out.insert(out.end(), code, code + insn.Length);
            relocations.push_back({field, out.size(), static_cast<std::int64_t>(insn.Offset + insn.Length) + disp - static_cast<std::int64_t>(start), false});
            if (pushes) out.insert(out.end(), storePushed.begin(), storePushed.end());
            continue;
        }
        out.insert(out.end(), code, code + insn.Length);
        if (pushes) out.insert(out.end(), storePushed.begin(), storePushed.end());
    }
    if (trace) traceStep(body.back());
    const bool endsInUd2 = body.back().Info.IsTwoByteOpcode && body.back().Info.Opcode == 0x0b;
    if (body.back().Info.FlowKind != ControlFlowKind::Return && body.back().Info.FlowKind != ControlFlowKind::UnconditionalJump && body.back().Info.FlowKind != ControlFlowKind::IndirectJump && !endsInUd2) {
        refusal = "falls off its end";
        return std::nullopt;
    }
    for (const auto& fixup : fixups) {
        const auto value = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int64_t>(placed.at(fixup.Target)) - static_cast<std::int64_t>(fixup.End)));
        for (std::size_t byte = 0; byte < 4; ++byte) out[fixup.Field + byte] = static_cast<std::uint8_t>(value >> (8 * byte));
    }
    TrampolineSite site;
    site.Offset = ph.Offset + start;
    site.Address = ph.MappedAddress + start;
    site.Length = entryEnd - start;
    site.OriginalBytes.assign(seg.begin() + static_cast<std::ptrdiff_t>(start), seg.begin() + static_cast<std::ptrdiff_t>(entryEnd));
    site.ReturnBranchOffset = out.size();
    out.insert(out.end(), std::begin(kJumpBack), std::end(kJumpBack));
    site.Body = std::move(out);
    site.Relocations = std::move(relocations);
    return site;
}

void frameSegment(
    const std::vector<std::uint8_t>& fileBytes,
    const Domain::ProgramHeader& ph,
    const std::vector<RedZoneFunction>& functions,
    const std::vector<TrampolineSite>& existing,
    std::vector<TrampolineSite>& sites,
    std::vector<RedZoneDisplacement>& displacements,
    RedZoneFrameReport& report,
    const bool relocateRefused
) {
    const auto segOffset = static_cast<std::size_t>(ph.Offset);
    const auto segSize = static_cast<std::size_t>(ph.FileSize);
    if (segOffset > fileBytes.size() || segSize > fileBytes.size() - segOffset) return;
    const std::vector<std::uint8_t> seg(fileBytes.begin() + static_cast<std::ptrdiff_t>(segOffset), fileBytes.begin() + static_cast<std::ptrdiff_t>(segOffset + segSize));
    const auto scanner = MakeInstructionScanner();
    const X64InstructionDecoder decoder;

    std::set<std::size_t> targets;
    for (const auto& match : scanner->ScanCodeSection(seg, 0, seg.size())) {
        const auto info = decoder.DecodeInstruction(seg.data() + match.Offset, match.Length);
        if (!info.HasBranchTarget || info.HasRipRelativeDisp || info.FlowKind == ControlFlowKind::Call) continue;
        const auto target = static_cast<std::int64_t>(match.Offset + match.Length) + info.BranchDisp;
        if (target >= 0 && static_cast<std::size_t>(target) < seg.size()) targets.insert(static_cast<std::size_t>(target));
    }
    std::vector<std::pair<std::size_t, std::size_t>> taken;
    for (const auto& site : existing) {
        if (site.Address < ph.MappedAddress || site.Address >= ph.MappedAddress + segSize) continue;
        const auto at = static_cast<std::size_t>(site.Address - ph.MappedAddress);
        taken.emplace_back(at, at + site.Length);
    }
    const auto overlapsExisting = [&](const std::size_t begin, const std::size_t end) {
        return std::any_of(taken.begin(), taken.end(), [&](const auto& range) { return begin < range.second && range.first < end; });
    };
    const auto targetInside = [&](const std::size_t begin, const std::size_t end) {
        const auto hit = targets.upper_bound(begin);
        return hit != targets.end() && *hit < end;
    };
    for (const auto& function : functions) {
        if (function.Begin < ph.MappedAddress || function.End <= function.Begin || function.End - ph.MappedAddress > segSize) continue;
        const auto start = static_cast<std::size_t>(function.Begin - ph.MappedAddress);
        const auto limit = static_cast<std::size_t>(function.End - ph.MappedAddress);
        ++report.Functions;
        const auto skip = [&](const char* reason) { ++report.Skipped[reason]; };

        const std::vector<std::uint8_t> code(seg.begin() + static_cast<std::ptrdiff_t>(start), seg.begin() + static_cast<std::ptrdiff_t>(limit));
        std::vector<Insn> body;
        for (const auto& match : scanner->ScanCodeSection(code, 0, code.size())) {
            if (code[match.Offset] == 0xcc && match.Length == 1) break;
            body.push_back({start + static_cast<std::size_t>(match.Offset), match.Length, decoder.DecodeInstruction(code.data() + match.Offset, match.Length)});
        }
        if (body.size() < 3) continue;
        const auto at = [&](const std::size_t k) { return seg.data() + body[k].Offset; };
        const bool rbpPrologue = body[0].Length == 1 && at(0)[0] == 0x55 && body[1].Length == 3 && std::memcmp(at(1), "\x48\x89\xe5", 3) == 0;
        std::vector<std::uint8_t> pushed;
        std::size_t p = rbpPrologue ? 2 : 0;
        while (p < body.size())
            if (const auto reg = pushedRegister(at(p), body[p])) { pushed.push_back(*reg); ++p; } else break;
        std::int64_t allocation = 0;
        if (!rbpPrologue && p < body.size())
            if (const auto adjustment = rspAdjustment(at(p), body[p]); adjustment && *adjustment < 0) { allocation = -*adjustment; ++p; }
        std::vector<std::uint8_t> expected(pushed.begin(), pushed.end());
        if (rbpPrologue) expected.insert(expected.begin(), kRegRbp);
        const auto popsBefore = [&](const std::size_t k) -> std::optional<std::size_t> {
            std::size_t q = k;
            std::vector<std::uint8_t> popped;
            while (q > p && popped.size() < expected.size())
                if (const auto reg = poppedRegister(at(q - 1), body[q - 1])) { popped.push_back(*reg); --q; } else break;
            if (popped != expected) return std::nullopt;
            return q;
        };
        std::set<std::size_t> frees;
        if (allocation > 0)
            for (std::size_t k = p; k < body.size(); ++k)
                if (body[k].Info.FlowKind == ControlFlowKind::Return)
                    if (const auto q = popsBefore(k); q && *q > p && rspAdjustment(at(*q - 1), body[*q - 1]) == allocation) frees.insert(*q - 1);

        bool leaf = true;
        bool redZone = false;
        const char* reason = nullptr;
        std::size_t reasonAt = body.size();
        std::vector<ExitSite> exits;
        std::map<std::size_t, std::pair<std::size_t, std::vector<std::uint8_t>>> moves;
        std::vector<std::size_t> stubbed;
        std::vector<std::size_t> stackMoves;
        const auto functionEnd = limit;
        std::int64_t depth = 0;
        bool wholeRedZone = rbpPrologue;
        for (std::size_t k = p; k < body.size(); ++k) {
            const auto use = memoryUse(at(k), body[k]);
            if (!use.RspBased) continue;
            if (use.RspDisp < 0) depth = std::max(depth, -use.RspDisp);
            if (use.Indexed || (use.Address && use.RspDisp < 0)) wholeRedZone = true;
        }
        const auto shift = wholeRedZone ? kRedZoneBytes : std::min(kRedZoneBytes, (depth + kStackAlignment - 1) / kStackAlignment * kStackAlignment);
        for (std::size_t k = p; k < body.size(); ++k) {
            const auto& insn = body[k];
            const auto* code = at(k);
            const auto flow = insn.Info.FlowKind;
            if (flow == ControlFlowKind::Call || flow == ControlFlowKind::IndirectCall) { leaf = false; break; }
            if (frees.contains(k)) continue;
            const auto use = memoryUse(code, insn);
            if (use.RbpBased && rbpPrologue && use.RbpDisp < -static_cast<std::int64_t>(8 * pushed.size())) redZone = true;
            if (use.RspBased && use.RspDisp < 0) redZone = true;
            if (reason != nullptr) continue;
            if (code[insn.Info.OpcodeOffset] == 0xc9 || code[insn.Info.OpcodeOffset] == 0xc8) { reason = "leave or enter"; continue; }
            if (!rbpPrologue && pushed.empty() && allocation == 0 && pushesOrPops(code, insn)) { reason = "push or pop in a leaf without pushes"; continue; }
            if (!rbpPrologue && pushesOrPops(code, insn)) stackMoves.push_back(k);
            if (rbpPrologue) {
                if (use.UsesRsp) { reason = "other use of rsp"; reasonAt = k; continue; }
            } else {
                if (use.RegisterRsp) { reason = "other use of rsp"; reasonAt = k; continue; }
                if (use.RspBased) {
                    if (code[insn.Info.OpcodeOffset] == 0x62) { reason = "rsp operand that cannot move with rsp"; continue; }
                    if (overlapsExisting(insn.Offset, insn.Offset + insn.Length)) { reason = "overlaps another stub"; continue; }
                    if (auto moved = movedDisplacement(use, shift)) moves.emplace(k, std::pair{insn.Offset + use.DispOffset, std::move(*moved)});
                    else stubbed.push_back(k);
                }
            }
            if (flow == ControlFlowKind::IndirectJump) { reason = "indirect jump"; continue; }
            if ((flow == ControlFlowKind::UnconditionalJump || flow == ControlFlowKind::ConditionalBranch) && insn.Info.HasBranchTarget) {
                const auto target = static_cast<std::int64_t>(insn.Offset + insn.Length) + insn.Info.BranchDisp;
                if (insn.Info.HasRipRelativeDisp || target < static_cast<std::int64_t>(start) || target >= static_cast<std::int64_t>(functionEnd)) { reason = "branch leaves the function"; continue; }
            }
            if (flow == ControlFlowKind::Return) {
                if (insn.Length != 1 || code[0] != 0xc3) { reason = "ret with an immediate"; continue; }
                const auto pops = popsBefore(k);
                if (!pops) { reason = "exit does not pop the prologue's registers"; continue; }
                auto q = *pops;
                if (allocation > 0) {
                    if (q == p || !frees.contains(q - 1)) { reason = "exit does not free the prologue's allocation"; continue; }
                    --q;
                }
                ExitSite exit{q, q, body[q].Offset, body[q].Offset, insn.Offset + 1, {}};
                while (exit.End - exit.Begin < kJumpBytes && exit.First > p) {
                    const auto j = exit.First - 1;
                    const auto& prior = body[j];
                    if (prior.Offset + prior.Length != exit.Begin || prior.Info.FlowKind != ControlFlowKind::Sequential || prior.Info.HasRipRelativeDisp || frees.contains(j) || pushesOrPops(at(j), prior)) break;
                    const auto priorUse = memoryUse(at(j), prior);
                    const auto bytes = priorUse.RspBased ? withDisplacement(at(j), prior, priorUse, priorUse.RspDisp + shift) : std::vector<std::uint8_t>(at(j), at(j) + prior.Length);
                    if (!bytes) break;
                    exit.Lead.insert(exit.Lead.begin(), bytes->begin(), bytes->end());
                    if (priorUse.RspBased) ++exit.Operands;
                    exit.Begin = prior.Offset;
                    exit.First = j;
                }
                if (exit.End - exit.Begin < kJumpBytes) { reason = "exit shorter than a jump"; continue; }
                if (targetInside(exit.Begin, exit.End)) { reason = exit.First == exit.Tail ? "branch into an exit" : "branch into the instructions before a short exit"; continue; }
                exits.push_back(std::move(exit));
            }
        }
        if (!leaf || !redZone) continue;
        ++report.RedZoneFunctions;
        const auto prologueEnd = p > 0 ? body[p - 1].Offset + body[p - 1].Length : start;
        if (reason == nullptr && std::any_of(stackMoves.begin(), stackMoves.end(), [&](const std::size_t k) { return std::none_of(exits.begin(), exits.end(), [&](const ExitSite& exit) { return exit.Begin <= body[k].Offset && body[k].Offset < exit.End; }); })) reason = "push or pop inside the body";
        auto entryEnd = prologueEnd;
        auto entryLast = p;
        std::vector<std::uint8_t> entryTail;
        std::size_t entryOperands = 0;
        while (entryEnd - start < kJumpBytes && entryLast < body.size()) {
            const auto& covered = body[entryLast];
            if (covered.Offset != entryEnd || covered.Info.FlowKind != ControlFlowKind::Sequential || covered.Info.HasRipRelativeDisp || frees.contains(entryLast) || pushesOrPops(at(entryLast), covered)) break;
            if (std::any_of(exits.begin(), exits.end(), [&](const ExitSite& exit) { return covered.Offset < exit.End && exit.Begin < covered.Offset + covered.Length; })) break;
            const auto use = memoryUse(at(entryLast), covered);
            const auto bytes = use.RspBased ? withDisplacement(at(entryLast), covered, use, use.RspDisp + shift) : std::vector<std::uint8_t>(at(entryLast), at(entryLast) + covered.Length);
            if (!bytes) break;
            entryTail.insert(entryTail.end(), bytes->begin(), bytes->end());
            if (use.RspBased) ++entryOperands;
            entryEnd += covered.Length;
            ++entryLast;
        }
        if (reason == nullptr && entryEnd - start < kJumpBytes) reason = "prologue shorter than a jump";
        if (reason == nullptr && targetInside(start, entryEnd)) reason = entryEnd == prologueEnd ? "branch into the prologue" : "branch into the instructions after a short prologue";
        if (reason == nullptr && entryEnd != prologueEnd && targets.contains(start)) reason = "branch to the start of a widened prologue";
        if (reason == nullptr && exits.empty()) reason = "no ret exit";
        if (reason == nullptr && std::any_of(exits.begin(), exits.end(), [&](const ExitSite& exit) { return exit.Begin < entryEnd; })) reason = "prologue shorter than a jump";
        if (reason == nullptr && (overlapsExisting(start, entryEnd) || std::any_of(exits.begin(), exits.end(), [&](const ExitSite& exit) { return overlapsExisting(exit.Begin, exit.End); }))) reason = "overlaps another stub";
        const auto carried = [&](const std::size_t k) {
            moves.erase(k);
            stubbed.erase(std::remove(stubbed.begin(), stubbed.end(), k), stubbed.end());
        };
        for (auto k = p; k < entryLast; ++k) carried(k);
        for (const auto& exit : exits)
            for (auto k = exit.First; k < exit.Tail; ++k) carried(k);
        std::vector<OperandStub> stubs;
        const auto overlapsSite = [&](const std::size_t begin, const std::size_t end) {
            if (begin < entryEnd && start < end) return true;
            if (std::any_of(exits.begin(), exits.end(), [&](const ExitSite& exit) { return begin < exit.End && exit.Begin < end; })) return true;
            return std::any_of(stubs.begin(), stubs.end(), [&](const OperandStub& stub) { return begin < stub.End && stub.Begin < end; });
        };
        for (const auto k : stubbed) {
            if (reason != nullptr) break;
            if (!stubs.empty() && k < stubs.back().Last) continue;
            OperandStub stub{k, k, body[k].Offset, body[k].Offset, {}};
            while (stub.Last < body.size() && stub.End - stub.Begin < kJumpBytes) {
                const auto& covered = body[stub.Last];
                if (covered.Offset != stub.End || covered.Info.FlowKind != ControlFlowKind::Sequential || covered.Info.HasRipRelativeDisp || frees.contains(stub.Last)) break;
                const auto use = memoryUse(at(stub.Last), covered);
                const auto bytes = use.RspBased ? withDisplacement(at(stub.Last), covered, use, use.RspDisp + shift) : std::vector<std::uint8_t>(at(stub.Last), at(stub.Last) + covered.Length);
                if (!bytes) break;
                stub.Body.insert(stub.Body.end(), bytes->begin(), bytes->end());
                if (use.RspBased) ++stub.Operands;
                stub.End += covered.Length;
                ++stub.Last;
            }
            while (stub.End - stub.Begin < kJumpBytes && stub.First > p) {
                const auto j = stub.First - 1;
                const auto& prior = body[j];
                if (prior.Offset + prior.Length != stub.Begin || prior.Info.FlowKind != ControlFlowKind::Sequential || prior.Info.HasRipRelativeDisp || frees.contains(j)) break;
                const auto use = memoryUse(at(j), prior);
                const auto bytes = use.RspBased ? withDisplacement(at(j), prior, use, use.RspDisp + shift) : std::vector<std::uint8_t>(at(j), at(j) + prior.Length);
                if (!bytes) break;
                stub.Body.insert(stub.Body.begin(), bytes->begin(), bytes->end());
                if (use.RspBased) ++stub.Operands;
                stub.Begin = prior.Offset;
                stub.First = j;
            }
            if (stub.End - stub.Begin < kJumpBytes || targetInside(stub.Begin, stub.End) || overlapsSite(stub.Begin, stub.End) || overlapsExisting(stub.Begin, stub.End)) reason = "rsp operand that cannot move and has no room for a stub";
            else stubs.push_back(std::move(stub));
        }
        if (reason != nullptr) {
            static const bool describe = std::getenv("APS5_RELINK_REDZONE_SKIPS") != nullptr;
            if (describe) {
                std::fprintf(stderr, "red-zone skip 0x%llx-0x%llx: %s", static_cast<unsigned long long>(ph.MappedAddress + start), static_cast<unsigned long long>(ph.MappedAddress + limit), reason);
                if (reasonAt < body.size()) {
                    std::fprintf(stderr, " at +0x%llx:", static_cast<unsigned long long>(body[reasonAt].Offset - start));
                    for (std::size_t index = 0; index < body[reasonAt].Length; ++index) std::fprintf(stderr, " %02x", at(reasonAt)[index]);
                }
                std::fprintf(stderr, "\n");
            }
            const char* refusal = nullptr;
            std::size_t operands = 0;
            if (!relocateRefused) {
                skip(reason);
                continue;
            }
            static const char* rangeText = std::getenv("APS5_RELINK_RELOCATE_RANGE");
            const auto ordinal = report.Relocated + [&] { std::size_t count = 0; for (const auto& [why, n] : report.NotRelocated) count += n; return count; }() + report.OutOfRange;
            if (rangeText != nullptr) {
                char* split = nullptr;
                const auto low = std::strtoull(rangeText, &split, 0);
                const auto high = split != nullptr && *split == ':' ? std::strtoull(split + 1, nullptr, 0) : ~0ull;
                if (ordinal < low || ordinal >= high) {
                    ++report.OutOfRange;
                    skip(reason);
                    continue;
                }
            }
            if (auto site = relocateLeaf(seg, ph, start, body, targets, overlapsExisting, refusal, operands)) {
                taken.emplace_back(start, site->Length + start);
                sites.push_back(std::move(*site));
                ++report.Relocated;
                report.RelocatedOperands += operands;
                continue;
            }
            ++report.NotRelocated[refusal];
            if (std::getenv("APS5_RELINK_REDZONE_SKIPS") != nullptr) std::fprintf(stderr, "red-zone not relocated 0x%llx-0x%llx (%zu instructions): %s\n", static_cast<unsigned long long>(ph.MappedAddress + start), static_cast<unsigned long long>(ph.MappedAddress + limit), body.size(), refusal);
            skip(reason);
            continue;
        }
        for (const auto& stub : stubs)
            for (auto k = stub.First; k < stub.Last; ++k) moves.erase(k);

        const auto addSite = [&](const std::size_t begin, const std::size_t end, std::vector<std::uint8_t> code) {
            TrampolineSite site;
            site.Offset = ph.Offset + begin;
            site.Address = ph.MappedAddress + begin;
            site.Length = end - begin;
            site.OriginalBytes.assign(seg.begin() + static_cast<std::ptrdiff_t>(begin), seg.begin() + static_cast<std::ptrdiff_t>(end));
            site.ReturnBranchOffset = code.size();
            code.insert(code.end(), std::begin(kJumpBack), std::end(kJumpBack));
            site.Body = std::move(code);
            taken.emplace_back(begin, end);
            sites.push_back(std::move(site));
        };
        const auto original = [&](const std::size_t begin, const std::size_t end) { return std::vector<std::uint8_t>(seg.begin() + static_cast<std::ptrdiff_t>(begin), seg.begin() + static_cast<std::ptrdiff_t>(end)); };
        auto entry = original(start, prologueEnd);
        const auto lower = lowerRsp(shift);
        entry.insert(entry.end(), lower.begin(), lower.end());
        entry.insert(entry.end(), entryTail.begin(), entryTail.end());
        addSite(start, entryEnd, std::move(entry));
        for (auto& exit : exits) {
            auto code = std::move(exit.Lead);
            const auto raise = raiseRsp(shift);
            code.insert(code.end(), raise.begin(), raise.end());
            const auto tail = original(exit.TailBegin, exit.End);
            code.insert(code.end(), tail.begin(), tail.end());
            addSite(exit.Begin, exit.End, std::move(code));
        }
        for (auto& stub : stubs) addSite(stub.Begin, stub.End, std::move(stub.Body));
        for (auto& [k, move] : moves) displacements.push_back({ph.Offset + move.first, std::move(move.second)});
        ++report.Framed;
        if (!rbpPrologue) ++report.FramedWithoutFramePointer;
        report.Exits += exits.size();
        if (entryLast != p) ++report.WidenedPrologues;
        report.WidenedOperands += entryOperands;
        for (const auto& exit : exits) {
            if (exit.First != exit.Tail) ++report.WidenedExits;
            report.WidenedOperands += exit.Operands;
        }
        report.MovedOperands += moves.size();
        report.OperandStubs += stubs.size();
        for (const auto& stub : stubs) report.StubbedOperands += stub.Operands;
    }
}

}

std::vector<TrampolineSite> FrameRedZoneFunctions(
    const std::vector<std::uint8_t>& fileBytes,
    const std::vector<Domain::ProgramHeader>& codeSegments,
    const std::vector<RedZoneFunction>& functions,
    const std::vector<TrampolineSite>& existing,
    RedZoneFrameReport& report,
    std::vector<RedZoneDisplacement>& displacements,
    const bool relocateRefused
) {
    std::vector<TrampolineSite> sites;
    for (const auto& ph : codeSegments) frameSegment(fileBytes, ph, functions, existing, sites, displacements, report, relocateRefused);
    return sites;
}

}
