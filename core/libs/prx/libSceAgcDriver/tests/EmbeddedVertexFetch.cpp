#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/EmbeddedVertexFetch.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <span>
#include <vector>

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

int NoCarryOffsets() {
    const std::array<std::uint32_t, 10> code{
        0x4a0a0a10u, 0x4a101011u,
        0xf4080506u, 0xfa000000u,
        0x02060b08u,
        0xbf8cc07fu,
        0xe0002000u, 0x80050a03u,
        0xbf8c3f70u, 0xbf810000u,
    };
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    int failures = 0;
    for (const auto wave : {32u, 64u}) {
        const auto analyze = [&](const RdnaProgram& program) { return EmbeddedVertexFetchAnalyzer{}.Analyze(program, 6, 4, 8, 16, wave); };
        const auto plan = analyze(decoded);
        if (plan.loads.size() != 1 || plan.vertexOffsetSgpr != 16 || plan.instanceOffsetSgpr != 17 || plan.vertexOffsetShared || plan.instanceOffsetShared || plan.vertexIndexObserved || plan.instanceIndexObserved) {
            std::fprintf(stderr, "wave%u: no-carry offsets: loads=%zu vertex=%d instance=%d shared=%d/%d observed=%d/%d\n", wave, plan.loads.size(), plan.vertexOffsetSgpr, plan.instanceOffsetSgpr, plan.vertexOffsetShared, plan.instanceOffsetShared, plan.vertexIndexObserved, plan.instanceIndexObserved);
            ++failures;
        }
        const auto rejected = [&](const char* label, std::initializer_list<std::uint32_t> instruction) {
            std::vector<std::uint32_t> words(instruction);
            words.insert(words.end(), code.begin() + 1, code.end());
            if (analyze(RdnaInstructionDecoder{}.Decode(words)).vertexOffsetSgpr == -1) return;
            std::fprintf(stderr, "wave%u: %s was admitted as a no-carry offset\n", wave, label);
            ++failures;
        };
        rejected("VOP3 saturation", {0xd5258005u, 0x00020a10u});
        rejected("SDWA byte source", {0x4a0a0af9u, 0x06800610u});
        rejected("vector offset", {0x4a0a0b10u});
        rejected("different index source", {0x4a0a0c10u});
        rejected("non-user scalar", {0x4a0a0a1fu});
        rejected("subtraction", {0x4c0a0a10u});
        auto sharedCode = std::vector<std::uint32_t>(code.begin(), code.end());
        sharedCode.insert(sharedCode.end() - 1, 0x7e080210u);
        const auto shared = analyze(RdnaInstructionDecoder{}.Decode(sharedCode));
        if (shared.vertexOffsetSgpr != 16 || !shared.vertexOffsetShared) {
            std::fprintf(stderr, "wave%u: a second reader of the offset was not marked shared\n", wave);
            ++failures;
        }
        auto observedCode = std::vector<std::uint32_t>(code.begin(), code.end());
        observedCode.insert(observedCode.begin(), 0x7e020305u);
        if (!analyze(RdnaInstructionDecoder{}.Decode(observedCode)).vertexIndexObserved) {
            std::fprintf(stderr, "wave%u: a read before the offset add was not preserved\n", wave);
            ++failures;
        }
    }
    return failures;
}

}

int main() {
    int failures = 0;
    failures += Expect("width 30 at offset 0", 0x9e000000u, 0x001e0000u, 0x1e000000u);
    failures += Expect("width 8 at offset 4", 0xfff00320u, 0x00080004u, 0x32u);
    failures += Expect("width 0", 0x00000320u, 0x00000000u, 0u);
    failures += Expect("width above 32", 0x00000320u, 0x007f0000u, 0x320u);
    failures += NoCarryOffsets();
    return failures == 0 ? 0 : 1;
}
