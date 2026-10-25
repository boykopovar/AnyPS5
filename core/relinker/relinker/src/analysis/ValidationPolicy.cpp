#include "relinker/analysis//ValidationPolicy.hpp"
#include <elfpatcher/general/ElfConstants.hpp>
#include <sstream>

namespace Relinker {

using namespace Elfpatcher;

void ValidationPolicy::_initializeSupportedRelocationTypes() {
    _supportedRelocationTypes = {
        R_X86_64_NONE,
        R_X86_64_64,
        R_X86_64_PC32,
        R_X86_64_GLOB_DAT,
        R_X86_64_JUMP_SLOT,
        R_X86_64_RELATIVE,
        R_X86_64_32,
        R_X86_64_DTPMOD64,
        R_X86_64_DTPOFF64,
        R_X86_64_TPOFF64,
    };
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

}
