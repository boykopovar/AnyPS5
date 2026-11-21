#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Color = std::array<float, 4>;
using Triangle = std::array<std::array<float, 4>, 3>;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
constexpr std::uint32_t Samples = 4;
constexpr std::uint32_t Format8888 = 10u;
constexpr std::uint32_t BlendClamp = 1u << 15u;
constexpr std::uint32_t FastClear = 1u << 13u;
constexpr std::uint32_t Compression = 1u << 14u;
constexpr std::uint32_t Thin1D = 13u;
constexpr std::size_t PlaneBytes = Width * Height * 4u;
constexpr std::size_t CmaskBytes = 512u;
constexpr std::size_t FmaskBytes = Width * Height;
constexpr std::size_t Slot = 0x10000;
constexpr std::size_t BlockBytes = 16u * Slot;
constexpr std::size_t EqaaColor = 0u * Slot;
constexpr std::size_t MsaaColor = 1u * Slot;
constexpr std::size_t OneFragmentColor = 2u * Slot;
constexpr std::size_t EqaaResolved = 3u * Slot;
constexpr std::size_t MsaaResolved = 4u * Slot;
constexpr std::size_t EqaaFmask = 5u * Slot;
constexpr std::size_t OneFragmentFmask = 6u * Slot;
constexpr std::size_t EqaaCmask = 7u * Slot;
constexpr std::size_t MsaaCmask = 8u * Slot;
constexpr std::size_t Vertices = 9u * Slot;
constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000u, 0x80000000u, 0xbf8c0f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

constexpr Triangle Lower{{{-1.0f, -1.0f, 0.5f, 1.0f}, {0.9f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.7f, 0.5f, 1.0f}}};
constexpr Triangle Upper{{{-0.5f, 1.0f, 0.5f, 1.0f}, {1.0f, -0.6f, 0.5f, 1.0f}, {1.0f, 1.0f, 0.5f, 1.0f}}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 2> Programs{};
std::size_t programCount = 0;

std::span<const std::uint32_t> Program(const Color& color) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        code[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(color[channel]);
    }
    code[at++] = 0xf800180fu;
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

class LowBlock {
public:
    LowBlock() {
#if defined(_WIN32) || defined(__APPLE__)
        data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
#ifdef _WIN32
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uint32_t ReadWriteProtection = 0x04;
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, ReadWriteProtection, BlockBytes);
#endif
#else
#ifdef ANYPS5_ENABLE_ASAN
        constexpr std::uintptr_t FirstHint = 0x60000000;
#else
        constexpr std::uintptr_t FirstHint = std::uintptr_t{1} << 32u;
#endif
        for (std::uintptr_t hint = FirstHint; data == nullptr && hint < GcnAddressLimit; hint += std::uintptr_t{1} << 32u) {
            void* mapped = mmap(reinterpret_cast<void*>(hint), BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (mapped != MAP_FAILED) data = static_cast<std::uint8_t*>(mapped);
        }
        if (data != nullptr && AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
#endif
        if (data == nullptr) return;
        std::memset(data, 0, BlockBytes);
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), BlockBytes);
    }

    ~LowBlock() {
        if (data == nullptr) return;
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(data);
#if defined(_WIN32) || defined(__APPLE__)
        GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes);
#else
        munmap(data, BlockBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
#endif
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    bool Usable() const { return data != nullptr && Address() + BlockBytes <= GcnAddressLimit; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }
    std::uint64_t At(std::size_t offset) const { return Address() + offset; }
    std::uint8_t* Data(std::size_t offset) const { return data + offset; }

private:
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(std::uint64_t color, std::uint64_t cmask, std::uint64_t fmask, std::uint32_t samples, std::uint32_t fragments, std::uint32_t flags) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(color >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = 0u;
    cx[0x31c] = (Format8888 << 2u) | BlendClamp | flags;
    cx[0x31d] = Thin1D | (static_cast<std::uint32_t>(std::countr_zero(samples)) << 12u) | (static_cast<std::uint32_t>(std::countr_zero(fragments)) << 15u);
    cx[0x31f] = static_cast<std::uint32_t>(cmask >> 8u);
    cx[0x320] = static_cast<std::uint32_t>(CmaskBytes / 128u - 1u);
    cx[0x321] = static_cast<std::uint32_t>(fmask >> 8u);
    cx[0x322] = Width * Height / 64u - 1u;
    const auto target = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(target.format == VK_FORMAT_R8G8B8A8_UNORM && target.elementBytes == 4u && target.tileMode == AgcDriver::Graphics::ColorTileMode::GcnThin1D, "the 8_8_8_8 unorm GCN target did not decode as a thin 1D RGBA8 target");
    return target;
}

ShaderRecompiler::ShaderPixelStageInfo PixelInfo() {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    return pixel;
}

void Draw(AgcDriver::VulkanDevice& device, const LowBlock& block, const AgcDriver::Graphics::ColorTarget& color, const Triangle& triangle, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    std::memcpy(block.Data(Vertices), triangle.data(), sizeof(triangle));
    const auto address = block.At(Vertices);
    const std::vector<std::uint32_t> vertexUserData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(sizeof(triangle)), 0xfacu | (7u << 12u) | (14u << 15u)};
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, PixelInfo(), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    state.samples = color.samples;
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::vector<std::uint8_t> Resolve(AgcDriver::VulkanDevice& device, const LowBlock& block, const AgcDriver::Graphics::ColorTarget& source, std::size_t destination) {
    AgcDriver::Graphics::ColorMetadataPass pass{};
    pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::Resolve;
    pass.source = source;
    pass.targets = {DecodeTarget(block.At(destination), 0u, 0u, 1u, 1u, 0u)};
    device.ColorMetadataPass(pass);
    AgcDriver::Graphics::StorageTexture::FlushPending(block.At(destination), PlaneBytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(PlaneBytes);
    AgcDriver::GuestMemory::Read(block.At(destination), std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::string GuardRefusal(std::uint64_t address, std::size_t bytes) {
    try {
        AgcDriver::Graphics::RequireNoMultisampleColorTarget(address, bytes, "a test read");
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Usable()) {
            std::printf("skipped, no guest block below the 40-bit GCN color base range (0x%llx)\n", static_cast<unsigned long long>(block.Address()));
            return VulkanTestSkipped;
        }

        const auto eqaa = DecodeTarget(block.At(EqaaColor), block.At(EqaaCmask), block.At(EqaaFmask), Samples, 2u, Compression | FastClear);
        const auto msaa = DecodeTarget(block.At(MsaaColor), block.At(MsaaCmask), 0u, Samples, Samples, Compression | FastClear);
        Require(eqaa.samples == Samples && eqaa.eqaaFragments == 2u && msaa.samples == Samples && msaa.eqaaFragments == 0u, "the GCN EQAA and 4x targets are not drawn at their 4 coverage samples");
        Require(eqaa.fmaskAddress == block.At(EqaaFmask) && eqaa.gcnFmaskBytes == FmaskBytes, "the GCN EQAA target's FMASK is not its CB_COLOR_FMASK base with 2 bits for each of its 4 samples");

        const auto red = Program({1.0f, 0.0f, 0.0f, 1.0f});
        const auto green = Program({0.0f, 1.0f, 0.0f, 1.0f});
        for (const auto* target : {&eqaa, &msaa}) {
            Draw(*device, block, *target, Lower, red);
            Draw(*device, block, *target, Upper, green);
        }

        const auto fmaskRefusal = GuardRefusal(block.At(EqaaFmask), 256u);
        Require(fmaskRefusal.find("the FMASK of EQAA color target") != std::string::npos, "a read of the GCN EQAA target's FMASK was not refused by name: " + fmaskRefusal);
        Require(!GuardRefusal(block.At(EqaaFmask) + FmaskBytes - 1u, 1u).empty(), "the last byte of the GCN EQAA target's FMASK is not guarded");
        const auto pastFmask = GuardRefusal(block.At(EqaaFmask) + FmaskBytes, 256u);
        Require(pastFmask.empty(), "memory past the FMASK size CB_COLOR_FMASK_SLICE gives is guarded as part of the GCN EQAA target's FMASK: " + pastFmask);
        Require(!GuardRefusal(block.At(EqaaColor) + 2u * PlaneBytes - 1u, 1u).empty(), "the last byte of the GCN EQAA target's two fragments is not guarded");
        Require(GuardRefusal(block.At(EqaaColor) + 2u * PlaneBytes, 2u * PlaneBytes).empty(), "memory past the GCN EQAA target's two fragments is guarded as if it held four samples");

        const auto eqaaTexels = Resolve(*device, block, eqaa, EqaaResolved);
        const auto msaaTexels = Resolve(*device, block, msaa, MsaaResolved);
        Require(eqaaTexels == msaaTexels, "the CB resolve of the GCN 4-sample 2-fragment target differs from the 4-sample 4-fragment one drawn the same way");
        const auto quarter = std::count_if(eqaaTexels.begin(), eqaaTexels.end(), [](std::uint8_t value) { return (value >= 60u && value <= 68u) || (value >= 187u && value <= 195u); });
        Require(quarter > 0, "no edge pixel of the GCN EQAA resolve holds a quarter or three-quarter coverage value, so its 4 coverage samples were not kept");
        const auto covered = std::count(eqaaTexels.begin(), eqaaTexels.end(), std::uint8_t{255});
        Require(covered > 0 && covered < static_cast<std::ptrdiff_t>(eqaaTexels.size()), "the GCN EQAA resolve holds no fully covered or no uncovered texels");

        const auto oneFragment = DecodeTarget(block.At(OneFragmentColor), 0u, block.At(OneFragmentFmask), Samples, 1u, Compression);
        Require(oneFragment.samples == 1u && oneFragment.coverageSamples == Samples && oneFragment.gcnFmaskBytes == FmaskBytes, "the GCN 4-sample 1-fragment target is not one stored sample whose FMASK holds 1 bit for each of 8 samples");
        Draw(*device, block, oneFragment, Lower, red);
        const auto oneFragmentRefusal = GuardRefusal(block.At(OneFragmentFmask), 256u);
        Require(oneFragmentRefusal.find("one-fragment color target") != std::string::npos, "a read of the GCN one-fragment target's FMASK was not refused by name: " + oneFragmentRefusal);
        Require(!GuardRefusal(block.At(OneFragmentFmask) + FmaskBytes - 1u, 1u).empty(), "the last byte of the GCN one-fragment target's FMASK is not guarded");
        const auto pastOneFragment = GuardRefusal(block.At(OneFragmentFmask) + FmaskBytes, 256u);
        Require(pastOneFragment.empty(), "memory past the FMASK size CB_COLOR_FMASK_SLICE gives is guarded as part of the GCN one-fragment target's FMASK: " + pastOneFragment);

        std::printf("GCN EQAA color target tests passed: 4-sample 2-fragment draws resolve like 4x MSAA (%zu bytes), %td quarter-coverage channels, FMASK of EQAA and one-fragment targets guarded over the %zu bytes CB_COLOR_FMASK_SLICE gives\n", eqaaTexels.size(), quarter, FmaskBytes);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
