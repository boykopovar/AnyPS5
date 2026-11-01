#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 48;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Slices = 4;
constexpr std::uint32_t FirstSlice = 1;
constexpr std::uint32_t Layers = Slices - FirstSlice;
constexpr std::uint32_t BandWidth = Width / Layers;
constexpr std::uint8_t Kept = 0x40;
constexpr std::uint32_t LayerExport = (1u << 18u) | (1u << 21u);
constexpr std::uint32_t ShaderViewportIndexLayerCapability = 5254;
constexpr std::uint32_t SetContextReg = 0x69u;
constexpr std::uint32_t SetShReg = 0x76u;
constexpr std::uint32_t SetUconfigReg = 0x79u;
constexpr std::uint32_t DrawIndexAuto = 0x2du;
constexpr std::uint32_t RenderTargetVolumeAttrib3 = (Slices - 1u) | (0x1bu << 14u) | (2u << 24u) | (1u << 27u);
constexpr std::size_t Block = 65536;

alignas(64) volatile std::uint32_t EndOfPipe = 0;

alignas(256) constexpr std::array<std::uint32_t, 10> VertexCode{
    0xe0382000, 0x80020005, 0xe030201c, 0x80020405, 0xbf8c3f70, 0xf80000cf, 0x03020100, 0xf80008d4, 0x00040000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 4> PixelCode{
    0x7e0e02f2, 0xf800180f, 0x07070707, 0xbf810000,
};

struct Vertex {
    std::array<float, 4> position;
    std::array<float, 3> color;
    std::uint32_t layer;
};

alignas(256) std::array<Vertex, 6 * Layers> Vertices{};

struct RegisteredShader {
    Shader shader;
    ShaderUserData userData;
};
RegisteredShader VertexShader{};
RegisteredShader PixelShader{};

const std::vector<std::pair<std::uint32_t, std::uint32_t>> ContextBaseline{
    {0x2d5, 0x2000},
    {0x1b6, 0}, {0x200, 0}, {0x203, 0x800},
    {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
    {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
    {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
    {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
    {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
    {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
    {0x31d, 0}, {0x1e0, 0},
    {0xc, 0},
    {0x81, 0x80000000}, {0x90, 0x80000000}, {0x94, 0x80000000},
    {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0}, {0x113, std::bit_cast<std::uint32_t>(1.0f)},
};

std::byte* AllocateWatched(std::size_t bytes) {
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, Block);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) throw std::runtime_error("cannot map the watched volume");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + Block - 1) & ~static_cast<std::uintptr_t>(Block - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + Block) munmap(reinterpret_cast<void*>(aligned + bytes), begin + Block - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    return static_cast<std::byte*>(block);
}

void Register(RegisteredShader& registered, const void* code, std::uint32_t bytes, std::uint8_t type) {
    registered.shader.file_header = 0x34333231u;
    registered.shader.version = 0x18u;
    registered.shader.user_data = &registered.userData;
    registered.shader.code = code;
    registered.shader.header_size = sizeof(RegisteredShader);
    registered.shader.shader_size = bytes;
    registered.shader.type = type;
    AgcDriverRegisterShader_nid_postfix(&registered.shader);
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

void SetRegister(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::uint32_t offset, std::uint32_t value) {
    words.push_back(0xc0010000u | (opcode << 8u));
    words.push_back(offset);
    words.push_back(value);
}

void BuildBands() {
    for (std::uint32_t layer = 0; layer < Layers; ++layer) {
        const float left = -1.0f + 2.0f * static_cast<float>(layer) / Layers;
        const float right = -1.0f + 2.0f * static_cast<float>(layer + 1u) / Layers;
        const std::array<std::array<float, 2>, 6> corners{{{left, -1.0f}, {right, -1.0f}, {left, 1.0f}, {right, -1.0f}, {right, 1.0f}, {left, 1.0f}}};
        for (std::uint32_t k = 0; k < corners.size(); ++k) Vertices[6u * layer + k] = {{corners[k][0], corners[k][1], 0.5f, 1.0f}, {1.0f, 1.0f, 1.0f}, layer};
    }
}

AgcDriver::Graphics::ColorTarget VolumeTarget(std::byte* volume) {
    AgcDriver::Graphics::ColorTarget color{reinterpret_cast<std::uintptr_t>(volume), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, 0, 0xe4u};
    color.tileMode = ColorTileMode::RenderTarget;
    color.elementBytes = 4;
    color.depth = Slices;
    color.depthSlice = FirstSlice;
    color.layers = Layers;
    color.bytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::SurfaceForTarget(color)).guestBytes;
    return color;
}

std::vector<std::uint32_t> Frame(const std::byte* volume, const std::byte* keys) {
    const auto colorAddress = reinterpret_cast<std::uintptr_t>(volume);
    const auto vertexAddress = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixelAddress = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    std::vector<std::uint32_t> words;
    for (const auto& [offset, value] : ContextBaseline) SetRegister(words, SetContextReg, offset, value);
    SetRegister(words, SetContextReg, 0x207, LayerExport);
    SetRegister(words, SetContextReg, 0x318, static_cast<std::uint32_t>(colorAddress >> 8u));
    SetRegister(words, SetContextReg, 0x390, static_cast<std::uint32_t>(colorAddress >> 40u));
    SetRegister(words, SetContextReg, 0x31b, FirstSlice | ((Slices - 1u) << 13u));
    SetRegister(words, SetContextReg, 0x3b0, ((Width - 1u) << 14u) | (Height - 1u));
    SetRegister(words, SetContextReg, 0x3b8, RenderTargetVolumeAttrib3);
    if (keys != nullptr) {
        const auto keyAddress = reinterpret_cast<std::uintptr_t>(keys);
        SetRegister(words, SetContextReg, 0x31c, 0x28028u | 0x10000000u);
        SetRegister(words, SetContextReg, 0x325, static_cast<std::uint32_t>(keyAddress >> 8u));
        SetRegister(words, SetContextReg, 0x3a8, static_cast<std::uint32_t>(keyAddress >> 40u));
    }
    SetRegister(words, SetContextReg, 0x10f, std::bit_cast<std::uint32_t>(Width / 2.0f));
    SetRegister(words, SetContextReg, 0x110, std::bit_cast<std::uint32_t>(Width / 2.0f));
    SetRegister(words, SetContextReg, 0x111, std::bit_cast<std::uint32_t>(-(Height / 2.0f)));
    SetRegister(words, SetContextReg, 0x112, std::bit_cast<std::uint32_t>(Height / 2.0f));
    const auto extent = (Height << 16u) | Width;
    SetRegister(words, SetContextReg, 0xd, extent);
    SetRegister(words, SetContextReg, 0x82, extent);
    SetRegister(words, SetContextReg, 0x91, extent);
    SetRegister(words, SetContextReg, 0x95, extent);
    SetRegister(words, SetUconfigReg, 0x242, 4u);
    SetRegister(words, SetShReg, 0x008, static_cast<std::uint32_t>(pixelAddress >> 8u));
    SetRegister(words, SetShReg, 0x009, static_cast<std::uint32_t>(pixelAddress >> 40u));
    SetRegister(words, SetShReg, 0x00b, 0u);
    SetRegister(words, SetShReg, 0x0c8, static_cast<std::uint32_t>(vertexAddress >> 8u));
    SetRegister(words, SetShReg, 0x0c9, static_cast<std::uint32_t>(vertexAddress >> 40u));
    SetRegister(words, SetShReg, 0x08b, 4u << 1u);
    const auto descriptor = BufferDescriptor(Vertices.data(), sizeof(Vertex), static_cast<std::uint32_t>(Vertices.size()));
    for (std::uint32_t i = 0; i < descriptor.size(); ++i) SetRegister(words, SetShReg, 0x08c + i, descriptor[i]);
    words.insert(words.end(), {0xc0000000u | (1u << 16u) | (DrawIndexAuto << 8u), static_cast<std::uint32_t>(Vertices.size()), 2u});
    const auto labelAddress = reinterpret_cast<std::uintptr_t>(&EndOfPipe);
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(static_cast<std::uint64_t>(labelAddress) >> 32u), 1u, 0u, 0u});
    return words;
}

std::uint64_t BlockOffset(const AgcDriver::Graphics::TextureSwizzleEquation& equation, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    std::uint64_t offset = 0;
    for (std::uint32_t bit = 0; bit < equation.bits.size(); ++bit) {
        const auto mask = equation.bits[bit];
        const auto selected = (x & (mask & 0xfffu)) ^ ((y << 12u) & (mask & 0xfff000u)) ^ ((z << 24u) & (mask & 0xff000000u));
        offset |= static_cast<std::uint64_t>(std::popcount(selected) & 1u) << bit;
    }
    return offset;
}

void Check(const std::vector<std::byte>& volume, const AgcDriver::Graphics::ColorTarget& color, std::uint8_t undrawn, const std::string& what) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::SurfaceForTarget(color));
    Require(!geometry.thick, "a 64 KiB render-target volume is not stored as thin slices");
    const auto* equation = AgcDriver::Graphics::FindTextureSwizzleEquation(static_cast<std::uint32_t>(ColorTileMode::RenderTarget), 4);
    Require(equation != nullptr, "missing the 64 KiB R_X swizzle equation");
    for (std::uint32_t slice = 0; slice < Slices; ++slice) {
        const auto* base = volume.data() + geometry.GuestLayerOffset(slice);
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const bool drawn = slice >= FirstSlice && x / BandWidth == slice - FirstSlice;
                const auto expected = drawn ? std::uint8_t{255} : undrawn;
                const auto* texel = base + BlockOffset(*equation, x, y, slice);
                for (std::uint32_t channel = 0; channel < 4; ++channel) {
                    const auto actual = std::to_integer<std::uint8_t>(texel[channel]);
                    Require(actual == expected, what + ": slice " + std::to_string(slice) + " texel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected) + (drawn ? " (the band of layer " + std::to_string(slice - FirstSlice) + ")" : slice < FirstSlice ? " (outside the color view)" : " (another layer's band)"));
                }
            }
        }
    }
}

bool DeviceRoutesLayers() {
    const auto device = OpenVulkanTestDevice();
    if (!device) return false;
    const auto target = device->Target();
    return std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), ShaderViewportIndexLayerCapability) != target.supportedCapabilities.end();
}

void Run(const char* what, std::optional<std::uint8_t> key) {
    const auto bytes = (VolumeTarget(nullptr).bytes + Block - 1u) & ~(Block - 1u);
    auto* volume = AllocateWatched(bytes);
    std::memset(volume, static_cast<int>(Kept), bytes);
    const auto color = VolumeTarget(volume);
    const auto keyBytes = static_cast<std::size_t>(color.bytes / 256u);
    std::byte* keys = nullptr;
    if (key) {
        keys = AllocateWatched(Block);
        std::memset(keys, *key, keyBytes);
    }
    EndOfPipe = 0;
    auto words = Frame(volume, keys);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, std::string(what) + ": the DCB submission failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(EndOfPipe == 1u, std::string(what) + ": the end-of-pipe label was not written");
    std::vector<std::byte> stored(bytes);
    AgcDriver::GuestMemory::Read(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(volume)), stored);
    Check(stored, color, key == std::uint8_t{0x00} ? std::uint8_t{0} : Kept, what);
    if (keys != nullptr) {
        std::vector<std::byte> storedKeys(keyBytes);
        AgcDriver::GuestMemory::Read(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(keys)), storedKeys);
        const auto uncompressed = static_cast<std::size_t>(std::count(storedKeys.begin(), storedKeys.end(), std::byte{0xff}));
        Require(uncompressed == keyBytes, std::string(what) + ": " + std::to_string(uncompressed) + " of " + std::to_string(keyBytes) + " DCC key bytes read uncompressed after the draw");
    }
}

}

int main() {
    try {
        if (!DeviceRoutesLayers()) {
            std::puts("skipped, no Vulkan device with VK_EXT_shader_viewport_index_layer");
            return VulkanTestSkipped;
        }
        BuildBands();
        Register(VertexShader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelShader, PixelCode.data(), sizeof(PixelCode), 1);
        Run("layered volume draw through the driver", std::nullopt);
        Run("layered volume draw through the driver under uncompressed DCC keys", std::uint8_t{0xff});
        Run("layered volume draw through the driver under 0000 fast-clear DCC keys", std::uint8_t{0x00});
        AgcDriverShutdown_nid_postfix();
        std::puts("layered volume draws through the driver passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
