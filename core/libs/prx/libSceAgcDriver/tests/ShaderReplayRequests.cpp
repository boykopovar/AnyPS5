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
    std::puts("synthetic guarded replay requests written");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
