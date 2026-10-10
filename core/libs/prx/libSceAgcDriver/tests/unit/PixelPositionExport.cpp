#include <Testing/Test.hpp>
#include "Recompiler.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Require;

const Case positionExport{"Recompile_PixelShaderExportingPosition_IsRejected", [] {
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
    const auto error = Testing::RequireThrows<std::runtime_error>([&] { static_cast<void>(Recompile(request)); },
                                                                  "a pixel shader exporting a position recompiled");
    Require(std::string_view(error.what()).find("vertex input info is missing for a position export") != std::string_view::npos,
            std::string("unexpected error: ") + error.what());
}};

} // namespace
