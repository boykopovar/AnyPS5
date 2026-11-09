#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 131072;
constexpr std::uint32_t Width = 2048;
constexpr std::uint32_t Slots = Width + 64;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::array<std::uint32_t, 4> PointSampler{0x92u, 0xfff000u, 0u, 0u};

struct Format {
    std::uint32_t id;
    bool normalized;
    std::array<std::uint32_t, 4> widths;
    std::uint32_t components;
};

constexpr std::array<Format, 6> Formats{{
    {31u, true, {11u, 11u, 10u, 0u}, 3u}, {35u, false, {11u, 11u, 10u, 0u}, 3u},
    {38u, true, {10u, 11u, 11u, 0u}, 3u}, {42u, false, {10u, 11u, 11u, 0u}, 3u},
    {45u, true, {2u, 10u, 10u, 10u}, 4u}, {49u, false, {2u, 10u, 10u, 10u}, 4u},
}};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        bytes = static_cast<std::uint8_t*>(std::aligned_alloc(65536u, BlockBytes));
#endif
        Require(bytes != nullptr, "packed signed image allocation failed");
        GuestAllocations::Mutation().Add(bytes, BlockBytes, true, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(bytes);
#ifdef _WIN32
        VirtualFree(bytes, 0, MEM_RELEASE);
#else
        std::free(bytes);
#endif
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint32_t* Texels() { return reinterpret_cast<std::uint32_t*>(bytes + 4096u); }
    std::uint32_t* Input() { return reinterpret_cast<std::uint32_t*>(bytes + 16384u); }
    std::uint32_t* Output() { return reinterpret_cast<std::uint32_t*>(bytes + 65536u); }
private:
    std::uint8_t* bytes;
};

std::string Hex(std::uint32_t value) {
    char result[16];
    std::snprintf(result, sizeof(result), "0x%08x", value);
    return result;
}

std::int32_t SignedField(std::uint32_t value, std::uint32_t bits) {
    const auto field = value & ((1u << bits) - 1u);
    return (field & (1u << (bits - 1u))) != 0u ? static_cast<std::int32_t>(field) - static_cast<std::int32_t>(1u << bits) : static_cast<std::int32_t>(field);
}

std::uint32_t Half(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto magnitude = bits & 0x7fffffffu;
    if (magnitude == 0u) return bits >> 16u;
    Require(magnitude >= 0x38800000u && magnitude <= 0x3f800000u, "packed signed normalized model is outside normal half range");
    auto result = ((bits >> 16u) & 0x8000u) | ((magnitude - 0x38000000u) >> 13u);
    const auto remainder = magnitude & 0x1fffu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (result & 1u) != 0u)) ++result;
    return result;
}

std::array<std::uint32_t, 4> Buffer(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> Texture(const void* data, const Format& format, std::uint32_t swizzle) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (format.id << 20u) | (((Width - 1u) & 3u) << 30u),
        (Width - 1u) >> 2u, swizzle | (9u << 28u), 0u, 0u, 0u, 0u};
}

void Dispatch(AgcDriver::VulkanDevice& device, GuestBlock& block, const Format& format, bool store, bool half, std::uint32_t swizzle, std::uint32_t dmask, bool sample = false) {
    std::fill_n(block.Output(), Slots * 4u, Sentinel);
    std::vector<std::uint32_t> code{0x7e080210u, 0x343c0885u, 0x4a3c3d00u, 0x34063c84u};
    if (store) code.insert(code.end(), {0xe0381000u, 0x80030a03u, 0xbf8c3f70u});
    else for (std::uint32_t reg = 10u; reg < 14u; ++reg) code.insert(code.end(), {0x7e0002ffu | (reg << 17u), Sentinel});
    code.insert(code.end(), {0x7e3e0280u});
    code.insert(code.end(), {(sample ? 0xf09c0008u : store ? 0xf0201008u : 0xf0001008u) | (dmask << 8u),
        (sample ? 0x00610a1eu : 0x00010a1eu) | (half ? 0x80000000u : 0u), 0xbf8c3f70u,
        0xe0781000u, 0x80000a03u, 0xbf810000u});
    std::vector<std::uint32_t> userData(16u);
    const auto output = Buffer(block.Output(), Slots * 16u);
    const auto image = Texture(block.Texels(), format, swizzle);
    const auto input = Buffer(block.Input(), Slots * 16u);
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(image.begin(), image.end(), userData.begin() + 4u);
    if (sample) std::copy(PointSampler.begin(), PointSampler.end(), userData.begin() + 12u);
    else std::copy(input.begin(), input.end(), userData.begin() + 12u);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32u, 1u, 1u}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
        {32u, 0, userData, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0, 0, 0, 128}};
    request.useCache = false;
    const auto shader = ShaderRecompiler::Recompile(request);
    device.Dispatch(shader, Width / 32u + (store ? 1u : 0u), 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    if (store) {
        AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(block.Texels()), Slots * 4u, nullptr, "packed signed test");
        device.WaitIdle();
    }
}

void CheckLoads(AgcDriver::VulkanDevice& device, GuestBlock& block, const Format& format, bool half, bool reverse, std::uint32_t dmask) {
    std::array<std::uint32_t, Width> original{};
    for (std::uint32_t lane = 0u; lane < Width; ++lane) {
        std::uint32_t word = 0u, offset = 0u;
        for (std::uint32_t c = 0u; c < format.components; ++c) {
            word |= ((lane * (2u * c + 1u) + c) & ((1u << format.widths[c]) - 1u)) << offset;
            offset += format.widths[c];
        }
        block.Texels()[lane] = word;
        original[lane] = word;
    }
    const auto swizzle = format.components == 3u ? (reverse ? 0x32eu : 0x3acu) : (reverse ? 0x977u : 0xfacu);
    Dispatch(device, block, format, false, half, swizzle, dmask);
    for (std::uint32_t lane = 0u; lane < Width; ++lane) {
        std::array<std::uint32_t, 4> fields{};
        std::uint32_t offset = 0u;
        for (std::uint32_t c = 0u; c < format.components; ++c) {
            const auto value = SignedField(original[lane] >> offset, format.widths[c]);
            const auto normalized = std::max(-1.0f, static_cast<float>(value) / static_cast<float>((1u << (format.widths[c] - 1u)) - 1u));
            fields[c] = format.normalized ? (half ? Half(normalized) : std::bit_cast<std::uint32_t>(normalized)) : static_cast<std::uint32_t>(value) & (half ? 0xffffu : 0xffffffffu);
            offset += format.widths[c];
        }
        std::array<std::uint32_t, 4> expected{Sentinel, Sentinel, Sentinel, Sentinel};
        if (half) for (std::uint32_t word = 0u; word < (std::popcount(dmask) + 1u) / 2u; ++word) expected[word] = 0u;
        std::uint32_t slot = 0u;
        for (std::uint32_t c = 0u; c < 4u; ++c) {
            if ((dmask & (1u << c)) == 0u) continue;
            const auto selector = (swizzle >> (3u * c)) & 7u;
            const auto value = selector == 1u ? (format.normalized ? (half ? 0x3c00u : 0x3f800000u) : 1u) : fields[selector - 4u];
            if (half) expected[slot / 2u] |= value << (16u * (slot % 2u));
            else expected[slot] = value;
            ++slot;
        }
        for (std::uint32_t c = 0u; c < 4u; ++c) {
            const auto actual = block.Output()[lane * 4u + c];
            const auto distance = actual > expected[c] ? actual - expected[c] : expected[c] - actual;
            const auto magnitude = expected[c] & 0x7fffffffu;
            Require(actual == expected[c] || (!half && format.normalized && magnitude != 0u && magnitude < 0x3f800000u && distance <= 3u),
                "signed packed load format " + std::to_string(format.id) + " D16 " + std::to_string(half) + " reversed " + std::to_string(reverse) + " lane " + std::to_string(lane) + " slot " + std::to_string(c) + " is " + Hex(actual) + ", expected " + Hex(expected[c]));
        }
    }
    Require(std::equal(original.begin(), original.end(), block.Texels()), "signed packed load changed input texels");
    Require(std::all_of(block.Output() + Width * 4u, block.Output() + Slots * 4u, [](auto value) { return value == Sentinel; }), "signed packed load overwrote the output guard");
}

float HalfValue(std::uint32_t half) {
    const auto sign = (half & 0x8000u) << 16u;
    const auto exponent = (half >> 10u) & 0x1fu;
    const auto mantissa = half & 0x3ffu;
    if (exponent == 31u) return std::bit_cast<float>(sign | 0x7f800000u | (mantissa << 13u));
    if (exponent != 0u) return std::bit_cast<float>(sign | ((exponent + 112u) << 23u) | (mantissa << 13u));
    return std::copysign(static_cast<float>(mantissa) * 0x1p-24f, std::bit_cast<float>(sign));
}

void CheckStores(AgcDriver::VulkanDevice& device, GuestBlock& block, const Format& format, bool half, bool partial, bool reverse) {
    constexpr std::array<std::int32_t, 24> integers{0, 1, -1, 511, -512, 1023, -1024, 32767, -32768, 2048, -2048, 37, INT32_MIN, INT32_MAX, -65536, 65536, -2, 2, 1022, -1023, 32768, -32769, 510, -511};
    constexpr std::array<std::uint32_t, 24> normalized{0u, 0x3f800000u, 0xbf800000u, 0x3f000000u, 0xbf000000u, 0x40000000u, 0xc0000000u, 0x80000000u, 0x3e800000u, 0xbe800000u, 0x3f400000u, 0xbf400000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u, 1u, 0x80000001u, 0x7f7fffffu, 0xff7fffffu, 0x00800000u, 0x80800000u, 0x3f7fffffu, 0xbf7fffffu};
    constexpr std::array<std::uint32_t, 24> halves{0u, 0x3c00u, 0xbc00u, 0x3800u, 0xb800u, 0x4000u, 0xc000u, 0x8000u, 0x3400u, 0xb400u, 0x3a00u, 0xba00u, 0x7c00u, 0xfc00u, 0x7e00u, 0xfe00u, 1u, 0x8001u, 0x7bffu, 0xfbffu, 0x0400u, 0x8400u, 0x3bffu, 0xbbffu};
    std::array<std::uint32_t, Width> expected{};
    std::fill_n(block.Texels() - 64u, Slots + 64u, 0xa5a5a5a5u);
    std::fill_n(block.Input(), Slots * 4u, 0u);
    const auto dmask = partial ? 5u : (1u << format.components) - 1u;
    const auto swizzle = format.components == 3u ? (reverse ? 0x32eu : 0x3acu) : (reverse ? 0x977u : 0xfacu);
    for (std::uint32_t lane = 0u; lane < Width + 32u; ++lane) {
        std::uint32_t source = 0u, word = 0u;
        for (std::uint32_t c = 0u; c < format.components; ++c) {
            const auto physical = ((swizzle >> (3u * c)) & 7u) - 4u;
            const auto bits = format.widths[physical];
            std::uint32_t offset = 0u;
            for (std::uint32_t i = 0u; i < physical; ++i) offset += format.widths[i];
            if ((dmask & (1u << c)) != 0u) {
                const auto index = (lane + c * 3u) % integers.size();
                const auto input = format.normalized ? (half ? halves[index] : normalized[index]) : static_cast<std::uint32_t>(integers[index]) & (half ? 0xffffu : 0xffffffffu);
                if (half) block.Input()[lane * 4u + source / 2u] |= input << (16u * (source % 2u));
                else block.Input()[lane * 4u + source] = input;
                ++source;
                const auto high = static_cast<std::int32_t>((1u << (bits - 1u)) - 1u);
                auto numeric = format.normalized ? static_cast<double>(half ? HalfValue(input) : std::bit_cast<float>(input)) : static_cast<double>(half ? SignedField(input, 16u) : integers[index]);
                if (std::isnan(numeric)) numeric = 0.0;
                const auto converted = format.normalized ? static_cast<std::int32_t>(std::nearbyint(std::clamp(numeric, -1.0, 1.0) * high)) : static_cast<std::int32_t>(std::clamp(numeric, static_cast<double>(-high - 1), static_cast<double>(high)));
                word |= (static_cast<std::uint32_t>(converted) & ((1u << bits) - 1u)) << offset;
            }
        }
        if (lane < Width) expected[lane] = word;
    }
    Dispatch(device, block, format, true, half, swizzle, dmask);
    Require(std::all_of(block.Texels() - 64u, block.Texels(), [](auto value) { return value == 0xa5a5a5a5u; }), "signed packed store overwrote the prefix guard");
    for (std::uint32_t lane = 0u; lane < Slots; ++lane) {
        const auto wanted = lane < Width ? expected[lane] : 0xa5a5a5a5u;
        Require(block.Texels()[lane] == wanted, "signed packed store format " + std::to_string(format.id) + " lane " + std::to_string(lane) + " is " + Hex(block.Texels()[lane]) + ", expected " + Hex(wanted));
    }
}

void CheckRefusals(AgcDriver::VulkanDevice& device, GuestBlock& block, const Format& format) {
    using namespace ShaderRecompiler;
    const auto swizzle = format.components == 3u ? 0x3acu : 0xfacu;
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.dimension = RdnaImageDimension::Dim2D;
    image.read = true;
    image.dimensionQueryOnly = false;
    DescriptorValue descriptor{Texture(block.Texels(), format, swizzle), 8u, 0xffu};
    const auto modes = ResourceMaterializer::RuntimeImageModes(image);
    Require(ResourceMaterializer::TryRuntimeImageMode(image, descriptor, modes).mode.has_value(), "direct signed packed image has no runtime mode");
    const auto refused = [&](const ImageResource& access) {
        const auto result = ResourceMaterializer::TryRuntimeImageMode(access, descriptor, modes);
        Require(!result.mode.has_value(), "unsupported signed packed access was accepted");
    };
    auto access = image;
    access.table = 0u;
    refused(access);
    access = image;
    access.packed = true;
    refused(access);
    access = image;
    access.srgbDecodeCompatible = false;
    refused(access);
    if (format.components == 3u) {
        descriptor.dwords[3] = (descriptor.dwords[3] & ~0xfffu) | 0xfacu;
        refused(image);
    }
    std::string reason;
    try {
        Dispatch(device, block, format, false, false, swizzle, 15u, true);
    } catch (const std::runtime_error& error) {
        reason = error.what();
    }
    Require(reason.find("converted signed images require direct unpacked image loads or stores") != std::string::npos, "signed packed sampling was not refused: " + reason);
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || argc == 2, "expected signed packed format id");
        const auto id = argc == 2 ? static_cast<std::uint32_t>(std::stoul(argv[1])) : 31u;
        const auto found = std::find_if(Formats.begin(), Formats.end(), [=](const Format& format) { return format.id == id; });
        Require(found != Formats.end(), "unknown signed packed format");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock block;
        for (const bool half : {false, true}) {
            for (const bool reverse : {false, true}) {
                for (const auto dmask : {15u, 5u, 2u}) CheckLoads(*device, block, *found, half, reverse, dmask);
                for (const bool partial : {false, true}) CheckStores(*device, block, *found, half, partial, reverse);
            }
        }
        CheckRefusals(*device, block, *found);
        std::printf("signed packed image format %u: load/store, D16, swizzle, clamps and guard checks passed\n", id);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
