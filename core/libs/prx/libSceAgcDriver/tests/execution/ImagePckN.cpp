#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 61;
constexpr std::uint32_t Height = 3;
constexpr std::uint32_t Levels = 2;
constexpr std::size_t Bytes = 16384;
constexpr std::uint32_t Sentinel = 0xcafe0000u;
constexpr std::uint32_t R8UInt = 5;
constexpr std::uint32_t R16UInt = 11;
constexpr std::uint32_t Rg8UInt = 18;
constexpr std::uint32_t R32UInt = 20;
alignas(256) std::array<std::uint8_t, Bytes> Texels{};
alignas(256) std::array<std::uint32_t, Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};

struct Format {
    std::uint32_t id;
    std::uint32_t bytes;
    std::uint32_t components;
};

std::vector<std::uint32_t> Code(std::uint32_t opcode, std::uint32_t dmask) {
    std::vector<std::uint32_t> code{0x34060084u, 0xe0381000u, 0x80031e03u, 0xbf8c3f70u};
    for (std::uint32_t index = 0; index < 4u; ++index) {
        code.push_back(0x7e0002ffu | ((10u + index) << 17u));
        code.push_back(Sentinel + index);
    }
    code.push_back(0xf0000008u | (opcode << 18u) | (dmask << 8u));
    code.push_back(0x00010a1eu);
    code.push_back(0xbf8c3f70u);
    code.push_back(0xe0781000u);
    code.push_back(0x80000a03u);
    code.push_back(0xbf810000u);
    return code;
}

std::array<std::uint32_t, 8> Descriptor(std::uint32_t format, std::uint32_t swizzle) {
    const auto address = reinterpret_cast<std::uintptr_t>(Texels.data());
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (format << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u), swizzle | ((Levels - 1u) << 16u) | (9u << 28u), 0u, 1u << 4u, 0u, 0u};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::uint32_t TexelValue(std::uint32_t level, std::uint32_t x, std::uint32_t y, std::uint32_t component, std::uint32_t bytes) {
    const auto value = level * 0x9e37u + y * 0x51u + x * 0x13u + component * 0x2bu + 1u;
    return bytes == 1u ? value & 0xffu : value & 0xffffu;
}

void FillTexels(const Format& format) {
    Texels.fill(0xeeu);
    const auto descriptor = Descriptor(format.id, 0xfacu);
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(descriptor));
    Require(surface.mips.size() == Levels, "image PCK mip count");
    for (std::uint32_t level = 0; level < Levels; ++level) {
        const auto& mip = surface.mips[level];
        const auto width = std::max(Width >> level, 1u);
        const auto height = std::max(Height >> level, 1u);
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                for (std::uint32_t component = 0; component < format.components; ++component) {
                    const auto value = TexelValue(level, x, y, component, format.bytes);
                    const auto offset = mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + (x * format.components + component) * format.bytes;
                    Require(offset + format.bytes <= Texels.size(), "image PCK texture size");
                    std::memcpy(Texels.data() + offset, &value, format.bytes);
                }
            }
        }
    }
}

void FillInput() {
    constexpr std::array<std::uint32_t, 16> xs{0u, 1u, 2u, 3u, 5u, 6u, 7u, Width - 4u, Width - 3u, Width - 2u, Width - 1u, Width, 29u, 30u, 0xffffffffu, 0x80000000u};
    constexpr std::array<std::uint32_t, 5> ys{0u, 1u, 2u, Height, 0xffffffffu};
    constexpr std::array<std::uint32_t, 3> mips{0u, 1u, Levels};
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Input[thread * 4u + 0u] = xs[thread % xs.size()];
        Input[thread * 4u + 1u] = ys[(thread * 3u) % ys.size()];
        Input[thread * 4u + 2u] = mips[(thread / 2u) % mips.size()];
        Input[thread * 4u + 3u] = 0u;
    }
}

std::vector<std::uint32_t> StoreCode(std::uint32_t opcode) {
    return {0x34060084u, 0xe0381000u, 0x80031e03u, 0xbf8c3f70u, 0xf0000108u | (opcode << 18u), 0x0001211eu, 0xbf810000u};
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t opcode, std::uint32_t format, std::uint32_t dmask, std::uint32_t swizzle = 0xfacu) {
    const auto code = opcode >= 0x76u ? StoreCode(opcode) : Code(opcode, dmask);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    const auto descriptor = Descriptor(format, swizzle);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(sizeof(Input)));
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4);
    std::copy(input.begin(), input.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0, 0, 0, 128}};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    if (opcode < 0x76u) return;
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(Texels.data()), Bytes, nullptr, "test");
    device.WaitIdle();
}

void FillStoreInput() {
    constexpr std::array<std::array<std::uint32_t, 3>, Threads> targets{{
        {32u, 0u, 0u}, {37u, 0u, 0u}, {42u, 0u, 0u}, {47u, 0u, 0u}, {52u, 0u, 0u}, {58u, 0u, 0u}, {60u, 0u, 0u},
        {0u, 1u, 0u}, {5u, 1u, 0u}, {10u, 1u, 0u}, {15u, 1u, 0u}, {21u, 1u, 0u}, {26u, 1u, 0u}, {31u, 1u, 0u}, {57u, 1u, 0u},
        {2u, 2u, 0u}, {7u, 2u, 0u}, {13u, 2u, 0u}, {19u, 2u, 0u}, {43u, 2u, 0u}, {59u, 2u, 0u},
        {Width, 1u, 0u}, {0xffffffffu, 1u, 0u}, {0x80000000u, 2u, 0u}, {4u, Height, 0u}, {4u, 0xffffffffu, 0u},
        {0u, 0u, 1u}, {5u, 0u, 1u}, {13u, 0u, 1u}, {26u, 0u, 1u}, {28u, 0u, 1u}, {9u, 0u, Levels},
    }};
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        Input[thread * 4u + 0u] = targets[thread][0];
        Input[thread * 4u + 1u] = targets[thread][1];
        Input[thread * 4u + 2u] = targets[thread][2];
        Input[thread * 4u + 3u] = 0x9e3779b9u * (thread + 1u);
    }
}

void CheckStore(AgcDriver::VulkanDevice& device, std::uint32_t opcode, const Format& format) {
    const auto elements = opcode == 0x77u || opcode == 0x7au ? 4u : 2u;
    const bool mip = opcode == 0x79u || opcode == 0x7au;
    const auto texelBytes = format.bytes * format.components;
    const auto group = 4u / texelBytes;
    const auto name = "image PCK store opcode " + std::to_string(opcode) + " format " + std::to_string(format.id);
    FillTexels(format);
    FillStoreInput();
    auto expected = Texels;
    const auto surface = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(Descriptor(format.id, 0xfacu)));
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto x = Input[thread * 4u + 0u];
        const auto y = Input[thread * 4u + 1u];
        const auto level = mip ? Input[thread * 4u + 2u] : 0u;
        const auto data = Input[thread * 4u + 3u];
        const auto width = level < Levels ? std::max(Width >> level, 1u) : 0u;
        const auto height = level < Levels ? std::max(Height >> level, 1u) : 0u;
        if (level >= Levels || x >= width || width - x < elements || y >= height) continue;
        for (std::uint32_t element = 0; (x & (group - 1u)) + element < group && element < elements; ++element) {
            const auto bits = data >> (element * texelBytes * 8u);
            const auto offset = surface.mips[level].tiledOffset + static_cast<std::uint64_t>(y) * surface.mips[level].pitchBytes + (x + element) * texelBytes;
            std::memcpy(expected.data() + offset, &bits, texelBytes);
        }
    }
    Run(device, opcode, format.id, 0x1u);
    for (std::size_t index = 0; index < Bytes; ++index) {
        Require(Texels[index] == expected[index], name + ": byte " + std::to_string(index) + " is " + std::to_string(Texels[index]) + ", expected " + std::to_string(expected[index]));
    }
}

void Check(AgcDriver::VulkanDevice& device, std::uint32_t opcode, const Format& format, std::uint32_t dmask) {
    const auto elements = opcode == 0x71u || opcode == 0x74u ? 4u : 2u;
    const bool mip = opcode == 0x73u || opcode == 0x74u;
    const auto texelBits = format.bytes * format.components * 8u;
    const auto name = "image PCK opcode " + std::to_string(opcode) + " format " + std::to_string(format.id) + " dmask " + std::to_string(dmask);
    FillTexels(format);
    FillInput();
    const auto initial = Texels;
    Output.fill(0xdeadbeefu);
    Run(device, opcode, format.id, dmask);
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto x = Input[thread * 4u + 0u];
        const auto y = Input[thread * 4u + 1u];
        const auto level = mip ? Input[thread * 4u + 2u] : 0u;
        const auto width = level < Levels ? std::max(Width >> level, 1u) : 0u;
        const auto height = level < Levels ? std::max(Height >> level, 1u) : 0u;
        const bool inside = level < Levels && x < width && width - x >= elements && y < height;
        const auto first = x & ~(elements - 1u);
        std::uint32_t packed = 0;
        for (std::uint32_t element = 0; inside && element < elements; ++element) {
            std::uint32_t texel = 0;
            for (std::uint32_t component = 0; component < format.components; ++component) texel |= TexelValue(level, first + element, y, component, format.bytes) << (component * format.bytes * 8u);
            packed |= texel << (element * texelBits);
        }
        for (std::uint32_t index = 0; index < 4u; ++index) {
            const auto expected = index == 0u ? packed : Sentinel + index;
            const auto actual = Output[thread * 4u + index];
            Require(actual == expected, name + ": thread " + std::to_string(thread) + " (x " + std::to_string(x) + ", y " + std::to_string(y) + ", mip " + std::to_string(level) + ") v" + std::to_string(10u + index) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
        }
    }
    Require(Texels == initial, name + ": the load changed the texture");
}

void Refused(AgcDriver::VulkanDevice& device, std::uint32_t opcode, std::uint32_t format, std::uint32_t dmask, std::uint32_t swizzle, const char* reason) {
    bool refused = false;
    try { Run(device, opcode, format, dmask, swizzle); }
    catch (const std::exception& error) { refused = std::string(error.what()).find(reason) != std::string::npos; }
    Require(refused, "image PCK opcode " + std::to_string(opcode) + " format " + std::to_string(format) + " was not refused");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestAllocations::Mutation().Add(Texels.data(), Bytes, true, true, true);
        constexpr Format r8{R8UInt, 1u, 1u};
        constexpr Format r16{R16UInt, 2u, 1u};
        constexpr Format rg8{Rg8UInt, 1u, 2u};
        for (const auto opcode : {0x70u, 0x73u}) {
            Check(*device, opcode, r8, 0x1u);
            Check(*device, opcode, r16, 0x1u);
            Check(*device, opcode, rg8, 0x1u);
        }
        for (const auto opcode : {0x71u, 0x74u}) Check(*device, opcode, r8, 0x1u);
        constexpr auto layout = "elements fill one dword";
        Refused(*device, 0x70u, R32UInt, 0x1u, 0xfacu, layout);
        Refused(*device, 0x71u, R16UInt, 0x1u, 0xfacu, layout);
        Refused(*device, 0x71u, Rg8UInt, 0x1u, 0xfacu, layout);
        Refused(*device, 0x70u, R16UInt, 0x1u, 0xf2eu, "identity swizzle");
        for (const auto opcode : {0x76u, 0x79u}) {
            CheckStore(*device, opcode, r8);
            CheckStore(*device, opcode, r16);
            CheckStore(*device, opcode, rg8);
        }
        for (const auto opcode : {0x77u, 0x7au}) CheckStore(*device, opcode, r8);
        Refused(*device, 0x76u, R32UInt, 0x1u, 0xfacu, layout);
        Refused(*device, 0x77u, R16UInt, 0x1u, 0xfacu, layout);
        Refused(*device, 0x76u, R16UInt, 0x1u, 0xf2eu, layout);
        GuestAllocations::Mutation().Remove(Texels.data());
        std::cout << "image PCK2/PCK4 load and store execution tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
