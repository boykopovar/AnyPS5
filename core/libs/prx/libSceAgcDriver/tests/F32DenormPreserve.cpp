#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
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
constexpr std::array<std::uint32_t, 4> MovStore{0x7e020300u, 0xe0700000u, 0x80000100u, 0xbf810000u};

std::vector<std::uint32_t> recompile(bool preserve, bool flush, bool flushCapability = true, std::optional<ShaderFloatMode> floatMode = std::nullopt, std::span<const std::uint32_t> code = CvtF16F32Store) {
    const std::array<std::uint32_t, 4> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityDenormPreserve, spv::CapabilityDenormFlushToZero};
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_storage_buffer_storage_class", "SPV_KHR_float_controls"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.context.floatMode = floatMode;
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = std::span<const std::uint32_t>(capabilities).first(flushCapability ? capabilities.size() : capabilities.size() - 1);
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.target.float32DenormPreserve = preserve;
    request.target.float32FlushToZero = flush;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    auto result = Recompile(request);
    require(!result.spirv.empty(), "the shader did not recompile");
    return result.spirv.Copy();
}

struct Modes {
    bool preserve32 = false;
    bool flush32 = false;
    bool preserveCapability = false;
};

Modes scan(std::span<const std::uint32_t> words) {
    Modes modes;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        require(count != 0 && cursor + count <= words.size(), "truncated SPIR-V instruction");
        if (op == spv::OpCapability && words[cursor + 1] == spv::CapabilityDenormPreserve) modes.preserveCapability = true;
        if (op == spv::OpExecutionMode && count == 4 && words[cursor + 3] == 32u) {
            if (words[cursor + 2] == spv::ExecutionModeDenormPreserve) modes.preserve32 = true;
            if (words[cursor + 2] == spv::ExecutionModeDenormFlushToZero) modes.flush32 = true;
        }
        cursor += count;
    }
    return modes;
}

void verify() {
    const auto preserved = scan(recompile(true, false));
    require(preserved.preserve32 && preserved.preserveCapability && !preserved.flush32, "float32DenormPreserve did not emit the 32-bit DenormPreserve execution mode and capability");
    const auto plain = scan(recompile(false, false));
    require(!plain.preserve32 && !plain.flush32, "a target without float32DenormPreserve or float32FlushToZero set a 32-bit denormal execution mode");
    const auto flushed = scan(recompile(true, true));
    require(flushed.flush32 && !flushed.preserve32, "the hardware 32-bit flush did not take precedence over the preserve flag");
    const auto nativeFlush = scan(recompile(true, true, false));
    require(!nativeFlush.preserve32 && !nativeFlush.flush32, "a hardware flush without the DenormFlushToZero capability declared a 32-bit denormal execution mode");
    const auto flushingMode = scan(recompile(true, true, true, ShaderFloatMode{0xc0u, true, false, false}, MovStore));
    require(flushingMode.flush32 && !flushingMode.preserve32, "FLOAT_MODE 0xc0 flushes f32 denormals but the module did not take the hardware flush");
    for (const std::uint32_t mode : {0xd0u, 0xe0u, 0xf0u}) {
        char name[64];
        std::snprintf(name, sizeof(name), "FLOAT_MODE 0x%02x keeps f32 denormals", mode);
        const auto kept = scan(recompile(true, true, true, ShaderFloatMode{mode, true, false, false}, MovStore));
        require(kept.preserve32 && kept.preserveCapability && !kept.flush32, std::string(name) + ", but the module did not declare DenormPreserve 32 instead of the hardware flush");
        const auto unpreserved = scan(recompile(false, true, true, ShaderFloatMode{mode, true, false, false}, MovStore));
        require(!unpreserved.flush32 && !unpreserved.preserve32, std::string(name) + ", but the module took the hardware flush on a device that cannot preserve them");
    }
}

}

int main() {
    try {
        verify();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "F32DenormPreserve: %s\n", error.what());
        return 1;
    }
    std::puts("F32DenormPreserve: the 32-bit DenormPreserve execution mode follows the target flag and yields to the hardware flush in shaders that flush f32 denormals");
    return 0;
}
