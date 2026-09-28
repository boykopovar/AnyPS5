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

AMD64_STUB(JmpRel32, 0xE9, 0x00, 0x00, 0x00, 0x00)
AMD64_STUB(LeaRspBelowRedZone, 0x48, 0x8D, 0xA4, 0x24, 0x70, 0xFF, 0xFF, 0xFF)
AMD64_STUB(LeaRspRestore, 0x48, 0x8D, 0xA4, 0x24, 0x90, 0x00, 0x00, 0x00)
AMD64_STUB(Nop1, 0x90)
AMD64_STUB(Nop2, 0x66, 0x90)
AMD64_STUB(Nop3, 0x0F, 0x1F, 0x00)
AMD64_STUB(Nop4, 0x0F, 0x1F, 0x40, 0x00)
AMD64_STUB(Nop5, 0x0F, 0x1F, 0x44, 0x00, 0x00)
AMD64_STUB(Nop6, 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00)
AMD64_STUB(Nop7, 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00)

#undef AMD64_STUB

inline constexpr Entry kNops[] = {kNop1, kNop2, kNop3, kNop4, kNop5, kNop6, kNop7};

inline constexpr Entry kMonitorx = {"MONITORX", nullptr, 0};
inline constexpr Entry kMwaitx = {"MWAITX", nullptr, 0};
inline constexpr Entry kClzero = {"CLZERO", nullptr, 0};
inline constexpr Entry kRdpru = {"RDPRU", nullptr, 0};
inline constexpr Entry kMcommit = {"MCOMMIT", nullptr, 0};
inline constexpr Entry kExtrq = {"EXTRQ", nullptr, 0};
inline constexpr Entry kInsertq = {"INSERTQ", nullptr, 0};
inline constexpr Entry kExtrqRegisterForm = {"EXTRQ register form", nullptr, 0};
inline constexpr Entry kInsertqRegisterForm = {"INSERTQ register form", nullptr, 0};
inline constexpr Entry kMovntss = {"MOVNTSS", nullptr, 0};
inline constexpr Entry kMovntsd = {"MOVNTSD", nullptr, 0};

inline constexpr std::uint8_t kMovsStoreOpcode = 0x11;
inline constexpr std::uint8_t kPshufbZero = 0x80;
inline constexpr std::uint8_t kTrapFill = 0xCC;
inline constexpr std::size_t kStubAlignment = 16;
inline constexpr std::size_t kRedZoneSpillFrame = 0x90;

} // namespace Amd64OnlySubstitutionTable
} // namespace Codegen

#endif
