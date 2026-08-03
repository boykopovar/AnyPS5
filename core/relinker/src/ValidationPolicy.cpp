#include "relinker/ValidationPolicy.hpp"
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

static constexpr std::uint32_t R_X86_64_NONE = 0;
static constexpr std::uint32_t R_X86_64_64 = 1;
static constexpr std::uint32_t R_X86_64_PC32 = 2;
static constexpr std::uint32_t R_X86_64_GOT32 = 3;
static constexpr std::uint32_t R_X86_64_PLT32 = 4;
static constexpr std::uint32_t R_X86_64_COPY = 5;
static constexpr std::uint32_t R_X86_64_GLOB_DAT = 6;
static constexpr std::uint32_t R_X86_64_JUMP_SLOT = 7;
static constexpr std::uint32_t R_X86_64_RELATIVE = 8;
static constexpr std::uint32_t R_X86_64_GOTPCREL = 9;
static constexpr std::uint32_t R_X86_64_32 = 10;
static constexpr std::uint32_t R_X86_64_32S = 11;
static constexpr std::uint32_t R_X86_64_GOTPCRELX = 41;
static constexpr std::uint32_t R_X86_64_REX_GOTPCRELX = 42;

void ValidationPolicy::_initializeSupportedRelocationTypes() {
    _supportedRelocationTypes = {
        R_X86_64_NONE,
        R_X86_64_64,
        R_X86_64_PC32,
        R_X86_64_GOT32,
        R_X86_64_PLT32,
        R_X86_64_COPY,
        R_X86_64_GLOB_DAT,
        R_X86_64_JUMP_SLOT,
        R_X86_64_RELATIVE,
        R_X86_64_GOTPCREL,
        R_X86_64_32,
        R_X86_64_32S,
        R_X86_64_GOTPCRELX,
        R_X86_64_REX_GOTPCRELX,
    };
}

void ValidationPolicy::RegisterLibraryImport(const std::string& library) {
    if (_supportedRelocationTypes.empty()) {
        _initializeSupportedRelocationTypes();
    }
    _importedLibraries.insert(library);
}

void ValidationPolicy::ValidateSyscallAbsence() {
}

void ValidationPolicy::ValidateRelocationTypeSupported(const std::uint32_t relocationTypeValue, const FileByteOffset fileByteOffset) {
    if (_supportedRelocationTypes.empty()) {
        _initializeSupportedRelocationTypes();
    }
    if (_supportedRelocationTypes.find(relocationTypeValue) == _supportedRelocationTypes.end()) {
        std::ostringstream msg;
        msg << "Unsupported relocation type 0x" << std::hex << relocationTypeValue
            << " at offset 0x" << fileByteOffset;
        throw RelinkerException(msg.str(), fileByteOffset);
    }
}

void ValidationPolicy::ValidateNidBelongsToLibrary(const std::string& Nid, const std::string& library) {
    if (_importedLibraries.find(library) == _importedLibraries.end()) {
        std::ostringstream msg;
        msg << "NID \"" << Nid << "\" references library \"" << library
            << "\" which is not in the NEEDED list";
        throw RelinkerException(msg.str(), 0);
    }
}

void ValidationPolicy::ValidateSceStructureSize(const ByteCount expectedSize, const ByteCount actualSize, const FileByteOffset fileByteOffset) {
    if (actualSize != expectedSize) {
        std::ostringstream msg;
        msg << "SCE structure size mismatch at offset 0x" << std::hex << fileByteOffset
            << ": expected " << std::dec << expectedSize << ", got " << actualSize;
        throw RelinkerException(msg.str(), fileByteOffset);
    }
}

void ValidationPolicy::ValidateDynamicFieldInterpretable(const std::string& fieldName, const FileByteOffset fileByteOffset) {
    std::ostringstream msg;
    msg << "Dynamic field \"" << fieldName
        << "\" cannot be interpreted at offset 0x" << std::hex << fileByteOffset;
    throw RelinkerException(msg.str(), fileByteOffset);
}

void ValidationPolicy::ValidateNoSyscallInstructions(
    const std::vector<std::uint8_t>& CodeSection, const FileByteOffset codeOffset) {
    for (std::size_t i = 0; i + 1 < CodeSection.size(); ++i) {
        const std::uint8_t b0 = CodeSection[i];
        const std::uint8_t b1 = CodeSection[i + 1];

        bool isSyscall = (b0 == SYSCALL_BYTE0 && b1 == SYSCALL_BYTE1);
        bool isInt80 = (b0 == INT80_BYTE0 && b1 == INT80_BYTE1);
        bool isSysenter = (b0 == SYSENTER_BYTE0 && b1 == SYSENTER_BYTE1);
        bool isSysret = (b0 == SYSRET_BYTE0 && b1 == SYSRET_BYTE1);

        if (isSyscall || isInt80 || isSysenter || isSysret) {
            FileByteOffset instrOffset = codeOffset + i;
            std::ostringstream msg;
            msg << "Forbidden syscall instruction at offset 0x" << std::hex << instrOffset;
            throw RelinkerException(msg.str(), instrOffset);
        }
    }
}

}
