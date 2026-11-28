#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t TwoChannelSwizzle = 0x22cu;
constexpr std::size_t TexelBytes = 8192;
constexpr std::size_t BlockBytes = 65536;

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::byte, 16384> Pixels{};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 12> StoreCode{
    0x34020084, 0xe0381000, 0x80001401, 0xbf8c3f70, 0x7e3c0300, 0x7e3e0280, 0xf0201f08, 0x0001141e,
    0x7e3e0281, 0xf0201508, 0x0001141e, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image store unorm rounding: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};


struct Format {
    const char* name;
    std::uint32_t format;
    std::uint32_t components;
    std::uint32_t bits;
    bool snorm;
};

constexpr std::array<Format, 5> Formats{{
    {"8_UNORM", 1u, 1u, 8u, false},
    {"16_16_UNORM", 23u, 2u, 16u, false},
    {"8_8_8_8_UNORM", 56u, 4u, 8u, false},
    {"8_8_8_8_SNORM", 57u, 4u, 8u, true},
    {"16_16_16_16_UNORM", 65u, 4u, 16u, false},
}};

struct Case {
    Format format;
    std::uint32_t value;
    std::uint32_t expected;
};

constexpr Format Unorm8{"8_UNORM", 1u, 1u, 8u, false};
constexpr Format Unorm16{"16_UNORM", 7u, 1u, 16u, false};
constexpr Format Snorm8{"8_SNORM", 2u, 1u, 8u, true};
constexpr Format Snorm16{"16_SNORM", 8u, 1u, 16u, true};
constexpr Format Float32{"32_FLOAT", 22u, 1u, 32u, false};

constexpr std::array<Case, 9> Cases{{
    {Unorm8, 0x3dcccccdu, 0x1au},
    {Unorm8, 0x3c909091u, 0x05u},
    {Unorm16, 0x3e99999au, 0x4ccdu},
    {Snorm8, 0xbefbf7f0u, 0xc1u},
    {Snorm8, 0x3d112245u, 0x05u},
    {Snorm16, 0x3c004101u, 0x0101u},
    {Float32, 0x3dcccccdu, 0x3dcccccdu},
    {Float32, 0x7fc12345u, 0x7fc12345u},
    {Float32, 0x00012345u, 0x00012345u},
}};

struct Write {
    std::uint32_t row;
    std::uint32_t dmask;
};

constexpr std::array<Write, 2> StoreWrites{{{0u, 0xfu}, {1u, 0x5u}}};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

float Scale(const Format& format) {
    return static_cast<float>((1u << (format.snorm ? format.bits - 1u : format.bits)) - 1u);
}

float Value(const Format& format, std::uint32_t tid, std::uint32_t index) {
    constexpr std::array<float, 16> Edges{0.0f, -0.0f, 1.0f, 0.5f, -0.25f, 1.5f, -1.5f, 1e-30f, -1e-30f, 0.999f, 0.0019607844f, 0.9980392f, 0.33333334f, -0.6666667f, 2e-5f, 0.75f};
    const auto slot = tid * 4u + index;
    if (slot < Edges.size()) return Edges[slot];
    if (slot == Edges.size()) return std::numeric_limits<float>::quiet_NaN();
    if (slot == Edges.size() + 1u) return std::numeric_limits<float>::infinity();
    if (slot == Edges.size() + 2u) return -std::numeric_limits<float>::infinity();
    const auto scale = Scale(format);
    const auto level = static_cast<float>((slot * 37u) % static_cast<std::uint32_t>(scale));
    auto value = (level + 0.5f) / scale;
    for (std::uint32_t step = 0; step < slot % 5u; ++step) value = std::nextafter(value, slot % 2u == 0u ? 2.0f : -2.0f);
    return format.snorm && slot % 3u == 0u ? -value : value;
}

std::uint32_t Level(const Format& format, float value) {
    const auto lower = format.snorm ? -1.0f : 0.0f;
    const auto clamped = std::isnan(value) ? 0.0f : std::clamp(value, lower, 1.0f);
    const auto product = static_cast<double>(clamped) * static_cast<double>(Scale(format));
    const auto level = static_cast<std::int32_t>(std::nearbyint(product));
    return static_cast<std::uint32_t>(level) & ((1u << format.bits) - 1u);
}

std::uint32_t TexelBytesOf(const Format& format) {
    return format.components * format.bits / 8u;
}

std::uint32_t SwizzleOf(const Format& format) {
    return format.components == 2u ? TwoChannelSwizzle : IdentitySwizzle;
}

AgcDriver::Graphics::TileMipLayout Mip(const Format& format) {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.format, Width, Height, 1u).at(0);
}

std::uint64_t TexelOffset(const Format& format, std::uint32_t x, std::uint32_t y) {
    const auto mip = Mip(format);
    return mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * TexelBytesOf(format);
}

std::vector<std::uint8_t> Initial() {
    std::vector<std::uint8_t> bytes(TexelBytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(0x80u + 37u * index + 11u * (index >> 4u));
    }
    return bytes;
}

std::uint32_t StoredComponent(const Format& format, std::uint32_t tid, std::uint32_t dmask, std::uint32_t component) {
    if (((dmask >> component) & 1u) == 0u) return 0u;
    std::uint32_t index = 0;
    for (std::uint32_t lower = 0; lower < component; ++lower) index += (dmask >> lower) & 1u;
    return Level(format, Value(format, tid, index));
}

std::vector<std::uint8_t> Expected(const Format& format) {
    auto bytes = Initial();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (const auto& write : StoreWrites) {
            auto* texel = bytes.data() + TexelOffset(format, tid, write.row);
            for (std::uint32_t component = 0; component < format.components; ++component) {
                const auto value = StoredComponent(format, tid, write.dmask, component);
                std::memcpy(texel + component * format.bits / 8u, &value, format.bits / 8u);
            }
        }
    }
    return bytes;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t format, std::uint32_t swizzle) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        swizzle | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint8_t* texels, std::span<const std::uint32_t> code, std::uint32_t format, std::uint32_t swizzle, const std::array<std::uint32_t, 4>& buffer) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, format, swizzle);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(!result.registrationModule, "image store unorm rounding: the compute shader was emitted in registration form");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

void Store(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t index = 0; index < 4u; ++index) Input[tid * 4u + index] = std::bit_cast<std::uint32_t>(Value(format, tid, index));
    }
    const auto initial = Initial();
    std::copy(initial.begin(), initial.end(), texels);
    Dispatch(device, texels, StoreCode, format.format, SwizzleOf(format), BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u)));
}

std::array<std::uint32_t, 4> VertexBufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::vector<std::uint32_t> CasePixelCode(std::uint32_t value) {
    return {0x7e3c0f00u, 0x7e3e0f01u, 0x7e2802ffu, value, 0x7e2a02ffu, value, 0x7e2c02ffu, value, 0x7e2e02ffu, value, 0xf0201f08u, 0x0001141eu, 0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
}

void StoreCase(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Case& entry) {
    Input.fill(entry.value);
    const auto initial = Initial();
    std::copy(initial.begin(), initial.end(), texels);
    Dispatch(device, texels, StoreCode, entry.format.format, IdentitySwizzle, BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u)));
}

void DrawCase(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Case& entry) {
    static const std::vector<std::array<float, 4>> triangles{
        {-1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
        {1.0f, -1.0f, 0.5f, 1.0f}, {1.0f, 1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
    };
    const auto& target = device.Target();
    Pixels.fill(std::byte{0});
    const auto initial = Initial();
    std::copy(initial.begin(), initial.end(), texels);
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = VertexBufferDescriptor(triangles.data(), 16u, static_cast<std::uint32_t>(triangles.size()));
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.wave32 = false;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(16, 0u);
    const auto texture = TextureDescriptor(texels, entry.format.format, IdentitySwizzle);
    std::copy(texture.begin(), texture.end(), pixelUserData.begin() + 4);
    const auto code = CasePixelCode(entry.value);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    Require(pixelResult.registrationModule == target.registrationModules, std::string("image store unorm rounding: the pixel shader was ") + (target.registrationModules ? "not " : "") + "emitted in registration form");
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    const AgcDriver::Graphics::ColorTargetLayout colorLayout(Width, Height, AgcDriver::Graphics::ColorTileMode::Linear, 4u);
    Require(colorLayout.Bytes() <= Pixels.size(), "image store unorm rounding: the color target does not fit its buffer");
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, colorLayout.Bytes(), 0xe4u};
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(triangles.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), TexelBytes, nullptr, "test");
    device.WaitIdle();
}

std::string CaseMismatch(const std::uint8_t* texels, const Case& entry, std::uint32_t rows, const std::string& shader) {
    const auto initial = Initial();
    const auto bytes = entry.format.bits / 8u;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = TexelOffset(entry.format, x, y);
            std::uint32_t actual = 0;
            std::uint32_t wanted = entry.expected;
            std::memcpy(&actual, texels + offset, bytes);
            if (y >= rows) std::memcpy(&wanted, initial.data() + offset, bytes);
            if (actual != wanted) return std::string("image_store of ") + entry.format.name + " value " + Hex(entry.value) + " (" + shader + "): texel (" + std::to_string(x) + ", " + std::to_string(y) + ") is " + Hex(actual) + ", expected " + Hex(wanted);
        }
    }
    return {};
}

void CheckTexels(const std::uint8_t* texels, const Format& format) {
    const auto expected = Expected(format);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = TexelOffset(format, x, y);
            for (std::uint32_t component = 0; component < format.components; ++component) {
                std::uint32_t actual = 0;
                std::uint32_t wanted = 0;
                std::memcpy(&actual, texels + offset + component * format.bits / 8u, format.bits / 8u);
                std::memcpy(&wanted, expected.data() + offset + component * format.bits / 8u, format.bits / 8u);
                Require(actual == wanted, std::string("image_store of ") + format.name + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") component " + std::to_string(component) + " is " + Hex(actual) + ", expected " + Hex(wanted) + " (level nearest to the exact product)");
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        auto* texels = block.Data();
        for (const auto& format : Formats) {
            Store(*device, texels, format);
            CheckTexels(texels, format);
        }
        const std::string pixelShader = device->Target().registrationModules ? "pixel shader, registration module" : "pixel shader, specialized module";
        std::string failures;
        for (const auto& entry : Cases) {
            StoreCase(*device, texels, entry);
            const auto compute = CaseMismatch(texels, entry, 2u, "compute shader, specialized module");
            DrawCase(*device, texels, entry);
            const auto pixel = CaseMismatch(texels, entry, Height, pixelShader);
            for (const auto& failure : {compute, pixel}) {
                if (!failure.empty()) failures += (failures.empty() ? "" : "\n") + failure;
            }
        }
        Require(failures.empty(), failures);
        std::printf("image store unorm rounding tests passed (compute shader, specialized module; %s)\n", pixelShader.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
