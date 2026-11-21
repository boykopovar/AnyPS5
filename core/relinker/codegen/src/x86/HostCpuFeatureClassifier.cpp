#include <codegen/x86/HostCpuFeatureClassifier.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <HostCpuFeatures.hpp>

namespace Codegen {

namespace {

using namespace VexEncoding;

bool isLegacyPrefix(const std::uint8_t byte) {
    return byte == 0xF0 || byte == 0xF2 || byte == 0xF3 || byte == 0x2E || byte == 0x36 || byte == 0x3E ||
           byte == 0x26 || byte == 0x64 || byte == 0x65 || byte == 0x66 || byte == 0x67;
}

bool isAvx1Vector256(const std::uint8_t map, const std::uint8_t prefix, const std::uint8_t opcode) {
    if (map == Map0F) {
        if (prefix == 0) {
            if (opcode >= 0x10 && opcode <= 0x17) return true;
            if (opcode >= 0x28 && opcode <= 0x2F) return true;
            if (opcode >= 0x50 && opcode <= 0x5F) return true;
            return opcode == 0x77 || opcode == 0xC2 || opcode == 0xC6;
        }
        if (prefix == Prefix66) {
            if (opcode >= 0x10 && opcode <= 0x17) return true;
            if (opcode >= 0x28 && opcode <= 0x2F) return true;
            if (opcode >= 0x50 && opcode <= 0x5F) return true;
            if (opcode == 0x6F || opcode == 0x7F || opcode == 0xE7) return true;
            if (opcode == 0x77 || opcode == 0x7C || opcode == 0x7D) return true;
            return opcode == 0xC2 || opcode == 0xC6 || opcode == 0xD0;
        }
        if (prefix == PrefixF3) {
            if (opcode == 0x10 || opcode == 0x11 || opcode == 0x12 || opcode == 0x16) return true;
            if (opcode >= 0x2A && opcode <= 0x2F) return true;
            if (opcode >= 0x51 && opcode <= 0x5F) return true;
            if (opcode == 0x6F || opcode == 0x7E || opcode == 0x7F) return true;
            return opcode == 0xC2;
        }
        if (prefix == PrefixF2) {
            if (opcode == 0x10 || opcode == 0x11 || opcode == 0x12) return true;
            if (opcode >= 0x2A && opcode <= 0x2F) return true;
            if (opcode >= 0x51 && opcode <= 0x5F) return true;
            if (opcode == 0x7C || opcode == 0x7D) return true;
            return opcode == 0xC2 || opcode == 0xD0 || opcode == 0xF0;
        }
        return false;
    }
    if (map == Map0F38 && prefix == Prefix66) {
        switch (opcode) {
        case 0x0C: case 0x0D: case 0x0E: case 0x0F: case 0x17:
        case 0x18: case 0x19: case 0x1A:
        case 0x2C: case 0x2D: case 0x2E: case 0x2F:
        case 0x41:
        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
            return true;
        default:
            return false;
        }
    }
    if (map == Map0F3A && prefix == Prefix66) {
        switch (opcode) {
        case 0x04: case 0x05: case 0x06:
        case 0x08: case 0x09: case 0x0A: case 0x0B:
        case 0x0C: case 0x0D:
        case 0x18: case 0x19:
        case 0x40: case 0x41:
        case 0x4A: case 0x4B:
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0xDF:
            return true;
        default:
            return false;
        }
    }
    return false;
}

bool isAvx2Vector128(const std::uint8_t map, const std::uint8_t prefix, const std::uint8_t opcode) {
    if (prefix != Prefix66) return false;
    if (map == Map0F38) {
        switch (opcode) {
        case 0x45: case 0x46: case 0x47:
        case 0x58: case 0x59:
        case 0x78: case 0x79:
        case 0x8C: case 0x8D: case 0x8E: case 0x8F:
        case 0x90: case 0x91: case 0x92: case 0x93:
            return true;
        default:
            return false;
        }
    }
    return map == Map0F3A && opcode == 0x02;
}

std::uint64_t classifyVex(const Instruction& instruction) {
    if (instruction.Map == Map0F38 && instruction.Prefix == Prefix66) {
        if ((instruction.Opcode >= 0x96 && instruction.Opcode <= 0x9F) ||
            (instruction.Opcode >= 0xA6 && instruction.Opcode <= 0xAF) ||
            (instruction.Opcode >= 0xB6 && instruction.Opcode <= 0xBF))
            return HostCpuFeatureBits::Fma;
        if (instruction.Opcode == 0x13) return HostCpuFeatureBits::F16c;
    }
    if (instruction.Map == Map0F3A && instruction.Prefix == Prefix66 && instruction.Opcode == 0x1D)
        return HostCpuFeatureBits::F16c;

    if (instruction.Map == Map0F38) {
        if (instruction.Prefix == 0) {
            if (instruction.Opcode == 0xF2 || instruction.Opcode == 0xF3 || instruction.Opcode == 0xF7)
                return HostCpuFeatureBits::Bmi1;
            if (instruction.Opcode == 0xF5 || instruction.Opcode == 0xF6)
                return HostCpuFeatureBits::Bmi2;
        }
        if (instruction.Prefix != 0) {
            if (instruction.Opcode == 0xF5 || instruction.Opcode == 0xF6 || instruction.Opcode == 0xF7)
                return HostCpuFeatureBits::Bmi2;
        }
    }
    if (instruction.Map == Map0F3A && instruction.Prefix == PrefixF2 && instruction.Opcode == 0xF0)
        return HostCpuFeatureBits::Bmi2;

    if (instruction.Vector256) {
        if (!isAvx1Vector256(instruction.Map, instruction.Prefix, instruction.Opcode))
            return HostCpuFeatureBits::Avx2;
        return 0;
    }
    if (isAvx2Vector128(instruction.Map, instruction.Prefix, instruction.Opcode))
        return HostCpuFeatureBits::Avx2;
    return 0;
}

}

std::uint64_t RequiredHostCpuFeatures(const std::uint8_t* data, const std::size_t length) {
    if (data == nullptr || length == 0) return 0;

    if (const auto instruction = Decode(data, length)) {
        if (instruction->End > length) return 0;
        return classifyVex(*instruction);
    }

    std::size_t position = 0;
    bool repeat = false;
    bool repeatNot = false;
    while (position < length) {
        const std::uint8_t byte = data[position];
        if (byte == 0xF3) repeat = true;
        if (byte == 0xF2) repeatNot = true;
        if (!isLegacyPrefix(byte) && (byte < 0x40 || byte > 0x4F)) break;
        ++position;
    }
    if (position + 1 >= length || data[position] != 0x0F) return 0;
    const std::uint8_t first = data[position + 1];
    if (repeat && first == 0xBC) return HostCpuFeatureBits::Bmi1;
    if (repeat && first == 0xBD) return HostCpuFeatureBits::Lzcnt;
    if (first == 0x38 && !repeat && !repeatNot && position + 2 < length) {
        const std::uint8_t second = data[position + 2];
        if (second == 0xF0 || second == 0xF1) return HostCpuFeatureBits::Movbe;
    }
    return 0;
}

}
