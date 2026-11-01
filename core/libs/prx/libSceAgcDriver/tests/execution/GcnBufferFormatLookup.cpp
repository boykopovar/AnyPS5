#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t Vertices = 3;
constexpr std::uint32_t Fill = 0xdeadbeefu;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uintptr_t GcnAddressLimit = std::uintptr_t{1} << 44u;
constexpr std::array<std::uint32_t, 4> InputWords{0x12345678u, 0x9abcdef0u, 0x0badf00du, 0x600dcafeu};
alignas(256) std::array<std::byte, Width * Height * 4> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 8> VertexCode{
    0xe0300000u, 0x80010100u, 0xbf8c0f70u, 0xe0700000u, 0x80020100u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u,
};

class LowBlock {
public:
    LowBlock() : block(static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes))) {
        Require(block != nullptr, "GCN buffer format lookup: cannot allocate the buffer block in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, 0x04u, BlockBytes);
    }

    ~LowBlock() {
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }
    bool Addressable() const { return Address() + BlockBytes <= GcnAddressLimit; }
    std::span<std::uint32_t, 4> Input() const { return std::span<std::uint32_t, 4>(block, 4); }
    std::span<std::uint32_t, 4> Output() const { return std::span<std::uint32_t, 4>(block + 64, 4); }

    std::array<std::uint32_t, 4> Stored() const {
        std::array<std::uint32_t, 4> words{};
        AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(Output().data()), std::as_writable_bytes(std::span(words)), 4);
        return words;
    }

private:
    std::uint32_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes, std::uint32_t dataFormat, std::uint32_t numberFormat) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0xfacu | (numberFormat << 12u) | (dataFormat << 15u)};
}

std::vector<std::uint32_t> UserData(const LowBlock& block, std::uint32_t dataFormat, std::uint32_t numberFormat) {
    std::vector<std::uint32_t> userData(12, 0u);
    const auto input = BufferDescriptor(block.Input().data(), 16u, dataFormat, numberFormat);
    const auto output = BufferDescriptor(block.Output().data(), 16u, 4u, 4u);
    std::copy(input.begin(), input.end(), userData.begin() + 4);
    std::copy(output.begin(), output.end(), userData.begin() + 8);
    return userData;
}

ShaderRecompiler::RecompileRequest VertexRequest(std::span<const std::uint32_t> userData, const ShaderRecompiler::SpirvTarget& target, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, memory},
        target,
        {0, 0, 0, 64}
    };
    request.useCache = false;
    return request;
}

std::uint32_t PrivateStores(std::span<const std::uint32_t> words) {
    std::set<std::uint32_t> privateMemory;
    std::uint32_t stores = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Require(count != 0 && count <= words.size() - offset, "GCN buffer format lookup: malformed SPIR-V instruction");
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpVariable && count >= 4 && words[offset + 3] == spv::StorageClassPrivate) privateMemory.insert(words[offset + 2]);
        if ((opcode == spv::OpAccessChain || opcode == spv::OpInBoundsAccessChain) && count >= 4 && privateMemory.contains(words[offset + 3])) privateMemory.insert(words[offset + 2]);
        if (opcode == spv::OpStore && count >= 3 && privateMemory.contains(words[offset + 1])) ++stores;
        offset += count;
    }
    return stores;
}

void CheckModule(const AgcDriver::VulkanDevice& device, const LowBlock& block, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    auto target = device.Target();
    target.registrationModules = true;
    const auto userData = UserData(block, 4u, 4u);
    const auto result = ShaderRecompiler::Recompile(VertexRequest(userData, target, memory));
    Require(result.registrationModule, "GCN buffer format lookup: the vertex shader was not emitted in registration form");
    const auto stores = PrivateStores(result.spirv.Words());
    Require(stores == 0u, "GCN buffer format lookup: the registration module stores " + std::to_string(stores) + " times into private memory to translate DATA_FORMAT and NUM_FORMAT, which drivers keep in per-invocation scratch");
}

void Draw(AgcDriver::VulkanDevice& device, const LowBlock& block, std::uint32_t dataFormat, std::uint32_t numberFormat, std::span<const ShaderRecompiler::MemoryRegion> vertexMemory, const ShaderRecompiler::RecompileResult& pixelResult, std::uint32_t vertexPush, std::vector<std::uint32_t>& registrationModule) {
    Pixels.fill(std::byte{0x40});
    std::copy(InputWords.begin(), InputWords.end(), block.Input().begin());
    std::fill(block.Output().begin(), block.Output().end(), Fill);
    const auto userData = UserData(block, dataFormat, numberFormat);
    const auto vertexResult = ShaderRecompiler::Recompile(VertexRequest(userData, device.Target(), vertexMemory));
    const auto what = "DATA_FORMAT " + std::to_string(dataFormat) + " NUM_FORMAT " + std::to_string(numberFormat);
    Require(static_cast<std::uint32_t>(vertexResult.pushConstants.size()) == vertexPush, "GCN buffer format lookup, " + what + ": the vertex push constants changed size");
    if (device.Target().registrationModules) {
        if (registrationModule.empty()) registrationModule = vertexResult.spirv.Copy();
        Require(std::ranges::equal(vertexResult.spirv.Words(), registrationModule), "GCN buffer format lookup, " + what + ": registration modules: the V# specialized the vertex module");
    }
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, Vertices, 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    const auto translated = ShaderRecompiler::GcnBufferFormats()[(dataFormat << 3u) | numberFormat];
    const auto expected = translated != 0u ? InputWords[0] : 0u;
    const auto output = block.Stored();
    char text[96];
    std::snprintf(text, sizeof(text), ": translated format %u, loaded 0x%08x, expected 0x%08x", translated, output[0], expected);
    Require(output[0] == expected, "GCN buffer format lookup, " + what + text);
    Require(std::all_of(output.begin() + 1, output.end(), [](std::uint32_t value) { return value == Fill; }), "GCN buffer format lookup, " + what + ": the store wrote past its dword");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Addressable()) {
            std::printf("skipped, the guest arena block at 0x%llx lies above the 44-bit GCN V# address range\n", static_cast<unsigned long long>(block.Address()));
            return VulkanTestSkipped;
        }
        const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
        CheckModule(*device, block, vertexMemory);
        const auto firstUserData = UserData(block, 4u, 4u);
        const auto vertexPush = static_cast<std::uint32_t>(ShaderRecompiler::Recompile(VertexRequest(firstUserData, device->Target(), vertexMemory)).pushConstants.size());
        ShaderRecompiler::ShaderPixelStageInfo pixel{};
        pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
        pixel.posX = true;
        pixel.posY = true;
        pixel.targetOutputMode[0] = 9;
        pixel.targetExportMapping.fill(0xe4u);
        const std::vector<std::uint32_t> pixelUserData(8, 0u);
        const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
        ShaderRecompiler::RecompileRequest fragment{
            {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
            {64u, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
            device->Target(),
            PixelPushLayout(vertexPush, device->Target())
        };
        fragment.useCache = false;
        const auto pixelResult = ShaderRecompiler::Recompile(fragment);
        std::vector<std::uint32_t> registrationModule;
        for (std::uint32_t dataFormat = 0; dataFormat < 16u; ++dataFormat) {
            for (std::uint32_t numberFormat = 0; numberFormat < 8u; ++numberFormat) Draw(*device, block, dataFormat, numberFormat, vertexMemory, pixelResult, vertexPush, registrationModule);
        }
        std::puts("GCN buffer format lookup tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
