#include <relinker/analysis/SyscallScanner.hpp>
#include <memory>
#include <sstream>

namespace Relinker {

static constexpr std::uint8_t SYSCALL_BYTE0 = 0x0F;
static constexpr std::uint8_t SYSCALL_BYTE1 = 0x05;
static constexpr std::uint8_t INT80_BYTE0 = 0xCD;
static constexpr std::uint8_t INT80_BYTE1 = 0x80;
static constexpr std::uint8_t SYSENTER_BYTE0 = 0x0F;
static constexpr std::uint8_t SYSENTER_BYTE1 = 0x34;
static constexpr std::uint8_t SYSRET_BYTE0 = 0x0F;
static constexpr std::uint8_t SYSRET_BYTE1 = 0x07;

class X64InstructionLengthDecoder {
public:
    struct DecodedInstruction {
        std::size_t Length;
        bool Valid;
    };

    [[nodiscard]] DecodedInstruction Decode(const std::uint8_t* data, std::size_t available) const {
        std::size_t pos = 0;
        bool rexPresent = false;
        std::uint8_t rex = 0;
        bool operandSizeOverride = false;

        while (pos < available) {
            const std::uint8_t b = data[pos];

            if (b == 0xF0 || b == 0xF2 || b == 0xF3 ||
                b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 ||
                b == 0x64 || b == 0x65) {
                pos += 1;
                continue;
            }

            if (b == 0x66) {
                operandSizeOverride = true;
                pos += 1;
                continue;
            }

            if (b == 0x67) {
                pos += 1;
                continue;
            }

            break;
        }

        if (pos < available && data[pos] >= 0x40 && data[pos] <= 0x4F) {
            rexPresent = true;
            rex = data[pos];
            pos += 1;
        }

        if (pos >= available) {
            return {1, false};
        }

        std::uint8_t opcode = data[pos];
        pos += 1;
        bool twoByteOpcode = false;

        if (opcode == 0x0F) {
            if (pos >= available) {
                return {1, false};
            }
            twoByteOpcode = true;
            opcode = data[pos];
            pos += 1;
        }

        bool hasModRm = false;
        std::size_t immediateSize = 0;

        if (!twoByteOpcode) {
            if ((opcode >= 0x00 && opcode <= 0x03) || (opcode >= 0x08 && opcode <= 0x0B) ||
                (opcode >= 0x10 && opcode <= 0x13) || (opcode >= 0x18 && opcode <= 0x1B) ||
                (opcode >= 0x20 && opcode <= 0x23) || (opcode >= 0x28 && opcode <= 0x2B) ||
                (opcode >= 0x30 && opcode <= 0x33) || (opcode >= 0x38 && opcode <= 0x3B) ||
                opcode == 0x62 || opcode == 0x63 ||
                (opcode >= 0x84 && opcode <= 0x8F) ||
                opcode == 0xC0 || opcode == 0xC1 ||
                opcode == 0xC4 || opcode == 0xC5 || opcode == 0xC6 || opcode == 0xC7 ||
                opcode == 0xD0 || opcode == 0xD1 || opcode == 0xD2 || opcode == 0xD3 ||
                opcode == 0xF6 || opcode == 0xF7 ||
                opcode == 0xFE || opcode == 0xFF ||
                (opcode >= 0x88 && opcode <= 0x8B)) {
                hasModRm = true;
            }

            if (opcode == 0xC6 || opcode == 0x80 ||
                opcode == 0x04 || opcode == 0x0C ||
                opcode == 0x14 || opcode == 0x1C ||
                opcode == 0x24 || opcode == 0x2C ||
                opcode == 0x34 || opcode == 0x3C ||
                (opcode >= 0xB0 && opcode <= 0xB7) ||
                opcode == 0x6A ||
                opcode == 0xA8) {
                immediateSize = 1;
            } else if (opcode == 0xC7 || opcode == 0x81 ||
                       opcode == 0x05 || opcode == 0x0D ||
                       opcode == 0x15 || opcode == 0x1D ||
                       opcode == 0x25 || opcode == 0x2D ||
                       opcode == 0x35 || opcode == 0x3D ||
                       opcode == 0x68 ||
                       opcode == 0xA9 ||
                       (opcode >= 0xB8 && opcode <= 0xBF)) {
                if (opcode >= 0xB8 && opcode <= 0xBF) {
                    immediateSize = (rexPresent && (rex & 0x08) != 0) ? 8 : (operandSizeOverride ? 2 : 4);
                } else {
                    immediateSize = operandSizeOverride ? 2 : 4;
                }
            } else if (opcode == 0x83) {
                immediateSize = 1;
            }

            if (opcode >= 0x70 && opcode <= 0x7F) {
                immediateSize = 1;
            }

            if (opcode == 0xE8 || opcode == 0xE9) {
                immediateSize = 4;
            }

            if (opcode == 0xEB) {
                immediateSize = 1;
            }
        } else {
            if (opcode >= 0x80 && opcode <= 0x8F) {
                immediateSize = 4;
            } else if (opcode == 0xBA) {
                hasModRm = true;
                immediateSize = 1;
            } else if (opcode == 0xA4 || opcode == 0xAC) {
                immediateSize = 1;
            } else if ((opcode >= 0x40 && opcode <= 0x4F) ||
                       (opcode >= 0xA3 && opcode <= 0xAB) ||
                       (opcode >= 0xB0 && opcode <= 0xB7) ||
                       (opcode >= 0xBC && opcode <= 0xBF) ||
                       (opcode >= 0x10 && opcode <= 0x17) ||
                       (opcode >= 0x28 && opcode <= 0x2F) ||
                       (opcode >= 0x38 && opcode <= 0x3A) ||
                       (opcode >= 0x54 && opcode <= 0x77) ||
                       (opcode >= 0xD0 && opcode <= 0xFE) ||
                       opcode == 0x1F) {
                hasModRm = true;
            }
        }

        if (!hasModRm) {
            if (pos + immediateSize > available) {
                return {pos + immediateSize, false};
            }
            return {pos + immediateSize, true};
        }

        if (pos >= available) {
            return {pos, false};
        }

        const std::uint8_t modrm = data[pos];
        pos += 1;

        const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
        const std::uint8_t rm = static_cast<std::uint8_t>(modrm & 0x07);

        if (mod != 0x03 && rm == 0x04) {
            if (pos >= available) {
                return {pos, false};
            }
            const std::uint8_t sib = data[pos];
            const std::uint8_t base = static_cast<std::uint8_t>(sib & 0x07);
            pos += 1;

            if (mod == 0x00 && base == 0x05) {
                pos += 4;
            }
        }

        if (mod == 0x01) {
            pos += 1;
        } else if (mod == 0x02) {
            pos += 4;
        } else if (mod == 0x00 && rm == 0x05) {
            pos += 4;
        }

        pos += immediateSize;

        if (pos > available) {
            return {pos, false};
        }

        return {pos, true};
    }
};

class SyscallScanner : public ISyscallScanner {
public:
    void ScanCodeSectionForSyscalls(
        const std::vector<std::uint8_t>& codeSection,
        FileByteOffset codeSectionOffset,
        FileByteOffset codeSectionSize) override;
};

void SyscallScanner::ScanCodeSectionForSyscalls(
    const std::vector<std::uint8_t>& codeSection,
    const FileByteOffset codeSectionOffset,
    const FileByteOffset codeSectionSize) {
    const std::size_t limit = std::min(codeSection.size(), static_cast<std::size_t>(codeSectionSize));

    std::size_t i = 0;

    while (i < limit) {
        X64InstructionLengthDecoder decoder;
        const std::uint8_t* cursor = codeSection.data() + i;
        const std::size_t available = limit - i;

        if (available >= 2) {
            const std::uint8_t b0 = cursor[0];
            const std::uint8_t b1 = cursor[1];

            const bool isSyscall = (b0 == SYSCALL_BYTE0 && b1 == SYSCALL_BYTE1);
            const bool isInt80 = (b0 == INT80_BYTE0 && b1 == INT80_BYTE1);
            const bool isSysenter = (b0 == SYSENTER_BYTE0 && b1 == SYSENTER_BYTE1);
            const bool isSysret = (b0 == SYSRET_BYTE0 && b1 == SYSRET_BYTE1);

            if (isSyscall || isInt80 || isSysenter || isSysret) {
                FileByteOffset instrOffset = codeSectionOffset + i;
                std::ostringstream msg;
                msg << "Forbidden syscall instruction at code offset 0x" << std::hex << instrOffset;
                throw RelinkerException(msg.str(), instrOffset);
            }
        }

        const auto decoded = decoder.Decode(cursor, available);

        if (!decoded.Valid || decoded.Length == 0) {
            i += 1;
            continue;
        }

        i += decoded.Length;
    }
}

std::unique_ptr<ISyscallScanner> MakeSyscallScanner() {
    return std::make_unique<SyscallScanner>();
}

}
