#ifndef CODEGEN_X86_AMD64ONLYSUBSTITUTIONTABLE_HPP
#define CODEGEN_X86_AMD64ONLYSUBSTITUTIONTABLE_HPP

#include <cstdint>
#include <cstddef>

namespace Codegen {
namespace Amd64OnlySubstitutionTable {

struct Entry {
    const char* Name;
    const std::uint8_t* Bytes;
    std::size_t Size;
};

#define AMD64_STUB(name, ...) \
    inline constexpr std::uint8_t k##name##Bytes[] = {__VA_ARGS__}; \
    inline constexpr Entry k##name = {#name, k##name##Bytes, sizeof(k##name##Bytes)};

AMD64_STUB(Monitorx, 0x0F, 0x01, 0xFA)
AMD64_STUB(Mwaitx, 0x0F, 0x01, 0xFB)
AMD64_STUB(Clzero, 0x0F, 0x01, 0xFC)
AMD64_STUB(Rdpru, 0x0F, 0x01, 0xFD)
AMD64_STUB(Mcommit, 0xF3, 0x0F, 0x01, 0xFA)

#undef AMD64_STUB

} // namespace Amd64OnlySubstitutionTable
} // namespace Codegen

#endif
