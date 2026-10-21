#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type3D = 10;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::uint32_t BlueGreenRedAlpha = 0xf2eu;
constexpr std::array<float, 20> Positions{-9.3f, -4.8f, -1.3f, -0.7f, -0.2f, 0.3f, 1.4985f, 1.6f, 3.499f, 3.7f,
                                          -1.3f, -0.7f, -0.2f, -0.5015f, 0.8f, 1.6f, 0.3f, 1.9f, -0.3f, -1.6f};
constexpr std::array<float, 8> Extremes{3.0e6f, -3.0e6f, 1.0e9f, -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), 65536.3f, -1000.3f};
constexpr std::array<std::uint32_t, 32> WideColumns{
    0x3f4412b9u, 0x3f78caa4u, 0x3dbebd8cu, 0x3f1fa5e0u, 0x3f6f111au, 0x3f656d67u, 0x3f03ec82u, 0x3f6869bcu, 0x3f6b91beu, 0x3f305e22u, 0x3f75a2a2u,
    0x3f0dbbe2u, 0x3eb2b6a3u, 0x3dc6ee00u, 0x3f4d5f12u, 0x3f69f2d2u, 0x3dc59097u, 0x3f050867u, 0x3e74d30bu, 0x3f436405u, 0x3f5924b9u, 0x3e4a2c04u,
    0x3eac3af2u, 0x3eb0277fu, 0x3ebd4a8eu, 0x3f67d0deu, 0x3ec6acbeu, 0x3f588bdbu, 0x3f4c84b1u, 0x3f62f419u, 0x3f452e9eu, 0x3f7e97a1u,
};
constexpr std::uint32_t WideWidth = 3001;
constexpr std::array<float, 8> Ties{127.5f / 2048.0f, -1.0f / 4096.0f, 0x1.0p-20f, -0.0f, -0x1.0p-149f, 255.5f / 1024.0f, 0x1.0p-149f, 383.5f / 1024.0f};
constexpr std::array<int, 20> PositionExtents{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, -1, -1};

alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<float, Threads * 4> Output{};
alignas(4096) std::array<std::uint8_t, 65536> Texels{};

alignas(256) constexpr std::array<std::uint32_t, 10> GatherRed{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf11c0110, 0x00820603, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 10> GatherBlue{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf11c0410, 0x00820603, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 10> GatherAlpha{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf11c0810, 0x00820603, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 14> GatherRedBesideSample{
    0x34020084, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf11c0110, 0x00820603, 0xf09c0110, 0x00a20a03,
    0xbf8c3f70, 0x10141480, 0x060c1506, 0xe0781000, 0x80010601, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 12> VertexGatherRed{
    0x34020a84, 0xe0381000, 0x80000201, 0xbf8c3f70, 0xf11c0110, 0x00820603, 0xbf8c3f70, 0xe0781000,
    0x80010601, 0xf80008cf, 0x09080706, 0xbf810000,
};
alignas(256) constexpr std::array<std::uint32_t, 5> PixelCode{0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};
constexpr std::uint32_t TargetExtent = 64u;
alignas(256) std::array<std::byte, TargetExtent * TargetExtent * 4u> Pixels{};

enum class Inputs { Positions, Ties };

struct Volume {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
    std::uint32_t baseLevel = 0;
    std::uint32_t lastLevel = 0;
    std::uint32_t swizzle = IdentitySwizzle;
};

struct SamplerState {
    std::array<std::uint32_t, 3> clamp;
    std::uint32_t border = 0;
    std::uint32_t extra0 = 0;
    std::uint32_t word1 = 0xfffu << 12u;
    std::uint32_t word2 = 0;
};

std::array<std::uint32_t, 3> Same(std::uint32_t clamp) {
    return {clamp, clamp, clamp};
}

struct Variant {
    std::span<const std::uint32_t> code;
    std::uint32_t channel;
    Volume volume;
    SamplerState sampler;
    std::string name;
    Inputs inputs = Inputs::Positions;
    bool vertex = false;
    bool null = false;
    SamplerState other{Same(2)};
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const Volume& volume) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (((volume.width - 1u) & 3u) << 30u),
        ((volume.width - 1u) >> 2u) | ((volume.height - 1u) << 14u),
        volume.swizzle | (volume.baseLevel << 12u) | (volume.lastLevel << 16u) | (Type3D << 28u),
        volume.depth - 1u,
        volume.lastLevel << 4u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> SamplerDescriptor(const SamplerState& sampler) {
    return {sampler.clamp[0] | (sampler.clamp[1] << 3u) | (sampler.clamp[2] << 6u) | sampler.extra0, sampler.word1, sampler.word2, sampler.border << 30u};
}

std::uint32_t LevelStart(const Volume& volume, std::uint32_t level) {
    std::uint32_t start = 0;
    for (std::uint32_t below = 0; below < level; ++below) {
        start += std::max(volume.width >> below, 1u) * std::max(volume.height >> below, 1u) * std::max(volume.depth >> below, 1u);
    }
    return start;
}

std::array<std::uint8_t, 4> Texel(const Volume& volume, std::uint32_t level, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    const auto width = std::max(volume.width >> level, 1u);
    const auto height = std::max(volume.height >> level, 1u);
    const auto id = LevelStart(volume, level) + x + width * y + width * height * z;
    return {static_cast<std::uint8_t>(id + 1u), 0x11, static_cast<std::uint8_t>(254u - id), static_cast<std::uint8_t>(id * 37u + 11u)};
}

void FillTexels(const Volume& volume) {
    const auto geometry = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(volume)));
    Require(geometry.guestBytes <= Texels.size(), "image gather volume: the volume does not fit the texel storage");
    Texels.fill(0);
    for (std::uint32_t level = 0; level <= volume.lastLevel; ++level) {
        const auto& mip = geometry.mips.at(level);
        for (std::uint32_t z = 0; z < std::max(volume.depth >> level, 1u); ++z) {
            for (std::uint32_t y = 0; y < mip.height; ++y) {
                for (std::uint32_t x = 0; x < mip.width; ++x) {
                    const auto value = Texel(volume, level, x, y, z);
                    std::copy(value.begin(), value.end(), &Texels[geometry.GuestLayerOffset(z) + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + x * 4u]);
                }
            }
        }
    }
}

float Position(std::uint32_t index, std::uint32_t extent) {
    return Positions[index] + static_cast<float>(PositionExtents[index] * static_cast<int>(extent));
}

void FillInput(const Volume& volume, Inputs inputs) {
    const auto width = std::max(volume.width >> volume.baseLevel, 1u);
    const auto height = std::max(volume.height >> volume.baseLevel, 1u);
    const auto depth = std::max(volume.depth >> volume.baseLevel, 1u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * 4u + 0u] = 0u;
        Input[tid * 4u + 1u] = std::bit_cast<std::uint32_t>(Position(tid % 20u, width) / static_cast<float>(width));
        Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(Position((tid * 7u + 3u) % 20u, height) / static_cast<float>(height));
        Input[tid * 4u + 3u] = std::bit_cast<std::uint32_t>((Position((tid * 11u + 5u) % 20u, depth) + 0.5f) / static_cast<float>(depth));
        if (inputs == Inputs::Ties) {
            for (std::uint32_t axis = 0; axis < 3u; ++axis) Input[tid * 4u + 1u + axis] = std::bit_cast<std::uint32_t>(Ties[(tid + axis * 3u + tid / Ties.size()) % Ties.size()]);
        } else if (volume.width == WideWidth) {
            Input[tid * 4u + 1u] = WideColumns[tid];
            Input[tid * 4u + 2u] = std::bit_cast<std::uint32_t>(0.25f);
            Input[tid * 4u + 3u] = std::bit_cast<std::uint32_t>(0.25f);
        } else if (tid >= Threads - Extremes.size()) {
            Input[tid * 4u + 1u + tid % 3u] = std::bit_cast<std::uint32_t>(Extremes[tid - (Threads - Extremes.size())]);
        }
    }
}

int Fold(int index) {
    return index < 0 ? -1 - index : index;
}

std::optional<int> Single(int index, int extent, std::uint32_t clamp) {
    if (clamp == 0u) return ((index % extent) + extent) % extent;
    if (clamp == 1u) {
        const int phase = ((index % (2 * extent)) + 2 * extent) % (2 * extent);
        return phase < extent ? phase : 2 * extent - 1 - phase;
    }
    const int source = (clamp & 1u) != 0u ? Fold(index) : index;
    if (clamp < 6u) return std::clamp(source, 0, extent - 1);
    if (source < 0 || source >= extent) return std::nullopt;
    return source;
}

std::array<std::optional<int>, 2> Footprint(int first, int extent, std::uint32_t clamp) {
    if ((clamp & 6u) != 4u) return {Single(first, extent, clamp), Single(first + 1, extent, clamp)};
    const std::array<int, 2> sources{clamp == 5u ? Fold(first) : first, clamp == 5u ? Fold(first + 1) : first + 1};
    const int low = std::min(sources[0], sources[1]);
    const int pinned = std::clamp(low, -1, extent - 1);
    std::array<std::optional<int>, 2> result;
    for (std::size_t index = 0; index < 2u; ++index) {
        const int effective = pinned + sources[index] - low;
        if (effective >= 0 && effective < extent) result[index] = effective;
    }
    return result;
}

float Reduced(float coordinate, std::uint32_t clamp) {
    if (std::isnan(coordinate)) return 0.0f;
    if (std::isinf(coordinate)) coordinate = coordinate < 0.0f ? -4.0f : 4.0f;
    if (clamp == 0u) return coordinate - std::floor(coordinate);
    if (clamp == 1u) return coordinate - 2.0f * std::floor(coordinate * 0.5f);
    return std::clamp(coordinate, -4.0f, 4.0f);
}

int Snapped(float coordinate, int extent, int offset, std::uint32_t clamp) {
    const auto steps = static_cast<long long>(std::floor(static_cast<double>(Reduced(coordinate, clamp)) * extent * 256.0 + 0.5)) - offset;
    return static_cast<int>(steps >= 0 ? steps / 256 : -((-steps + 255) / 256));
}

std::uint32_t Channel(const Variant& variant, const std::array<std::uint8_t, 4>& texel) {
    const auto select = (variant.volume.swizzle >> (3u * variant.channel)) & 7u;
    return texel.at(select - 4u);
}

std::uint32_t BorderValue(const Variant& variant) {
    if (variant.sampler.border == 2u) return 255u;
    return variant.sampler.border == 1u && variant.channel == 3u ? 255u : 0u;
}

std::array<std::uint32_t, 4> Expected(std::uint32_t tid, const Variant& variant) {
    const auto& volume = variant.volume;
    const auto level = volume.baseLevel;
    const auto width = static_cast<int>(std::max(volume.width >> level, 1u));
    const auto height = static_cast<int>(std::max(volume.height >> level, 1u));
    const auto depth = static_cast<int>(std::max(volume.depth >> level, 1u));
    const float u = std::bit_cast<float>(Input[tid * 4u + 1u]);
    const float v = std::bit_cast<float>(Input[tid * 4u + 2u]);
    const float r = std::bit_cast<float>(Input[tid * 4u + 3u]);
    const auto& clamp = variant.sampler.clamp;
    const auto columns = Footprint(Snapped(u, width, 128, clamp[0]), width, clamp[0]);
    const auto rows = Footprint(Snapped(v, height, 128, clamp[1]), height, clamp[1]);
    const auto slice = Single(Snapped(r, depth, 0, clamp[2]), depth, (clamp[2] & 6u) == 4u ? (clamp[2] & 1u) | 2u : clamp[2]);
    const auto value = [&](std::size_t column, std::size_t row) {
        if (!columns[column] || !rows[row] || !slice) return BorderValue(variant);
        return Channel(variant, Texel(volume, level, static_cast<std::uint32_t>(*columns[column]), static_cast<std::uint32_t>(*rows[row]), static_cast<std::uint32_t>(*slice)));
    };
    return {value(0, 1), value(1, 1), value(1, 0), value(0, 0)};
}

std::vector<std::uint32_t> UserData(const Variant& variant) {
    std::vector<std::uint32_t> userData(24, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto texture = variant.null ? std::array<std::uint32_t, 8>{} : TextureDescriptor(variant.volume);
    const auto sampler = SamplerDescriptor(variant.sampler);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 16);
    const auto other = SamplerDescriptor(variant.other);
    std::copy(other.begin(), other.end(), userData.begin() + 20);
    return userData;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const Variant& variant) {
    const auto userData = UserData(variant);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(variant.code.data()), std::as_bytes(variant.code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(variant.code.data()), variant.code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Draw(AgcDriver::VulkanDevice& device, const Variant& variant) {
    const auto userData = UserData(variant);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(variant.code.data()), std::as_bytes(variant.code)}}};
    auto target = device.Target();
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(variant.code.data()), variant.code, 0, {}},
        {32, 0, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, memory},
        target,
        {0, 0, 0, 128}
    };
    vertexRequest.useCache = false;
    const auto vertex = ShaderRecompiler::Recompile(vertexRequest);
    Require(vertex.registrationModule == target.registrationModules, variant.name + ": the vertex shader was not emitted in the device's module form");
    const auto vertexPush = static_cast<std::uint32_t>(vertex.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
    pixelInfo.wave32 = true;
    pixelInfo.targetOutputMode[0] = 9u;
    pixelInfo.targetExportMapping.fill(0xe4u);
    ShaderRecompiler::RecompileRequest pixelRequest{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(PixelCode.data()), PixelCode, 0, {}},
        {32, 0, {}, std::nullopt, pixelInfo, std::nullopt, {}},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto pixel = ShaderRecompiler::Recompile(pixelRequest);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &pixel, PixelPushOffset(vertexPush, target)}}};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(Pixels.data()), {TargetExtent, TargetExtent}, VK_FORMAT_R8G8B8A8_UNORM, Pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {TargetExtent, TargetExtent};
    state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    state.viewport = {0.0f, static_cast<float>(TargetExtent), static_cast<float>(TargetExtent), -static_cast<float>(TargetExtent), 0.0f, 1.0f};
    state.scissor = {{0, 0}, {TargetExtent, TargetExtent}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15u;
    state.blends = {state.blend};
    const AgcDriver::Pm4::DrawParameters draw{0u, Threads, 0u, 1u, 0u, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

void Dispatch(AgcDriver::VulkanDevice& device, const Variant& variant) {
    FillTexels(variant.volume);
    FillInput(variant.volume, variant.inputs);
    Output.fill(-1.0f);
    if (variant.vertex) {
        Draw(device, variant);
        return;
    }
    const auto result = Compile(device, variant);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(variant.code.data()));
    device.WaitIdle();
}

void Run(AgcDriver::VulkanDevice& device, const Variant& variant) {
    Dispatch(device, variant);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = Expected(tid, variant);
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const float scaled = Output[tid * 4u + component] * 255.0f;
            Require(std::fabs(scaled - std::round(scaled)) < 1e-3f && static_cast<std::uint32_t>(std::lround(scaled)) == expected[component], variant.name + ": thread " + std::to_string(tid) + " component " + std::to_string(component) + " is " + std::to_string(scaled) + "/255, expected " + std::to_string(expected[component]) + "/255");
        }
    }
}

void RunNull(AgcDriver::VulkanDevice& device, Variant variant) {
    variant.null = true;
    Dispatch(device, variant);
    for (std::uint32_t index = 0; index < Output.size(); ++index) {
        Require(Output[index] == 0.0f, variant.name + ": value " + std::to_string(index) + " is " + std::to_string(Output[index]) + ", expected 0");
    }
}

void RunThrows(AgcDriver::VulkanDevice& device, const Variant& variant, const std::string& message) {
    try {
        Dispatch(device, variant);
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(message) != std::string::npos, variant.name + ": threw '" + error.what() + "', expected '" + message + "'");
        return;
    }
    throw std::runtime_error(variant.name + ": did not throw '" + message + "'");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const Volume cube{8, 4, 4};
        const Volume odd{7, 5, 3};
        const char* clampNames[8] = {"wrap", "mirror", "clamp to edge", "mirror once to edge", "clamp to half border", "mirror once to half border", "clamp to border", "mirror once to border"};
        const char* borderNames[3] = {"transparent black", "opaque black", "opaque white"};
        for (std::uint32_t clamp = 0; clamp < 8u; ++clamp) {
            for (std::uint32_t border = 0; border < (clamp >= 4u ? 3u : 1u); ++border) {
                const auto suffix = std::string(clampNames[clamp]) + (clamp >= 4u ? std::string(", ") + borderNames[border] : std::string());
                Run(*device, {GatherRed, 0, cube, {Same(clamp), border}, "3D gather of red, " + suffix});
                Run(*device, {GatherAlpha, 3, cube, {Same(clamp), border}, "3D gather of alpha, " + suffix});
                Run(*device, {GatherBlue, 2, odd, {Same(clamp), border}, "3D gather of blue from a 7x5x3 volume, " + suffix});
                Run(*device, {GatherRed, 0, cube, {Same(clamp), border}, "3D gather of red at rounding ties and denormals, " + suffix, Inputs::Ties});
            }
        }
        const std::array<std::array<std::uint32_t, 3>, 6> mixed{{{0, 6, 1}, {6, 1, 0}, {1, 0, 7}, {2, 2, 4}, {3, 5, 5}, {4, 7, 2}}};
        for (const auto& clamps : mixed) {
            const auto suffix = std::string(clampNames[clamps[0]]) + " / " + clampNames[clamps[1]] + " / " + clampNames[clamps[2]];
            Run(*device, {GatherRed, 0, cube, {clamps, 2}, "3D gather of red with per-axis modes " + suffix});
            Run(*device, {GatherBlue, 2, odd, {clamps, 1}, "3D gather of blue from a 7x5x3 volume with per-axis modes " + suffix});
            Run(*device, {VertexGatherRed, 0, cube, {clamps, 2}, "vertex shader 3D gather of red with per-axis modes " + suffix, Inputs::Positions, true});
        }
        Run(*device, {VertexGatherRed, 0, odd, {Same(5), 2}, "vertex shader 3D gather of red from a 7x5x3 volume, mirror once to half border", Inputs::Positions, true});
        Run(*device, {GatherRed, 0, {8, 4, 4, 0, 0, BlueGreenRedAlpha}, {{2, 2, 4}}, "3D gather of red through a blue-green-red-alpha swizzle with half border only on z"});
        Run(*device, {GatherRed, 0, {8, 4, 4, 0, 0, BlueGreenRedAlpha}, {{2, 3, 5}}, "3D gather of red through a blue-green-red-alpha swizzle with mirror once to half border only on z"});
        Run(*device, {GatherRedBesideSample, 0, cube, {Same(0)}, "3D gather of red, wrap, beside a 3D sample through a clamp to border sampler", Inputs::Positions, false, false, {Same(6), 2}});
        Run(*device, {GatherRedBesideSample, 0, {8, 4, 4, 0, 0, BlueGreenRedAlpha}, {Same(1)}, "3D gather of red through a swizzle, mirror, beside a 3D sample through a clamp to border sampler", Inputs::Positions, false, false, {Same(7), 1}});
        const SamplerState lodState{Same(2), 0, 4u << 9u, 0x100u | (0xfffu << 12u), 0x100u | (1u << 20u) | (1u << 22u) | (2u << 26u)};
        Run(*device, {GatherRed, 0, {8, 4, 4, 0, 1}, lodState, "3D gather of red ignores the sampler's LOD clamp, bias, filters and anisotropy"});
        Run(*device, {GatherRed, 0, {8, 4, 4, 1, 1}, {Same(0)}, "3D gather of red reads the base level"});
        Run(*device, {GatherRed, 0, {WideWidth, 2, 2}, {Same(2)}, "3D gather of red from a 3001 texel wide volume at 1/256 texel boundaries"});
        Run(*device, {GatherRed, 0, {8, 4, 4, 0, 0, BlueGreenRedAlpha}, {Same(1)}, "3D gather of red through a blue-green-red-alpha swizzle"});
        RunThrows(*device, {GatherRed, 0, cube, {Same(6), 3}, "3D gather with a border color table"}, "a 3D gather with a border color table is not implemented");
        RunThrows(*device, {GatherRed, 0, {8, 4, 4, 0, 0, BlueGreenRedAlpha}, {Same(6), 2}, "3D gather of a swizzled volume with border addressing"}, "a 3D gather of a swizzled texture with border addressing is not implemented");
        RunNull(*device, {GatherRed, 0, cube, {Same(0)}, "3D gather of red from a null image descriptor, wrap"});
        RunNull(*device, {VertexGatherRed, 0, cube, {Same(1)}, "vertex shader 3D gather of red from a null image descriptor, mirror", Inputs::Positions, true});
        std::puts("image 3D gather tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
