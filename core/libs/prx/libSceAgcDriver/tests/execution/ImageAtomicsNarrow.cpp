#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 32;
constexpr std::uint32_t Operations = 13;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 16;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t TexelBytes = 16384;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t ContentionOffset = 32768;

alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::uint32_t ContentionGroups = 32;
constexpr std::uint32_t ContentionThreads = 64;
constexpr std::uint32_t ContentionValues = ContentionGroups * ContentionThreads;
constexpr std::uint32_t ContentionExtent = 8;
alignas(256) std::array<std::uint32_t, ContentionValues> ContentionInput{};
alignas(256) std::array<std::uint32_t, ContentionValues> ContentionOutput{};

alignas(256) constexpr std::array<std::uint32_t, 207> Code{
    0x34020084, 0x34060087, 0xe0301000, 0x80000401, 0xe0301004, 0x80000501, 0xe0301008, 0x80000601,
    0xbf8c3f70, 0x7e100300, 0x7e120280, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf03c2108,
    0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701000, 0x80010b03, 0xe0701004,
    0x80010c03, 0x7e120281, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0x7e180306, 0xf0402308,
    0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701008, 0x80010b03, 0xe070100c,
    0x80010c03, 0x7e120282, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0442108, 0x00020b08,
    0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701010, 0x80010b03, 0xe0701014, 0x80010c03,
    0x7e120283, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0482108, 0x00020b08, 0x7e180280,
    0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701018, 0x80010b03, 0xe070101c, 0x80010c03, 0x7e120284,
    0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0502108, 0x00020b08, 0x7e180280, 0xf0442108,
    0x00020c08, 0xbf8c3f70, 0xe0701020, 0x80010b03, 0xe0701024, 0x80010c03, 0x7e120285, 0x7e140304,
    0xf03c0108, 0x00020a08, 0x7e160305, 0xf0542108, 0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08,
    0xbf8c3f70, 0xe0701028, 0x80010b03, 0xe070102c, 0x80010c03, 0x7e120286, 0x7e140304, 0xf03c0108,
    0x00020a08, 0x7e160305, 0xf0582108, 0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70,
    0xe0701030, 0x80010b03, 0xe0701034, 0x80010c03, 0x7e120287, 0x7e140304, 0xf03c0108, 0x00020a08,
    0x7e160305, 0xf05c2108, 0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701038,
    0x80010b03, 0xe070103c, 0x80010c03, 0x7e120288, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305,
    0xf0602108, 0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701040, 0x80010b03,
    0xe0701044, 0x80010c03, 0x7e120289, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0642108,
    0x00020b08, 0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701048, 0x80010b03, 0xe070104c,
    0x80010c03, 0x7e12028a, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0682108, 0x00020b08,
    0x7e180280, 0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701050, 0x80010b03, 0xe0701054, 0x80010c03,
    0x7e12028b, 0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf06c2108, 0x00020b08, 0x7e180280,
    0xf0442108, 0x00020c08, 0xbf8c3f70, 0xe0701058, 0x80010b03, 0xe070105c, 0x80010c03, 0x7e12028c,
    0x7e140304, 0xf03c0108, 0x00020a08, 0x7e160305, 0xf0702108, 0x00020b08, 0x7e180280, 0xf0442108,
    0x00020c08, 0xbf8c3f70, 0xe0701060, 0x80010b03, 0xe0701064, 0x80010c03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 14> ContentionAdd{
    0x8f118810, 0x34060082, 0x4a060611, 0xe0301000, 0x80000a03, 0xbf8c3f70, 0x7e100280,
    0x7e120280, 0xf0442108, 0x00020a08, 0xbf8c3f70, 0xe0701000, 0x80010a03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 14> ContentionUMax{
    0x8f118810, 0x34060082, 0x4a060611, 0xe0301000, 0x80000a03, 0xbf8c3f70, 0x7e100280,
    0x7e120280, 0xf05c2108, 0x00020a08, 0xbf8c3f70, 0xe0701000, 0x80010a03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 10> ReadBack{
    0x7e100280, 0x7e120280, 0x7e140280, 0xf0442108, 0x00020a08, 0xbf8c3f70, 0x7e060280, 0xe0701000, 0x80010a03, 0xbf810000,
};

constexpr std::array<const char*, Operations> Names{
    "image_atomic_swap", "image_atomic_cmpswap", "image_atomic_add", "image_atomic_sub", "image_atomic_smin", "image_atomic_umin", "image_atomic_smax",
    "image_atomic_umax", "image_atomic_and", "image_atomic_or", "image_atomic_xor", "image_atomic_inc", "image_atomic_dec",
};

struct Format {
    const char* name;
    std::uint32_t format;
    std::uint32_t bits;
};

constexpr std::array<Format, 4> Formats{{
    {"8_UINT", 5u, 8u},
    {"8_SINT", 6u, 8u},
    {"16_UINT", 11u, 16u},
    {"16_SINT", 12u, 16u},
}};

constexpr std::array<std::array<std::uint32_t, 2>, 24> Edges{{
    {0x00u, 0x00u}, {0x00u, 0x05u}, {0x05u, 0x00u}, {0x05u, 0x05u}, {0x04u, 0x05u}, {0x06u, 0x05u},
    {0x7fu, 0x01u}, {0x80u, 0x7fu}, {0xffu, 0x01u}, {0x01u, 0xffu}, {0xfeu, 0xffu}, {0x100u, 0xffu},
    {0x7fffu, 0x0001u}, {0x8000u, 0x7fffu}, {0xffffu, 0x0001u}, {0x0001u, 0xffffu}, {0xfffeu, 0xffffu}, {0x10000u, 0xffffu},
    {0xffffffffu, 0x1u}, {0x80000000u, 0x7fffffffu}, {0x12345678u, 0x87654321u}, {0xaaaaaaaau, 0x55555555u}, {0xf0f0f0f0u, 0xff00ff00u}, {0x0000ff80u, 0xffff0080u},
}};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "image atomics narrow: cannot allocate the guest block");
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

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Mask(const Format& format) {
    return (1u << format.bits) - 1u;
}

std::uint32_t Element(const Format& format, std::uint32_t value) {
    return value & Mask(format);
}

std::int32_t Signed(const Format& format, std::uint32_t value) {
    const auto shift = 32u - format.bits;
    return static_cast<std::int32_t>(value << shift) >> shift;
}

std::uint32_t Next(const Format& format, std::uint32_t operation, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const auto mask = Mask(format);
    a &= mask;
    b &= mask;
    c &= mask;
    std::uint32_t next = 0;
    switch (operation) {
    case 0: next = b; break;
    case 1: next = a == c ? b : a; break;
    case 2: next = a + b; break;
    case 3: next = a - b; break;
    case 4: next = Signed(format, b) < Signed(format, a) ? b : a; break;
    case 5: next = std::min(a, b); break;
    case 6: next = Signed(format, b) > Signed(format, a) ? b : a; break;
    case 7: next = std::max(a, b); break;
    case 8: next = a & b; break;
    case 9: next = a | b; break;
    case 10: next = a ^ b; break;
    case 11: next = a >= b ? 0u : a + 1u; break;
    case 12: next = (a == 0u || a > b) ? b : a - 1u; break;
    default: Require(false, "image atomics narrow: unknown operation");
    }
    return next & mask;
}

void FillInput() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        auto* words = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < 3u; ++j) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            words[j] = static_cast<std::uint32_t>(state >> 32u);
        }
        if (tid < Edges.size()) {
            words[0] = Edges[tid][0];
            words[1] = Edges[tid][1];
        }
        if ((tid & 1u) != 0u) words[2] = words[0];
        if ((tid & 3u) == 2u) words[2] = words[0] ^ 0x10000u;
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, std::uint32_t width, std::uint32_t height, std::uint32_t format) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

AgcDriver::Graphics::TileMipLayout Mip(const Format& format, std::uint32_t width, std::uint32_t height) {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, format.format, width, height, 1u).at(0);
}

std::uint32_t TexelAt(const std::uint8_t* texels, const Format& format, std::uint32_t width, std::uint32_t height, std::uint32_t x, std::uint32_t y) {
    const auto mip = Mip(format, width, height);
    std::uint32_t value = 0;
    std::memcpy(&value, texels + mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * (format.bits / 8u), format.bits / 8u);
    return value;
}

void Flush(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, std::size_t bytes) {
    AgcDriver::Graphics::StorageTexture::FlushPending(reinterpret_cast<std::uintptr_t>(texels), bytes, nullptr, "test");
    device.WaitIdle();
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, std::uint32_t threads, std::uint32_t waveSize, bool groupId) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {groupId, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format) {
    Output.fill(0xdeadbeefu);
    for (std::size_t index = 0; index < TexelBytes; ++index) texels[index] = static_cast<std::uint8_t>(0x5au + 29u * index);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    const auto texture = TextureDescriptor(texels, Width, Height, format.format);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const auto result = Compile(device, Code, userData, Threads, 32u, false);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    Flush(device, texels, TexelBytes);
}

void Check(const std::uint8_t* texels, const Format& format) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto* in = &Input[tid * Inputs];
        for (std::uint32_t j = 0; j < Operations; ++j) {
            const auto next = Next(format, j, in[0], in[1], in[2]);
            const auto where = std::string(Names[j]) + " of " + format.name + ": thread " + std::to_string(tid) + " (a " + Hex(in[0]) + ", b " + Hex(in[1]) + ", c " + Hex(in[2]) + ")";
            const auto returned = Output[tid * Results + 2u * j];
            const auto loaded = Output[tid * Results + 2u * j + 1u];
            Require(returned == Element(format, in[0]), where + " returned " + Hex(returned) + ", expected " + Hex(Element(format, in[0])));
            Require(loaded == Element(format, next), where + " left " + Hex(loaded) + ", expected " + Hex(Element(format, next)));
            const auto stored = TexelAt(texels, format, Width, Height, tid, j);
            Require(stored == next, where + " stored the bits " + Hex(stored) + " in guest memory, expected " + Hex(next));
        }
    }
    for (std::uint32_t y = Operations; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto mip = Mip(format, Width, Height);
            const auto offset = mip.tiledOffset + static_cast<std::uint64_t>(y) * mip.pitchBytes + static_cast<std::uint64_t>(x) * (format.bits / 8u);
            for (std::uint32_t byte = 0; byte < format.bits / 8u; ++byte) {
                const auto index = offset + byte;
                const auto wanted = static_cast<std::uint8_t>(0x5au + 29u * index);
                Require(texels[index] == wanted, std::string("image atomics of ") + format.name + ": untouched texel (" + std::to_string(x) + ", " + std::to_string(y) + ") changed");
            }
        }
    }
}

void DispatchContention(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::span<const std::uint32_t> code, std::uint32_t waveSize, std::uint32_t groups) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto input = BufferDescriptor(ContentionInput.data(), static_cast<std::uint32_t>(ContentionInput.size() * 4u));
    const auto output = BufferDescriptor(ContentionOutput.data(), static_cast<std::uint32_t>(ContentionOutput.size() * 4u));
    const auto texture = TextureDescriptor(texels, ContentionExtent, ContentionExtent, format.format);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const auto result = Compile(device, code, userData, ContentionThreads, waveSize, true);
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::uint32_t Contend(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::span<const std::uint32_t> code, std::uint32_t waveSize, std::uint32_t initial) {
    ContentionOutput.fill(0xdeadbeefu);
    std::memset(texels, 0, 4096);
    std::memcpy(texels, &initial, format.bits / 8u);
    DispatchContention(device, texels, format, code, waveSize, ContentionGroups);
    const auto returned = ContentionOutput;
    ContentionOutput.fill(0xdeadbeefu);
    DispatchContention(device, texels, format, ReadBack, waveSize, 1u);
    const auto final = ContentionOutput[0];
    ContentionOutput = returned;
    Flush(device, texels, 4096);
    const auto stored = TexelAt(texels, format, ContentionExtent, ContentionExtent, 0u, 0u);
    Require(Element(format, stored) == final, std::string("contended image atomics of ") + format.name + ": guest memory holds " + Hex(stored) + " but the texel reads back " + Hex(final));
    return final;
}

void CheckContendedAdd(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::uint32_t waveSize) {
    const auto name = std::string("image_atomic_add of ") + format.name + " contended by " + std::to_string(ContentionValues) + " wave" + std::to_string(waveSize) + " threads on one texel";
    ContentionInput.fill(1u);
    const auto final = Contend(device, texels, format, ContentionAdd, waveSize, 0u);
    std::map<std::uint32_t, std::uint32_t> seen;
    for (std::uint32_t thread = 0; thread < ContentionValues; ++thread) ++seen[ContentionOutput[thread]];
    const auto period = std::min(ContentionValues, Mask(format) + 1u);
    Require(seen.size() == period, name + ": " + std::to_string(seen.size()) + " distinct values were returned, expected " + std::to_string(period));
    for (std::uint32_t sum = 0; sum < period; ++sum) {
        const auto found = seen.find(Element(format, sum));
        Require(found != seen.end() && found->second == ContentionValues / period, name + ": the earlier sum " + std::to_string(sum) + " was not returned exactly " + std::to_string(ContentionValues / period) + " times");
    }
    Require(final == Element(format, ContentionValues), name + ": the texel holds " + Hex(final) + ", expected " + Hex(Element(format, ContentionValues)));
}

void CheckContendedUMax(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Format& format, std::uint32_t waveSize) {
    const auto name = std::string("image_atomic_umax of ") + format.name + " contended by " + std::to_string(ContentionValues) + " wave" + std::to_string(waveSize) + " threads on one texel";
    constexpr std::uint32_t initial = 5u;
    std::uint64_t state = 0x2545f4914f6cdd1dull + waveSize;
    std::set<std::uint32_t> values{Element(format, initial)};
    std::uint32_t expected = initial;
    for (auto& value : ContentionInput) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        value = static_cast<std::uint32_t>(state >> 32u);
        values.insert(Element(format, value));
        expected = std::max(expected, value & Mask(format));
    }
    const auto final = Contend(device, texels, format, ContentionUMax, waveSize, initial);
    for (std::uint32_t thread = 0; thread < ContentionValues; ++thread) {
        const auto returned = ContentionOutput[thread];
        Require(values.contains(returned), name + ": thread " + std::to_string(thread) + " returned " + Hex(returned) + ", which no thread stored");
    }
    Require(final == Element(format, expected), name + ": the texel holds " + Hex(final) + ", expected " + Hex(Element(format, expected)));
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityStorageImageReadWithoutFormat) || !TargetHasCapability(device->Target(), spv::CapabilityStorageImageWriteWithoutFormat)) {
            std::puts("skipped, the device lacks shaderStorageImageReadWithoutFormat or shaderStorageImageWriteWithoutFormat");
            return VulkanTestSkipped;
        }
        GuestBlock block;
        FillInput();
        for (const auto& format : Formats) {
            Run(*device, block.Data(), format);
            Check(block.Data(), format);
        }
        for (const auto& format : Formats) {
            for (const auto waveSize : {32u, 64u}) {
                CheckContendedAdd(*device, block.Data() + ContentionOffset, format, waveSize);
                CheckContendedUMax(*device, block.Data() + ContentionOffset, format, waveSize);
            }
        }
        std::puts("image atomics narrow tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
