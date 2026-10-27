#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Words = 16;
constexpr std::uint32_t ResultWord = 4;
constexpr std::int32_t Width = 4;
constexpr std::int32_t Height = 3;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Entry = 2;
alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::uint8_t, 4096> Texels{};
alignas(256) std::array<std::uint32_t, 128> Table{};

alignas(256) constexpr std::array<std::uint32_t, 18> SampleCode{
    0x34020086, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xbf8c3f70, 0xf09c0f08, 0x00610802,
    0xbf8c3f70, 0xe0701010, 0x80000801, 0xe0701014, 0x80000901, 0xe0701018, 0x80000a01, 0xe070101c,
    0x80000b01, 0xbf810000,
};

struct Coordinate {
    float u;
    float v;
};

std::array<double, 4> TexelOf(std::int32_t x, std::int32_t y) {
    return {16.0 * x + 32.0, 48.0 * y + 16.0, 200.0, 255.0};
}

void FillTexture() {
    const auto mips = AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888UNorm, Width, Height, 1);
    Require(AgcDriver::Graphics::ComputeSurfaceSize(mips, 1) <= Texels.size(), "border color table: the texture does not fit its storage");
    std::fill(Texels.begin(), Texels.end(), std::uint8_t{0xeeu});
    for (std::uint32_t y = 0; y < mips[0].height; ++y) {
        for (std::uint32_t x = 0; x < mips[0].width; ++x) {
            auto* texel = &Texels[mips[0].tiledOffset + static_cast<std::uint64_t>(y) * mips[0].pitchBytes + x * 4u];
            const auto value = TexelOf(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
            for (std::uint32_t component = 0; component < 4u; ++component) texel[component] = static_cast<std::uint8_t>(value[component]);
        }
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor() {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Texels.data()));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | ((static_cast<std::uint32_t>(Width - 1) & 3u) << 30u),
        (static_cast<std::uint32_t>(Width - 1) >> 2u) | (static_cast<std::uint32_t>(Height - 1) << 14u),
        0xfacu | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> TableSampler(std::uint32_t entry) {
    return {0x000080b6u, 0u, 0u, (3u << 30u) | entry};
}

std::uint64_t TableBase() {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Table.data()));
}

void SetEntry(std::uint32_t entry, std::array<float, 4> color) {
    for (std::uint32_t component = 0; component < 4u; ++component) Table[entry * 4u + component] = std::bit_cast<std::uint32_t>(color[component]);
}

std::vector<Coordinate> Coordinates() {
    std::vector<Coordinate> result;
    for (std::int32_t y = -1; y <= Height; ++y) {
        for (std::int32_t x = -1; x <= Width; ++x) result.push_back({static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f});
    }
    return result;
}

std::vector<std::array<float, 4>> Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 4>& sampler, std::optional<std::uint64_t> table, const std::vector<Coordinate>& coordinates) {
    Require(coordinates.size() <= Threads, "border color table: too many coordinates for one dispatch");
    Buffer.fill(0xdeadbeefu);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto coordinate = lane < coordinates.size() ? coordinates[lane] : Coordinate{0.5f, 0.5f};
        Buffer[lane * Words] = std::bit_cast<std::uint32_t>(coordinate.u);
        Buffer[lane * Words + 1u] = std::bit_cast<std::uint32_t>(coordinate.v);
    }
    std::vector<std::uint32_t> userData(16, 0u);
    const auto buffer = BufferDescriptor(Buffer.data(), static_cast<std::uint32_t>(Buffer.size() * 4u));
    const auto texture = TextureDescriptor();
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code = SampleCode;
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
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()), nullptr, nullptr, std::nullopt, table);
    device.WaitIdle();
    std::vector<std::array<float, 4>> samples;
    for (std::uint32_t lane = 0; lane < coordinates.size(); ++lane) {
        std::array<float, 4> values{};
        for (std::uint32_t component = 0; component < 4u; ++component) values[component] = std::bit_cast<float>(Buffer[lane * Words + ResultWord + component]);
        samples.push_back(values);
    }
    return samples;
}

void Check(const char* what, const std::vector<Coordinate>& coordinates, const std::vector<std::array<float, 4>>& samples, std::array<float, 4> border) {
    Require(samples.size() == coordinates.size(), std::string(what) + ": a dispatch lost samples");
    for (std::size_t index = 0; index < coordinates.size(); ++index) {
        const auto x = static_cast<std::int32_t>(std::floor(coordinates[index].u));
        const auto y = static_cast<std::int32_t>(std::floor(coordinates[index].v));
        const bool outside = x < 0 || x >= Width || y < 0 || y >= Height;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const double expected = outside ? static_cast<double>(border[component]) : TexelOf(x, y)[component] / 255.0;
            const double value = samples[index][component];
            Require(std::fabs(value - expected) <= (outside ? 1.0 : 0.5) / 255.0, std::string(what) + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") component " + std::to_string(component) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
        }
    }
}

void ExpectFailure(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 4>& sampler, std::optional<std::uint64_t> table, std::string_view reason, const char* what) {
    const std::vector<Coordinate> one{{0.5f, 0.5f}};
    try {
        static_cast<void>(Run(device, sampler, table, one));
    } catch (const std::exception& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string(what) + ": unexpected error: " + error.what());
        return;
    }
    throw std::runtime_error(std::string(what) + " was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillTexture();
        const auto coordinates = Coordinates();
        SetEntry(0, {1.0f, 0.0f, 0.0f, 1.0f});
        SetEntry(1, {0.0f, 1.0f, 0.0f, 1.0f});
        SetEntry(Entry, {0.15f, 0.5f, 0.05f, 0.0f});
        SetEntry(3, {0.0f, 0.0f, 1.0f, 1.0f});
        std::vector<std::array<float, 4>> first;
        try {
            first = Run(*device, TableSampler(Entry), TableBase(), coordinates);
        } catch (const std::exception& error) {
            if (std::string_view(error.what()).find("VK_EXT_custom_border_color") == std::string_view::npos) throw;
            std::puts("skipped, the device has no VK_EXT_custom_border_color with customBorderColorWithoutFormat");
            return VulkanTestSkipped;
        }
        Check("table entry 2", coordinates, first, {0.15f, 0.5f, 0.05f, 0.0f});
        Check("table entry 0", coordinates, Run(*device, TableSampler(0), TableBase(), coordinates), {1.0f, 0.0f, 0.0f, 1.0f});
        SetEntry(Entry, {0.25f, 0.75f, 1.0f, 0.5f});
        Check("table entry 2 rewritten between dispatches", coordinates, Run(*device, TableSampler(Entry), TableBase(), coordinates), {0.25f, 0.75f, 1.0f, 0.5f});
        SetEntry(16u + Entry, {0.5f, 0.25f, 0.125f, 1.0f});
        Check("table base moved by 256 bytes", coordinates, Run(*device, TableSampler(Entry), TableBase() + 256u, coordinates), {0.5f, 0.25f, 0.125f, 1.0f});
        ExpectFailure(*device, TableSampler(Entry), std::nullopt, "table base register was never written", "a table border colour with no table base");
        std::puts("border color table tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
