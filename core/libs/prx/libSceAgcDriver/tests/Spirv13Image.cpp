#include "BdaAbi.hpp"
#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace {

using namespace ShaderRecompiler;

alignas(256) std::array<std::uint32_t, 65536> Output{};
alignas(256) std::array<std::uint8_t, 32768> Texture{};
alignas(256) constexpr std::array<std::uint32_t, 17> Code{
    0x7e080218u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u, 0x2c3e3c87u, 0x363c3cffu, 0x0000007fu,
    0x7e140280u, 0x7e160280u, 0x7e180280u, 0x7e1a0280u, 0xf0001f08u, 0x00010a1eu,
    0xbf8c3f70u, 0xe0781000u, 0x80000a03u, 0xbf810000u,
};

void CheckImage(std::uint32_t version) {
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const auto textureAddress = reinterpret_cast<std::uintptr_t>(Texture.data());
    const std::array<std::uint32_t, 4> buffer{
        static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu),
        static_cast<std::uint32_t>(Output.size() * 4u), 0x31016facu,
    };
    const std::array<std::uint32_t, 8> image{
        static_cast<std::uint32_t>(textureAddress >> 8u),
        static_cast<std::uint32_t>((textureAddress >> 40u) & 0xffu) | (6u << 20u) | (3u << 30u),
        31u | (127u << 14u), 0x24cu | (9u << 28u), 0u, 0u, 0u, 0u,
    };
    const std::array<std::uint32_t, 4> sampler{0x92u, 0x00fff000u, 0u, 0u};
    std::array<std::uint32_t, 24> userData{};
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(image.begin(), image.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const std::array<std::uint32_t, 15> capabilities{
        spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses,
        spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle,
        spv::CapabilityImageQuery, spv::CapabilityStorageImageExtendedFormats, spv::CapabilityStorageImageReadWithoutFormat,
        spv::CapabilityStorageImageWriteWithoutFormat, spv::CapabilitySampled1D, spv::CapabilityImage1D,
        spv::CapabilitySampledBuffer, spv::CapabilityImageBuffer, spv::CapabilityFloat16,
    };
    const std::array<std::string_view, 4> extensions{
        "SPV_EXT_descriptor_indexing", "SPV_KHR_float_controls", "SPV_KHR_physical_storage_buffer", "SPV_KHR_16bit_storage",
    };
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}};
    request.context = {32, 0, userData, ShaderComputeStageInfo{{32, 1, 1}, 0, {true, false, false}, false, 1}, std::nullopt, std::nullopt, memory};
    request.target = {0x00401000u, version, 32, BdaAbi::Version, capabilities, extensions, false, {1024, 1024, 64}, 1024, 32768, {}, {}};
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    const auto result = Recompile(request);
    const auto& words = result.spirv.Words();
    if (words.size() < 5 || words[0] != spv::MagicNumber || words[1] > version) {
        throw std::runtime_error("image compilation did not preserve the requested SPIR-V target");
    }
}

}

int main() {
    try {
        CheckImage(0x00010300u);
        CheckImage(0x00010400u);
        std::puts("SPIR-V 1.3 and 1.4 image compilation passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
