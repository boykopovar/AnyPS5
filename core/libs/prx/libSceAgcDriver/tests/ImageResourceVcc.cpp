#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>

using namespace ShaderRecompiler;

int main() {
    try {
        constexpr std::uint32_t ImageLoad = 0xf0001f08u;
        constexpr std::uint32_t ImageSample = 0xf0800f08u;
        constexpr std::uint32_t R128 = 0x8000u;
        for (const auto first : {ImageLoad, ImageSample, ImageLoad | R128, ImageSample | R128}) {
            const bool r128 = (first & R128) != 0u;
            const auto decode = [&](std::uint32_t resource) {
                const std::array<std::uint32_t, 2> code{first, (26u << 21u) | (resource << 16u) | (16u << 8u) | 4u};
                return DecodeRdnaMimg(0x100u, code, 0u);
            };
            const std::uint32_t last = r128 ? 26u : 25u;
            for (std::uint32_t resource = 0u; resource <= last; ++resource) {
                const auto instruction = decode(resource);
                if (instruction.programCounter != 0x100u || instruction.sourceCount != 3u ||
                    instruction.source1.kind != RdnaOperandKind::ScalarRegister || instruction.source1.reg != resource * 4u ||
                    instruction.source2.kind != RdnaOperandKind::ScalarRegister || instruction.source2.reg != 104u ||
                    instruction.source0.kind != RdnaOperandKind::VectorRegister || instruction.source0.reg != 4u ||
                    instruction.destination.kind != RdnaOperandKind::VectorRegister || instruction.destination.reg != 16u ||
                    instruction.imageR128 != r128)
                    throw std::runtime_error("image descriptor decode changed another operand");
            }
            for (std::uint32_t resource = last + 1u; resource < 32u; ++resource) {
                bool refused = false;
                try { static_cast<void>(decode(resource)); }
                catch (const std::runtime_error& error) {
                    refused = std::string_view(error.what()) == "MIMG descriptor register range overflow";
                }
                if (!refused) throw std::runtime_error("image descriptor extends past the VCC tuple");
            }
        }
        std::puts("MIMG mixed SGPR/VCC descriptors and out-of-range refusals pass");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
