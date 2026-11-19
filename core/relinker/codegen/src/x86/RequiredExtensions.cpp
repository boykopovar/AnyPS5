#include <codegen/x86/RequiredExtensions.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <cstddef>

namespace Codegen {

namespace {

constexpr std::uint8_t kOperandSizePrefix = 0x66;
constexpr std::uint8_t kRepPrefix = 0xF3;
constexpr std::uint8_t kRepnePrefix = 0xF2;
constexpr std::uint8_t kTwoByteEscape = 0x0F;
constexpr std::uint8_t kThreeByteVex = 0xC4;
constexpr std::uint8_t kTwoByteVex = 0xC5;
constexpr std::uint8_t kEvex = 0x62;
constexpr std::uint8_t kMap0F = 1;
constexpr std::uint8_t kMap0F38 = 2;
constexpr std::uint8_t kMap0F3A = 3;

struct Encoding {
    bool Valid = false;
    bool Vex = false;
    bool Evex = false;
    std::uint8_t Map = 0;
    std::uint8_t Prefix = 0;
    bool Vector256 = false;
    std::uint8_t Opcode = 0;
    std::uint8_t ModRm = 0;
    bool HasModRm = false;
};

bool _rex(const std::uint8_t byte) {
    return (byte & 0xF0) == 0x40;
}

bool _legacyPrefix(const std::uint8_t byte) {
    switch (byte) {
    case 0xF0:
    case 0xF2:
    case 0xF3:
    case 0x2E:
    case 0x36:
    case 0x3E:
    case 0x26:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
        return true;
    default:
        return false;
    }
}

std::uint8_t _vexPrefix(const std::uint8_t encoded) {
    switch (encoded & 3) {
    case 1: return kOperandSizePrefix;
    case 2: return kRepPrefix;
    case 3: return kRepnePrefix;
    default: return 0;
    }
}

Encoding _parse(const std::uint8_t* data, const std::size_t length) {
    Encoding encoding;
    std::size_t index = 0;
    std::uint8_t prefix = 0;
    while (index < length && _legacyPrefix(data[index])) {
        if (data[index] == kOperandSizePrefix || data[index] == kRepPrefix || data[index] == kRepnePrefix)
            prefix = data[index];
        ++index;
    }
    while (index < length && _rex(data[index])) ++index;
    if (index >= length) return encoding;
    if (data[index] == kEvex) return Encoding{.Valid = true, .Evex = true};
    if (data[index] == kThreeByteVex) {
        if (index + 4 > length) return encoding;
        encoding.Valid = true;
        encoding.Vex = true;
        encoding.Map = static_cast<std::uint8_t>(data[index + 1] & 0x1F);
        encoding.Prefix = _vexPrefix(data[index + 2]);
        encoding.Vector256 = (data[index + 2] & 4) != 0;
        encoding.Opcode = data[index + 3];
        if (index + 5 <= length) {
            encoding.ModRm = data[index + 4];
            encoding.HasModRm = true;
        }
        return encoding;
    }
    if (data[index] == kTwoByteVex) {
        if (index + 3 > length) return encoding;
        encoding.Valid = true;
        encoding.Vex = true;
        encoding.Map = kMap0F;
        encoding.Prefix = _vexPrefix(data[index + 1]);
        encoding.Vector256 = (data[index + 1] & 4) != 0;
        encoding.Opcode = data[index + 2];
        if (index + 4 <= length) {
            encoding.ModRm = data[index + 3];
            encoding.HasModRm = true;
        }
        return encoding;
    }
    if (data[index] != kTwoByteEscape) {
        encoding.Valid = true;
        encoding.Prefix = prefix;
        encoding.Opcode = data[index];
        return encoding;
    }
    if (index + 1 >= length) return encoding;
    encoding.Valid = true;
    encoding.Prefix = prefix;
    if (data[index + 1] == 0x38 || data[index + 1] == 0x3A) {
        if (index + 2 >= length) return encoding;
        encoding.Map = data[index + 1] == 0x38 ? kMap0F38 : kMap0F3A;
        encoding.Opcode = data[index + 2];
        return encoding;
    }
    encoding.Map = kMap0F;
    encoding.Opcode = data[index + 1];
    return encoding;
}

bool _avx1(const Encoding& encoding) {
    const auto map = encoding.Map;
    const auto prefix = encoding.Prefix;
    const auto opcode = encoding.Opcode;
    if (map == kMap0F) {
        if (prefix == 0) {
            if (opcode >= 0x10 && opcode <= 0x17) return true;
            if (opcode >= 0x28 && opcode <= 0x2F) return true;
            if (opcode >= 0x50 && opcode <= 0x5F) return true;
            if (opcode == 0x77 || opcode == 0xAE || opcode == 0xC2 || opcode == 0xC6) return true;
            return false;
        }
        if (prefix == kOperandSizePrefix) {
            if (opcode >= 0x10 && opcode <= 0x17) return true;
            if (opcode >= 0x28 && opcode <= 0x2F) return true;
            if (opcode >= 0x50 && opcode <= 0x5F) return true;
            if (opcode == 0x6E || opcode == 0x6F || opcode == 0x7E || opcode == 0x7F) return true;
            if (opcode == 0xC2 || opcode == 0xC6 || opcode == 0xD6 || opcode == 0xE6 || opcode == 0xE7) return true;
            return false;
        }
        if (prefix == kRepPrefix || prefix == kRepnePrefix) {
            if (opcode == 0x10 || opcode == 0x11 || opcode == 0x12 || opcode == 0x13) return true;
            if (opcode == 0x16 || opcode == 0x17) return true;
            if (opcode == 0x2A || opcode == 0x2C || opcode == 0x2D || opcode == 0x2E || opcode == 0x2F) return true;
            if (opcode >= 0x51 && opcode <= 0x5F && opcode != 0x54 && opcode != 0x55) return true;
            if (opcode == 0x7E || opcode == 0x7F || opcode == 0xD6 || opcode == 0xE6) return true;
            return false;
        }
        return false;
    }
    if (map == kMap0F38 && prefix == kOperandSizePrefix) {
        if (opcode == 0x0E || opcode == 0x0F || opcode == 0x17) return true;
        if (opcode == 0x18 || opcode == 0x19 || opcode == 0x1A) return true;
        if (opcode == 0x2C || opcode == 0x2E) return true;
        return false;
    }
    if (map == kMap0F3A && prefix == kOperandSizePrefix) {
        if (opcode == 0x06) return true;
        if (opcode >= 0x08 && opcode <= 0x0D) return true;
        if (opcode == 0x18 || opcode == 0x19) return true;
        if (opcode == 0x4A || opcode == 0x4B) return true;
        return false;
    }
    return false;
}

std::uint32_t _classify(const Encoding& encoding) {
    constexpr auto lzcnt = static_cast<std::uint32_t>(CpuExtension::Lzcnt);
    constexpr auto bmi1 = static_cast<std::uint32_t>(CpuExtension::Bmi1);
    constexpr auto bmi2 = static_cast<std::uint32_t>(CpuExtension::Bmi2);
    constexpr auto movbe = static_cast<std::uint32_t>(CpuExtension::Movbe);
    constexpr auto avx2 = static_cast<std::uint32_t>(CpuExtension::Avx2);
    constexpr auto fma = static_cast<std::uint32_t>(CpuExtension::Fma);
    constexpr auto f16c = static_cast<std::uint32_t>(CpuExtension::F16c);
    if (!encoding.Valid) return 0;
    if (encoding.Evex) return AllCpuExtensions;
    const auto map = encoding.Map;
    const auto prefix = encoding.Prefix;
    const auto opcode = encoding.Opcode;
    if (!encoding.Vex) {
        if (map == kMap0F && prefix == kRepPrefix) {
            if (opcode == 0xBC) return bmi1;
            if (opcode == 0xBD) return lzcnt;
        }
        if (map == kMap0F38 && (opcode == 0xF0 || opcode == 0xF1)) return movbe;
        return 0;
    }
    if (map == kMap0F38 && prefix == kOperandSizePrefix && (opcode == 0x18 || opcode == 0x19) &&
        encoding.HasModRm && (encoding.ModRm & 0xC0) == 0xC0)
        return avx2;
    if (map == kMap0F38) {
        if (opcode == 0xF2 || opcode == 0xF3 || opcode == 0xF5 || opcode == 0xF6 || opcode == 0xF7)
            return bmi1 | bmi2;
        if (prefix == kOperandSizePrefix) {
            if ((opcode >= 0x96 && opcode <= 0x9F) || (opcode >= 0xA6 && opcode <= 0xAF) || (opcode >= 0xB6 && opcode <= 0xBF))
                return fma;
            if (opcode == 0x13) return f16c;
            if (opcode == 0xF0 || opcode == 0xF1) return movbe;
        } else if (opcode == 0xF0 || opcode == 0xF1) {
            return movbe;
        }
    }
    if (map == kMap0F3A && prefix == kOperandSizePrefix && opcode == 0x1D) return f16c;
    return _avx1(encoding) ? 0 : avx2;
}

}

std::uint32_t RequiredExtensions(const std::uint8_t* data, const std::size_t size, const std::vector<Domain::CodeRange>& codeRanges) {
    if (data == nullptr || size == 0) return 0;
    const auto ranges = MergeCodeRanges(codeRanges);
    const auto scanner = MakeInstructionScanner({}, ranges);
    const std::vector<std::uint8_t> bytes(data, data + size);
    std::uint32_t required = 0;
    try {
        scanner->VisitCodeSection(bytes, 0, size, [&](const Codegen::InstructionMatch& match) {
            if (match.Offset > size || match.Length > size - match.Offset) {
                required |= AllCpuExtensions;
                return;
            }
            required |= _classify(_parse(data + match.Offset, match.Length));
        });
    } catch (...) {
        return required | AllCpuExtensions;
    }
    std::size_t covered = 0;
    for (const auto& range : ranges) {
        const auto begin = static_cast<std::size_t>(std::min<std::uint64_t>(range.Begin, size));
        const auto end = static_cast<std::size_t>(std::min<std::uint64_t>(range.End, size));
        if (begin > covered) required |= RequiredExtensions(data + covered, begin - covered);
        covered = std::max(covered, end);
    }
    if (covered < size) required |= RequiredExtensions(data + covered, size - covered);
    return required;
}

std::uint32_t RequiredExtensions(const std::uint8_t* data, const std::size_t size) {
    if (data == nullptr || size == 0) return 0;
    const X64InstructionDecoder decoder;
    std::uint32_t required = 0;
    std::size_t offset = 0;
    while (offset < size) {
        std::size_t length = 0;
        try {
            length = decoder.Decode(data + offset, size - offset);
        } catch (...) {
            return required | AllCpuExtensions;
        }
        if (length == 0) return required | AllCpuExtensions;
        required |= _classify(_parse(data + offset, length));
        offset += length;
    }
    return required;
}

}
