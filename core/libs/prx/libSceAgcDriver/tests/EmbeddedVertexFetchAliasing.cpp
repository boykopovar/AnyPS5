#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/EmbeddedVertexFetch.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace ShaderRecompiler;

int main() {
    int failures = 0;
    for (const std::uint32_t waveSize : {32u, 64u}) {
        for (const std::uint32_t indexRegister : {0u, 1u, 2u, 3u, 16u}) {
            const std::array<std::uint32_t, 8> code{
                0xf4000004u | (indexRegister << 6u), 0xfa00000cu,
                0xf4080005u, indexRegister << 25u,
                0x02140b08u,
                0xe00c2000u, 0x80000b0au,
                0xbf810000u,
            };
            const auto decoded = RdnaInstructionDecoder{}.Decode(code);
            const auto plan = EmbeddedVertexFetchAnalyzer{}.Analyze(decoded, 0, 2, 0, 16, waveSize);
            if (plan.loads.size() != 1 || plan.loads.front().attributeId != 3 || plan.loads.front().componentCount != 4 ||
                plan.loads.front().programCounter != 20 || plan.loads.front().prologLoads != std::vector<std::uint32_t>{0, 8}) {
                std::fprintf(stderr, "wave%u descriptor offset s%u lost its attribute or load provenance\n", waveSize, indexRegister);
                ++failures;
            }
        }
    }
    return failures == 0 ? 0 : 1;
}
