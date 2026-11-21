#include "Recompiler.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace ShaderRecompiler;

constexpr std::uint64_t ThresholdAddress = 0x40000u;
constexpr std::uint64_t ValueAddress = 0x50000u;

constexpr std::array<std::uint32_t, 27> FreshMaskCode{
    0xf4200200u, 0xfa000000u, 0xbe8a047eu, 0xbefe0a7eu, 0xd7650001u, 0x000100c1u, 0x7e020d01u, 0xbf8cc07fu, 0x7c080208u, 0x8a8a6a0au,
    0xbefe0a0au, 0xbeea047eu, 0xd5480002u, 0x02051100u, 0xbefe046au, 0x7d840481u, 0xbeea2d6au, 0x8f0c8c6au, 0x88070c07u, 0xf4200502u,
    0xfa000000u, 0xbf8cc07fu, 0x7e060214u, 0xbefe040au, 0xf800180fu, 0x03030303u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 28> OverwrittenMaskCode{
    0xf4200200u, 0xfa000000u, 0xbe8a047eu, 0xbefe0a7eu, 0xd7650001u, 0x000100c1u, 0x7e020d01u, 0x7e040301u, 0xbf8cc07fu, 0x7c080208u,
    0x8a8a6a0au, 0xbefe0a0au, 0xbeea047eu, 0xd5480002u, 0x02051100u, 0xbefe046au, 0x7d840481u, 0xbeea2d6au, 0x8f0c8c6au, 0x88070c07u,
    0xf4200502u, 0xfa000000u, 0xbf8cc07fu, 0x7e060214u, 0xbefe040au, 0xf800180fu, 0x03030303u, 0xbf810000u,
};

struct Program {
    const char* name;
    std::span<const std::uint32_t> code;
};

constexpr std::array<Program, 2> Programs{{
    {"a sample bit extracted into a fresh VGPR", FreshMaskCode},
    {"a sample bit extracted over a VGPR written before the discard", OverwrittenMaskCode},
}};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32) & 0xffffu) | (4u << 16), 64u, 0x01016facu};
}

std::string Compile(std::span<const std::uint32_t> code, bool withThreshold) {
    const auto threshold = BufferDescriptor(ThresholdAddress);
    const auto value = BufferDescriptor(ValueAddress);
    std::array<std::uint32_t, 8> userData{};
    std::memcpy(userData.data(), threshold.data(), sizeof(threshold));
    std::memcpy(userData.data() + 4, value.data(), sizeof(value));
    alignas(16) static std::array<std::byte, 256> thresholdBytes{};
    alignas(16) static std::array<std::byte, 256> valueBytes{};
    const float half = 0.5f;
    std::memcpy(thresholdBytes.data(), &half, sizeof(half));
    std::array<MemoryRegion, 2> regions{MemoryRegion{ValueAddress, valueBytes}, MemoryRegion{ThresholdAddress, thresholdBytes}};
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userData = userData;
    request.context.memory = std::span<const MemoryRegion>(regions.data(), withThreshold ? 2u : 1u);
    ShaderPixelStageInfo pixel{};
    pixel.inputAddr = PixelInputBit(PixelInput::Ancillary);
    pixel.ancillary = true;
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping.fill(0xe4u);
    request.context.pixel = pixel;
    const std::array<std::uint32_t, 8> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformVote, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    try {
        static_cast<void>(Recompile(request));
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

}

int main() {
    try {
        for (const auto& program : Programs) {
            const auto complete = Compile(program.code, true);
            Require(complete.empty(), std::string(program.name) + ": the shader failed to recompile with all memory present: " + complete);
            const auto descriptorOnly = Compile(program.code, false);
            Require(descriptorOnly.empty(), std::string(program.name) + ": the descriptor of a single-sample pixel shader read the discard threshold, so its mask of odd-sample lanes was not folded to zero: " + descriptorOnly);
        }
        std::printf("sample bit descriptor tests passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
