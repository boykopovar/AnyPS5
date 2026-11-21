#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
using Triangle = std::array<std::array<float, 4>, 3>;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 64;
constexpr std::uint32_t Slices = 6;
constexpr std::uint32_t ElementBytes = 4;
constexpr std::uint32_t Format8888 = 10u;
constexpr std::uint32_t BlendClamp = 1u << 15u;
constexpr std::uint32_t Display2D = 10u;
constexpr std::uint32_t Thin2D = 14u;
constexpr std::size_t SliceBytes = std::size_t{Width} * Height * ElementBytes;
constexpr std::size_t ArrayBytes = SliceBytes * Slices;
constexpr std::size_t ArrayStride = 0x40000;
constexpr std::size_t Vertices = 2u * ArrayStride;
constexpr std::size_t BlockBytes = 3u * ArrayStride;
constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;
constexpr std::array<std::uint32_t, 4> DrawnSlices{0u, 1u, 2u, 5u};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000u, 0x80000000u, 0xbf8c0f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

constexpr Triangle Lower{{{-1.0f, -1.0f, 0.5f, 1.0f}, {0.9f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.7f, 0.5f, 1.0f}}};

std::uint8_t DrawnBlue(std::uint32_t slice) {
    return static_cast<std::uint8_t>(16u + 16u * slice);
}

alignas(256) std::array<std::array<std::uint32_t, 16>, DrawnSlices.size()> Programs{};

std::span<const std::uint32_t> Program(std::size_t index, std::uint32_t slice) {
    constexpr std::uint32_t inverse255 = 0x3b808081u;
    auto& code = Programs.at(index);
    code = {
        0x7e084900u, 0x7e0a4901u,
        0x100808ffu, inverse255, 0x100a0affu, inverse255,
        0x7e0c02ffu, std::bit_cast<std::uint32_t>(static_cast<float>(DrawnBlue(slice)) / 255.0f),
        0x7e0e02f2u,
        0xf800180fu, 0x07060504u,
        0xbf810000u,
    };
    return std::span<const std::uint32_t>(code.data(), 12);
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

struct ArrayLayout {
    AgcDriver::Graphics::GcnTileGeometry geometry;
    AgcDriver::Graphics::SurfaceGeometry surface;
};

ArrayLayout TexturePlacement(AgcDriver::Graphics::TextureTileMode mode, std::uint64_t base) {
    AgcDriver::Graphics::GuestTextureResource array{};
    array.baseAddress = base;
    array.width = Width;
    array.height = Height;
    array.depthOrLastArray = Slices - 1u;
    array.mipCount = 1u;
    array.allocatedMipCount = 1u;
    array.tileMode = mode;
    array.dimension = AgcDriver::Graphics::TextureDimension::k2DArray;
    array.format = 56u;
    array.gcnPitch = Width;
    ArrayLayout layout{AgcDriver::Graphics::GcnTiling(mode, ElementBytes), AgcDriver::Graphics::DescribeSurface(array)};
    Require(layout.surface.layers == Slices && layout.surface.guestBytes == ArrayBytes && !layout.surface.mips[0].gcnMicroTiled, "the texture path does not see the 2D-tiled GCN array as six whole macro-tiled slices");
    return layout;
}

std::size_t TextureOffset(const ArrayLayout& layout, std::uint32_t slice, std::uint32_t x, std::uint32_t y) {
    const auto& geometry = layout.geometry;
    const auto& mip = layout.surface.mips[0];
    std::uint32_t inBlock = 0;
    for (std::uint32_t bit = 0; bit < 16u; ++bit) inBlock |= static_cast<std::uint32_t>(std::popcount((x & geometry.equation[bit] & 0xfffu) ^ ((y << 12u) & geometry.equation[bit] & 0xfff000u)) & 1) << bit;
    const auto block = static_cast<std::uint64_t>(y / geometry.blockHeight) * mip.blocksPerRow + x / geometry.blockWidth;
    return static_cast<std::size_t>(layout.surface.GuestOffset(slice, mip) + block * geometry.blockBytes + (inBlock ^ AgcDriver::Graphics::GcnLayerBankXor(geometry, slice)));
}

std::array<std::uint8_t, 4> Background(std::uint32_t slice, std::uint32_t x, std::uint32_t y) {
    return {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), static_cast<std::uint8_t>(200u + slice), 0u};
}

std::array<std::uint8_t, 4> Drawn(std::uint32_t slice, std::uint32_t x, std::uint32_t y) {
    return {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), DrawnBlue(slice), 255u};
}

AgcDriver::Graphics::ColorTarget DecodeSlice(std::uint64_t base, std::uint32_t tileIndex, std::uint32_t slice) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(base >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = slice | (slice << 13u);
    cx[0x31c] = (Format8888 << 2u) | BlendClamp;
    cx[0x31d] = tileIndex;
    const auto target = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(target.format == VK_FORMAT_R8G8B8A8_UNORM && target.samples == 1u && target.address == base + slice * SliceBytes && target.bytes == SliceBytes, "a slice of the 2D-tiled GCN array did not decode as one RGBA8 slice at the array base plus its index");
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

std::string Describe(const char* mode, std::uint32_t slice, std::uint32_t x, std::uint32_t y, const std::uint8_t* texel) {
    char text[192];
    std::snprintf(text, sizeof(text), "%s slice %u: the texel the texture path places at (%u, %u) holds %02x %02x %02x %02x", mode, slice, x, y, texel[0], texel[1], texel[2], texel[3]);
    return text;
}

std::size_t CheckArray(AgcDriver::VulkanDevice& device, const LowBlock& block, std::size_t offset, std::uint32_t tileIndex, AgcDriver::Graphics::TextureTileMode mode, const char* name) {
    const auto base = block.At(offset);
    const auto layout = TexturePlacement(mode, base);
    std::vector<std::uint8_t> guest(ArrayBytes);
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) std::memcpy(guest.data() + TextureOffset(layout, slice, x, y), Background(slice, x, y).data(), ElementBytes);
        }
    }
    AgcDriver::GuestMemory::Write(base, std::as_bytes(std::span(guest)), 256);
    for (std::size_t index = 0; index < DrawnSlices.size(); ++index) {
        const auto slice = DrawnSlices[index];
        const auto target = DecodeSlice(base, tileIndex, slice);
        Require(target.pipeBankXor == AgcDriver::Graphics::GcnLayerBankXor(layout.geometry, slice), std::string(name) + ": the color target of slice " + std::to_string(slice) + " does not carry the bank rotation of its array layer");
        Draw(device, block, target, Program(index, slice));
    }
    AgcDriver::Graphics::StorageTexture::FlushPending(base, ArrayBytes, nullptr, "test read-back");
    device.WaitIdle();
    AgcDriver::GuestMemory::Read(base, std::as_writable_bytes(std::span(guest)), 256);
    std::size_t drawnTotal = 0;
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        const bool drawnSlice = std::ranges::find(DrawnSlices, slice) != DrawnSlices.end();
        std::size_t drawn = 0;
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto* texel = guest.data() + TextureOffset(layout, slice, x, y);
                const bool kept = std::memcmp(texel, Background(slice, x, y).data(), ElementBytes) == 0;
                const bool written = drawnSlice && std::memcmp(texel, Drawn(slice, x, y).data(), ElementBytes) == 0;
                Require(kept || written, Describe(name, slice, x, y, texel) + (drawnSlice ? ", neither its background nor the color drawn for it" : ", which no draw covered, but not its background"));
                drawn += written ? 1u : 0u;
            }
        }
        const auto texels = std::size_t{Width} * Height;
        Require(!drawnSlice || (drawn > texels / 4u && drawn < texels * 3u / 4u), std::string(name) + " slice " + std::to_string(slice) + ": the triangle covers " + std::to_string(drawn) + " of " + std::to_string(texels) + " texels");
        drawnTotal += drawn;
    }
    return drawnTotal;
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
        const auto thin = CheckArray(*device, block, 0u, Thin2D, AgcDriver::Graphics::TextureTileMode::kGcnThin2D, "Thin2D");
        const auto display = CheckArray(*device, block, ArrayStride, Display2D, AgcDriver::Graphics::TextureTileMode::kGcnDisplay2D, "Display2D");
        std::printf("GCN color array slice tests passed: triangles drawn into slices 0, 1, 2 and 5 of six-slice 2D-tiled arrays land where the texture path places those layers (%zu Thin2D and %zu Display2D texels), and every other texel keeps its background\n", thin, display);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
