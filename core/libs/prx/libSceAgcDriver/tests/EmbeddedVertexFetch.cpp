#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/EmbeddedVertexFetch.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t AttributeTable = 0;
constexpr std::uint32_t BufferTable = 2;

std::int32_t FetchedAttribute(std::uint32_t source, std::uint32_t field) {
    const std::array<std::uint32_t, 11> code{
        0xbe9403ffu, source,
        0xbe9503ffu, field,
        0x93961514u,
        0xf4080405u, 0x2c000000u,
        0x02140b08u,
        0xe00c2000u, 0x80040b0au,
        0xbf810000u,
    };
    const auto program = RdnaInstructionDecoder{}.Decode(code);
    const auto plan = EmbeddedVertexFetchAnalyzer{}.Analyze(program, AttributeTable, BufferTable, 0, 16, 64);
    return plan.loads.size() == 1 ? plan.loads.front().attributeId : -1;
}

int Expect(const char* name, std::uint32_t source, std::uint32_t field, std::uint32_t offset) {
    const auto expected = static_cast<std::int32_t>(offset / 16u);
    const auto actual = FetchedAttribute(source, field);
    if (actual == expected) return 0;
    std::fprintf(stderr, "%s: s_bfe_u32 0x%08x, 0x%08x gives buffer slot %d, expected %d\n", name, source, field, actual, expected);
    return 1;
}

}

int main() {
    int failures = 0;
    failures += Expect("width 30 at offset 0", 0x9e000000u, 0x001e0000u, 0x1e000000u);
    failures += Expect("width 8 at offset 4", 0xfff00320u, 0x00080004u, 0x32u);
    failures += Expect("width 0", 0x00000320u, 0x00000000u, 0u);
    failures += Expect("width above 32", 0x00000320u, 0x007f0000u, 0x320u);
    return failures == 0 ? 0 : 1;
}
