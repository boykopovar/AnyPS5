#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
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
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Side = 16;
constexpr std::uint32_t Samples = 4;
constexpr std::uint32_t SampleShift = 2;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Float16x4Info = 0x0730u;
constexpr std::uint32_t MultisampledAttrib = (SampleShift << 12u) | (SampleShift << 15u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t Float32Export = 9;
constexpr std::uint32_t Uint16AbgrExport = 7;
constexpr std::uint32_t Format16161616Float = 71;
constexpr std::uint32_t Unorm8x4SwappedInfo = 0x8828u;
constexpr std::uint32_t Uint8x4Info = 0x8428u;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t Format8888Uint = 60;
constexpr std::uint32_t Format8888Sint = 61;
constexpr std::uint32_t Float10_11_11Info = 0x0718u;
constexpr std::uint32_t Format11_11_10Float = 36;
constexpr std::uint32_t Type2DMsaa = 14;
constexpr std::uint32_t TileRenderTarget = 0x1b;
constexpr std::uint32_t TileZ64KBX = 0x18;
constexpr std::uint32_t TileS64KBX = 0x19;
constexpr std::uint32_t TileD64KBX = 0x1a;
constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t LoadResult = 12;
constexpr std::uint32_t BdaThreads = 64;
constexpr std::uint32_t BdaLoadBase = 0x100;
constexpr std::uint32_t BdaOutputBase = 0x8000;
constexpr std::array<float, 4> Drawn{0.25f, 0.5f, 0.125f, 1.0f};
constexpr double EdgeMargin = 1.0 / 32.0;
constexpr std::uint32_t FillOnes = 0x3c003c00u;
constexpr std::array<float, 4> Ones{1.0f, 1.0f, 1.0f, 1.0f};
constexpr std::array<float, 4> Written{0.75f, 0.25f, 0.5f, 1.0f};
constexpr std::uint32_t StoredSample = 1;
constexpr std::array<std::uint32_t, 4> PointMipSampler{0u, 0xfffu << 12u, 1u << 26u, 0u};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelCode{
    0x7e0802ff, 0x3e800000, 0x7e0a02ff, 0x3f000000, 0x7e0c02ff, 0x3e000000, 0x7e0e02ff, 0x3f800000, 0xf800180f, 0x07060504, 0xbf810000,
};
constexpr std::array<float, 4> DrawnUnorm8{64.0f / 255.0f, 128.0f / 255.0f, 32.0f / 255.0f, 1.0f};
alignas(256) constexpr std::array<std::uint32_t, 11> Unorm8PixelCode{
    0x7e0802ff, std::bit_cast<std::uint32_t>(DrawnUnorm8[0]), 0x7e0a02ff, std::bit_cast<std::uint32_t>(DrawnUnorm8[1]), 0x7e0c02ff, std::bit_cast<std::uint32_t>(DrawnUnorm8[2]), 0x7e0e02ff, std::bit_cast<std::uint32_t>(DrawnUnorm8[3]), 0xf800180f, 0x07060504, 0xbf810000,
};
constexpr std::array<std::uint32_t, 4> DrawnUint8{0x12u, 0xbeu, 0x42u, 0x81u};
alignas(256) constexpr std::array<std::uint32_t, 7> Uint8PixelCode{
    0x7e0802ff, DrawnUint8[0] | (DrawnUint8[1] << 16u), 0x7e0a02ff, DrawnUint8[2] | (DrawnUint8[3] << 16u), 0xf8001c0f, 0x00000504, 0xbf810000,
};
constexpr std::uint32_t FilledSint8 = 0x81beff12u;
constexpr std::array<std::uint32_t, 4> FilledSint8Texel{0x12u, 0xffffffffu, 0xffffffbeu, 0xffffff81u};

alignas(256) constexpr std::array<std::uint32_t, 26> LoadCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf0001f30, 0x00011005,
    0xbf8c3f70, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038, 0x80001201, 0xe070103c,
    0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 26> ArrayLoadCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf0001f38, 0x00011004,
    0xbf8c3f70, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038, 0x80001201, 0xe070103c,
    0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 44> PlaneCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xbf8c3f70, 0xf0001f08, 0x00011005, 0xf09c0f08, 0x00611402,
    0xf0900f08, 0x00611802, 0xbf8c3f70, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038,
    0x80001201, 0xe070103c, 0x80001301, 0xe0701040, 0x80001401, 0xe0701044, 0x80001501, 0xe0701048,
    0x80001601, 0xe070104c, 0x80001701, 0xe0701050, 0x80001801, 0xe0701054, 0x80001901, 0xe0701058,
    0x80001a01, 0xe070105c, 0x80001b01, 0xbf810000,
};

constexpr auto AdjustedPlaneCodeWords = [] {
    auto code = PlaneCode;
    code[14] = 0xf0800f09u;
    return code;
}();
alignas(256) constexpr std::array<std::uint32_t, 44> AdjustedPlaneCode = AdjustedPlaneCodeWords;

alignas(256) constexpr std::array<std::uint32_t, 19> StoreCode{
    0x34020087, 0xe030100c, 0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xe0301018,
    0x80001001, 0xe030101c, 0x80001101, 0xe0301020, 0x80001201, 0xe0301024, 0x80001301, 0xbf8c3f70,
    0xf0201f30, 0x00011005, 0xbf810000,
};

struct SampleWrite {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t sample;
};

alignas(256) constexpr std::array<std::uint32_t, 17> BdaSpanCode{
    0x34040084, 0x4a0404ff, BdaLoadBase, 0x34080081, 0x36080882, 0x4a040504, 0x34060083, 0x4a0606ff,
    BdaOutputBase, 0xdc388ff8, 0x04000002, 0xbf8c3f70, 0xdc708000, 0x00000503, 0xdc708004, 0x00000703, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {0.83f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.31f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};

alignas(256) constexpr std::array<std::uint32_t, 7> BufferLoadCode{0x34020082u, 0xe8001000u | (20u << 19u), 0x80000201u, 0xbf8c0000u, 0xe0701000u, 0x80010201u, 0xbf810000u};

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }
    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

struct Block {
    explicit Block(std::size_t bytes, bool watched = false) : bytes(bytes), watched(watched) {
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
        std::memset(data, 0, bytes);
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "the color target is not write-watched");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
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

AgcDriver::Graphics::ColorTarget DecodeTarget(std::uint64_t address, std::uint32_t sampleShift = SampleShift, VkExtent2D extent = {Side, Side}) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Float16x4Info;
    cx[0x31d] = (sampleShift << 12u) | (sampleShift << 15u);
    cx[0x3b0] = ((extent.width - 1u) << 14u) | (extent.height - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R16G16B16A16_SFLOAT && color.samples == 1u << sampleShift && color.extent.width == extent.width && color.extent.height == extent.height, "the target did not decode as an RGBA16F surface of " + std::to_string(extent.width) + "x" + std::to_string(extent.height) + " and " + std::to_string(1u << sampleShift) + " samples");
    return color;
}

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t pipeBankXor = 0) {
    const auto color = DecodeTarget(block.Address() | pipeBankXor);
    Require(color.address == block.Address() && color.pipeBankXor == pipeBankXor, "the color target lost its base address or pipe/bank XOR");
    return color;
}

AgcDriver::Graphics::ColorTarget DecodeSwappedTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Unorm8x4SwappedInfo;
    cx[0x31d] = MultisampledAttrib;
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_B8G8R8A8_UNORM && color.samples == Samples && color.extent.width == Side && color.extent.height == Side, "the swapped 8_8_8_8 target did not decode as a 16x16 BGRA8 surface of 4 samples");
    return color;
}

AgcDriver::Graphics::ColorTarget DecodePackedFloatTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Float10_11_11Info;
    cx[0x31d] = MultisampledAttrib;
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    const auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_B10G11R11_UFLOAT_PACK32 && color.samples == Samples && color.extent.width == Side && color.extent.height == Side, "the 10_11_11 float target did not decode as a 16x16 B10G11R11 surface of 4 samples");
    return color;
}

AgcDriver::Graphics::ColorTarget DecodeUintTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Uint8x4Info;
    cx[0x31d] = MultisampledAttrib;
    cx[0x3b0] = ((Side - 1u) << 14u) | (Side - 1u);
    cx[0x3b8] = TiledAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_R8G8B8A8_UINT && color.samples == Samples && color.extent.width == Side && color.extent.height == Side, "the 8_8_8_8_UINT target did not decode as a 16x16 RGBA8 UINT surface of 4 samples");
    color.uintExport = true;
    return color;
}

void CheckSwappedSamples(const std::vector<std::array<float, 4>>& loaded) {
    const std::array<float, 4> swapped{DrawnUnorm8[2], DrawnUnorm8[1], DrawnUnorm8[0], DrawnUnorm8[3]};
    const auto codes = [](const std::array<float, 4>& texel) {
        std::string text;
        for (const auto value : texel) text += (text.empty() ? "" : ", ") + std::to_string(value * 255.0f);
        return "(" + text + ")";
    };
    std::size_t drawn = 0;
    for (const auto& texel : loaded) {
        if (texel == std::array<float, 4>{}) continue;
        for (std::uint32_t channel = 0; channel < 4u; ++channel) {
            if (std::abs(texel[channel] - swapped[channel]) <= 0.5f / 255.0f) continue;
            Require(false, "a sample of a BGRA8 target loaded through an RGBA8 texture reads " + codes(texel) + " in UNORM8 codes instead of the drawn color with red and blue swapped " + codes(swapped));
        }
        ++drawn;
    }
    Require(drawn != 0, "no sample of the BGRA8 target holds the drawn color");
}

void CheckPackedFloatSamples(const std::vector<std::array<float, 4>>& loaded) {
    const std::array<float, 4> expected{Drawn[0], Drawn[1], Drawn[2], 1.0f};
    const std::array<float, 4> background{0.0f, 0.0f, 0.0f, 1.0f};
    std::size_t drawn = 0;
    for (const auto& texel : loaded) {
        if (texel == background) continue;
        char text[256];
        std::snprintf(text, sizeof(text), "a sample of the 10_11_11 float target read through a format 36 descriptor reads (%g, %g, %g, %g) instead of the drawn (%g, %g, %g, 1)", texel[0], texel[1], texel[2], texel[3], expected[0], expected[1], expected[2]);
        Require(texel == expected, text);
        ++drawn;
    }
    Require(drawn != 0, "no sample of the 10_11_11 float target holds the drawn color");
}

void CheckUintSamples(const std::vector<std::array<float, 4>>& loaded) {
    std::size_t drawn = 0;
    for (const auto& texel : loaded) {
        const auto words = std::bit_cast<std::array<std::uint32_t, 4>>(texel);
        if (words == std::array<std::uint32_t, 4>{}) continue;
        char text[128];
        std::snprintf(text, sizeof(text), "a sample of the 8_8_8_8_UINT target reads %08x %08x %08x %08x instead of the drawn color", words[0], words[1], words[2], words[3]);
        Require(words == DrawnUint8, text);
        ++drawn;
    }
    Require(drawn != 0, "no sample of the 8_8_8_8_UINT target holds the drawn color");
}

AgcDriver::Graphics::State TargetState(const AgcDriver::Graphics::ColorTarget& color) {
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = color.extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(color.extent.height), static_cast<float>(color.extent.width), -static_cast<float>(color.extent.height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, color.extent};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    state.samples = color.samples;
    return state;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::State& state, std::uint64_t indexAddress = 0, std::uint32_t indexSize = 2u, std::span<const std::uint32_t> pixelCode = PixelCode) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Triangle.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
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
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, state.color.uintExport ? Uint16AbgrExport : Float32Export}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {waveSize, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(context, {}), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    const auto handle = ShaderRecompiler::PrepareShader(fragment);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{codeAddress, 0, 1, {pixelCode.begin(), pixelCode.end()}, {}};
    snapshot.prepared->entries.push_back({0, handle});
    fragment.shader.code = snapshot.code;
    fragment.context.pixel = AgcDriver::Graphics::DecodePixelStageInfo(context, AgcDriver::Graphics::ExportMappings(state));
    const ShaderRecompiler::SrtRuntime runtime{};
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, fragment);
    const auto pixelResult = invocation.Materialize(*invocation.Capture(runtime));
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, pixelResult.get(), PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{indexAddress, static_cast<std::uint32_t>(Triangle.size()), indexAddress != 0 ? indexSize : 0u, 1, 0, indexAddress != 0};
    device.Draw(state, draw, shaders);
}

std::array<std::uint32_t, 8> TargetDescriptor(const Block& block, std::uint32_t sampleShift, std::uint32_t pipeBankXor = 0, std::uint32_t format = Format16161616Float, std::uint32_t tileMode = TileRenderTarget) {
    const auto address = block.Address() | pipeBankXor;
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (sampleShift << 16u) | (tileMode << 20u) | (Type2DMsaa << 28u),
        0u,
        sampleShift << 4u,
        0u,
        0u,
    };
}

ShaderRecompiler::RecompileResult CompileProgram(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 8>& texture, std::vector<std::uint32_t>& userData, const std::array<std::uint32_t, 4>& sampler = {}) {
    const auto address = reinterpret_cast<std::uintptr_t>(Buffer.data());
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(Buffer.size() * 4u), 0x31016facu};
    userData.assign(16, 0u);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(sampler.begin(), sampler.end(), userData.begin() + 12);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

ShaderRecompiler::RecompileResult CompileLoad(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, std::vector<std::uint32_t>& userData, bool arrayed = false) {
    return CompileProgram(device, arrayed ? ArrayLoadCode : LoadCode, texture, userData);
}

void StoreSamples(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::vector<SampleWrite>& writes) {
    std::vector<std::uint32_t> userData;
    const auto program = CompileProgram(device, StoreCode, texture, userData);
    for (std::size_t first = 0; first < writes.size(); first += Threads) {
        Buffer.fill(0u);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto& write = writes[std::min(first + tid, writes.size() - 1)];
            const std::array<std::uint32_t, 10> words{0u, 0u, 0u, write.x, write.y, write.sample, std::bit_cast<std::uint32_t>(Written[0]), std::bit_cast<std::uint32_t>(Written[1]), std::bit_cast<std::uint32_t>(Written[2]), std::bit_cast<std::uint32_t>(Written[3])};
            std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
        }
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(StoreCode.data()));
        device.WaitIdle();
    }
}

std::vector<SampleWrite> SampleWrites(bool (*selected)(std::uint32_t x, std::uint32_t y, std::uint32_t sample)) {
    std::vector<SampleWrite> writes;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            for (std::uint32_t sample = 0; sample < Samples; ++sample) {
                if (selected(x, y, sample)) writes.push_back({x, y, sample});
            }
        }
    }
    return writes;
}

void CheckWritten(const std::vector<std::array<float, 4>>& before, const std::vector<std::array<float, 4>>& after, bool (*selected)(std::uint32_t x, std::uint32_t y, std::uint32_t sample), const std::string& what) {
    std::uint32_t written = 0;
    for (std::uint32_t index = 0; index < after.size(); ++index) {
        const auto pixel = index / Samples;
        const auto x = pixel % Side;
        const auto y = pixel / Side;
        const auto sample = index % Samples;
        const bool chosen = selected(x, y, sample);
        char where[96];
        std::snprintf(where, sizeof(where), "sample %u of pixel (%u, %u)", sample, x, y);
        Require(after[index] == (chosen ? Written : before[index]), std::string(where) + (chosen ? " does not hold the stored color " : " changed although it was not stored to ") + what);
        written += chosen ? 1u : 0u;
    }
    Require(written != 0 && written != after.size(), "the stores covered every sample or none " + what);
}

void RequireStoreRefused(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::string& expected, const std::string& what) {
    std::string refusal;
    try {
        StoreSamples(device, texture, {{0u, 0u, 0u}});
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, what + " was not refused: " + refusal);
}

bool DeclaresCapability(std::span<const std::uint32_t> words, spv::Capability capability) {
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        Require(count != 0u && count <= words.size() - cursor, "a SPIR-V instruction is truncated");
        if ((words[cursor] & 0xffffu) == spv::OpCapability && words[cursor + 1u] == static_cast<std::uint32_t>(capability)) return true;
        cursor += count;
    }
    return false;
}

std::vector<std::array<float, 4>> RunLoad(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, bool arrayed, std::uint32_t fragments) {
    std::vector<std::uint32_t> userData;
    const auto program = CompileLoad(device, texture, userData, arrayed);
    Require(!DeclaresCapability(program.spirv, spv::CapabilityImageMSArray), "the multisampled image_load declared ImageMSArray, which only multisampled arrayed storage images need");
    std::vector<std::array<float, 4>> loaded(Side * Side * fragments);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>((arrayed ? ArrayLoadCode : LoadCode).data());
    for (std::uint32_t first = 0; first < loaded.size(); first += Threads) {
        Buffer.fill(0xdeadbeefu);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto index = first + tid;
            const auto pixel = index / fragments;
            const std::array<std::uint32_t, 6> words = arrayed
                ? std::array<std::uint32_t, 6>{0u, 0u, pixel % Side, pixel / Side, 0u, index % fragments}
                : std::array<std::uint32_t, 6>{0u, 0u, 0u, pixel % Side, pixel / Side, index % fragments};
            std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
        }
        device.Dispatch(program, 1, 1, 1, {}, codeAddress);
        device.WaitIdle();
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t component = 0; component < 4u; ++component) loaded[first + tid][component] = std::bit_cast<float>(Buffer[tid * Words + LoadResult + component]);
        }
    }
    return loaded;
}

std::vector<std::array<float, 4>> LoadSamples(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, std::uint32_t fragments = Samples) {
    auto loaded = RunLoad(device, texture, false, fragments);
    const auto loadedArray = RunLoad(device, texture, true, fragments);
    Require(loadedArray == loaded, "Dim2DMsaaArray image_load disagreed with Dim2DMsaa image_load on the multisampled color target");
    return loaded;
}

std::vector<std::array<float, 4>> LoadSamples(AgcDriver::VulkanDevice& device, const Block& block, std::uint32_t fragments = Samples, std::uint32_t pipeBankXor = 0, std::uint32_t format = Format16161616Float, std::uint32_t tileMode = TileRenderTarget) {
    return LoadSamples(device, TargetDescriptor(block, static_cast<std::uint32_t>(std::countr_zero(fragments)), pipeBankXor, format, tileMode), fragments);
}

void CheckFirstSampleReads(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::vector<std::array<float, 4>>& samples, bool distinct, const std::string& what, const std::array<std::uint32_t, 44>& code = PlaneCode, const char* second = "image_sample_lz") {
    std::vector<std::uint32_t> userData;
    const auto program = CompileProgram(device, code, texture, userData, PointMipSampler);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
    const auto secondRead = std::string("a DIM 2D ") + second;
    const auto bits = [&](std::size_t index) { return std::bit_cast<std::array<std::uint32_t, 4>>(samples[index]); };
    bool differs = false;
    for (std::uint32_t first = 0; first < Side * Side; first += Threads) {
        Buffer.fill(0xdeadbeefu);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto pixel = first + tid;
            const std::array<std::uint32_t, 5> words{std::bit_cast<std::uint32_t>((pixel % Side + 0.5f) / Side), std::bit_cast<std::uint32_t>((pixel / Side + 0.5f) / Side), std::bit_cast<std::uint32_t>(1.0f), pixel % Side, pixel / Side};
            std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
        }
        device.Dispatch(program, 1, 1, 1, {}, codeAddress);
        device.WaitIdle();
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto pixel = first + tid;
            const auto sampleZero = bits(pixel * Samples);
            for (std::uint32_t sample = 1; sample < Samples; ++sample) differs = differs || bits(pixel * Samples + sample) != sampleZero;
            for (std::uint32_t read = 0; read < 3u; ++read) {
                std::array<std::uint32_t, 4> texel{};
                std::copy_n(&Buffer[tid * Words + LoadResult + read * 4u], texel.size(), texel.begin());
                char where[200];
                std::snprintf(where, sizeof(where), "%s of pixel (%u, %u) read %08x %08x %08x %08x, sample 0 is %08x %08x %08x %08x", read == 0u ? "a DIM 2D image_load" : read == 1u ? secondRead.c_str() : "a DIM 2D image_sample_l at LOD 1", pixel % Side, pixel / Side, texel[0], texel[1], texel[2], texel[3], sampleZero[0], sampleZero[1], sampleZero[2], sampleZero[3]);
                Require(texel == sampleZero, std::string(where) + " " + what);
            }
        }
    }
    Require(!distinct || differs, "no pixel's sample 0 differs from its other samples " + what);
}

std::array<std::uint32_t, 8> CompressedDescriptor(const Block& block, std::uint32_t tileMode, std::uint64_t htile) {
    auto texture = TargetDescriptor(block, SampleShift, 0u, Format16161616Float, tileMode);
    const auto meta = htile >> 8u;
    texture[6] = (1u << 20u) | (1u << 21u) | (static_cast<std::uint32_t>(meta & 0xffu) << 24u);
    texture[7] = static_cast<std::uint32_t>(meta >> 8u);
    return texture;
}

double EdgeDistance(double x, double y, std::size_t from) {
    const auto pixels = [](const std::array<float, 4>& vertex) {
        return std::array<double, 2>{(static_cast<double>(vertex[0]) + 1.0) * 0.5 * Side, Side - (static_cast<double>(vertex[1]) + 1.0) * 0.5 * Side};
    };
    const auto a = pixels(Triangle[from]);
    const auto b = pixels(Triangle[(from + 1u) % Triangle.size()]);
    const auto c = pixels(Triangle[(from + 2u) % Triangle.size()]);
    const auto length = std::hypot(b[0] - a[0], b[1] - a[1]);
    const auto side = [&](double px, double py) { return ((b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0])) / length; };
    return side(c[0], c[1]) < 0 ? -side(x, y) : side(x, y);
}

void CheckSamples(const AgcDriver::Graphics::State& state, const std::vector<std::array<float, 4>>& loaded, const std::array<float, 4>& background, std::span<const std::array<float, 4>> supplied = {}) {
    const auto locations = AgcDriver::Graphics::SampleLocations(state);
    Require(loaded.size() == Side * Side * state.samples, "the multisample result has the wrong element count");
    Require(supplied.empty() || supplied.size() == loaded.size(), "the initial sample count does not match the rendered target");
    std::vector<std::uint32_t> pixelsByCoverage(state.samples + 1u);
    std::vector<std::uint32_t> coveredBySample(state.samples);
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            std::uint32_t covered = 0;
            for (std::uint32_t sample = 0; sample < state.samples; ++sample) {
                const auto index = (y * Side + x) * state.samples + sample;
                const auto& texel = loaded[index];
                const bool drawn = texel == Drawn;
                const bool empty = texel == (supplied.empty() ? background : supplied[index]);
                char where[160];
                std::snprintf(where, sizeof(where), "sample %u of pixel (%u, %u) loaded (%g, %g, %g, %g)", sample, x, y, texel[0], texel[1], texel[2], texel[3]);
                Require(drawn || empty, std::string(where) + ", neither the drawn color nor the initial target sample");
                covered += drawn ? 1u : 0u;
                coveredBySample[sample] += drawn ? 1u : 0u;
                const auto px = x + static_cast<double>(locations[sample].x);
                const auto py = y + static_cast<double>(locations[sample].y);
                const auto distance = std::min({EdgeDistance(px, py, 0), EdgeDistance(px, py, 1), EdgeDistance(px, py, 2)});
                if (std::fabs(distance) < EdgeMargin) continue;
                ++checked;
                Require(drawn == (distance > 0), std::string(where) + (distance > 0 ? ", which the triangle covers" : ", which the triangle does not cover"));
            }
            ++pixelsByCoverage[covered];
        }
    }
    Require(checked >= Side * Side * state.samples * 9u / 10u, "fewer than nine in ten samples are clear of the triangle's edges");
    Require(pixelsByCoverage[0] != 0 && pixelsByCoverage[state.samples] != 0, "the triangle covers every pixel or none");
    Require(std::any_of(pixelsByCoverage.begin() + 1, pixelsByCoverage.end() - 1, [](auto count) { return count != 0; }), "no pixel is partly covered, so the samples of a pixel were not told apart");
    Require(std::adjacent_find(coveredBySample.begin(), coveredBySample.end(), std::not_equal_to<>()) != coveredBySample.end(), "every sample index has the same coverage, so the sample index was not told apart");
}

void CheckUniform(const std::vector<std::array<float, 4>>& loaded, const std::array<float, 4>& expected, const std::string& what) {
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        const auto& texel = loaded[index];
        char where[160];
        std::snprintf(where, sizeof(where), "sample %zu of pixel (%zu, %zu) loaded (%g, %g, %g, %g)", index % Samples, index / Samples % Side, index / Samples / Side, texel[0], texel[1], texel[2], texel[3]);
        Require(texel == expected, std::string(where) + " " + what);
    }
}

void CheckNativeIndices(AgcDriver::VulkanDevice& device, const Block& source, const Block& destination) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (context.hostImportAlignment == 0) {
        std::puts("native index alias checks not run: host imports unavailable");
        return;
    }
    const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(DecodeTarget(source));
    Require(colorBytes <= source.bytes && source.bytes - colorBytes >= BlockBytes, "the native-index fixture has no neighboring block");
    const auto neighbor = source.Address() + colorBytes;
    AgcDriver::Graphics::RequireNoMultisampleTarget(neighbor, 12u, "an index neighbor control");
    std::string guarded;
    try {
        AgcDriver::Graphics::RequireNoMultisampleTarget(source.Address(), 48u, "a draw-input control");
    } catch (const std::exception& error) {
        guarded = error.what();
    }
    Require(guarded.find("whose samples are only on the host") != std::string::npos, "the draw-input source is not guarded as a native target");
    const auto hostOnly = AgcDriver::Graphics::HostOnlySampleRanges(source.Address(), 48u);
    Require(std::ranges::any_of(hostOnly, [&](const auto& range) { return range.first <= source.Address() && source.Address() < range.first + range.second; }), "the host-only samples of the draw-input source are not reported to the frame capture snapshot");
    Require(AgcDriver::Graphics::HostOnlySampleRanges(neighbor, 12u).empty(), "the neighbor of a native target is reported as host-only samples");
    std::vector<std::byte> captured(static_cast<std::size_t>(colorBytes) + 12u, std::byte{0xcc});
    const auto zeroed = AgcDriver::ReadAroundRanges(source.Address(), captured, AgcDriver::Graphics::HostOnlySampleRanges(source.Address(), captured.size()), AgcDriver::GuestMemory::ReadCommitted);
    std::vector<std::byte> after(12u);
    AgcDriver::GuestMemory::ReadCommitted(neighbor, after);
    Require(zeroed == colorBytes && AgcDriver::AllZero(std::span(captured).first(static_cast<std::size_t>(colorBytes))) && std::ranges::equal(std::span(captured).last(12u), after), "a capture read across a native target and its neighbor did not zero exactly the target");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto* native = AgcDriver::Graphics::HostImportFor(context, source.Address(), 12u);
        const auto* adjacent = AgcDriver::Graphics::HostImportFor(context, neighbor, 12u);
        Require(native != nullptr && native == adjacent, "the native indices and their neighbor do not share a host import");
    }
    const auto state = TargetState(DecodeTarget(destination));
    AgcDriver::Graphics::NoteColorMetadataFill(source.Address(), static_cast<std::size_t>(colorBytes), FillOnes);
    std::uint32_t missed = 0;
    for (const std::uint32_t indexSize : {2u, 4u}) {
        if (indexSize == 2u) {
            const std::array<std::uint16_t, 3> indices{0u, 1u, 2u};
            std::memcpy(source.data + colorBytes, indices.data(), sizeof(indices));
        } else {
            const std::array<std::uint32_t, 3> indices{0u, 1u, 2u};
            std::memcpy(source.data + colorBytes, indices.data(), sizeof(indices));
        }
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::NoteColorMetadataFill(destination.Address(), static_cast<std::size_t>(AgcDriver::Graphics::MultisampledColorBytes(state.color)), 0u);
            AgcDriver::Graphics::MaterializeMultisampleTargetClear(context, state.color);
        }
        CheckUniform(LoadSamples(device, destination), {}, "before the neighboring indexed draw");
        Draw(device, state, neighbor, indexSize);
        device.WaitIdle();
        CheckSamples(state, LoadSamples(device, destination), {});
        for (const bool edge : {false, true}) {
            const auto address = edge ? neighbor - indexSize : source.Address();
            std::string refusal;
            try {
                Draw(device, state, address, indexSize);
                device.WaitIdle();
            } catch (const std::exception& error) {
                refusal = error.what();
            }
            const bool refused = refusal.find("whose samples are only on the host") != std::string::npos;
            std::printf("native index %u %s: refused=%u neighbor=passed detail=%s\n", indexSize * 8u, edge ? "edge" : "base", refused ? 1u : 0u, refusal.c_str());
            Require(refusal.empty() || refused, "the native index input failed for another reason: " + refusal);
            missed += refused ? 0u : 1u;
        }
    }
    CheckUniform(LoadSamples(device, source), Ones, "after native draw-input alias attempts");
    std::puts("native index controls: shared-import=1 source-samples=1024 neighbor-triangle=passed");
    Require(missed == 0, "native indices missed " + std::to_string(missed) + " resident-image refusals");
}

void CheckOverwrittenTarget(AgcDriver::VulkanDevice& device) {
    Block reused(4u * BlockBytes, true);
    if (!AgcDriver::GuestMemory::Watched(reused.Address(), reused.bytes)) {
        std::puts("overwritten target retirement not tested: the guest block is not write-watched");
        return;
    }
    const auto color = DecodeTarget(reused);
    const auto state = TargetState(color);
    Draw(device, state);
    device.WaitIdle();
    CheckSamples(state, LoadSamples(device, reused), {});
    const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(color);
    Require(colorBytes >= 2u * 256u && colorBytes <= reused.bytes, "the watched 4x target's color memory does not fit its block");
    std::string refusal;
    try {
        AgcDriver::Graphics::RequireNoMultisampleTarget(reused.Address(), 64u, "a read of a drawn target the title has not written");
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("whose samples are only on the host") != std::string::npos, "a drawn target in watched memory is not guarded");
    const auto written = reused.Address() + colorBytes / 2u;
    std::memset(reused.data + colorBytes / 2u, 0x5a, 256u);
    AgcDriver::Graphics::RequireNoMultisampleTarget(written, 256u, "a read of target memory the title wrote over");
    AgcDriver::Graphics::RequireNoMultisampleTarget(reused.Address(), 64u, "a read of the rest of a target the title wrote over");
    Draw(device, state);
    device.WaitIdle();
    refusal.clear();
    try {
        AgcDriver::Graphics::RequireNoMultisampleTarget(written, 256u, "a read of a target drawn again over memory the title wrote");
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("whose samples are only on the host") != std::string::npos, "a target drawn again after the title wrote over it is not guarded");
    std::puts("a drawn 4x target in watched memory stays guarded until the title writes over its color memory; the next guarded read then retires it, and a later draw guards the new target");
}

bool Refused(std::uint64_t address, std::size_t bytes, const char* access) {
    try {
        AgcDriver::Graphics::RequireNoMultisampleTarget(address, bytes, access);
    } catch (const std::exception& error) {
        if (std::string_view(error.what()).find("whose samples are only on the host") == std::string_view::npos) throw;
        return true;
    }
    return false;
}

void CheckClaimedTargets(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if ((context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_2_BIT) == 0 || (context.limits.framebufferStencilSampleCounts & VK_SAMPLE_COUNT_2_BIT) == 0) {
        std::puts("claimed target retirement not tested: the device cannot render two-sample color and stencil");
        return;
    }
    const auto bindStencil = [&](const Block& plane, const Block& depth) {
        AgcDriver::Graphics::DepthTarget stencil{depth.Address(), plane.Address(), {Side, Side}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0u, depth.Address() + BlockBytes, false, 2u};
        stencil.htileDepthOnly = true;
        AgcDriver::Graphics::DepthSurfaceView(context, stencil);
        device.WaitIdle();
    };
    for (const bool drawnAgain : {false, true}) {
        Block color(BlockBytes);
        Block depth(2u * BlockBytes);
        const auto state = TargetState(DecodeTarget(color.Address(), 1u));
        Draw(device, state);
        device.WaitIdle();
        Require(Refused(color.Address(), 0x1000u, "a read of a drawn 2x target"), "a drawn 2x target is not guarded");
        bindStencil(color, depth);
        if (drawnAgain) {
            Draw(device, state);
            device.WaitIdle();
        }
        const bool refused = Refused(color.Address(), 0x1000u, drawnAgain ? "a read of a 2x target drawn again after a stencil plane was bound over it" : "a read of a 2x target that a stencil plane was bound over");
        AgcDriver::Graphics::ClearDepthSurfaces(context.device);
        Require(refused == drawnAgain, drawnAgain ? "a 2x target drawn again after a D32S8 target's stencil plane was bound over it is not guarded" : "a 2x target that a D32S8 target's stencil plane was bound over after its last draw is still guarded");
    }
    {
        Block reused(BlockBytes);
        Draw(device, TargetState(DecodeTarget(reused.Address(), 1u)));
        device.WaitIdle();
        Draw(device, TargetState(DecodeTarget(reused.Address(), 0u)));
        device.WaitIdle();
        Require(!Refused(reused.Address(), 0x1000u, "a read of a 2x target that a single-sample color target was drawn over"), "a 2x target that a single-sample color target was drawn over is still guarded");
    }
    {
        Block reused(2u * BlockBytes);
        const auto wide = TargetState(DecodeTarget(reused.Address(), 1u, {8u * Side, 4u * Side}));
        const auto narrow = TargetState(DecodeTarget(reused.Address()));
        Require(AgcDriver::Graphics::MultisampledColorBytes(wide.color) == 2u * BlockBytes && AgcDriver::Graphics::MultisampledColorBytes(narrow.color) == BlockBytes, "the 2x target does not span the 4x target and one more block");
        Draw(device, wide);
        device.WaitIdle();
        Draw(device, narrow);
        device.WaitIdle();
        Require(!Refused(reused.Address() + BlockBytes, 0x1000u, "a read of the part of a 2x target that a 4x target drawn over it does not cover"), "a 2x target that a 4x target was drawn over is still guarded");
        Require(Refused(reused.Address(), 0x1000u, "a read of a 4x target drawn over a 2x target"), "a 4x target drawn over a 2x target is not guarded");
    }
    if (context.storageImageMultisample) {
        Block color(BlockBytes);
        Block depth(2u * BlockBytes);
        Draw(device, TargetState(DecodeTarget(color.Address(), 1u)));
        device.WaitIdle();
        bindStencil(color, depth);
        StoreSamples(device, TargetDescriptor(color, 1u), {{0u, 0u, 0u}});
        const bool refused = Refused(color.Address(), 0x1000u, "a read of a 2x target that a dispatch stored to after a stencil plane was bound over it");
        AgcDriver::Graphics::ClearDepthSurfaces(context.device);
        Require(refused, "a 2x target that a dispatch stored to after a D32S8 target's stencil plane was bound over it is not guarded");
    } else {
        std::puts("a dispatch's store after a claim not tested: storage image multisampling unavailable");
    }
    {
        Block fmask(BlockBytes);
        Block depth(2u * BlockBytes);
        AgcDriver::Graphics::ColorTarget oneFragment{};
        oneFragment.extent = {128u, 128u};
        oneFragment.samples = 1u;
        oneFragment.coverageSamples = 4u;
        oneFragment.fmaskAddress = fmask.Address();
        AgcDriver::Graphics::GuardOneFragmentFmask(context, oneFragment);
        bindStencil(fmask, depth);
        std::string refusal;
        try {
            AgcDriver::Graphics::RequireNoMultisampleTarget(fmask.Address(), 0x1000u, "a read of claimed FMASK");
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        AgcDriver::Graphics::ClearDepthSurfaces(context.device);
        Require(refusal.find("overlaps the FMASK") != std::string::npos, "a stencil plane bound over a one-fragment target's FMASK retired its guard: " + refusal);
    }
    std::puts("a 2x target whose memory a D32S8 stencil plane, a single-sample color target or a 4x target was bound over after its last draw is retired at the next guarded read; one drawn again or stored to after the stencil bind stays guarded, and a one-fragment target's FMASK guard keeps refusing");
}

void StoreWords(const Block& block, std::uint64_t bytes, std::uint32_t word) {
    for (std::uint64_t offset = 0; offset < bytes; offset += sizeof(word)) std::memcpy(block.data + offset, &word, sizeof(word));
}

ShaderRecompiler::RecompileResult CompileBdaSpan(AgcDriver::VulkanDevice& device, std::uint64_t address) {
    const std::span<const std::uint32_t> code(BdaSpanCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{BdaThreads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(std::ranges::any_of(result.bindings, [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; }), "the native multisample BDA test has no address-table binding");
    return result;
}

void CheckBdaSpan(AgcDriver::VulkanDevice& device, const Block& block, std::size_t offset, bool refused) {
    std::vector<std::uint8_t> before(block.bytes);
    for (std::size_t index = 0; index < before.size(); ++index) before[index] = static_cast<std::uint8_t>(index * 37u + 11u);
    const auto output = offset + 0x8000u;
    std::fill(before.begin() + static_cast<std::ptrdiff_t>(output), before.begin() + static_cast<std::ptrdiff_t>(output + BdaThreads * 8u), 0xcdu);
    std::memcpy(block.data, before.data(), before.size());
    auto expected = before;
    for (std::uint32_t tid = 0; tid < BdaThreads; ++tid) {
        const auto load = offset + BdaLoadBase + tid * 16u + (tid & 1u) * 2u - 8u;
        std::memcpy(expected.data() + output + tid * 8u, before.data() + load + 4u, sizeof(std::uint32_t));
        std::memcpy(expected.data() + output + tid * 8u + 4u, before.data() + load + 12u, sizeof(std::uint32_t));
    }
    std::string failure;
    StderrCapture log;
    try {
        const auto address = block.Address() + offset;
        device.Dispatch(CompileBdaSpan(device, address), 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(BdaSpanCode.data()));
        device.WaitIdle();
    } catch (const std::exception& error) {
        failure = error.what();
    }
    const auto deferred = log.Finish();
    if (failure.empty()) failure = deferred;
    if (refused) {
        Require(failure.find("BDA access failed:") != std::string::npos, "BDA access to a resident native multisample range was not refused: " + failure);
        Require(std::equal(before.begin(), before.end(), block.data), "a refused BDA access changed guest memory");
    } else {
        Require(failure.empty(), "BDA access to an ordinary neighboring range failed: " + failure);
        Require(std::equal(expected.begin(), expected.end(), block.data), "the ordinary BDA span did not preserve its reads, writes or boundaries");
    }
}

void RequireRefused(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& texture, const std::string& expected, const std::string& what) {
    std::string refusal;
    try {
        std::vector<std::uint32_t> userData;
        const auto program = CompileLoad(device, texture, userData);
        Buffer.fill(0u);
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(LoadCode.data()));
        device.WaitIdle();
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, what + " was not refused: " + refusal);
}

std::vector<std::array<float, 4>> ReadHalfSamples(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color) {
    const auto bytes = AgcDriver::Graphics::ReadMultisampleTargetSamples_nid_no_patch(context, color);
    Require(bytes.size() == Side * Side * color.samples * 8u, "native color readback has the wrong byte count");
    std::vector<std::array<float, 4>> result(Side * Side * color.samples);
    constexpr std::array<std::array<std::uint16_t, 4>, 4> bits{{{}, {0x3400u, 0x3800u, 0x3000u, 0x3c00u}, {0x3c00u, 0x3c00u, 0x3c00u, 0x3c00u}, {0x3a00u, 0x3400u, 0x3800u, 0x3c00u}}};
    constexpr std::array<std::array<float, 4>, 4> values{{{}, Drawn, Ones, Written}};
    for (std::size_t index = 0; index < result.size(); ++index) {
        std::array<std::uint16_t, 4> sample{};
        std::memcpy(sample.data(), bytes.data() + index * sizeof(sample), sizeof(sample));
        const auto found = std::find(bits.begin(), bits.end(), sample);
        Require(found != bits.end(), "native color readback changed a half-float sample at element " + std::to_string(index));
        result[index] = values[static_cast<std::size_t>(found - bits.begin())];
    }
    return result;
}

void RequireReadbackRefused(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color, const std::string& expected) {
    std::string refusal;
    try {
        static_cast<void>(AgcDriver::Graphics::ReadMultisampleTargetSamples_nid_no_patch(context, color));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "native color readback was not refused: " + refusal);
}

void RequireInitialRefused(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color, const std::string& expected) {
    std::string refusal;
    try {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::MultisampleTargetView(context, color);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "native color initialization was not refused: " + refusal);
}

bool InitialCountSupported(const AgcDriver::Graphics::Context& context, VkFormat format, std::uint32_t samples) {
    VkImageFormatProperties supported{};
    const auto result = context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0u, &supported);
    if (result != VK_ERROR_FORMAT_NOT_SUPPORTED) AgcDriver::Graphics::Check(result, "native color initialization fixture format query");
    if (result == VK_SUCCESS && (supported.sampleCounts & samples) != 0 && (context.limits.framebufferColorSampleCounts & samples) != 0) return true;
    std::printf("skipped native color initialization subcase: format=%d samples=%u is unavailable\n", static_cast<int>(format), samples);
    return false;
}

PFN_vkGetPhysicalDeviceImageFormatProperties imageFormatProperties = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL NoReadbackImage(VkPhysicalDevice physical, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    if ((flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) != 0 && usage == (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return imageFormatProperties(physical, format, type, tiling, usage, flags, properties);
}

void Readback(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    context.limits.maxStorageBufferRange = 256u;
    std::uint32_t exercised = 0;
    for (const auto fragments : {2u, 4u, 8u}) {
        if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, fragments)) continue;
        Block block(BlockBytes);
        auto color = DecodeTarget(block);
        color.samples = fragments;
        RequireReadbackRefused(context, color, "requires an existing resident target");
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            MultisampleTargetView(context, color);
        }
        const auto state = TargetState(color);
        Draw(device, state);
        CheckSamples(state, ReadHalfSamples(context, color), {});
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            auto* recording = Recorder::Active();
            Require(recording != nullptr && context.recorder != nullptr, "the readback fixture has no owning recorder");
            Recorder other(context);
            other.Activate();
            RequireReadbackRefused(context, color, "another active recorder on the same device");
            recording->Activate();
        }
        auto otherDevice = context;
        otherDevice.device = VK_NULL_HANDLE;
        RequireReadbackRefused(otherDevice, color, "requires an existing resident target");
        auto otherMip = color;
        otherMip.mip = 1;
        RequireReadbackRefused(context, otherMip, "subresource does not match");
        NoteColorMetadataFill(block.Address(), static_cast<std::size_t>(MultisampledColorBytes(color)), FillOnes);
        CheckUniform(ReadHalfSamples(context, color), Ones, "after a native color fill");
        if (fragments == Samples) {
            CheckUniform(LoadSamples(device, block), Ones, "before storage migration");
            const auto selected = [](std::uint32_t x, std::uint32_t, std::uint32_t sample) { return x < Side / 2u && sample == StoredSample; };
            StoreSamples(device, TargetDescriptor(block, SampleShift), SampleWrites(selected));
            CheckWritten(std::vector<std::array<float, 4>>(Side * Side * Samples, Ones), ReadHalfSamples(context, color), selected, "after storage migration and per-sample writes");
        }
        std::printf("native color readback: RGBA16F samples=%u coverage/fill/refusals/chunks passed\n", fragments);
        ++exercised;
        {
            Block exact(BlockBytes);
            auto exactColor = DecodeTarget(exact, 0x8100u);
            exactColor.samples = fragments;
            const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, 8u, fragments, Side, Side, 1u, exactColor.pipeBankXor);
            constexpr std::array<std::uint16_t, 12> patterns{0u, 0x8000u, 1u, 0x8001u, 0x03ffu, 0x0400u, 0x7c00u, 0xfc00u, 0x7e01u, 0xfe55u, 0x7d01u, 0xfdffu};
            std::vector<std::uint16_t> words(static_cast<std::size_t>(layout.LinearBytes()) / sizeof(std::uint16_t));
            for (std::size_t index = 0; index < words.size(); ++index) words[index] = patterns[index % patterns.size()];
            const auto expected = std::as_bytes(std::span(words));
            auto tiled = std::as_writable_bytes(std::span(exact.data, exact.bytes));
            layout.Tile(expected, tiled);
            const std::vector<std::byte> before(tiled.begin(), tiled.end());
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                MultisampleTargetView(context, exactColor);
            }
            const auto samples = ReadMultisampleTargetSamples_nid_no_patch(context, exactColor);
            Require(std::equal(samples.begin(), samples.end(), expected.begin(), expected.end()), "native half-float readback changed an exceptional-value bit pattern");
            const auto snapshot = ReadMultisampleTargetColorSnapshot_nid_no_patch(context, exactColor);
            Require(snapshot.original == before && snapshot.current == before && std::equal(before.begin(), before.end(), tiled.begin()), "native exceptional-value snapshot changed samples, padding or guest bytes");
            std::printf("native color readback: RGBA16F samples=%u signed zeros/subnormals/infinities/NaN payload bits preserved\n", fragments);
        }
    }
    constexpr std::array<VkFormat, 5> formats{VK_FORMAT_R8_UINT, VK_FORMAT_R16_UINT, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32A32_UINT};
    constexpr std::array<std::uint32_t, 4> pattern{0x5au, 0x89abcdefu, 0x01234567u, 0xfedcba98u};
    for (std::size_t index = 0; index < formats.size(); ++index) {
        if (!InitialCountSupported(context, formats[index], Samples)) continue;
        Block block(BlockBytes);
        auto color = DecodeTarget(block);
        color.format = formats[index];
        color.elementBytes = 1u << index;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            VkImage image = VK_NULL_HANDLE;
            MultisampleTargetView(context, color, &image);
            Recorder::Active()->Sync();
            CommandBatch batch(context);
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            VkClearColorValue clear{};
            std::copy(pattern.begin(), pattern.end(), clear.uint32);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(batch.Handle(), image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1u, &range);
            batch.SubmitAndWait();
        }
        const auto bytes = ReadMultisampleTargetSamples_nid_no_patch(context, color);
        Require(bytes.size() == Side * Side * Samples * color.elementBytes, "native integer sample count is wrong");
        for (std::size_t offset = 0; offset < bytes.size(); offset += color.elementBytes) Require(std::memcmp(bytes.data() + offset, pattern.data(), color.elementBytes) == 0, "native integer sample bytes changed at " + std::to_string(offset));
        std::printf("native color readback: elementBytes=%u integer bits/chunks passed\n", color.elementBytes);
    }
    if (InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, Samples)) {
        Block block(BlockBytes);
        const auto color = DecodeTarget(block);
        auto unsupported = context;
        imageFormatProperties = context.imageFormatProperties;
        unsupported.imageFormatProperties = NoReadbackImage;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            MultisampleTargetView(unsupported, color);
        }
        Draw(device, TargetState(color));
        RequireReadbackRefused(unsupported, color, "multisample color readback");
        CheckSamples(TargetState(color), LoadSamples(device, block), {});
        std::puts("native color readback: unsupported snapshot format refuses readback and retains rendering");
        Block invalidBlock(BlockBytes);
        auto invalid = DecodeTarget(invalidBlock);
        invalid.elementBytes = 4u;
        RequireInitialRefused(context, invalid, "color format or element size is unsupported");
        std::puts("native color readback: mismatched source format and element size is refused before view creation");
    }
    Require(exercised != 0, "no supported native color readback sample count was exercised");
}

std::vector<std::array<float, 4>> StoreInitial(const Block& block, std::uint32_t fragments, std::uint32_t pipeBankXor, bool uniform) {
    using namespace AgcDriver::Graphics;
    const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, 8u, fragments, Side, Side, 1u, pipeBankXor);
    Require(layout.Bytes() <= block.bytes, "the initial color footprint exceeds its allocation");
    StoreWords(block, block.bytes, 0x38003800u);
    std::vector<std::byte> linear(static_cast<std::size_t>(layout.LinearBytes()));
    std::vector<std::array<float, 4>> expected(Side * Side * fragments);
    for (std::uint32_t index = 0; index < expected.size(); ++index) {
        const auto sample = index % fragments;
        const auto pixel = index / fragments;
        const bool ones = uniform || ((pixel % Side) * 3u + (pixel / Side) * 5u + sample * 7u) % 2u == 0u;
        const std::array<std::uint16_t, 4> bits = ones ? std::array<std::uint16_t, 4>{0x3c00u, 0x3c00u, 0x3c00u, 0x3c00u} : std::array<std::uint16_t, 4>{0x3a00u, 0x3400u, 0x3800u, 0x3c00u};
        std::memcpy(linear.data() + index * sizeof(bits), bits.data(), sizeof(bits));
        expected[index] = ones ? Ones : Written;
    }
    layout.Tile(linear, std::as_writable_bytes(std::span(block.data, static_cast<std::size_t>(layout.Bytes()))));
    return expected;
}

VKAPI_ATTR VkResult VKAPI_CALL NoInitialStorageImage(VkPhysicalDevice physical, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    if ((flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) != 0 && (usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return imageFormatProperties(physical, format, type, tiling, usage, flags, properties);
}

int InitialReuse(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, 2u) || !InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, Samples)) return VulkanTestSkipped;
    Block reused(2u * BlockBytes);
    const auto expected = StoreInitial(reused, Samples, 0u, false);
    const std::vector<std::uint8_t> before(reused.data, reused.data + reused.bytes);
    const auto wide = TargetState(DecodeTarget(reused.Address(), 1u, {8u * Side, 4u * Side}));
    const auto narrow = TargetState(DecodeTarget(reused));
    Require(MultisampledColorBytes(wide.color) == 2u * BlockBytes && MultisampledColorBytes(narrow.color) == BlockBytes, "the reused initial color footprints differ from two blocks and one block");
    Draw(device, wide);
    device.WaitIdle();
    Require(Refused(reused.Address(), 0x1000u, "a read before initial color memory reuse"), "the live wide target is not guarded before reuse");
    Draw(device, narrow);
    CheckSamples(narrow, LoadSamples(device, reused), {}, expected);
    Require(std::equal(before.begin(), before.end(), reused.data), "initialization of a reused target changed guest color bytes or padding");
    RequireNoMultisampleTarget(reused.Address() + BlockBytes, 0x1000u, "a read of the retired target outside its replacement");
    Require(Refused(reused.Address(), 0x1000u, "a read of the replacement target after initial reuse"), "the replacement target is not guarded after reuse");
    std::puts("native color initial reuse: the replacement preserves guest samples and bytes, retires the old owner and keeps its own guard");
    return 0;
}

std::vector<std::byte> HalfSamples(std::span<const std::array<float, 4>> samples) {
    constexpr std::array<std::array<std::uint16_t, 4>, 4> bits{{{}, {0x3400u, 0x3800u, 0x3000u, 0x3c00u}, {0x3c00u, 0x3c00u, 0x3c00u, 0x3c00u}, {0x3a00u, 0x3400u, 0x3800u, 0x3c00u}}};
    constexpr std::array<std::array<float, 4>, 4> values{{{}, Drawn, Ones, Written}};
    std::vector<std::byte> result(samples.size() * sizeof(bits[0]));
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto found = std::find(values.begin(), values.end(), samples[index]);
        Require(found != values.end(), "guest MIMG returned an unexpected sample color");
        std::memcpy(result.data() + index * sizeof(bits[0]), bits[static_cast<std::size_t>(found - values.begin())].data(), sizeof(bits[0]));
    }
    return result;
}

void RequireColorSnapshotRefused(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color, const std::string& expected) {
    std::string refusal;
    try {
        static_cast<void>(AgcDriver::Graphics::ReadMultisampleTargetColorSnapshot_nid_no_patch(context, color));
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "guest color snapshot was not refused: " + refusal);
}

void CheckColorSnapshot(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const Block& block, const std::vector<std::array<float, 4>>& loaded, std::optional<std::uint32_t> padding = std::nullopt) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto* begin = reinterpret_cast<const std::byte*>(block.data);
    const std::vector<std::byte> before(begin, begin + block.bytes);
    const auto snapshot = ReadMultisampleTargetColorSnapshot_nid_no_patch(context, color);
    Require(snapshot.original == before, "guest snapshot baseline differs from the mapped original bytes");
    auto expected = before;
    if (padding) {
        for (std::size_t offset = 0; offset < expected.size(); offset += sizeof(*padding)) std::memcpy(expected.data() + offset, &*padding, sizeof(*padding));
    }
    const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, color.elementBytes, color.samples, Side, Side, 1u, color.pipeBankXor);
    layout.Tile(HalfSamples(loaded), expected);
    Require(snapshot.current == expected, "guest color snapshot differs from the MIMG samples or preserved padding");
    Require(std::equal(before.begin(), before.end(), begin), "capturing the guest color snapshot modified guest memory");
    Require(ReadHalfSamples(context, color) == loaded, "native snapshot readback disagrees with guest MIMG");
    Require(LoadSamples(device, block, color.samples, color.pipeBankXor) == loaded, "sampling after the native snapshot changed the image contents");
    std::string guard;
    try {
        RequireNoMultisampleTarget(block.Address(), 1u, "snapshot validation");
    } catch (const std::exception& error) {
        guard = error.what();
    }
    Require(guard.find("overlaps multisampled color target") != std::string::npos, "capturing a snapshot removed the native target's guest-access guard");
    Draw(device, TargetState(color));
    Require(LoadSamples(device, block, color.samples, color.pipeBankXor) == loaded, "attachment reuse after the native snapshot changed untouched samples");
}

void InitialContents(AgcDriver::VulkanDevice& device, bool snapshots = false) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::uint32_t exercised = 0;
    for (const auto fragments : {2u, 4u, 8u}) {
        if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, fragments)) continue;
        for (const auto pipeBankXor : {0u, 0x8100u}) {
            for (const bool uniform : {false, true}) {
                Block block(BlockBytes);
                auto color = DecodeTarget(block, pipeBankXor);
                color.samples = fragments;
                const auto expected = StoreInitial(block, fragments, pipeBankXor, uniform);
                const std::vector<std::uint8_t> before(block.data, block.data + block.bytes);
                const auto state = TargetState(color);
                Draw(device, state);
                const auto loaded = LoadSamples(device, block, fragments, pipeBankXor);
                CheckSamples(state, loaded, {}, expected);
                if (snapshots) CheckColorSnapshot(device, color, block, loaded);
                Require(std::equal(before.begin(), before.end(), block.data), "initialization or the draw changed guest color bytes or padding");
                std::printf("native color initial samples: fragments=%u xor=0x%x %s background survives first partial draw\n", fragments, pipeBankXor, uniform ? "uniform" : "varying pixel/fragment");
                if (snapshots && !uniform && pipeBankXor == 0x8100u) {
                    for (const auto pattern : {FillOnes, 0u}) {
                        NoteColorMetadataFill(block.Address(), block.bytes, pattern);
                        Draw(device, state);
                        const auto filled = LoadSamples(device, block, fragments, pipeBankXor);
                        CheckSamples(state, filled, pattern == FillOnes ? Ones : std::array<float, 4>{});
                        CheckColorSnapshot(device, color, block, filled, pattern);
                    }
                    StoreWords(block, block.bytes, FillOnes);
                    NoteColorMetadataFill(block.Address(), block.bytes, FillOnes);
                    Draw(device, state);
                    const auto filled = LoadSamples(device, block, fragments, pipeBankXor);
                    CheckSamples(state, filled, Ones);
                    CheckColorSnapshot(device, color, block, filled, FillOnes);
                    const auto previous = block.data[block.bytes - 1u];
                    block.data[block.bytes - 1u] ^= 0x5au;
                    RequireColorSnapshotRefused(context, color, "no longer matches its initial contents or a known full fill");
                    block.data[block.bytes - 1u] = previous;
                    auto wrong = color;
                    wrong.pipeBankXor = 0u;
                    RequireColorSnapshotRefused(context, wrong, "subresource does not match the resident target");
                    std::printf("native color snapshot: fragments=%u full fills preserve padding; changed guest bytes and mismatched XOR refused\n", fragments);
                }
                ++exercised;
            }
        }
        {
            Block block(BlockBytes);
            StoreWords(block, block.bytes, FillOnes);
            auto color = DecodeTarget(block);
            color.samples = fragments;
            auto noStorage = context;
            noStorage.storageImageMultisample = false;
            noStorage.storageImageWriteWithoutFormat = false;
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                MultisampleTargetView(noStorage, color);
            }
            const auto state = TargetState(color);
            Draw(device, state);
            const auto loaded = LoadSamples(device, block, fragments);
            CheckSamples(state, loaded, Ones);
            if (snapshots) CheckColorSnapshot(device, color, block, loaded);
            std::printf("native color initial samples: fragments=%u exact uniform clear without storage writes passed\n", fragments);
            ++exercised;
        }
        {
            Block block(BlockBytes);
            auto color = DecodeTarget(block, 0x8100u);
            color.samples = fragments;
            const auto expected = StoreInitial(block, fragments, 0x8100u, false);
            auto linear = color;
            linear.tileMode = ColorTileMode::Linear;
            RequireInitialRefused(context, linear, "initial multisample color footprint is not implemented");
            for (const auto profile : {0u, 1u, 2u}) {
                auto unsupported = context;
                if (profile == 0) unsupported.storageImageMultisample = false;
                if (profile == 1) unsupported.storageImageWriteWithoutFormat = false;
                if (profile == 2) unsupported.detiler = nullptr;
                RequireInitialRefused(unsupported, color, "require enabled formatless multisampled storage image writes");
            }
            auto noRange = context;
            noRange.limits.maxStorageBufferRange = 0u;
            RequireInitialRefused(noRange, color, "storage range cannot hold an aligned element");
            auto noImage = context;
            imageFormatProperties = context.imageFormatProperties;
            noImage.imageFormatProperties = NoInitialStorageImage;
            RequireInitialRefused(noImage, color, "cannot initialize the multisampled color target through its unsigned storage view");
            auto chunked = context;
            chunked.limits.maxStorageBufferRange = 256u;
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                MultisampleTargetView(chunked, color);
            }
            const auto state = TargetState(color);
            Draw(device, state);
            const auto loaded = LoadSamples(device, block, fragments, 0x8100u);
            CheckSamples(state, loaded, {}, expected);
            if (snapshots) CheckColorSnapshot(device, color, block, loaded);
            std::printf("native color initial samples: fragments=%u five unsupported capability profiles and linear layout refused, restored 256-byte chunks passed\n", fragments);
            ++exercised;
        }
    }
    if (snapshots) {
        Block block(BlockBytes);
        Block metadata(BlockBytes);
        auto color = DecodeTarget(block);
        color.cmaskAddress = metadata.Address();
        RequireInitialRefused(context, color, "CMASK require a modeled metadata import");
        std::puts("native color snapshot: unmodeled CMASK import refused before target creation");
        if (InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, Samples)) {
            auto completed = std::make_shared<Block>(BlockBytes);
            auto completedColor = DecodeTarget(*completed);
            StoreInitial(*completed, Samples, 0u, false);
            Draw(device, TargetState(completedColor));
            auto completionRefusals = std::make_shared<std::array<std::string, 2>>();
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                auto* recorder = context.recorder != nullptr ? context.recorder : Recorder::Active();
                Require(recorder != nullptr, "the snapshot fixture has no owning recorder");
                recorder->OnComplete([completed, completedColor, context, completionRefusals] {
                    try {
                        static_cast<void>(ReadMultisampleTargetSamples_nid_no_patch(context, completedColor));
                    } catch (const std::exception& error) {
                        (*completionRefusals)[0] = error.what();
                    }
                    try {
                        static_cast<void>(ReadMultisampleTargetColorSnapshot_nid_no_patch(context, completedColor));
                    } catch (const std::exception& error) {
                        (*completionRefusals)[1] = error.what();
                    }
                    StoreWords(*completed, completed->bytes, FillOnes);
                    NoteColorMetadataFill(completed->Address(), completed->bytes, FillOnes);
                });
                auto* recording = Recorder::Active();
                Require(recording != nullptr, "the snapshot fixture has no active recorder");
                Recorder other(context);
                other.Activate();
                RequireColorSnapshotRefused(context, completedColor, "another active recorder on the same device");
                recording->Activate();
            }
            const auto snapshot = ReadMultisampleTargetColorSnapshot_nid_no_patch(context, completedColor);
            Require(std::all_of(completionRefusals->begin(), completionRefusals->end(), [](const auto& error) { return error.find("during a recorder completion") != std::string::npos; }), "a native color snapshot recursively completed its producer");
            std::vector<std::byte> expected(completed->bytes);
            for (std::size_t offset = 0; offset < expected.size(); offset += sizeof(FillOnes)) std::memcpy(expected.data() + offset, &FillOnes, sizeof(FillOnes));
            Require(snapshot.original == expected && snapshot.current == expected, "the native snapshot captured its baseline before producer completion");
            std::puts("native color snapshot: producer completion precedes baseline capture; another active recorder remains refused");
        }
    }
    Require(exercised != 0, "no supported native color initial-content case was exercised");
}

alignas(256) std::array<std::uint32_t, 12> VertexValues{};

void DrawVertexInput(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::State& state, std::uint64_t address, std::uint32_t stride) {
    alignas(256) static constexpr std::array<std::uint32_t, 7> code{
        0xbe822100u, 0x34020a84u, 0xe0781000u, 0x80010801u, 0xf80008cfu, 0x0b0a0908u, 0xbf810000u,
    };
    VertexValues.fill(0xdeadbeefu);
    const auto output = reinterpret_cast<std::uintptr_t>(VertexValues.data());
    std::vector<std::uint32_t> userData(12u, 0u);
    userData[0] = 0x10000000u;
    userData[1] = 1u;
    userData[4] = static_cast<std::uint32_t>(output);
    userData[5] = static_cast<std::uint32_t>((output >> 32u) & 0xffffu);
    userData[6] = static_cast<std::uint32_t>(sizeof(VertexValues));
    userData[7] = 0x31016facu;
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1u;
    info.resources[0].fields = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), stride == 0u ? 16u : 3u, 0x3104dfacu};
    info.resourcesDst[0] = {8, 4, 0, 0};
    info.fetchAttribReg = 8u;
    info.fetchBufferReg = 10u;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    ShaderRecompiler::RecompileRequest vertexRequest{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64u, 0, userData, std::nullopt, std::nullopt, info, vertexMemory},
        device.Target(), {0, 0, 0, 64}
    };
    vertexRequest.useCache = false;
    const auto vertex = ShaderRecompiler::Recompile(vertexRequest);
    Require(vertex.vertexAttributes.size() == 1u, "the native vertex probe did not create a fixed-function attribute");
    const auto vertexPush = static_cast<std::uint32_t>(vertex.pushConstants.size());
    const auto pixelAddress = reinterpret_cast<std::uintptr_t>(PixelCode.data());
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{pixelAddress, std::as_bytes(std::span(PixelCode))}}};
    const AgcDriver::Registers registers{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
    ShaderRecompiler::RecompileRequest pixelRequest{
        {ShaderStage::Fragment, pixelAddress, PixelCode, 0, {}},
        {64u, 0, {}, std::nullopt, AgcDriver::Graphics::DecodePixelStageInfo(registers, AgcDriver::Graphics::ExportMappings(state)), std::nullopt, pixelMemory},
        device.Target(), PixelPushLayout(vertexPush, device.Target())
    };
    pixelRequest.useCache = false;
    const auto pixel = ShaderRecompiler::Recompile(pixelRequest);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &pixel, PixelPushOffset(vertexPush, device.Target())}}};
    const AgcDriver::Pm4::DrawParameters draw{0u, 3u, 0u, 1u, 0u, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

ShaderRecompiler::RecompileResult CompileBufferLoad(AgcDriver::VulkanDevice& device, std::uint64_t source, std::uint64_t destination) {
    const std::array<std::uint32_t, 8> userData{
        static_cast<std::uint32_t>(source), static_cast<std::uint32_t>((source >> 32u) & 0xffffu), Threads * 4u, 0x31016facu,
        static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>((destination >> 32u) & 0xffffu), Threads * 4u, 0x31016facu
    };
    const std::span<const std::uint32_t> code(BufferLoadCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void CheckBufferLoad(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& program, const Block& output, std::shared_ptr<const AgcDriver::Recipe>* recipe = nullptr) {
    const std::span words(reinterpret_cast<std::uint32_t*>(output.data), Threads * 2u);
    std::fill(words.begin(), words.end(), 0xdeadbeefu);
    device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()), nullptr, recipe);
    device.WaitIdle();
    Require(std::all_of(words.begin(), words.begin() + Threads, [](auto word) { return word == 0; }), "the nonresident typed buffer load did not read zeroes");
    Require(std::all_of(words.begin() + Threads, words.end(), [](auto word) { return word == 0xdeadbeefu; }), "the typed buffer load overwrote its output boundary");
}

template<class TAction>
void CheckBufferRefused(TAction&& action, const char* name, std::vector<std::string>& missing, std::string_view expected = "whose samples are only on the host") {
    std::string refusal;
    try {
        action();
    } catch (const std::runtime_error& error) {
        refusal = error.what();
    }
    const bool refused = refusal.find(expected) != std::string::npos;
    std::cout << "native buffer " << name << ": " << (refused ? "refused" : "missing refusal") << '\n';
    if (!refused) missing.emplace_back(std::string(name) + ": " + refusal);
}

void CheckVertexValues(std::uint64_t address, std::uint32_t stride) {
    std::array<std::uint32_t, 4> expected{};
    for (std::size_t vertex = 0; vertex < 3u; ++vertex) {
        std::memcpy(expected.data(), reinterpret_cast<const void*>(address + vertex * stride), sizeof(expected));
        for (std::size_t component = 0; component < expected.size(); ++component) {
            Require(VertexValues[vertex * 4u + component] == expected[component], "the fixed-function vertex probe did not observe the original guest bytes");
        }
    }
}

int NativeVertexInput(std::unique_ptr<AgcDriver::VulkanDevice>& device) {
    Block source(4u * BlockBytes);
    Block destination(BlockBytes);
    struct ReleaseDevice {
        std::unique_ptr<AgcDriver::VulkanDevice>& value;
        ~ReleaseDevice() { value.reset(); }
    } releaseDevice{device};
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    if (context.hostImportAlignment == 0u) {
        std::puts("native vertex input not tested: host imports unavailable");
        return VulkanTestSkipped;
    }
    const auto color = DecodeTarget(source);
    const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(color);
    Require(colorBytes >= 48u && colorBytes + 48u <= source.bytes, "the native vertex probe has no neighboring range");
    constexpr std::array<float, 4> element{0.25f, -2.0f, 7.5f, 1.0f};
    for (std::size_t vertex = 0; vertex < 3u; ++vertex) {
        std::memcpy(source.data + vertex * sizeof(element), element.data(), sizeof(element));
        std::memcpy(source.data + colorBytes + vertex * sizeof(element), element.data(), sizeof(element));
    }
    std::memcpy(source.data + colorBytes - 8u, element.data(), 8u);
    Draw(*device, TargetState(color));
    AgcDriver::Graphics::NoteColorMetadataFill(source.Address(), static_cast<std::size_t>(colorBytes), FillOnes);
    CheckUniform(LoadSamples(*device, source), Ones, "before the fixed-function vertex probe");
    const auto neighbor = source.Address() + colorBytes;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto* native = AgcDriver::Graphics::HostImportFor(context, source.Address(), 48u);
        const auto* adjacent = AgcDriver::Graphics::HostImportFor(context, neighbor, 48u);
        Require(native != nullptr && native == adjacent, "the native and neighboring vertices do not share an import");
    }
    AgcDriver::Graphics::RequireNoMultisampleTarget(neighbor, 48u, "the vertex neighbor control");
    const auto state = TargetState(DecodeTarget(destination));
    std::uint32_t missed = 0u;
    for (const auto stride : {0u, 16u}) {
        for (const bool edge : {false, true}) {
            DrawVertexInput(*device, state, neighbor, stride);
            CheckVertexValues(neighbor, stride);
            const auto address = edge ? neighbor - 8u : source.Address();
            std::string refusal;
            try {
                DrawVertexInput(*device, state, address, stride);
                CheckVertexValues(address, stride);
            } catch (const std::exception& error) {
                refusal = error.what();
            }
            const bool refused = refusal.find("whose samples are only on the host") != std::string::npos;
            Require(refusal.empty() || refused, "the native vertex input failed for another reason: " + refusal);
            std::printf("native vertex stride %u %s: refused=%u neighbor=passed original-guest-values=%u detail=%s\n", stride, edge ? "edge" : "base", refused ? 1u : 0u, refused ? 0u : 12u, refusal.c_str());
            missed += refused ? 0u : 1u;
        }
    }
    CheckUniform(LoadSamples(*device, source), Ones, "after fixed-function vertex inputs");
    std::puts("native vertex controls: shared-import=1 source-samples=1024 neighbor-values=passed");
    Require(missed == 0u, "native vertex inputs missed " + std::to_string(missed) + " resident-image refusals");
    return 0;
}

void RequirePublicationRefused(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::ColorTarget& color, const std::string& expected) {
    std::string refusal;
    try {
        AgcDriver::Graphics::PublishMultisampleTargetColor_nid_no_patch(context, color);
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find(expected) != std::string::npos, "native color publication was not refused: " + refusal);
}

int ReusedPublication(AgcDriver::VulkanDevice& device, std::string_view cause, bool automatic) {
    using namespace AgcDriver::Graphics;
    Require(cause == "live" || cause == "claimed" || cause == "written", "unknown publication reuse case");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, Samples)) return VulkanTestSkipped;
    Block block(2u * BlockBytes, true);
    if (cause == "written" && !AgcDriver::GuestMemory::Watched(block.Address(), block.bytes)) return VulkanTestSkipped;
    const auto color = DecodeTarget(block);
    StoreInitial(block, Samples, 0u, false);
    Draw(device, TargetState(color));
    const auto loaded = LoadSamples(device, block);
    device.WaitIdle();
    const auto colorBytes = static_cast<std::size_t>(MultisampledColorBytes(color));
    Require(colorBytes >= 256u && colorBytes <= block.bytes, "the publication reuse footprint does not fit its block");
    if (cause == "written") Require(AgcDriver::GuestMemory::Watched(block.Address(), colorBytes), "the publication reuse target lost write watching");
    if (cause == "claimed") ClaimMultisampleTargetMemory(color.address, colorBytes);
    if (cause == "written") std::memset(block.data + colorBytes - 256u, 0x5a, 256u);
    const auto* begin = reinterpret_cast<const std::byte*>(block.data);
    const std::vector<std::byte> before(begin, begin + block.bytes);
    auto expected = before;
    if (cause == "live") {
        const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, color.elementBytes, Samples, Side, Side, 1u, 0u);
        layout.Tile(HalfSamples(loaded), std::span(expected).first(colorBytes));
        Require(expected != before, "the live publication control has no changed native samples");
    }
    if (automatic) {
        std::array<std::byte, 16> read{};
        AgcDriver::GuestMemory::Read(block.Address(), read);
        Require(std::equal(read.begin(), read.end(), expected.begin()), "the synchronized read changed the reused or live owner's bytes");
    } else PublishMultisampleTargetColor_nid_no_patch(context, color);
    Require(std::equal(expected.begin(), expected.end(), begin), "publication overwrote reused guest data or lost live native samples");
    RequireReadbackRefused(context, color, "requires an existing resident target");
    RequireNoMultisampleTarget(block.Address(), colorBytes, "after publication of a reused or live owner");
    std::printf("native color publication reuse: %.*s, %s, complete bytes and retirement passed\n", static_cast<int>(cause.size()), cause.data(), automatic ? "CPU read" : "explicit");
    return 0;
}

void Publication(AgcDriver::VulkanDevice& device, bool automatic = false) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::uint32_t exercised = 0;
    for (const auto fragments : {2u, 4u, 8u}) {
        if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, fragments)) continue;
        for (const auto pipeBankXor : {0u, 0x8100u}) {
            for (const auto profile : {0u, 1u, 2u}) {
                Block block(2u * BlockBytes);
                auto color = DecodeTarget(block, pipeBankXor);
                color.samples = fragments;
                const auto initial = StoreInitial(block, fragments, pipeBankXor, false);
                RequirePublicationRefused(context, color, "requires an existing resident target");
                const auto state = TargetState(color);
                Draw(device, state);
                const auto colorBytes = static_cast<std::size_t>(MultisampledColorBytes(color));
                if (profile != 0u) {
                    NoteColorMetadataFill(block.Address(), colorBytes, profile == 1u ? FillOnes : 0u);
                    Draw(device, state);
                }
                const auto loaded = LoadSamples(device, block, fragments, pipeBankXor);
                if (profile == 0u) CheckSamples(state, loaded, {}, initial);
                else CheckSamples(state, loaded, profile == 1u ? Ones : std::array<float, 4>{});
                const auto* begin = reinterpret_cast<const std::byte*>(block.data);
                const std::vector<std::byte> before(begin, begin + block.bytes);
                auto expected = before;
                if (profile != 0u) {
                    const auto pattern = profile == 1u ? FillOnes : 0u;
                    for (std::size_t offset = 0; offset < colorBytes; offset += sizeof(pattern)) std::memcpy(expected.data() + offset, &pattern, sizeof(pattern));
                }
                const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, color.elementBytes, fragments, Side, Side, 1u, pipeBankXor);
                layout.Tile(HalfSamples(loaded), std::span(expected).first(colorBytes));
                if (fragments == Samples && pipeBankXor == 0u && profile == 0u) {
                    {
                        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                        auto* recording = Recorder::Active();
                        Require(recording != nullptr && context.recorder != nullptr, "the publication fixture has no owning recorder");
                        Recorder other(context);
                        other.Activate();
                        RequireReadbackRefused(context, color, "another active recorder on the same device");
                        RequirePublicationRefused(context, color, "another active recorder on the same device");
                        recording->Activate();
                    }
                    auto wrong = color;
                    wrong.pipeBankXor = 0x8100u;
                    RequirePublicationRefused(context, wrong, "subresource does not match");
                    {
                        GuestAllocations::Mutation mutation;
                        mutation.Protect(block.data + 4096u, 4096u, true, false, true, [] {});
                    }
                    RequirePublicationRefused(context, color, "requires writable registered guest memory");
                    {
                        GuestAllocations::Mutation mutation;
                        mutation.Protect(block.data + 4096u, 4096u, true, true, true, [] {});
                    }
                    block.data[colorBytes - 1u] ^= 0x5au;
                    RequirePublicationRefused(context, color, "no longer matches its initial contents or a known full fill");
                    block.data[colorBytes - 1u] ^= 0x5au;
                    Require(std::equal(before.begin(), before.end(), begin), "refused publication changed guest bytes");
                    std::string guard;
                    try {
                        RequireNoMultisampleTarget(block.Address(), 1u, "publication refusal validation");
                    } catch (const std::exception& error) {
                        guard = error.what();
                    }
                    Require(guard.find("overlaps multisampled color target") != std::string::npos, "refused publication lost its resident guard");
                }
                std::shared_ptr<void> retained;
                VkImage oldImage = VK_NULL_HANDLE;
                {
                    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                    MultisampleTargetView(context, color, retained, &oldImage);
                }
                std::vector<std::byte> read(block.bytes);
                if (automatic) {
                    std::array<std::byte, 128> adjacent{};
                    AgcDriver::GuestMemory::Read(block.Address() + colorBytes, adjacent);
                    Require(ReadMultisampleTargetSamples_nid_no_patch(context, color).size() == Side * Side * fragments * 8u, "an adjacent CPU read retired the color owner");
                    if (profile == 0u) AgcDriver::GuestMemory::Read(block.Address(), read);
                    else if (profile == 1u) AgcDriver::GuestMemory::ReadCommitted(block.Address(), read);
                    else AgcDriver::GuestMemory::ReadCommittedUnsynced(block.Address(), read);
                } else {
                    PublishMultisampleTargetColor_nid_no_patch(context, color);
                    AgcDriver::GuestMemory::Read(block.Address(), read);
                }
                RequireNoMultisampleTarget(block.Address(), colorBytes, "after explicit publication");
                RequireReadbackRefused(context, color, "requires an existing resident target");
                Require(read == expected, "published guest color differs from MIMG samples, padding or neighboring bytes");
                Draw(device, state);
                Require(LoadSamples(device, block, fragments, pipeBankXor) == loaded, "a fresh target did not import the published samples");
                VkImage newImage = VK_NULL_HANDLE;
                {
                    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                    MultisampleTargetView(context, color, &newImage);
                }
                Require(newImage != oldImage, "publication reused the retired native image");
                std::printf("native color publication: fragments=%u xor=0x%x profile=%u complete bytes, bounds, retirement and reimport passed\n", fragments, pipeBankXor, profile);
                ++exercised;
            }
        }
    }
    Require(exercised != 0u, "no supported native color publication case was exercised");
    if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, Samples)) return;
    auto completed = std::make_shared<Block>(BlockBytes);
    auto color = DecodeTarget(*completed);
    StoreInitial(*completed, Samples, 0u, false);
    Draw(device, TargetState(color));
    const auto completionRefusal = std::make_shared<std::string>();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        auto* recorder = context.recorder != nullptr ? context.recorder : Recorder::Active();
        Require(recorder != nullptr, "the publication fixture has no owning recorder");
        recorder->OnComplete([completed, color, context, automatic, completionRefusal] {
            try {
                if (automatic) {
                    std::array<std::byte, 4> word{};
                    AgcDriver::GuestMemory::Read(completed->Address(), word);
                } else PublishMultisampleTargetColor_nid_no_patch(context, color);
            } catch (const std::exception& error) {
                *completionRefusal = error.what();
            }
            StoreWords(*completed, completed->bytes, FillOnes);
            NoteColorMetadataFill(completed->Address(), completed->bytes, FillOnes);
        });
    }
    if (!automatic) PublishMultisampleTargetColor_nid_no_patch(context, color);
    std::vector<std::uint32_t> read(completed->bytes / sizeof(std::uint32_t));
    AgcDriver::GuestMemory::Read(completed->Address(), std::as_writable_bytes(std::span(read)));
    Require(completionRefusal->find("during a recorder completion") != std::string::npos, "recursive publication from a completion was not refused: " + *completionRefusal);
    Require(std::all_of(read.begin(), read.end(), [](const auto word) { return word == FillOnes; }), "publication captured its guest baseline before producer completions");
    std::puts("native color publication: known full fill from a producer completion precedes the publication baseline; recursive publication stays refused");
}

void BufferAdjacentControls(std::unique_ptr<AgcDriver::VulkanDevice>& device) {
    Block block(4u * BlockBytes);
    Block output(2u * 1024u * 1024u);
    struct ReleaseDevice {
        decltype(device)& value;
        ~ReleaseDevice() { value.reset(); }
    } releaseDevice{device};
    auto color = DecodeTarget(block);
    color.tileMode = AgcDriver::Graphics::ColorTileMode::Standard4KB;
    color.bytes = AgcDriver::Graphics::ColorTargetLayout(Side, Side, color.tileMode, color.elementBytes).Bytes();
    const auto state = TargetState(color);
    Draw(*device, state);
    CheckSamples(state, LoadSamples(*device, block), {});
    const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(color);
    Require(colorBytes != 0 && colorBytes <= block.bytes, "the 4x target's color memory does not fit its block");
    AgcDriver::Graphics::NoteColorMetadataFill(block.Address(), static_cast<std::size_t>(colorBytes), FillOnes);
    CheckUniform(LoadSamples(*device, block), Ones, "after a fill of the target's color memory with half-float ones");
    const auto guardedBytes = std::max(color.bytes * color.samples, static_cast<std::uint64_t>(Side) * Side * color.elementBytes * color.samples);
    Require(guardedBytes + Threads * 4u <= block.bytes, "the target guard leaves no adjacent buffer control");
    CheckBufferLoad(*device, CompileBufferLoad(*device, block.Address() + guardedBytes, output.Address()), output);
    CheckUniform(LoadSamples(*device, block), Ones, "after the adjacent buffer control");
    std::puts("native buffer controls: adjacent read passed without changing native samples");
}

void BufferPublication(std::unique_ptr<AgcDriver::VulkanDevice>& device) {
    using namespace AgcDriver::Graphics;
    Block block(4u * BlockBytes);
    Block output(2u * 1024u * 1024u);
    struct ReleaseDevice {
        std::unique_ptr<AgcDriver::VulkanDevice>& value;
        ~ReleaseDevice() { value.reset(); }
    } releaseDevice{device};
    const auto color = DecodeTarget(block);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    const auto bytes = static_cast<std::size_t>(MultisampledColorBytes(color));
    const auto warm = CompileBufferLoad(*device, block.Address(), output.Address());
    std::shared_ptr<const AgcDriver::Recipe> recipe;
    const auto check = [&](const auto& action, bool edge, std::uint32_t value) {
        const std::span words(reinterpret_cast<std::uint32_t*>(output.data), Threads * 2u);
        std::fill(words.begin(), words.end(), 0xdeadbeefu);
        action();
        device->WaitIdle();
        for (std::size_t index = 0; index < Threads; ++index) Require(words[index] == (edge && index != 0u ? 0u : value), "a native buffer consumer read stale or reordered color words at lane " + std::to_string(index));
        Require(std::all_of(words.begin() + Threads, words.end(), [](auto word) { return word == 0xdeadbeefu; }), "a native buffer consumer changed its output boundary");
    };
    const auto dispatch = [&](const auto& program) { device->Dispatch(program, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data())); };
    check([&] { device->Dispatch(warm, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()), nullptr, &recipe); }, false, 0u);
    Require(recipe != nullptr, "the buffer publication fixture did not capture a recipe");
    const auto hit = device->PrepareRecipe(recipe, false);
    Require(hit != nullptr, "the buffer publication fixture did not prepare a recipe hit");
    const auto deferredProgram = CompileBufferLoad(*device, block.Address() + Threads * 4u, output.Address());
    const auto prepared = device->PrepareDispatch(deferredProgram, {}, Threads, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()));
    Require(prepared != nullptr, "the buffer publication fixture did not prepare a deferred dispatch");
    const std::vector<std::byte> zeros(bytes);
    const auto arm = [&] {
        AgcDriver::GuestMemory::Write(block.Address(), zeros);
        Draw(*device, TargetState(color));
        NoteColorMetadataFill(block.Address(), bytes, FillOnes);
        CheckUniform(LoadSamples(*device, block), Ones, "before a native buffer consumer");
    };
    arm();
    check([&] { dispatch(CompileBufferLoad(*device, block.Address() + bytes, output.Address())); }, false, 0u);
    Require(ReadMultisampleTargetSamples_nid_no_patch(context, color).size() == Side * Side * Samples * 8u, "a neighboring logical buffer retired the native owner");
    CheckUniform(LoadSamples(*device, block), Ones, "after the neighboring logical buffer control");
    check([&] { dispatch(warm); }, false, FillOnes);
    RequireNoMultisampleTarget(block.Address(), bytes, "after warmed buffer publication");
    std::puts("native buffer publication: adjacent range retains owner; warmed dispatch reads native words");
    arm();
    check([&] { dispatch(CompileBufferLoad(*device, block.Address() + 4u, output.Address())); }, false, FillOnes);
    std::puts("native buffer publication: fresh dispatch reads native words");
    arm();
    check([&] { dispatch(CompileBufferLoad(*device, block.Address() + bytes - 4u, output.Address())); }, true, FillOnes);
    std::puts("native buffer publication: edge range preserves the neighboring words");
    arm();
    check([&] { device->Dispatch(deferredProgram, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()), prepared); }, false, FillOnes);
    std::puts("native buffer publication: deferred dispatch refreshes the captured input");
    arm();
    check([&] {
        AgcDriver::VulkanDevice::IndirectOutcome outcome{};
        device->DispatchRecipe(warm, 1u, 1u, 1u, 0u, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()), hit, outcome);
    }, false, FillOnes);
    std::puts("native buffer publication: recipe dispatch reads native words");

}

ShaderRecompiler::RecompileResult CompileMovedVertex(AgcDriver::VulkanDevice& device, std::uint64_t address) {
    const std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangle.size()), 0x01016facu};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, memory},
        device.Target(), {0, 0, 0, 64}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::vector<std::array<std::uint32_t, 4>> RasterMovedSnapshot(const AgcDriver::Graphics::Context& context, const AgcDriver::Graphics::State& state, std::span<const AgcDriver::Graphics::CompiledShader> shaders, const AgcDriver::Graphics::ShaderResources& resources, const AgcDriver::Graphics::ShaderResources::DrawBindings& prepared) {
    using namespace AgcDriver::Graphics;
    const auto vertexInput = BuildVertexInputLayout(context, shaders.front().program->vertexAttributes);
    Require(vertexInput.bindings.empty(), "the moved raster shader does not fetch positions through its guest buffer");
    constexpr auto pixelBytes = sizeof(std::array<std::uint32_t, 4>);
    constexpr auto resultBytes = Side * Side * pixelBytes;
    AgcDriver::Graphics::Buffer staging(context, resultBytes + 2u * pixelBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::fill(staging.Bytes().begin(), staging.Bytes().end(), std::byte{0xa5});
    auto target = std::make_shared<RenderTarget>(context, state.color, false);
    Pipeline pipeline(context, state, vertexInput, resources, shaders, VK_IMAGE_LAYOUT_GENERAL);
    const std::array views{target->View()};
    const std::array<std::shared_ptr<void>, 1> owners{target};
    const auto framebuffer = pipeline.AcquireFramebuffer(views, owners, state.renderExtent);
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    VkImageMemoryBarrier initialize{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    initialize.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    initialize.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    initialize.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    initialize.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    initialize.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    initialize.image = target->Image();
    initialize.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &initialize);
    VkClearColorValue clear{};
    context.Resolved(&DeviceFunctions::cmdClearColorImage, "vkCmdClearColorImage")(commands, target->Image(), VK_IMAGE_LAYOUT_GENERAL, &clear, 1u, &initialize.subresourceRange);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
    pipeline.Begin(commands, *framebuffer, state.renderExtent, state);
    auto pushes = AssemblePushConstants(shaders);
    resources.PatchPushConstants(pushes);
    pipeline.PushConstants(commands, PushConstantStages(shaders), pushes);
    if (prepared.allocation.set != VK_NULL_HANDLE) context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Layout(), 0u, 1u, &prepared.allocation.set, 0u, nullptr);
    if (prepared.fragmentAllocation.set != VK_NULL_HANDLE) context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Layout(), ShaderRecompiler::RuntimeAbi::FragmentDescriptorSet, 1u, &prepared.fragmentAllocation.set, 0u, nullptr);
    context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(commands, static_cast<std::uint32_t>(Triangle.size()), 1u, 0u, 0u);
    EndRenderPass(context, commands);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy copy{};
    copy.bufferOffset = pixelBytes;
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    copy.imageExtent = {Side, Side, 1u};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, target->Image(), VK_IMAGE_LAYOUT_GENERAL, staging.Handle(), 1u, &copy);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    batch.SubmitAndWait();
    staging.Invalidate();
    const auto bytes = staging.Bytes();
    Require(std::all_of(bytes.begin(), bytes.begin() + pixelBytes, [](auto value) { return value == std::byte{0xa5}; }) && std::all_of(bytes.end() - pixelBytes, bytes.end(), [](auto value) { return value == std::byte{0xa5}; }), "moved raster readback changed its outer sentinels");
    std::vector<std::array<std::uint32_t, 4>> result(Side * Side);
    std::memcpy(result.data(), bytes.data() + pixelBytes, resultBytes);
    return result;
}

void CheckRecipePixels(AgcDriver::VulkanDevice& device, const Block& destination, const AgcDriver::Graphics::ColorTarget& target, std::uint32_t word) {
    device.WaitIdle();
    std::vector<std::byte> tiled(destination.bytes);
    AgcDriver::GuestMemory::Read(destination.Address(), tiled);
    const AgcDriver::Graphics::ColorTargetLayout layout(Side, Side, target.tileMode, target.elementBytes);
    Require(layout.Bytes() == target.bytes && target.bytes <= tiled.size(), "the recipe output footprint is invalid");
    Require(std::all_of(tiled.begin() + target.bytes, tiled.end(), [](auto value) { return value == std::byte{0xa5}; }), "the recipe output changed its neighboring bytes");
    std::vector<std::array<std::uint32_t, 4>> pixels(Side * Side);
    layout.Detile(std::span(tiled).first(target.bytes), std::as_writable_bytes(std::span(pixels)));
    const std::array<std::uint32_t, 4> drawn{word, 0u, 0u, std::bit_cast<std::uint32_t>(1.0f)};
    std::uint32_t checked = 0u;
    std::uint32_t covered = 0u;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            const auto& pixel = pixels[y * Side + x];
            Require(pixel == drawn || pixel == std::array<std::uint32_t, 4>{}, "the recipe draw rendered stale or unexpected buffer bits at pixel " + std::to_string(y * Side + x) + ": expected red " + std::to_string(word) + ", got " + std::to_string(pixel[0]) + "," + std::to_string(pixel[1]) + "," + std::to_string(pixel[2]) + "," + std::to_string(pixel[3]));
            covered += pixel == drawn ? 1u : 0u;
            const auto distance = std::min({EdgeDistance(x + 0.5, y + 0.5, 0u), EdgeDistance(x + 0.5, y + 0.5, 1u), EdgeDistance(x + 0.5, y + 0.5, 2u)});
            if (std::fabs(distance) < EdgeMargin) continue;
            ++checked;
            Require((pixel == drawn) == (distance > 0.0), "the recipe result disagrees with triangle coverage");
        }
    }
    Require(checked >= Side * Side * 9u / 10u && covered != 0u && covered != pixels.size(), "the recipe fixture did not check covered and clear pixels");
    std::printf("native draw recipe: word=0x%08x covered=%u checked=%u; pixels and neighboring bytes passed\n", word, covered, checked);
}

void DrawRecipePublication(std::unique_ptr<AgcDriver::VulkanDevice>& device) {
    using namespace AgcDriver::Graphics;
    Block source(4u * BlockBytes);
    Block movedSource(4u * BlockBytes);
    Block destination(2u * BlockBytes);
    Block vertexData(BlockBytes);
    struct ReleaseDevice {
        std::unique_ptr<AgcDriver::VulkanDevice>& value;
        ~ReleaseDevice() { value.reset(); }
    } releaseDevice{device};
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    Require(context.recorder != nullptr, "the draw recipe fixture has no recorder");
    StoreWords(source, source.bytes, FillOnes);
    StoreWords(movedSource, movedSource.bytes, FillOnes);
    std::memcpy(vertexData.data, Triangle.data(), sizeof(Triangle));
    ColorTarget output{};
    output.address = destination.Address();
    output.surfaceAddress = output.address;
    output.extent = {Side, Side};
    output.surfaceExtent = output.extent;
    output.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    output.componentMapping = 0xe4u;
    output.elementBytes = 16u;
    output.tileMode = ColorTileMode::RenderTarget;
    output.bytes = ColorTargetLayout(Side, Side, output.tileMode, output.elementBytes).Bytes();
    Require(output.bytes < destination.bytes, "the recipe target has no neighboring sentinel range");
    std::fill(destination.data + output.bytes, destination.data + destination.bytes, std::uint8_t{0xa5});
    const auto state = TargetState(output);
    auto vertex = CompileMovedVertex(*device, vertexData.Address());
    alignas(256) static constexpr std::array<std::uint32_t, 9> code{
        0x7e020280u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x7e000280u, 0x7e0602f2u, 0xf800180fu, 0x03010002u, 0xbf810000u,
    };
    const auto address = source.Address() + 4u;
    const std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), Threads * 4u, 0x31016facu};
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{codeAddress, std::as_bytes(std::span(code))}}};
    const AgcDriver::Registers registers{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
    const auto vertexPush = static_cast<std::uint32_t>(vertex.pushConstants.size());
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Fragment, codeAddress, code, 0, {}},
        {64, 0, userData, std::nullopt, DecodePixelStageInfo(registers, ExportMappings(state)), std::nullopt, memory},
        device->Target(), PixelPushLayout(vertexPush, device->Target())
    };
    request.useCache = false;
    auto fragment = ShaderRecompiler::Recompile(request);
    const auto binding = std::find_if(fragment.bindings.begin(), fragment.bindings.end(), [](const auto& entry) { return entry.role == ShaderRecompiler::DescriptorRole::GuestBuffers; });
    Require(binding != fragment.bindings.end() && binding->count == 1u && binding->guestDescriptor.size() == 4u && binding->bufferWritten.size() == 1u && !binding->bufferWritten[0], "the recipe fragment shader does not have one read-only guest buffer");
    const std::array<CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &fragment, PixelPushOffset(vertexPush, device->Target())}}};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
    device->Draw(state, draw, shaders, {}, &recipe);
    Require(recipe != nullptr, "the initial buffer draw did not capture a recipe");
    CheckRecipePixels(*device, destination, output, FillOnes);
    Require(device->DrawFromRecipe(state, draw, shaders, {}, recipe) == AgcDriver::RecipeOutcome::Recorded, "the ordinary buffer draw recipe did not replay");
    CheckRecipePixels(*device, destination, output, FillOnes);
    const auto arm = [&](const Block& block, std::uint32_t fill) {
        const auto color = DecodeTarget(block);
        Draw(*device, TargetState(color));
        NoteColorMetadataFill(block.Address(), static_cast<std::size_t>(MultisampledColorBytes(color)), fill);
        CheckUniform(LoadSamples(*device, block), fill == FillOnes ? Ones : std::array<float, 4>{}, "before recipe source writes");
        StoreSamples(*device, TargetDescriptor(block, SampleShift), SampleWrites([](std::uint32_t, std::uint32_t y, std::uint32_t sample) { return y < 2u && sample == 0u; }));
        const auto loaded = LoadSamples(*device, block);
        const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, color.elementBytes, Samples, Side, Side, 1u, 0u);
        std::vector<std::byte> expected(static_cast<std::size_t>(layout.Bytes()));
        for (std::size_t offset = 0; offset < expected.size(); offset += sizeof(fill)) std::memcpy(expected.data() + offset, &fill, sizeof(fill));
        layout.Tile(HalfSamples(loaded), expected);
        for (std::size_t offset = 0; offset < expected.size(); offset += sizeof(FillOnes)) Require(std::memcmp(block.data + offset, &FillOnes, sizeof(FillOnes)) == 0, "the native recipe source was published early");
        return expected;
    };
    const auto checkSource = [&](const Block& block, const std::vector<std::byte>& expected) {
        RequireNoMultisampleTarget(block.Address(), expected.size(), "after a recipe consumer");
        Require(std::equal(expected.begin(), expected.end(), reinterpret_cast<const std::byte*>(block.data)), "the recipe consumer published incorrect source bytes or padding");
    };
    const auto consume = [&](bool replay) {
        if (!replay) {
            device->Draw(state, draw, shaders, {}, &recipe);
            std::puts("native draw routing: a moved descriptor uses the ordinary draw path");
            return;
        }
        const auto outcome = device->DrawFromRecipe(state, draw, shaders, {}, recipe);
        Require(outcome == AgcDriver::RecipeOutcome::Recorded, "the unchanged guarded input did not replay from its recipe");
        std::puts("native draw recipe: guarded input recorded from recipe");
    };
    const auto expected = arm(source, FillOnes);
    std::uint32_t word = 0u;
    std::memcpy(&word, expected.data() + 4u, sizeof(word));
    Require(word != FillOnes, "the guarded recipe source did not change its loaded word");
    consume(true);
    CheckRecipePixels(*device, destination, output, word);
    checkSource(source, expected);
    AgcDriver::GuestMemory::StoreOwnBytes(source.Address(), source.bytes, [&] { StoreWords(source, source.bytes, FillOnes); });
    device->Draw(state, draw, shaders, {}, &recipe);
    Require(recipe != nullptr, "the restored ordinary buffer draw did not capture a recipe");
    CheckRecipePixels(*device, destination, output, FillOnes);
    auto oldColor = DecodeTarget(source);
    oldColor.tileMode = ColorTileMode::Standard4KB;
    oldColor.bytes = ColorTargetLayout(Side, Side, oldColor.tileMode, oldColor.elementBytes).Bytes();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        MultisampleTargetView(context, oldColor);
    }
    const auto retainedRecipe = recipe;
    const auto retainedTemplate = retainedRecipe->templateRef.lock();
    Require(retainedTemplate != nullptr, "the moved draw lost its warmed resource template");
    const auto movedExpected = arm(movedSource, 0u);
    const auto movedAddress = movedSource.Address() + 8u;
    binding->guestDescriptor[0] = static_cast<std::uint32_t>(movedAddress);
    binding->guestDescriptor[1] = (binding->guestDescriptor[1] & 0xffff0000u) | static_cast<std::uint32_t>((movedAddress >> 32u) & 0xffffu);
    std::uint32_t movedWord = 0u;
    std::memcpy(&movedWord, movedExpected.data() + 8u, sizeof(movedWord));
    Require(movedWord != FillOnes && movedWord != word, "the moved recipe source did not distinguish current and old buffers");
    consume(false);
    Require(SharedResourceCache().Find(retainedRecipe->key) == retainedTemplate, "the moved draw rebuilt its template instead of rebinding it");
    CheckRecipePixels(*device, destination, output, movedWord);
    checkSource(movedSource, movedExpected);
    std::string refusal;
    try {
        RequireNoMultisampleTarget(source.Address(), Threads * 4u, "the unused original recipe buffer");
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    Require(refusal.find("overlaps multisampled color target") != std::string::npos, "the moved recipe consumed or retired its unused original range");
    std::puts("native draw recipe: current source, moved source, old guard, complete bytes and output bounds passed");
}

alignas(256) constexpr std::array<std::uint32_t, 10> NativeFmaskCode{0x34020084u, 0xe0381000u, 0x80000201u, 0xbf8c3f70u, 0xf0001f08u, 0x00020602u, 0xbf8c3f70u, 0xe0781000u, 0x80010601u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, Threads * 4u> NativeFmaskInput{};
alignas(256) std::array<std::uint32_t, Threads * 4u + 4u> NativeFmaskOutput{};

std::array<std::uint32_t, 8> NativeFmaskDescriptor(const Block& fmask, const Block& cmask, std::uint32_t pipeXor) {
    const auto address = fmask.Address() | pipeXor;
    const auto metadata = cmask.Address();
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (166u << 20u) | (((Side - 1u) & 3u) << 30u), ((Side - 1u) >> 2u) | ((Side - 1u) << 14u), 4u | (1u << 3u) | (4u << 6u) | (24u << 20u) | (9u << 28u), 0u, 0u, (1u << 19u) | (1u << 21u) | (static_cast<std::uint32_t>(metadata >> 8u) << 24u), static_cast<std::uint32_t>(metadata >> 16u)};
}

ShaderRecompiler::RecompileResult CompileNativeFmask(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& image, bool registration) {
    std::vector<std::uint32_t> userData(16u);
    const auto buffer = [](const void* data, std::uint32_t bytes) {
        const auto address = reinterpret_cast<std::uintptr_t>(data);
        return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, bytes, 0x31016facu};
    };
    const auto input = buffer(NativeFmaskInput.data(), sizeof(NativeFmaskInput));
    const auto output = buffer(NativeFmaskOutput.data(), Threads * 16u);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4u);
    std::copy(image.begin(), image.end(), userData.begin() + 8u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(NativeFmaskCode.data()), std::as_bytes(std::span(NativeFmaskCode))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    auto target = device.Target();
    target.registrationModules = registration;
    ShaderRecompiler::RecompileRequest request{{ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(NativeFmaskCode.data()), NativeFmaskCode, 0u, {}}, {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory}, target, {0u, 0u, 0u, 128u}};
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void DispatchNativeFmask(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, 8>& image, bool registration) {
    device.Dispatch(CompileNativeFmask(device, image, registration), 1u, 1u, 1u);
    device.WaitIdle();
}

void NativeFmask(std::unique_ptr<AgcDriver::VulkanDevice>& device) {
    using namespace AgcDriver::Graphics;
    Block colorMemory(4u * BlockBytes, true);
    Block dcc(BlockBytes, true);
    Block cmask(BlockBytes, true);
    Block fmask(BlockBytes, true);
    Block unrelated(BlockBytes, true);
    struct ReleaseDevice {
        std::unique_ptr<AgcDriver::VulkanDevice>& value;
        ~ReleaseDevice() { value.reset(); }
    } releaseDevice{device};
    constexpr std::uint32_t sentinel = 0xdeadbeefu;
    for (const auto pipeXor : {0u, 0xf00u}) {
        std::memset(colorMemory.data, 0, colorMemory.bytes);
        std::memset(dcc.data, 0, dcc.bytes);
        std::memset(cmask.data, 0xcc, cmask.bytes);
        std::memset(fmask.data, 0x11, fmask.bytes);
        AgcDriver::Registers cx{{0x318, static_cast<std::uint32_t>(colorMemory.Address() >> 8u)}, {0x31b, 0u}, {0x31c, Float16x4Info | 0x10006000u}, {0x31d, (3u << 12u) | (3u << 15u)}, {0x31e, 0u}, {0x31f, static_cast<std::uint32_t>(cmask.Address() >> 8u)}, {0x321, static_cast<std::uint32_t>((fmask.Address() | pipeXor) >> 8u)}, {0x325, static_cast<std::uint32_t>(dcc.Address() >> 8u)}, {0x390, static_cast<std::uint32_t>(colorMemory.Address() >> 40u)}, {0x398, static_cast<std::uint32_t>(cmask.Address() >> 40u)}, {0x3a0, static_cast<std::uint32_t>(fmask.Address() >> 40u)}, {0x3a8, static_cast<std::uint32_t>(dcc.Address() >> 40u)}, {0x3b0, ((Side - 1u) << 14u) | (Side - 1u)}, {0x3b8, TiledAttrib3}};
        const auto color = DecodeColorBuffer(cx, 0u);
        Require(color.samples == 8u && color.cmaskAddress == cmask.Address(), "native FMASK fixture lost its decoded target association");
        std::array<std::array<float, 4>, 8> expected{};
        for (std::uint32_t sample = 0u; sample < 8u; ++sample) {
            expected[sample] = {static_cast<float>(sample + 1u) / 8.0f, 0.25f, 0.5f, 1.0f};
            auto pixel = PixelCode;
            pixel[1] = std::bit_cast<std::uint32_t>(expected[sample][0]);
            pixel[3] = std::bit_cast<std::uint32_t>(expected[sample][1]);
            pixel[5] = std::bit_cast<std::uint32_t>(expected[sample][2]);
            pixel[7] = std::bit_cast<std::uint32_t>(expected[sample][3]);
            auto state = TargetState(color);
            state.quadSampleMask.fill(static_cast<std::uint16_t>(1u << sample));
            Draw(*device, state, 0u, 2u, pixel);
        }
        const auto native = LoadSamples(*device, TargetDescriptor(colorMemory, 3u), 8u);
        std::vector<std::uint32_t> covered;
        for (std::uint32_t pixel = 0; pixel < Side * Side; ++pixel) {
            bool all = true;
            for (std::size_t sample = 0; sample < 8u; ++sample) all = all && native[pixel * 8u + sample] == expected[sample];
            if (all) covered.push_back(pixel);
        }
        Require(!covered.empty(), "native FMASK fixture did not render eight distinct sample colors");
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto pixel = covered[lane % covered.size()];
            NativeFmaskInput[lane * 4u] = pixel % Side;
            NativeFmaskInput[lane * 4u + 1u] = pixel / Side;
            NativeFmaskInput[lane * 4u + 2u] = 0u;
            NativeFmaskInput[lane * 4u + 3u] = 0u;
        }
        const auto image = NativeFmaskDescriptor(fmask, cmask, pipeXor);
        const auto check = [&] {
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                const std::array<std::uint32_t, 4> mapping{0x76543210u, 1u, 0x76543210u, 0u};
                for (std::size_t component = 0; component < 4u; ++component) Require(NativeFmaskOutput[lane * 4u + component] == mapping[component], "native FMASK load did not preserve its expanded sample indices and selectors");
                for (std::uint32_t sample = 0; sample < 8u; ++sample) {
                    const auto fragment = NativeFmaskOutput[lane * 4u] >> (4u * sample) & 15u;
                    const auto pixel = NativeFmaskInput[lane * 4u + 1u] * Side + NativeFmaskInput[lane * 4u];
                    Require(native[pixel * 8u + fragment] == expected[sample], "native FMASK remapped a sampled color to another fragment");
                }
            }
            Require(std::all_of(NativeFmaskOutput.begin() + Threads * 4u, NativeFmaskOutput.end(), [](auto word) { return word == sentinel; }), "native FMASK wrote beyond its output");
        };
        for (const auto registration : {false, true}) {
            for (std::uint32_t repeat = 0; repeat < 2u; ++repeat) {
                NativeFmaskOutput.fill(sentinel);
                DispatchNativeFmask(*device, image, registration);
                check();
            }
            const auto program = CompileNativeFmask(*device, image, registration);
            const auto codeAddress = reinterpret_cast<std::uintptr_t>(NativeFmaskCode.data());
            std::shared_ptr<const AgcDriver::Recipe> recipe;
            NativeFmaskOutput.fill(sentinel);
            device->Dispatch(program, 1u, 1u, 1u, {}, codeAddress, nullptr, &recipe);
            device->WaitIdle();
            check();
            Require(recipe == nullptr, "native FMASK emitted a reusable recipe without an ownership proof");
            const auto prepared = device->PrepareDispatch(program, {}, Threads, codeAddress);
            Require(prepared != nullptr, "native FMASK did not prepare a deferred dispatch");
            NativeFmaskOutput.fill(sentinel);
            device->Dispatch(program, 1u, 1u, 1u, {}, codeAddress, prepared);
            device->WaitIdle();
            check();
        }
        Require(LoadSamples(*device, TargetDescriptor(colorMemory, 3u), 8u) == native, "FMASK sampling changed the native samples");
        Require(std::all_of(fmask.data, fmask.data + fmask.bytes, [](auto byte) { return byte == 0x11u; }) && std::all_of(cmask.data, cmask.data + cmask.bytes, [](auto byte) { return byte == 0xccu; }), "native FMASK sampling published or changed guest metadata");
        const auto refused = [&](const auto& descriptor, std::string_view reason) {
            NativeFmaskOutput.fill(sentinel);
            std::string error;
            try { DispatchNativeFmask(*device, descriptor, false); } catch (const std::runtime_error& e) { error = e.what(); }
            Require(error.find(reason) != std::string::npos, "native FMASK missing refusal: " + error);
            Require(std::all_of(NativeFmaskOutput.begin(), NativeFmaskOutput.end(), [sentinel](auto word) { return word == sentinel; }), "rejected native FMASK wrote output");
        };
        refused(NativeFmaskDescriptor(unrelated, cmask, pipeXor), "descriptor does not match");
        refused(NativeFmaskDescriptor(fmask, unrelated, pipeXor), "descriptor does not match");
        refused(NativeFmaskDescriptor(fmask, cmask, pipeXor ^ 0x100u), "descriptor does not match");
        auto resized = image;
        resized[2] += 1u;
        refused(resized, "extent or subresource does not match");
        const auto deferredProgram = CompileNativeFmask(*device, image, false);
        const auto codeAddress = reinterpret_cast<std::uintptr_t>(NativeFmaskCode.data());
        const auto preparedBeforeStore = device->PrepareDispatch(deferredProgram, {}, Threads, codeAddress);
        Require(preparedBeforeStore != nullptr, "native FMASK did not prepare its metadata invalidation probe");
        std::array<std::byte, 4> same{};
        same.fill(std::byte{0x11});
        AgcDriver::GuestMemory::Write(fmask.Address(), same);
        refused(image, "writes to a resident native FMASK");
        NativeFmaskOutput.fill(sentinel);
        std::string deferredError;
        try {
            device->Dispatch(deferredProgram, 1u, 1u, 1u, {}, codeAddress, preparedBeforeStore);
            device->WaitIdle();
        } catch (const std::runtime_error& error) {
            deferredError = error.what();
        }
        Require(deferredError.find("writes to a resident native FMASK") != std::string::npos, "deferred native FMASK missed its metadata store: " + deferredError);
        Require(std::all_of(NativeFmaskOutput.begin(), NativeFmaskOutput.end(), [](auto word) { return word == sentinel; }), "rejected deferred native FMASK wrote output");
        device->SubmitRecorded(true);
        device->WaitIdle();
        ClearMultisampleTargets(device->Device());
        ClearCachedTextures(device->Device());
        cx[0x31c] &= ~0x2000u;
        cx[0x31f] = 0u;
        cx[0x398] = 0u;
        const auto withoutCmask = DecodeColorBuffer(cx, 0u);
        Require(withoutCmask.cmaskAddress == 0u, "the no-CMASK control retained metadata");
        Draw(*device, TargetState(withoutCmask));
        const auto uncompressed = LoadSamples(*device, TargetDescriptor(colorMemory, 3u), 8u);
        Require(std::find(uncompressed.begin(), uncompressed.end(), Drawn) != uncompressed.end(), "tracking native FMASK broke ordinary DCC rendering without CMASK");
        device->SubmitRecorded(true);
        device->WaitIdle();
        ClearMultisampleTargets(device->Device());
        ClearCachedTextures(device->Device());
        Draw(*device, TargetState(color));
        auto recreated = color;
        recreated.pipeBankXor ^= 0x4000u;
        recreated.fmaskAddress = 0u;
        recreated.fmaskPipeBankXor = 0u;
        Draw(*device, TargetState(recreated));
        std::array<std::byte, 4> formerFmask{};
        formerFmask.fill(std::byte{0x22});
        AgcDriver::GuestMemory::Write(fmask.Address(), formerFmask);
        Draw(*device, TargetState(recreated));
        auto recreatedDescriptor = TargetDescriptor(colorMemory, 3u);
        recreatedDescriptor[0] |= recreated.pipeBankXor >> 8u;
        const auto recreatedSamples = LoadSamples(*device, recreatedDescriptor, 8u);
        Require(std::find(recreatedSamples.begin(), recreatedSamples.end(), Drawn) != recreatedSamples.end(), "former FMASK metadata affected the recreated native target");
        device->SubmitRecorded(true);
        device->WaitIdle();
        ClearMultisampleTargets(device->Device());
        ClearCachedTextures(device->Device());
        std::printf("native FMASK: XOR=0x%x, eight distinct samples, both registration modes, cached reuse, association/extent/write refusals and unchanged guest metadata passed\n", pipeXor);
    }
}

}

int main(int argc, char** argv) {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 4 && std::string_view(argv[1]) == "--reused-publication") {
            Require(std::string_view(argv[3]) == "explicit" || std::string_view(argv[3]) == "cpu", "unknown publication reuse path");
            return ReusedPublication(*device, argv[2], std::string_view(argv[3]) == "cpu");
        }
        if (argc == 2 && std::string_view(argv[1]) == "--draw-recipe") {
            DrawRecipePublication(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--buffer-refusal") {
            BufferAdjacentControls(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--buffer-publication") {
            BufferPublication(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--cpu-publication") {
            Publication(*device, true);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--publication") {
            Publication(*device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--readback") {
            Readback(*device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--snapshot") {
            InitialContents(*device, true);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--initial-reuse") return InitialReuse(*device);
        if (argc == 2 && std::string_view(argv[1]) == "--initial") {
            InitialContents(*device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--native-fmask") {
            {
                const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
                if (!context.storageImageMultisample || (context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_8_BIT) == 0) {
                    std::puts("native FMASK skipped, the device does not render or store 8-sample color");
                    return VulkanTestSkipped;
                }
            }
            NativeFmask(device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--native-vertex-input") return NativeVertexInput(device);
        Block block(4u * BlockBytes);
        Block other(BlockBytes);
        Block output(2u * 1024u * 1024u);
        Block destination(BlockBytes);
        struct ReleaseDevice {
            decltype(device)& value;
            ~ReleaseDevice() { value.reset(); }
        } releaseDevice{device};
        const auto warm = CompileBufferLoad(*device, block.Address(), output.Address());
        std::shared_ptr<const AgcDriver::Recipe> recipe;
        CheckBufferLoad(*device, warm, output, &recipe);
        const auto deferredProgram = CompileBufferLoad(*device, block.Address() + Threads * 4u, output.Address());
        auto prepared = device->PrepareDispatch(deferredProgram, {}, Threads, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()));
        const auto color = DecodeTarget(block);
        const auto state = TargetState(color);
        Draw(*device, state);
        const auto drawn = LoadSamples(*device, block);
        CheckSamples(state, drawn, {});
        CheckFirstSampleReads(*device, TargetDescriptor(block, SampleShift), drawn, true, "from the drawn 4x target");
        CheckFirstSampleReads(*device, TargetDescriptor(block, SampleShift), drawn, true, "from the drawn 4x target", AdjustedPlaneCode, "image_sample_a");
        std::puts("a DIM 2D image_sample_a read of a 4x color target returns each pixel's sample 0");
        Draw(*device, state, 0, 2u, Unorm8PixelCode);
        const auto redrawn = LoadSamples(*device, block);
        Require(redrawn != drawn, "drawing the 4x target again with another color left its samples unchanged");
        CheckFirstSampleReads(*device, TargetDescriptor(block, SampleShift), redrawn, true, "from the 4x target drawn again with another color after a first read");
        Block zeroed(BlockBytes);
        CheckFirstSampleReads(*device, TargetDescriptor(zeroed, SampleShift), std::vector<std::array<float, 4>>(Side * Side * Samples), false, "from zeroed color memory no draw rendered to");
        Block aliased(4u * BlockBytes);
        Draw(*device, TargetState(DecodeSwappedTarget(aliased)), 0, 2u, Unorm8PixelCode);
        Draw(*device, TargetState(DecodeTarget(aliased)));
        const auto aliasedSamples = LoadSamples(*device, aliased);
        CheckSamples(state, aliasedSamples, {});
        CheckFirstSampleReads(*device, TargetDescriptor(aliased, SampleShift), aliasedSamples, true, "from a 4x RGBA16F target drawn after a BGRA8 target at the same address");
        std::puts("DIM 2D image_load, image_sample_lz and image_sample_l reads of a 4x color target return each pixel's sample 0");
        Block swapped(4u * BlockBytes);
        Draw(*device, TargetState(DecodeSwappedTarget(swapped)), 0, 2u, Unorm8PixelCode);
        const auto swappedSamples = LoadSamples(*device, swapped, Samples, 0u, Format8888Unorm);
        CheckSwappedSamples(swappedSamples);
        CheckFirstSampleReads(*device, TargetDescriptor(swapped, SampleShift, 0u, Format8888Unorm), swappedSamples, true, "from a 4x BGRA8 target read through an RGBA8 descriptor");
        std::puts("a 4x BGRA8 color target loaded through an RGBA8 texture descriptor reads its drawn color with red and blue swapped");
        Block packedFloat(4u * BlockBytes);
        Draw(*device, TargetState(DecodePackedFloatTarget(packedFloat)));
        const auto packedSamples = LoadSamples(*device, packedFloat, Samples, 0u, Format11_11_10Float);
        CheckPackedFloatSamples(packedSamples);
        CheckFirstSampleReads(*device, TargetDescriptor(packedFloat, SampleShift, 0u, Format11_11_10Float), packedSamples, true, "from a 4x 10_11_11 float target read through a format 36 descriptor");
        std::puts("a 4x 10_11_11 float color target loaded through a format 36 (11_11_10 float) texture descriptor reads its drawn color");
        Block integer(4u * BlockBytes);
        Block signedFill(BlockBytes);
        if ((AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).limits.sampledImageIntegerSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0) {
            std::puts("DIM 2D reads of 4x 8_8_8_8_UINT and 8_8_8_8_SINT targets not tested: sampledImageIntegerSampleCounts lacks VK_SAMPLE_COUNT_4_BIT");
        } else {
            Draw(*device, TargetState(DecodeUintTarget(integer)), 0, 2u, Uint8PixelCode);
            const auto integerSamples = LoadSamples(*device, integer, Samples, 0u, Format8888Uint);
            CheckUintSamples(integerSamples);
            CheckFirstSampleReads(*device, TargetDescriptor(integer, SampleShift, 0u, Format8888Uint), integerSamples, true, "from a drawn 4x 8_8_8_8_UINT target");
            StoreWords(signedFill, BlockBytes, FilledSint8);
            CheckFirstSampleReads(*device, TargetDescriptor(signedFill, SampleShift, 0u, Format8888Sint), std::vector<std::array<float, 4>>(Side * Side * Samples, std::bit_cast<std::array<float, 4>>(FilledSint8Texel)), false, "from 8_8_8_8_SINT color memory no draw rendered to");
            std::puts("DIM 2D reads of a drawn 4x 8_8_8_8_UINT target and of 8_8_8_8_SINT color memory no draw rendered to return each pixel's sample 0 bit for bit");
        }
        CheckOverwrittenTarget(*device);
        CheckClaimedTargets(*device);
        if (!AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).storageImageMultisample) {
            std::puts("multisampled target fills, stores and swizzle modes skipped, the device has no shaderStorageImageMultisample");
            return VulkanTestSkipped;
        }
        const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(color);
        Require(colorBytes == BlockBytes, "the native color target does not occupy exactly one BDA block");
        Block bda(4u * BlockBytes);
        const auto bdaState = TargetState(DecodeTarget(bda));
        CheckBdaSpan(*device, bda, 0, false);
        CheckBdaSpan(*device, bda, BlockBytes, false);
        Draw(*device, bdaState);
        CheckBdaSpan(*device, bda, 0, true);
        CheckBdaSpan(*device, bda, BlockBytes, false);
        Draw(*device, state);
        CheckBdaSpan(*device, block, 0, true);
        CheckBdaSpan(*device, block, BlockBytes, false);
        CheckSamples(state, LoadSamples(*device, block), {});
        Require(colorBytes != 0 && colorBytes <= block.bytes, "the 4x target's color memory does not fit its block");
        AgcDriver::Graphics::NoteColorMetadataFill(block.Address(), static_cast<std::size_t>(colorBytes), FillOnes);
        CheckUniform(LoadSamples(*device, block), Ones, "after a fill of the target's color memory with half-float ones");
        const std::vector<std::uint8_t> guardedBytes(block.data, block.data + block.bytes);
        constexpr std::string_view mismatch = "guest color no longer matches its initial contents or a known full fill";
        std::vector<std::string> missing;
        const auto dispatch = [&](const auto& program) {
            device->Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()));
            device->WaitIdle();
        };
        CheckBufferRefused([&] { dispatch(CompileBufferLoad(*device, block.Address() + 4u, output.Address())); }, "fresh read", missing, mismatch);
        CheckBufferRefused([&] { dispatch(CompileBufferLoad(*device, block.Address() + colorBytes - 4u, output.Address())); }, "edge read", missing, mismatch);
        CheckBufferRefused([&] { dispatch(CompileBufferLoad(*device, block.Address() + colorBytes, block.Address())); }, "write", missing);
        CheckBufferRefused([&] { dispatch(warm); }, "warmed dispatch", missing, mismatch);
        if (prepared != nullptr) {
            CheckBufferRefused([&] {
                device->Dispatch(deferredProgram, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()), prepared);
                device->WaitIdle();
            }, "deferred dispatch", missing, mismatch);
        }
        bool recipeReused = false;
        if (recipe != nullptr) {
            const auto hit = device->PrepareRecipe(recipe, false);
            Require(hit != nullptr, "the warmed buffer recipe did not retain its template");
            recipeReused = true;
            CheckBufferRefused([&] {
                AgcDriver::VulkanDevice::IndirectOutcome outcome{};
                device->DispatchRecipe(warm, 1, 1, 1, 0, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()), hit, outcome);
                device->WaitIdle();
            }, "recipe dispatch", missing, mismatch);
        }
        Require(std::equal(guardedBytes.begin(), guardedBytes.end(), block.data), "a refused buffer publication changed guest color bytes");
        Require(Refused(block.Address(), static_cast<std::size_t>(colorBytes), "before the adjacent buffer control"), "refused buffer publication removed the live color guard");
        std::memset(block.data + colorBytes, 0, BlockBytes);
        const std::vector<std::uint8_t> controlBytes(block.data, block.data + block.bytes);
        CheckBufferLoad(*device, CompileBufferLoad(*device, block.Address() + colorBytes, output.Address()), output);
        CheckUniform(LoadSamples(*device, block), Ones, "after the native buffer access refusals and adjacent buffer control");
        std::cout << "native buffer controls: adjacent=passed deferred=" << (prepared != nullptr) << " recipe=" << recipeReused << std::endl;
        std::string failures;
        for (const auto& item : missing) failures += " " + item;
        Require(missing.empty(), "buffer aliases of live native MSAA color memory bypassed the guard:" + failures);
        Require(std::equal(controlBytes.begin(), controlBytes.end(), block.data), "the adjacent buffer control changed guest bytes");
        Require(Refused(block.Address(), static_cast<std::size_t>(colorBytes), "after refused buffer publication"), "refused buffer publication removed the live color guard");
        recipe.reset();
        prepared.reset();
        RequireRefused(*device, TargetDescriptor(block, 3u), "texture of 8 samples", "loading a 4x target as an 8x texture");
        Require(colorBytes <= other.bytes, "the 4x target's color memory does not fit one block");
        CheckUniform(LoadSamples(*device, other), {}, "from zeroed color memory no draw rendered to");
        Block stored(BlockBytes);
        StoreWords(stored, colorBytes, FillOnes);
        CheckUniform(LoadSamples(*device, stored), Ones, "from color memory of half-float ones no draw rendered to");
        Block reused(BlockBytes);
        StoreWords(reused, colorBytes, FillOnes);
        CheckUniform(LoadSamples(*device, reused), Ones, "from color memory of half-float ones no draw rendered to, before the title reuses it");
        AgcDriver::Graphics::RequireNoMultisampleColorTarget(reused.Address(), static_cast<std::size_t>(colorBytes / 2u), "a storage image access");
        StoreWords(reused, colorBytes, 0u);
        CheckUniform(LoadSamples(*device, reused), {}, "from color memory no draw rendered to after the title reused it for zeros");
        std::string drawnRefusal;
        try {
            AgcDriver::Graphics::RequireNoMultisampleColorTarget(block.Address(), static_cast<std::size_t>(colorBytes / 2u), "a storage image access");
        } catch (const std::exception& error) {
            drawnRefusal = error.what();
        }
        Require(drawnRefusal.find("whose samples are only on the host") != std::string::npos, "a guest access over a drawn 4x target is not refused");
        std::puts("a guest access over a 4x texture no draw rendered to retires it, so a later load reads the memory's new value, and a guest access over a drawn target is still refused");
        const auto storedState = TargetState(DecodeTarget(stored));
        Draw(*device, storedState);
        CheckSamples(storedState, LoadSamples(*device, stored), Ones);
        Block mixed(BlockBytes);
        StoreWords(mixed, colorBytes, FillOnes);
        const std::uint32_t odd = 0x3c000000u;
        std::memcpy(mixed.data + colorBytes - sizeof(odd), &odd, sizeof(odd));
        RequireRefused(*device, TargetDescriptor(mixed, SampleShift), "does not hold one repeated 32-bit value", "loading a multisampled texture no draw rendered to whose color memory is not uniform");
        const auto oneSample = [](std::uint32_t, std::uint32_t, std::uint32_t sample) { return sample == StoredSample; };
        const auto drawnBefore = LoadSamples(*device, block);
        StoreSamples(*device, TargetDescriptor(block, SampleShift), SampleWrites(oneSample));
        CheckWritten(drawnBefore, LoadSamples(*device, block), oneSample, "into one sample of each pixel of the drawn target");
        const auto leftHalf = [](std::uint32_t x, std::uint32_t, std::uint32_t sample) { return x < Side / 2u && sample != 0u; };
        Block undrawn(BlockBytes);
        StoreSamples(*device, TargetDescriptor(undrawn, SampleShift), SampleWrites(leftHalf));
        CheckWritten(std::vector<std::array<float, 4>>(Side * Side * Samples), LoadSamples(*device, undrawn), leftHalf, "into zeroed color memory no draw rendered to");
        const auto refusedOver = [&](const Block& over) {
            try {
                AgcDriver::Graphics::RequireNoMultisampleColorTarget(over.Address(), static_cast<std::size_t>(colorBytes / 2u), "a storage image access");
            } catch (const std::exception& error) {
                return std::string(error.what()).find("whose samples are only on the host") != std::string::npos;
            }
            return false;
        };
        Require(refusedOver(undrawn), "a guest access over a 4x texture no draw rendered to but a dispatch wrote is not refused");
        Block absorbing(BlockBytes);
        StoreWords(absorbing, colorBytes, FillOnes);
        CheckUniform(LoadSamples(*device, absorbing), Ones, "from color memory of half-float ones no draw rendered to, before a fill the texture absorbs");
        Require(AgcDriver::Graphics::MultisampledColorFillAbsorbs(absorbing.Address(), static_cast<std::size_t>(colorBytes), 0u), "a fill of 0 over the whole color memory of a 4x texture no draw rendered to is not absorbed");
        AgcDriver::Graphics::NoteColorMetadataFill(absorbing.Address(), static_cast<std::size_t>(colorBytes), 0u);
        Require(refusedOver(absorbing), "a guest access over a 4x texture no draw rendered to that absorbed a fill is not refused");
        CheckUniform(LoadSamples(*device, absorbing), {}, "from a 4x texture no draw rendered to after it absorbed a fill of 0");
        std::puts("a guest access over a 4x texture no draw rendered to is still refused once a dispatch wrote it or it absorbed a fill");
        Block mixedStore(BlockBytes);
        StoreWords(mixedStore, colorBytes, FillOnes);
        std::memcpy(mixedStore.data + colorBytes - sizeof(odd), &odd, sizeof(odd));
        RequireStoreRefused(*device, TargetDescriptor(mixedStore, SampleShift), "writing the 16x16 multisampled texture", "storing to a multisampled texture no draw rendered to whose color memory is not uniform");
        std::puts("stores to one sample of each pixel of a drawn 4x target and to samples of zeroed color memory no draw rendered to reach exactly those samples, and a store to non-uniform color memory no draw rendered to is refused");
        for (const auto mode : {TileZ64KBX, TileD64KBX}) {
            Block swizzled(BlockBytes);
            StoreWords(swizzled, colorBytes, FillOnes);
            CheckUniform(LoadSamples(*device, swizzled, Samples, 0u, Format16161616Float, mode), Ones, "from the uniform memory of a " + std::to_string(mode) + " swizzle surface no draw rendered to");
        }
        const auto htileBytes = AgcDriver::Graphics::HtileMetadataBytes({Side, Side});
        Require(htileBytes <= BlockBytes, "the HTILE of the 4x texture does not fit one block");
        Block cleared(2u * BlockBytes);
        StoreWords(cleared, colorBytes, FillOnes);
        cleared.data[0] ^= 1u;
        const auto clearedHtile = cleared.Address() + BlockBytes;
        CheckUniform(LoadSamples(*device, CompressedDescriptor(cleared, TileZ64KBX, clearedHtile)), {}, "from a Z swizzle surface whose HTILE is all cleared to 0.0 over nonuniform color memory");
        AgcDriver::Graphics::NoteColorMetadataFill(clearedHtile, sizeof(std::uint32_t), 0u);
        RequireRefused(*device, CompressedDescriptor(cleared, TileZ64KBX, clearedHtile), "over the HTILE", "loading a Z swizzle texture after a fill over part of its HTILE");
        AgcDriver::Graphics::NoteColorMetadataFill(clearedHtile, static_cast<std::size_t>(htileBytes), 0u);
        CheckUniform(LoadSamples(*device, CompressedDescriptor(cleared, TileZ64KBX, clearedHtile)), {}, "from a Z swizzle surface after a fill of 0 over its whole HTILE");
        Block expanded(2u * BlockBytes);
        StoreWords(expanded, colorBytes, FillOnes);
        std::fill_n(reinterpret_cast<std::uint32_t*>(expanded.data + BlockBytes), htileBytes / sizeof(std::uint32_t), 0xffffffffu);
        CheckUniform(LoadSamples(*device, CompressedDescriptor(expanded, TileZ64KBX, expanded.Address() + BlockBytes)), Ones, "from the uniform memory of a Z swizzle surface whose HTILE is all expanded");
        Block partial(2u * BlockBytes);
        StoreWords(partial, colorBytes, FillOnes);
        const std::uint32_t expandedTile = 0xfu;
        std::memcpy(partial.data + BlockBytes + htileBytes - sizeof(expandedTile), &expandedTile, sizeof(expandedTile));
        RequireRefused(*device, CompressedDescriptor(partial, TileZ64KBX, partial.Address() + BlockBytes), "neither all cleared to 0.0 nor all expanded", "loading a Z swizzle texture whose HTILE mixes cleared and expanded tiles");
        Block display(2u * BlockBytes);
        StoreWords(display, colorBytes, FillOnes);
        RequireRefused(*device, CompressedDescriptor(display, TileD64KBX, display.Address() + BlockBytes), "compression metadata", "loading a D swizzle multisampled texture no draw rendered to with compression metadata");
        Block standard(BlockBytes);
        StoreWords(standard, colorBytes, FillOnes);
        RequireRefused(*device, TargetDescriptor(standard, SampleShift, 0u, Format16161616Float, TileS64KBX), "has no modeled multisampled color footprint", "loading an S swizzle multisampled texture no draw rendered to");
        device->WaitIdle();
        AgcDriver::Graphics::ClearMultisampleTargets(device->Device());
        CheckBdaSpan(*device, block, 0, false);
        Draw(*device, TargetState(DecodeTarget(block)));
        std::puts("every sample of a 4x color target loaded through its multisampled texture descriptor matches the triangle drawn into it, then the fill of its color memory; a surface no draw rendered to loads the value its color memory repeats and keeps it under a later draw");
        struct ReleaseIndexDevice {
            std::unique_ptr<AgcDriver::VulkanDevice>& device;
            ~ReleaseIndexDevice() { device.reset(); }
        } releaseIndexDevice{device};
        CheckNativeIndices(*device, block, destination);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
