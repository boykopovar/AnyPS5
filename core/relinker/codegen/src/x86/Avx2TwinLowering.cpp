#include <codegen/x86/Avx2TwinLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <cstring>
#include <span>
#include <string>

namespace Codegen {

namespace {

constexpr std::uint8_t kThreeByteVex = 0xC4;
constexpr std::uint8_t kMap0F = 1;
constexpr std::uint8_t kMap0F38 = 2;
constexpr std::uint8_t kMap0F3A = 3;
constexpr std::uint8_t kPrefix66 = 1;
constexpr std::uint8_t kPrefixF2 = 3;

struct Rewrite {
    const char* Name;
    std::uint8_t Opcode;
};

std::optional<Rewrite> _rewrite(const std::uint8_t map, const bool wide, const bool vector256, const bool memory, const std::uint8_t opcode) {
    if (wide) return std::nullopt;
    if (map == kMap0F3A) {
        if (opcode == 0x38 && vector256) return Rewrite{"VINSERTI128", 0x18};
        if (opcode == 0x39 && vector256) return Rewrite{"VEXTRACTI128", 0x19};
        if (opcode == 0x46 && vector256) return Rewrite{"VPERM2I128", 0x06};
        if (opcode == 0x02) return Rewrite{"VPBLENDD", 0x0C};
        return std::nullopt;
    }
    if (map != kMap0F38 || !memory) return std::nullopt;
    if (opcode == 0x5A && vector256) return Rewrite{"VBROADCASTI128", 0x1A};
    if (opcode == 0x58) return Rewrite{"VPBROADCASTD", 0x18};
    if (opcode == 0x59) return Rewrite{"VPBROADCASTQ", vector256 ? std::uint8_t{0x19} : std::uint8_t{0x12}};
    return std::nullopt;
}

}

std::optional<Avx2Twin> DecodeAvx2Twin(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && ScalarLowering::LegacyPrefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != kThreeByteVex || (data[vex + 2] & 3) != kPrefix66)
        return std::nullopt;
    const auto map = static_cast<std::uint8_t>(data[vex + 1] & 0x1F);
    const bool wide = (data[vex + 2] & 0x80) != 0;
    const bool vector256 = (data[vex + 2] & 4) != 0;
    const bool memory = (data[vex + 4] >> 6) != 3;
    const auto rewrite = _rewrite(map, wide, vector256, memory, data[vex + 3]);
    if (!rewrite) return std::nullopt;
    Avx2Twin twin{rewrite->Name, std::vector<std::uint8_t>(data, data + length)};
    twin.Replacement[vex + 3] = rewrite->Opcode;
    if (map == kMap0F38 && data[vex + 3] == 0x59 && !vector256) {
        twin.Replacement[vex + 1] = static_cast<std::uint8_t>((data[vex + 1] & 0xE0) | kMap0F);
        twin.Replacement[vex + 2] = static_cast<std::uint8_t>((data[vex + 2] & 0x78) | kPrefixF2);
    }
    return twin;
}

void EmitAvx2Twin(StubBodyBuilder& body, const Avx2Twin& twin) {
    const X64InstructionDecoder decoder;
    const std::span<const std::uint8_t> bytes(twin.Replacement);
    const auto info = decoder.DecodeInstruction(bytes.data(), bytes.size());
    if (info.Length != bytes.size())
        throw CodegenException(std::string(twin.Name) + " replacement does not decode to one instruction");
    if (!info.HasRipRelativeDisp) {
        body.Raw(bytes);
        return;
    }
    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes.data() + info.RipRelativeDispOffset, sizeof(displacement));
    body.RipRelative(bytes, info.RipRelativeDispOffset, static_cast<std::int64_t>(bytes.size()) + displacement);
}

}
