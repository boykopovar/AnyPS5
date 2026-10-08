#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 512;
constexpr std::uint32_t Height = 256;
constexpr std::uint32_t ScissorWidth = 256;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t SurfaceBytes = 2 * BlockBytes;
constexpr std::size_t KeyBytes = SurfaceBytes / 256;
constexpr std::size_t LinearBytes = std::size_t{Width} * Height * 4;
constexpr std::uint32_t Srgb8Info = 0x8604u;
constexpr std::uint32_t Rgba8Info = 0x8028u;
constexpr std::uint32_t DccEnable = 0x10000000u;
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint8_t ClearCode = 0x5d;
constexpr std::uint8_t Kept = 0x40;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 64>, 8> Programs{};
std::size_t programCount = 0;
alignas(256) std::array<std::uint8_t, KeyBytes> Keys{};

double Eotf(std::uint32_t code) {
    const double encoded = code / 255.0;
    return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

int Oetf(double linear) {
    linear = std::clamp(linear, 0.0, 1.0);
    const double encoded = linear <= 0.0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
    return static_cast<int>(std::lround(encoded * 255.0));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::span<const std::uint32_t> Program(std::array<float, 4> srgb8, bool rgba8) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    const auto move = [&](std::uint32_t vgpr, float value) {
        code[at++] = 0x7e0002ffu | (vgpr << 17u);
        code[at++] = std::bit_cast<std::uint32_t>(value);
    };
    for (std::uint32_t channel = 0; channel < 4; ++channel) move(4 + channel, srgb8[channel]);
    if (rgba8) {
        for (std::uint32_t channel = 0; channel < 4; ++channel) move(8 + channel, channel % 2 == 0 ? 1.0f : 0.0f);
        code[at++] = 0xf800000fu;
        code[at++] = 0x0b0a0908u;
        code[at++] = 0xf800181fu;
    } else {
        code[at++] = 0xf800180fu;
    }
    code[at++] = 0x07060504u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes), watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map the color target");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "cannot allocate the color target");
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "the color target is not write-watched");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        } else {
            std::free(data);
        }
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    bool watched;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(std::uint32_t slot, const Block& block, std::uint32_t info, std::uint32_t attrib3, std::uint32_t clear = 0) {
    const auto stride = slot * 0xfu;
    AgcDriver::Registers cx;
    cx[0x318 + stride] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b + stride] = 0;
    cx[0x31c + stride] = info;
    cx[0x31d + stride] = 0;
    cx[0x323 + stride] = clear;
    cx[0x324 + stride] = 0;
    cx[0x3b0 + slot] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8 + slot] = attrib3;
    cx[0x390 + slot] = static_cast<std::uint32_t>(block.Address() >> 40u);
    if ((info & DccEnable) != 0) {
        const auto keys = reinterpret_cast<std::uintptr_t>(Keys.data());
        cx[0x325 + stride] = static_cast<std::uint32_t>(keys >> 8u);
        cx[0x3a8 + slot] = static_cast<std::uint32_t>(keys >> 40u);
    }
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, slot);
    color.exportIndex = slot;
    return color;
}

VkPipelineColorBlendAttachmentState Blend(bool enable, VkBlendFactor source, VkBlendFactor destination) {
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = enable ? VK_TRUE : VK_FALSE;
    blend.srcColorBlendFactor = source;
    blend.dstColorBlendFactor = destination;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = source;
    blend.dstAlphaBlendFactor = destination;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = 0xf;
    return blend;
}

const VkPipelineColorBlendAttachmentState BlendOff = Blend(false, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);
const VkPipelineColorBlendAttachmentState Keep = Blend(true, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE);
const VkPipelineColorBlendAttachmentState Multiply = Blend(true, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC_COLOR);

void Draw(AgcDriver::VulkanDevice& device, std::vector<AgcDriver::Graphics::ColorTarget> colors, std::vector<VkPipelineColorBlendAttachmentState> blends, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangle.data(), 16u, static_cast<std::uint32_t>(Triangle.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {waveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    for (std::size_t index = 0; index < colors.size(); ++index) pixel.targetOutputMode.at(index) = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        {0, 0, vertexPush, 128 - vertexPush}
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, vertexPush}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = std::move(colors);
    state.color = state.colors.front();
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {ScissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = std::move(blends);
    state.blend = state.blends.front();
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

std::vector<std::uint8_t> ReadBack(AgcDriver::VulkanDevice& device, std::uint64_t address, std::size_t bytes) {
    AgcDriver::Graphics::StorageTexture::FlushPending(address, bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(bytes);
    AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

void ExpectSurface(AgcDriver::VulkanDevice& device, const Block& block, const std::vector<std::uint8_t>& before, const std::function<int(std::uint8_t)>& drawn, int tolerance, const std::string& what) {
    const auto stored = ReadBack(device, block.Address(), SurfaceBytes);
    for (std::size_t offset = 0; offset < SurfaceBytes; ++offset) {
        const bool inside = offset < BlockBytes;
        const int expected = inside ? drawn(before[offset]) : before[offset];
        const int actual = stored[offset];
        if (std::abs(actual - expected) <= (inside ? tolerance : 0)) continue;
        throw std::runtime_error(what + ": byte " + std::to_string(offset) + (inside ? " inside" : " outside") + " the scissor is " + std::to_string(actual) + ", expected " + std::to_string(expected) + " from " + std::to_string(before[offset]));
    }
}

std::vector<std::uint8_t> Fill(Block& block) {
    for (std::size_t offset = 0; offset < block.bytes; ++offset) block.data[offset] = static_cast<std::uint8_t>((offset * 7u + offset / 256u) & 0xffu);
    return std::vector<std::uint8_t>(block.data, block.data + block.bytes);
}

void RecordedDrawTests(AgcDriver::VulkanDevice& device, Block& block) {
    auto before = Fill(block);
    const auto color = DecodeTarget(0, block, Srgb8Info, TiledAttrib3);
    Require(color.format == VK_FORMAT_R8_SRGB && color.elementBytes == 1 && color.bytes == SurfaceBytes, "the 8_SRGB target did not decode as a 512x256 R8_SRGB surface");
    const auto keep = Program({0.0f, 0.25f, 0.5f, 0.75f}, false);
    Draw(device, {color}, {Keep}, keep);
    ExpectSurface(device, block, before, [](std::uint8_t code) { return code; }, 0, "a ZERO/ONE blend into an 8_SRGB target");
    before = ReadBack(device, block.Address(), SurfaceBytes);
    constexpr float factor = 0.6f;
    Draw(device, {color}, {Multiply}, Program({factor, 0.25f, 0.5f, 0.75f}, false));
    ExpectSurface(device, block, before, [](std::uint8_t code) { return Oetf(Eotf(code) * factor); }, 1, "a multiply blend into an 8_SRGB target");
    before = ReadBack(device, block.Address(), SurfaceBytes);
    const auto written = static_cast<float>(Eotf(150));
    Draw(device, {color}, {BlendOff}, Program({written, 0.25f, 0.5f, 0.75f}, false));
    Draw(device, {color}, {Multiply}, Program({0.5f, 0.25f, 0.5f, 0.75f}, false));
    ExpectSurface(device, block, before, [&](std::uint8_t) { return Oetf(static_cast<double>(written) * 0.5); }, 1, "a write and a multiply blend in one pass over an 8_SRGB target");
    before = ReadBack(device, block.Address(), SurfaceBytes);
    Draw(device, {color}, {BlendOff}, Program({written, 0.25f, 0.5f, 0.75f}, false));
    Draw(device, {color}, {Blend(true, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_DST_ALPHA)}, Program({0.5f, 0.25f, 0.5f, 0.75f}, false));
    ExpectSurface(device, block, before, [](std::uint8_t) { return 150; }, 1, "a destination-alpha blend after a write in one pass over an 8_SRGB target");
}

void ClearTests(AgcDriver::VulkanDevice& device, Block& block) {
    Fill(block);
    Keys.fill(0x20);
    const auto color = DecodeTarget(0, block, Srgb8Info | DccEnable, TiledAttrib3, ClearCode);
    Require(color.dccAddress == reinterpret_cast<std::uintptr_t>(Keys.data()), "the 8_SRGB target lost its DCC keys");
    const auto written = static_cast<float>(Eotf(200));
    Draw(device, {color}, {BlendOff}, Program({written, 0.25f, 0.5f, 0.75f}, false));
    const std::vector<std::uint8_t> cleared(SurfaceBytes, ClearCode);
    ExpectSurface(device, block, cleared, [](std::uint8_t) { return 200; }, 1, "a draw over a fast-cleared 8_SRGB target");
    std::array<std::uint8_t, KeyBytes> keys{};
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(Keys.data()), std::as_writable_bytes(std::span(keys)), 1);
    Require(std::all_of(keys.begin(), keys.end(), [](std::uint8_t key) { return key == 0xff; }), "the drawn 8_SRGB target kept compressed DCC keys");
}

void SynchronousDrawTests(AgcDriver::VulkanDevice& device, Block& linear, Block& block) {
    Require(AgcDriver::Graphics::ColorTargetLayout(Width, Height, AgcDriver::Graphics::ColorTileMode::Linear, 4).Bytes() == LinearBytes, "the linear RGBA8 target is padded");
    std::memset(linear.data, Kept, LinearBytes);
    const auto before = Fill(block);
    const auto rgba8 = DecodeTarget(0, linear, Rgba8Info, LinearAttrib3);
    const auto srgb8 = DecodeTarget(1, block, Srgb8Info, TiledAttrib3);
    constexpr float factor = 0.35f;
    Draw(device, {rgba8, srgb8}, {BlendOff, Multiply}, Program({factor, 0.25f, 0.5f, 0.75f}, true));
    ExpectSurface(device, block, before, [](std::uint8_t code) { return Oetf(Eotf(code) * factor); }, 1, "a multiply blend into an 8_SRGB target beside a linear target");
    const auto pixels = ReadBack(device, linear.Address(), LinearBytes);
    for (std::size_t offset = 0; offset < LinearBytes; ++offset) {
        const bool inside = (offset / 4u) % Width < ScissorWidth;
        const std::uint8_t expected = inside ? (offset % 2u == 0 ? 255 : 0) : Kept;
        Require(pixels[offset] == expected, "the linear RGBA8 target beside an 8_SRGB target holds " + std::to_string(pixels[offset]) + " at byte " + std::to_string(offset) + ", expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Block recorded(SurfaceBytes);
        Block cleared(SurfaceBytes);
        Block linear(LinearBytes);
        Block synchronous(SurfaceBytes);
        RecordedDrawTests(*device, recorded);
        ClearTests(*device, cleared);
        SynchronousDrawTests(*device, linear, synchronous);
        std::puts("8_SRGB color target tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
