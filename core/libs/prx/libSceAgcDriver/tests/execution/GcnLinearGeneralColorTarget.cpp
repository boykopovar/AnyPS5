#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
using Triangle = std::array<std::array<float, 4>, 3>;

constexpr std::uint32_t Width = 72;
constexpr std::uint32_t Height = 64;
constexpr std::uint32_t ElementBytes = 4;
constexpr std::uint32_t Format8888 = 10u;
constexpr std::uint32_t BlendClamp = 1u << 15u;
constexpr std::uint32_t LinearGeneral = 1u << 7u;
constexpr std::uint32_t Thin2D = 14u;
constexpr std::size_t SurfaceBytes = std::size_t{Width} * Height * ElementBytes;
constexpr std::size_t Guard = 256;
constexpr std::size_t Vertices = 0x20000;
constexpr std::size_t BlockBytes = 0x30000;
constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;
constexpr std::array<std::uint32_t, 2> ByteOffsets{0x00u, 0x44u};
constexpr std::uint8_t GuardByte = 0xa5u;
constexpr std::uint8_t DrawnBlue = 0x60u;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000u, 0x80000000u, 0xbf8c0f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

constexpr Triangle Lower{{{-1.0f, -1.0f, 0.5f, 1.0f}, {0.9f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.7f, 0.5f, 1.0f}}};

alignas(256) std::array<std::uint32_t, 12> PixelCode{};

std::span<const std::uint32_t> Program() {
    constexpr std::uint32_t inverse255 = 0x3b808081u;
    PixelCode = {
        0x7e084900u, 0x7e0a4901u,
        0x100808ffu, inverse255, 0x100a0affu, inverse255,
        0x7e0c02ffu, std::bit_cast<std::uint32_t>(static_cast<float>(DrawnBlue) / 255.0f),
        0x7e0e02f2u,
        0xf800180fu, 0x07060504u,
        0xbf810000u,
    };
    return PixelCode;
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

std::array<std::uint8_t, 4> Background(std::uint32_t x, std::uint32_t y) {
    return {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 200u, 0u};
}

std::array<std::uint8_t, 4> Drawn(std::uint32_t x, std::uint32_t y) {
    return {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), DrawnBlue, 255u};
}

AgcDriver::Graphics::ColorTarget DecodeTarget(std::uint64_t base, std::uint32_t byteOffset) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(base >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = byteOffset;
    cx[0x31c] = (Format8888 << 2u) | BlendClamp | LinearGeneral;
    cx[0x31d] = Thin2D;
    const auto target = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(target.format == VK_FORMAT_R8G8B8A8_UNORM && target.samples == 1u && target.tileMode == AgcDriver::Graphics::ColorTileMode::GcnLinearGeneral && target.address == base + byteOffset && target.bytes == SurfaceBytes && target.extent.width == Width && target.extent.height == Height, "the linear-general GCN target did not decode as an unpadded 72x64 RGBA8 surface at CB_COLOR_BASE plus SLICE_START's low byte");
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

void Draw(AgcDriver::VulkanDevice& device, const LowBlock& block, const AgcDriver::Graphics::ColorTarget& color, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    std::memcpy(block.Data(Vertices), Lower.data(), sizeof(Lower));
    const auto address = block.At(Vertices);
    const std::vector<std::uint32_t> vertexUserData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(sizeof(Lower)), 0xfacu | (7u << 12u) | (14u << 15u)};
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
    state.samples = 1u;
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Lower.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::string Describe(std::uint32_t byteOffset, std::uint32_t x, std::uint32_t y, const std::uint8_t* texel) {
    char text[192];
    std::snprintf(text, sizeof(text), "byte offset 0x%02x: the texel at (%u, %u) of the unpadded rows holds %02x %02x %02x %02x", byteOffset, x, y, texel[0], texel[1], texel[2], texel[3]);
    return text;
}

std::size_t CheckTarget(AgcDriver::VulkanDevice& device, const LowBlock& block, std::size_t offset, std::uint32_t byteOffset) {
    const auto base = block.At(offset);
    std::vector<std::uint8_t> guest(Guard + SurfaceBytes + Guard, GuardByte);
    const auto texelAt = [&](std::uint32_t x, std::uint32_t y) { return guest.data() + Guard + byteOffset + (static_cast<std::size_t>(y) * Width + x) * ElementBytes; };
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) std::memcpy(texelAt(x, y), Background(x, y).data(), ElementBytes);
    }
    const auto window = base - Guard;
    AgcDriver::GuestMemory::Write(window, std::as_bytes(std::span(guest)), 256);
    Draw(device, block, DecodeTarget(base, byteOffset), Program());
    AgcDriver::Graphics::StorageTexture::FlushPending(window, guest.size(), nullptr, "test read-back");
    device.WaitIdle();
    AgcDriver::GuestMemory::Read(window, std::as_writable_bytes(std::span(guest)), 256);
    for (std::size_t index = 0; index < Guard + byteOffset; ++index) Require(guest[index] == GuardByte, "byte offset " + std::to_string(byteOffset) + ": the draw wrote " + std::to_string(Guard + byteOffset - index) + " bytes before the target");
    for (std::size_t index = Guard + byteOffset + SurfaceBytes; index < guest.size(); ++index) Require(guest[index] == GuardByte, "byte offset " + std::to_string(byteOffset) + ": the draw wrote past the target's last unpadded row");
    std::size_t drawn = 0;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto* texel = texelAt(x, y);
            const bool kept = std::memcmp(texel, Background(x, y).data(), ElementBytes) == 0;
            const bool written = std::memcmp(texel, Drawn(x, y).data(), ElementBytes) == 0;
            Require(kept || written, Describe(byteOffset, x, y, texel) + ", neither its background nor the color drawn for it");
            drawn += written ? 1u : 0u;
        }
    }
    constexpr auto texels = std::size_t{Width} * Height;
    Require(drawn > texels / 4u && drawn < texels * 3u / 4u, "byte offset " + std::to_string(byteOffset) + ": the triangle covers " + std::to_string(drawn) + " of " + std::to_string(texels) + " texels");
    return drawn;
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
        std::string counts;
        for (std::size_t index = 0; index < ByteOffsets.size(); ++index) {
            const auto drawn = CheckTarget(*device, block, Guard + index * 0x8000u, ByteOffsets[index]);
            counts += (index == 0 ? "" : ", ") + std::to_string(drawn);
        }
        std::printf("GCN linear-general color target tests passed: a triangle drawn into an unpadded 72-texel-pitch RGBA8 target at CB_COLOR_BASE and at CB_COLOR_BASE + 0x44 lands in row-major texels (%s texels) and leaves every other texel and the bytes around the target unchanged\n", counts.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
