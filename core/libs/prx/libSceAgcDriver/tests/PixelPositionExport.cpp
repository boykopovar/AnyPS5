#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>

int main() {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 3> code{0xf80008cfu, 0u, 0xbf810000u};
    ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = 0;
    pixel.inputAddr = 0x2;
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    try {
        static_cast<void>(Recompile(request));
        std::fprintf(stderr, "a pixel shader exporting a position recompiled\n");
    } catch (const std::runtime_error& error) {
        if (std::string_view(error.what()).find("vertex input info is missing for a position export") != std::string_view::npos) return 0;
        std::fprintf(stderr, "unexpected error: %s\n", error.what());
    }
    return 1;
}
