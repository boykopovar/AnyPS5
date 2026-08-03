#include <relinker/ISyscallScanner.hpp>
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

class SyscallScanner : public ISyscallScanner {
public:
    void ScanCodeSectionForSyscalls(
        const std::vector<std::uint8_t>& CodeSection,
        Offset CodeSectionOffset,
        Offset CodeSectionSize) override;
};

void SyscallScanner::ScanCodeSectionForSyscalls(
    const std::vector<std::uint8_t>& CodeSection,
    Offset CodeSectionOffset,
    Offset CodeSectionSize) {
    std::size_t limit = std::min(CodeSection.size(), static_cast<std::size_t>(CodeSectionSize));

    for (std::size_t i = 0; i + 1 < limit; ++i) {
        std::uint8_t b0 = CodeSection[i];
        std::uint8_t b1 = CodeSection[i + 1];

        bool isSyscall = (b0 == SYSCALL_BYTE0 && b1 == SYSCALL_BYTE1);
        bool isInt80 = (b0 == INT80_BYTE0 && b1 == INT80_BYTE1);
        bool isSysenter = (b0 == SYSENTER_BYTE0 && b1 == SYSENTER_BYTE1);
        bool isSysret = (b0 == SYSRET_BYTE0 && b1 == SYSRET_BYTE1);

        if (isSyscall || isInt80 || isSysenter || isSysret) {
            Offset instrOffset = CodeSectionOffset + i;
            std::ostringstream msg;
            msg << "Forbidden syscall instruction at code offset 0x" << std::hex << instrOffset;
            throw RelinkerException(msg.str(), instrOffset);
        }
    }
}

std::unique_ptr<ISyscallScanner> MakeSyscallScanner() {
    return std::make_unique<SyscallScanner>();
}

}
