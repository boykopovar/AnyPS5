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

std::vector<std::uint32_t> recompile(std::span<const std::uint32_t> code) {
    const std::array<std::uint32_t, 12> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu, 0u, 0u, 0u, 0u, 7u, 9u, 0x10002000u, 0u};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    const std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
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
    require(!result.spirv.empty(), "the shader did not recompile");
    return result.spirv.Copy();
}

void requireDistinctTargets(const char* name, std::span<const std::uint32_t> code) {
    const auto words = recompile(code);
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        require(count != 0 && cursor + count <= words.size(), std::string(name) + ": truncated SPIR-V instruction");
        if ((words[cursor] & 0xffffu) == spv::OpBranchConditional) {
            require(words[cursor + 2] != words[cursor + 3], std::string(name) + ": OpBranchConditional with the same true and false label, invalid in SPIR-V 1.6");
        }
        cursor += count;
    }
}

}

int main() {
    try {
        constexpr std::array<std::uint32_t, 2> zeroOffsetBranch{0xbf840000u, 0xbf810000u};
        requireDistinctTargets("s_cbranch_scc0 to the next instruction", zeroOffsetBranch);
        constexpr std::array<std::uint32_t, 4> unalignedStore{0x7e020281u, 0xe0700000u, 0x80020100u, 0xbf810000u};
        requireDistinctTargets("store through a descriptor with an unaligned base", unalignedStore);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SpirvBranchTargets: %s\n", error.what());
        return 1;
    }
    std::puts("SpirvBranchTargets: no conditional branch names one label twice");
    return 0;
}
