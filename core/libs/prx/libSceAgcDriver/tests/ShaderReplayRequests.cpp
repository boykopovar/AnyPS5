#include "BdaAbi.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

int main(int argc, char** argv) try {
    if (argc != 2) return 2;
    using namespace ShaderRecompiler;
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);
    const std::vector<std::uint32_t> code{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xbf068010u, 0xbf850006u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    std::array<std::uint32_t, 32> srt{};
    srt[0] = 0x12345678u;
    srt[1] = 0xffffffffu;
    srt[2] = 0xffffffffu;
    srt[3] = 0x20000000u;
    srt[16] = 0x100000u;
    srt[18] = 16u;
    srt[19] = 0xfacu;
    const std::array<std::uint32_t, 2> userData{0x200000u, 0u};
    const std::array<MemoryRegion, 1> regions{{{0x200000u, std::as_bytes(std::span(srt))}}};
    const std::array<std::uint32_t, 4> capabilities{spv::CapabilityImageGatherExtended, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x22000u, code, 0, {}};
    request.context.waveSize = 32u;
    request.context.userData = userData;
    request.context.memory = regions;
    request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32u;
    request.target.bdaAbiVersion = BdaAbi::Version;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.layout.pushConstantSizeBytes = 128u;
    for (const auto mode : {0u, 1u}) {
        srt[20] = mode;
        std::ofstream file(directory / (mode == 0u ? "inactive.req" : "invalid.req"), std::ios::binary);
        file << RequestSerializer{}.Serialize(request);
        if (!file) throw std::runtime_error("cannot write synthetic replay request");
    }
    const std::array<std::uint32_t, 19> ldsCode{
        0x34040082u, 0x4a020081u, 0xd8340000u, 0x00000102u, 0xbf8cc07fu, 0xbf8a0000u, 0x7da800ffu, 0x00000080u,
        0xbf880006u, 0xd8d80200u, 0x03000002u, 0xbf8cc07fu, 0x4a020701u, 0xd8340000u, 0x00000102u, 0xbefe03c1u,
        0xbf8cc07fu, 0xbf8a0000u, 0xbf810000u,
    };
    const std::array<std::uint32_t, 2> ldsCapabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform};
    const std::array<std::string_view, 1> ldsExtensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest lds{};
    lds.shader = {ShaderStage::Compute, 0x20000u, ldsCode, 0, {}};
    lds.context.waveSize = 32u;
    lds.context.compute = ShaderComputeStageInfo{{256u, 1u, 1u}, 512u, {false, false, false}, false, 1u};
    lds.target.vulkanVersion = 0x00403000u;
    lds.target.spirvVersion = 0x00010600u;
    lds.target.subgroupSize = 32u;
    lds.target.bdaAbiVersion = 1u;
    lds.target.supportedCapabilities = ldsCapabilities;
    lds.target.supportedExtensions = ldsExtensions;
    lds.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    lds.target.maxWorkgroupInvocations = 1024u;
    lds.target.maxWorkgroupSharedMemoryBytes = 49152u;
    lds.layout = {0, 0, 0, 128};
    {
        std::ofstream file(directory / "lds.req", std::ios::binary);
        file << RequestSerializer{}.Serialize(lds);
        if (!file) throw std::runtime_error("cannot write synthetic replay request");
    }
    std::puts("synthetic guarded replay requests written");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
