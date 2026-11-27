#ifndef CODEGEN_X86_LENGTHCHANGINGFIXUPS_HPP
#define CODEGEN_X86_LENGTHCHANGINGFIXUPS_HPP

#include <codegen/AddressMap.hpp>
#include <domain/Types.hpp>
#include <cstdint>
#include <span>

namespace Codegen {

struct CodeExtent {
    Domain::VirtualAddress Address;
    Domain::ByteCount Size;
};

enum class RelocationAddendKind : std::uint8_t {
    SymbolValue,
    CodeAddress,
};

struct CodeRelocation {
    Domain::VirtualAddress Offset;
    std::int64_t Addend;
    RelocationAddendKind AddendKind = RelocationAddendKind::SymbolValue;
};

struct CodeSymbol {
    Domain::VirtualAddress Value;
};

struct RelativeJumpTableRef {
    std::span<std::int32_t> Entries;
    Domain::VirtualAddress Address;
};

struct LengthChangingFixups {
    CodeExtent Code;
    std::span<CodeRelocation> Relocations;
    std::span<CodeSymbol> Symbols;
    std::span<const RelativeJumpTableRef> JumpTables;
    std::span<std::uint8_t> EhFrame;
    Domain::VirtualAddress EhFrameAddress = 0;
};

void FixupCodeRelocations(std::span<CodeRelocation> relocations, const CodeExtent& code, const AddressMap& map);

void FixupCodeSymbols(std::span<CodeSymbol> symbols, const CodeExtent& code, const AddressMap& map);

void FixupRelativeJumpTable(const RelativeJumpTableRef& table, const CodeExtent& code, const AddressMap& map);

void FixupEhFrame(std::span<std::uint8_t> ehFrame, Domain::VirtualAddress ehFrameAddress, const CodeExtent& code, const AddressMap& map);

void ApplyLengthChangingFixups(const LengthChangingFixups& fixups, const AddressMap& map);

}

#endif