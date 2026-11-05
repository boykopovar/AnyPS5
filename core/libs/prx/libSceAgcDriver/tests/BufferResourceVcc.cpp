#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>

using namespace ShaderRecompiler;

int main() {
    try {
        for (const auto first : {0xe8b00000u, 0xe8b40000u, 0xe0000000u, 0xe0300000u, 0xe0700000u}) {
            const auto decode = [&](std::uint32_t resource) {
                const std::array<std::uint32_t, 2> code{first, (128u << 24u) | (resource << 16u) | (7u << 8u) | 3u};
                return first >> 26u == 0x3au ? DecodeRdnaMtbuf(0x100u, code, 0u) : DecodeRdnaMubuf(0x100u, code, 0u);
            };
            for (std::uint32_t resource = 0u; resource <= 26u; ++resource) {
                const auto instruction = decode(resource);
                if (instruction.programCounter != 0x100u || instruction.sourceCount != 3u ||
                    instruction.source1.kind != RdnaOperandKind::ScalarRegister || instruction.source1.reg != resource * 4u ||
                    instruction.source0.kind != RdnaOperandKind::VectorRegister || instruction.source0.reg != 3u ||
                    instruction.destination.kind != RdnaOperandKind::VectorRegister || instruction.destination.reg != 7u)
                    throw std::runtime_error("buffer descriptor decode changed another operand");
                if (first >> 26u == 0x3au && (instruction.dataFormat != 6u || instruction.numberFormat != 1u))
                    throw std::runtime_error("MTBUF VCC descriptor changed the instruction format");
            }
            for (std::uint32_t resource = 27u; resource < 32u; ++resource) {
                bool refused = false;
                try { static_cast<void>(decode(resource)); }
                catch (const std::runtime_error& error) {
                    const auto expected = resource != 31u ? "unsupported scalar source operand" :
                        first >> 26u == 0x3au ? "MTBUF resource descriptor register range overflow" :
                        "MUBUF resource descriptor register range overflow";
                    refused = std::string_view(error.what()) == expected;
                }
                if (!refused) throw std::runtime_error("buffer descriptor extends past the VCC tuple");
            }
        }
        std::puts("MUBUF and MTBUF mixed SGPR/VCC descriptors and invalid-base refusals pass");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
