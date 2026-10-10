#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::array<std::uint32_t, 4> CvtF16F32Store{0x7e021500u, 0xe0700000u, 0x80000100u, 0xbf810000u};

std::vector<std::uint32_t> recompile(std::span<const std::uint32_t> capabilities) {
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, CvtF16F32Store, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    auto result = Recompile(request);
    require(!result.spirv.empty(), "v_cvt_f16_f32 did not recompile");
    return std::move(result.spirv);
}

struct Usage {
    std::size_t floatConverts = 0;
    bool float16Capability = false;
    bool roundingModeRte16 = false;
    bool denormPreserve16 = false;
};

Usage scan(std::span<const std::uint32_t> words) {
    Usage usage;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        require(count != 0 && cursor + count <= words.size(), "truncated SPIR-V instruction");
        if (op == spv::OpFConvert) ++usage.floatConverts;
        if (op == spv::OpCapability && words[cursor + 1] == spv::CapabilityFloat16) usage.float16Capability = true;
        if (op == spv::OpExecutionMode && count == 4 && words[cursor + 3] == 16u) {
            if (words[cursor + 2] == spv::ExecutionModeRoundingModeRTE) usage.roundingModeRte16 = true;
            if (words[cursor + 2] == spv::ExecutionModeDenormPreserve) usage.denormPreserve16 = true;
        }
        cursor += count;
    }
    return usage;
}

void verifyNative() {
    const std::array<std::uint32_t, 5> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityFloat16, spv::CapabilityRoundingModeRTE, spv::CapabilityDenormPreserve};
    const auto usage = scan(recompile(capabilities));
    require(usage.floatConverts != 0 && usage.float16Capability, "a device that rounds 16-bit floats to nearest even did not get OpFConvert to f16");
    require(usage.roundingModeRte16 && usage.denormPreserve16, "native f16 conversion without the 16-bit RoundingModeRTE and DenormPreserve execution modes");
}

void verifyEmulated() {
    const std::array<std::uint32_t, 3> withoutRte{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityFloat16};
    const auto usage = scan(recompile(withoutRte));
    require(usage.floatConverts == 0 && !usage.float16Capability && !usage.roundingModeRte16, "a device without RoundingModeRTE for 16-bit floats got native f16 conversion");
}

}

int main() {
    try {
        verifyNative();
        verifyEmulated();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Float16Conversion: %s\n", error.what());
        return 1;
    }
    std::puts("Float16Conversion: native and emulated f16 conversion selected by device capabilities");
    return 0;
}
