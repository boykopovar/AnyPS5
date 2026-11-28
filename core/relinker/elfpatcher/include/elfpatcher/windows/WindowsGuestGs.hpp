#pragma once

#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <codegen/CodegenTypes.hpp>

namespace Elfpatcher::Windows {

class WindowsGuestGs {
public:
    static bool TryAppend(PeSection& section, std::uint32_t rva, const std::uint8_t* bytes, const Codegen::DecodedInstructionInfo& info);
};

}
