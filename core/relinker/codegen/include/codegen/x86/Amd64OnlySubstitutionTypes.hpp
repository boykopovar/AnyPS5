#ifndef CODEGEN_X86_AMD64ONLYSUBSTITUTIONTYPES_HPP
#define CODEGEN_X86_AMD64ONLYSUBSTITUTIONTYPES_HPP

#include <domain/Types.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Codegen {

enum class Amd64OnlyTarget : std::uint8_t {
    Intel,
    Rosetta
};

enum class Amd64OnlyLowering : std::uint8_t {
    InPlace,
    Trampoline,
    Unsupported,
    Kept
};

struct StubRelocation {
    std::size_t DisplacementOffset;
    std::size_t InstructionEnd;
    std::int64_t SiteTarget;
};

struct Amd64OnlyMatch {
    std::string InstructionName;
    std::size_t Length;
    Amd64OnlyLowering Lowering;
    std::vector<std::uint8_t> ReplacementBytes;
    std::vector<std::uint8_t> StubBody;
    std::size_t ReturnBranchOffset;
    bool Optional = false;
    std::vector<StubRelocation> Relocations = {};
};

struct Amd64OnlySubstitutionReport {
    std::string InstructionName;
    Domain::FileByteOffset Offset;
    std::size_t OriginalLength;
    std::size_t ReplacementLength;
    Amd64OnlyLowering Lowering;
};

}

#endif
