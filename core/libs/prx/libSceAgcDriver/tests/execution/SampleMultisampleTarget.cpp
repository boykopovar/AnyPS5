#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#include "DepthFastClearHarness.hpp"
#include "tests/TemporaryFile.hpp"
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
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
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
constexpr std::uint32_t Format8Uint = 5;
constexpr std::uint32_t Float10_11_11Info = 0x0718u;
constexpr std::uint32_t Format11_11_10Float = 36;
constexpr std::uint32_t Format32Float = 22;
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
    {-1.0f, -1.0f, 0.5f, 1.0f}, {0.83f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.71f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};

alignas(256) constexpr std::array<std::uint32_t, 7> BufferLoadCode{0x34020082u, 0xe8001000u | (20u << 19u), 0x80000201u, 0xbf8c0000u, 0xe0701000u, 0x80010201u, 0xbf810000u};

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = TemporaryFile::Open();
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

bool BlocksWriteWatched() {
#ifdef _WIN32
    return GuestArena::GuestArenaAvailable_nid_postfix() && GuestArena::GuestArenaWriteWatched_nid_postfix();
#else
    return GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
#endif
}

struct Block {
    explicit Block(std::size_t bytes, bool watched = false, bool initialise = true) : bytes(bytes), watched(watched) {
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
        if (initialise) std::memset(data, 0, bytes);
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block) {
    return DecodeTarget(block.Address());
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

std::array<std::uint32_t, 8> TargetDescriptor(const Block& block, std::uint32_t sampleShift, std::uint32_t format = Format16161616Float, std::uint32_t tileMode = TileRenderTarget) {
    const auto address = block.Address();
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

std::vector<std::array<float, 4>> LoadSamples(AgcDriver::VulkanDevice& device, const Block& block, std::uint32_t format = Format16161616Float, std::uint32_t tileMode = TileRenderTarget) {
    return LoadSamples(device, TargetDescriptor(block, SampleShift, format, tileMode));
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
    auto texture = TargetDescriptor(block, SampleShift, Format16161616Float, tileMode);
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

void CheckSamples(const AgcDriver::Graphics::State& state, const std::vector<std::array<float, 4>>& loaded, const std::array<float, 4>& background) {
    const auto locations = AgcDriver::Graphics::SampleLocations(state);
    std::array<std::uint32_t, Samples + 1u> pixelsByCoverage{};
    std::array<std::uint32_t, Samples> coveredBySample{};
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            std::uint32_t covered = 0;
            for (std::uint32_t sample = 0; sample < Samples; ++sample) {
                const auto& texel = loaded[(y * Side + x) * Samples + sample];
                const bool drawn = texel == Drawn;
                const bool empty = texel == background;
                char where[160];
                std::snprintf(where, sizeof(where), "sample %u of pixel (%u, %u) loaded (%g, %g, %g, %g)", sample, x, y, texel[0], texel[1], texel[2], texel[3]);
                Require(drawn || empty, std::string(where) + ", neither the drawn color nor the cleared target");
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
    Require(checked >= Side * Side * Samples * 9u / 10u, "fewer than nine in ten samples are clear of the triangle's edges");
    Require(pixelsByCoverage[0] != 0 && pixelsByCoverage[Samples] != 0, "the triangle covers every pixel or none");
    Require(pixelsByCoverage[1] + pixelsByCoverage[2] + pixelsByCoverage[3] != 0, "no pixel is partly covered, so the samples of a pixel were not told apart");
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

bool FormatlessMultisampleWrites(const AgcDriver::Graphics::Context& context) {
    return context.storageImageMultisample && context.storageImageWriteWithoutFormat;
}

bool InitialColorUploadSupported(const AgcDriver::Graphics::Context& context) {
    return FormatlessMultisampleWrites(context) || context.sampleRateShading;
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
    const std::vector<std::uint8_t> guest(reused.data, reused.data + reused.bytes);
    if (InitialColorUploadSupported(AgcDriver::VulkanDeviceTestAccess::ContextOf(device))) {
        Draw(device, state);
        device.WaitIdle();
        Require(std::equal(guest.begin(), guest.end(), reused.data), "a fresh draw after a partial guest overwrite changed guest color memory");
    } else {
        refusal.clear();
        try { Draw(device, state); }
        catch (const std::runtime_error& error) { refusal = error.what(); }
        Require(refusal.find("requires storage writes or sample rate shading") != std::string::npos, "a fresh draw after a partial guest overwrite did not refuse initial samples the device cannot upload");
        Require(std::equal(guest.begin(), guest.end(), reused.data), "a refused draw after a partial guest overwrite changed guest color memory");
        std::memset(reused.data, 0, static_cast<std::size_t>(colorBytes));
        Draw(device, state);
        device.WaitIdle();
    }
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
        AgcDriver::GuestMemory::FlushGpuWrites(reused.Address(), reused.bytes);
    }
    {
        Block reused(2u * BlockBytes);
        const auto wide = TargetState(DecodeTarget(reused.Address(), 1u, {8u * Side, 4u * Side}));
        const auto narrow = TargetState(DecodeTarget(reused.Address()));
        Require(AgcDriver::Graphics::MultisampledColorBytes(wide.color) == 2u * BlockBytes && AgcDriver::Graphics::MultisampledColorBytes(narrow.color) == BlockBytes, "the 2x target does not span the 4x target and one more block");
        Draw(device, wide);
        device.WaitIdle();
        std::string refusal;
        try { Draw(device, narrow); }
        catch (const std::runtime_error& error) { refusal = error.what(); }
        Require(refusal.find("overlap another native owner") != std::string::npos, "an incompatible target shape did not refuse unpublished native samples");
        Require(Refused(reused.Address() + BlockBytes, 0x1000u, "a read after a refused narrower target"), "a refused narrower target retired the original owner's tail");
        Require(Refused(reused.Address(), 0x1000u, "a read of the original target after a refused replacement"), "a refused narrower target retired the original owner");
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
    {
        Block copied(BlockBytes, true);
        const auto copiedTexture = TargetDescriptor(copied, 1u, Format8Uint, TileZ64KBX);
        const auto zeros = LoadSamples(device, copiedTexture, 2u);
        Require(std::all_of(zeros.begin(), zeros.end(), [](const auto& sample) { return std::bit_cast<std::uint32_t>(sample[0]) == 0u; }), "the memory-backed 2x R8 copy for the direct claim did not read the zeros in memory");
        Require(AgcDriver::Graphics::MultisampleColorTargetOverlaps(copied.Address(), copied.bytes), "reading zeroed memory as a 2x R8 texture kept no memory-backed copy for the direct claim");
        AgcDriver::Graphics::ClaimMultisampleTargetMemory(copied.Address(), copied.bytes);
        const bool kept = AgcDriver::Graphics::MultisampleColorTargetOverlaps(copied.Address(), copied.bytes);
        device.SubmitRecorded(true);
        device.WaitIdle();
        AgcDriver::Graphics::ClearCachedTextures(context.device);
        AgcDriver::Graphics::ClearMultisampleTargets(context.device);
        Require(!kept, "a direct claim left the memory-backed 2x copy registered");
    }
    {
        Block plane(BlockBytes, true);
        Block depth(2u * BlockBytes, true);
        std::fill_n(reinterpret_cast<std::uint32_t*>(depth.data + BlockBytes), BlockBytes / 4u, 0xfffffff0u);
        const auto stencilTexture = TargetDescriptor(plane, 1u, Format8Uint, TileZ64KBX);
        const auto zeros = LoadSamples(device, stencilTexture, 2u);
        Require(std::all_of(zeros.begin(), zeros.end(), [](const auto& sample) { return std::bit_cast<std::uint32_t>(sample[0]) == 0u; }), "the memory-backed 2x R8 copy did not read the zeros in memory");
        Require(AgcDriver::Graphics::MultisampleColorTargetOverlaps(plane.Address(), plane.bytes), "reading zeroed memory as a 2x R8 texture kept no memory-backed copy");
        Require(!AgcDriver::Graphics::MultisampleColorTargetOver(plane.Address(), plane.bytes).has_value(), "a memory-backed 2x copy was reported as a colour target whose samples are only on the host");
        AgcDriver::Graphics::DepthTarget native{depth.Address(), plane.Address(), {Side, Side}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0u, depth.Address() + BlockBytes, false, 2u};
        native.depthSwizzle = 24u;
        native.iterate256 = native.htilePipeAligned = native.htileDepthOnly = true;
        AgcDriver::Graphics::DepthSurfaceView(context, native);
        device.WaitIdle();
        const bool kept = AgcDriver::Graphics::MultisampleColorTargetOverlaps(plane.Address(), plane.bytes);
        std::string refusal;
        std::vector<std::array<float, 4>> stencil;
        try { stencil = LoadSamples(device, stencilTexture, 2u); }
        catch (const std::runtime_error& error) { refusal = error.what(); }
        device.SubmitRecorded(true);
        device.WaitIdle();
        AgcDriver::Graphics::ClearDepthSurfaces(context.device);
        AgcDriver::Graphics::ClearCachedTextures(context.device);
        AgcDriver::Graphics::ClearMultisampleTargets(context.device);
        Require(refusal.empty(), "sampling a stencil plane bound over a memory-backed 2x copy refused: " + refusal);
        Require(!kept, "a memory-backed 2x copy stayed registered after a D32S8 target's stencil plane was bound over its memory");
        Require(std::all_of(stencil.begin(), stencil.end(), [](const auto& sample) { return std::bit_cast<std::uint32_t>(sample[0]) == 0u; }), "the stencil plane bound over a memory-backed 2x copy did not read the stencil in memory");
    }
    std::puts("a 2x target whose memory a D32S8 stencil plane, a single-sample color target or a 4x target was bound over after its last draw is retired at the next guarded read; one drawn again or stored to after the stencil bind stays guarded, a one-fragment target's FMASK guard keeps refusing, and a memory-backed 2x copy is dropped by a direct claim of its memory and when a stencil plane is bound over it");
}

void StoreWords(const Block& block, std::uint64_t bytes, std::uint32_t word) {
    for (std::uint64_t offset = 0; offset < bytes; offset += sizeof(word)) std::memcpy(block.data + offset, &word, sizeof(word));
}

std::vector<std::array<float, 4>> StoreVaryingUnorm(const Block& block, AgcDriver::Graphics::TextureTileMode tileMode) {
    using namespace AgcDriver::Graphics;
    const MultisampleTextureLayout layout(tileMode, 4u, Samples, Side, Side, 1u);
    Require(layout.Bytes() <= block.bytes, "the varying fixture does not fit its color block");
    StoreWords(block, block.bytes, 0u);
    std::vector<std::byte> linear(static_cast<std::size_t>(layout.LinearBytes()));
    std::vector<std::array<float, 4>> expected(Side * Side * Samples);
    for (std::uint32_t index = 0; index < expected.size(); ++index) {
        const auto pixel = index / Samples;
        const bool odd = ((pixel % Side) * 3u + (pixel / Side) * 5u + (index % Samples) * 7u) % 2u != 0u;
        const std::uint32_t word = odd ? 0x00ff00ffu : 0xff00ff00u;
        std::memcpy(linear.data() + index * sizeof(word), &word, sizeof(word));
        expected[index] = odd ? std::array<float, 4>{1.0f, 0.0f, 1.0f, 0.0f} : std::array<float, 4>{0.0f, 1.0f, 0.0f, 1.0f};
    }
    layout.Tile(linear, std::as_writable_bytes(std::span(block.data, block.bytes)));
    return expected;
}

void CheckLoaded(const std::vector<std::array<float, 4>>& loaded, const std::vector<std::array<float, 4>>& expected, const std::string& what) {
    Require(loaded.size() == expected.size(), "the loaded sample count differs " + what);
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        char where[96];
        std::snprintf(where, sizeof(where), "sample %zu of pixel (%zu, %zu)", index % Samples, index / Samples % Side, index / Samples / Side);
        Require(loaded[index] == expected[index], std::string(where) + " does not hold its guest color " + what);
    }
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
    if (failure.empty() && deferred.find("BDA access failed:") != std::string::npos) failure = deferred;
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

int NonuniformInitialContents(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (!InitialColorUploadSupported(context)) {
        std::puts("nonuniform initial contents skipped, the device lacks multisampled storage writes and sample rate shading");
        return VulkanTestSkipped;
    }
    std::uint32_t exercised = 0u;
    for (const auto shift : {1u, 2u, 3u}) {
        const auto fragments = 1u << shift;
        if ((context.limits.framebufferColorSampleCounts & fragments) == 0u || (context.limits.sampledImageColorSampleCounts & fragments) == 0u) continue;
        for (const auto pipeBankXor : {0u, 0x8100u}) {
            Block block(4u * BlockBytes);
            const auto expected = StoreInitial(block, fragments, pipeBankXor, false);
            const std::vector<std::uint8_t> before(block.data, block.data + block.bytes);
            auto color = DecodeTarget(block.Address(), shift);
            color.pipeBankXor = pipeBankXor;
            const auto state = TargetState(color);
            Draw(device, state);
            auto descriptor = TargetDescriptor(block, shift);
            descriptor[0] ^= pipeBankXor >> 8u;
            const auto loaded = LoadSamples(device, descriptor, fragments);
            const auto locations = SampleLocations(state);
            std::uint32_t drawn = 0u, untouched = 0u;
            for (std::uint32_t y = 0u; y < Side; ++y) {
                for (std::uint32_t x = 0u; x < Side; ++x) {
                    for (std::uint32_t sample = 0u; sample < fragments; ++sample) {
                        const auto index = (y * Side + x) * fragments + sample;
                        const auto& texel = loaded[index];
                        Require(texel == Drawn || texel == expected[index], "a first partial draw lost a nonuniform initial sample");
                        const auto px = x + static_cast<double>(locations[sample].x);
                        const auto py = y + static_cast<double>(locations[sample].y);
                        const auto distance = std::min({EdgeDistance(px, py, 0u), EdgeDistance(px, py, 1u), EdgeDistance(px, py, 2u)});
                        if (std::fabs(distance) < EdgeMargin) continue;
                        Require(texel == (distance > 0.0 ? Drawn : expected[index]), "nonuniform initial contents and raster coverage disagree");
                        drawn += distance > 0.0;
                        untouched += distance < 0.0;
                    }
                }
            }
            Require(drawn != 0u && untouched != 0u, "nonuniform samples were not checked on both sides of the draw");
            Require(std::equal(before.begin(), before.end(), block.data), "nonuniform initialization changed guest bytes or padding");
            device.SubmitRecorded(true);
            device.WaitIdle();
            ClearCachedTextures(context.device);
            ClearMultisampleTargets(context.device);
            std::printf("nonuniform initial contents: %u fragments xor=0x%x preserve untouched samples and guest padding\n", fragments, pipeBankXor);
            ++exercised;
        }
    }
    Require(exercised != 0u, "no nonuniform initial multisample case was exercised");
    return 0;
}

int NonuniformInitialLifetime(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    Require((context.limits.framebufferColorSampleCounts & Samples) != 0u, "initial lifetime sample count is unavailable");
    device.WaitIdle();
    auto* previous = Recorder::Active();
    Require(previous != nullptr && previous->Device() == context.device, "initial lifetime fixture has no device recorder to restore");
    struct Restore { Recorder* previous; ~Restore() { previous->Activate(); } } restore{previous};
    const bool uploads = InitialColorUploadSupported(context);
    for (const bool uniform : {false, true}) {
        if (!uniform && !uploads) continue;
        Block block(BlockBytes);
        if (uniform) StoreWords(block, block.bytes, FillOnes);
        const auto expected = uniform ? std::vector<std::array<float, 4>>(Side * Side * Samples, Ones) : StoreInitial(block, Samples, 0u, false);
        const auto color = DecodeTarget(block);
        const auto state = TargetState(color);
        {
            auto recorded = context;
            recorded.bufferPool = std::make_shared<BufferPool>(context);
            Recorder recorder(recorded);
            std::shared_ptr<void> owner;
            VkImageView view = VK_NULL_HANDLE;
            std::size_t initialBytes = 0;
            constexpr std::size_t uploadBytes = Side * Side * Samples * 8u;
            constexpr auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            const auto usage = BufferPool::Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, properties);
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                recorder.Activate();
                PrepareMultisampleTargetForDraw_nid_no_patch(recorded, color);
                VkImage image = VK_NULL_HANDLE;
                view = MultisampleTargetView(recorded, color, owner, &image);
                VkMemoryRequirements requirements{};
                context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
                initialBytes = recorder.OpenKeptBytes();
                Require(initialBytes == requirements.size + BlockBytes + (uniform ? 0u : uploadBytes), "initial native color accounting omits the retained guest baseline or transient upload");
                const auto premature = recorded.bufferPool->Take(uploadBytes, usage);
                if (premature) recorded.bufferPool->Put(*premature);
                Require(!premature.has_value(), "initial upload was recycled before submission completed");
                recorder.Submit();
            }
            recorder.Sync();
            Require(recorder.InFlightKeptBytes() == 0, "completed initial contents still count as in-flight kept bytes");
            auto released = recorded.bufferPool->Take(uploadBytes, usage);
            Require(released.has_value() != uniform, "initial upload was not returned to the caller's pool after completion, or uniform clear allocated an upload");
            if (released) recorded.bufferPool->Put(*released);
            for (int round = 0; round < 3; ++round) {
                {
                    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                    std::shared_ptr<void> reusedOwner;
                    Require(MultisampleTargetView(recorded, color, reusedOwner) == view && reusedOwner == owner, "initial-content accounting did not reuse its resident target");
                    const auto targetBytes = recorder.OpenKeptBytes();
                    Require(targetBytes != 0 && initialBytes == targetBytes + (uniform ? 0u : uploadBytes), "reused target still counts initial upload bytes");
                    KeepMultisampleTarget_nid_no_patch(owner);
                    Require(recorder.OpenKeptBytes() == targetBytes, "one target is counted twice within a submission");
                    recorder.Submit();
                }
                recorder.Sync();
            }
        }
        previous->Activate();
        Draw(device, state);
        const auto loaded = LoadSamples(device, block);
        Require(loaded.size() == expected.size(), "initial lifetime sample count differs");
        for (std::size_t index = 0; index < loaded.size(); ++index) Require(loaded[index] == Drawn || loaded[index] == expected[index], "initial contents were lost after completion and reuse");
        std::printf("native color initial lifetime: %s contents survive completion and three resident reuses without retaining initialization bytes\n", uniform ? "uniform" : "varying");
    }
    if (!uploads) {
        std::puts("native color initial lifetime: varying contents skipped, the device does not enable multisampled formatless storage writes");
        return VulkanTestSkipped;
    }
    return 0;
}

VKAPI_ATTR VkResult VKAPI_CALL RefuseInitialImage(VkPhysicalDevice, VkFormat, VkImageType, VkImageTiling, VkImageUsageFlags, VkImageCreateFlags, VkImageFormatProperties*) {
    return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

int NonuniformInitialCapabilities(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    Block block(4u * BlockBytes);
    const auto expected = StoreInitial(block, Samples, 0x8100u, false);
    const std::vector<std::uint8_t> before(block.data, block.data + block.bytes);
    auto color = DecodeTarget(block);
    color.pipeBankXor = 0x8100u;
    const auto refused = [&](const Context& unavailable, const char* expected) {
        std::string error;
        try {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            PrepareMultisampleTargetForDraw_nid_no_patch(unavailable, color);
        } catch (const std::runtime_error& failure) { error = failure.what(); }
        Require(error.find(expected) != std::string::npos, "initial upload capability refusal differs: " + error);
        Require(!MultisampleColorTargetOver(color.address, static_cast<std::size_t>(MultisampledColorBytes(color))), "refused initial upload left a resident owner");
        Require(std::equal(before.begin(), before.end(), block.data), "refused initial upload changed guest contents");
    };
    for (const auto missing : {0u, 1u, 2u}) {
        auto unavailable = context;
        if (missing == 0u) unavailable.storageImageMultisample = false;
        if (missing == 1u) unavailable.storageImageWriteWithoutFormat = false;
        if (missing == 2u) unavailable.detiler = nullptr;
        unavailable.sampleRateShading = false;
        refused(unavailable, "requires storage writes or sample rate shading");
    }
    color.tileMode = ColorTileMode::Z64KBX;
    refused(context, "nonuniform initial multisample color tiling");
    color.tileMode = ColorTileMode::RenderTarget;
    if (!InitialColorUploadSupported(context)) {
        std::puts("nonuniform initial upload: missing capabilities and an unsupported tiling refuse without ownership or byte changes; chunked upload checks skipped, the device lacks multisampled storage writes and sample rate shading");
        return VulkanTestSkipped;
    }
    auto unavailable = context;
    unavailable.limits.maxStorageBufferRange = 0u;
    refused(unavailable, "at least one aligned element");
    unavailable = context;
    unavailable.imageFormatProperties = RefuseInitialImage;
    refused(unavailable, "vkGetPhysicalDeviceImageFormatProperties multisampled target");
    auto chunked = context;
    chunked.limits.maxStorageBufferRange = 256u;
    if (chunked.sampleRateShading) chunked.storageImageMultisample = false;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        PrepareMultisampleTargetForDraw_nid_no_patch(chunked, color);
    }
    Draw(device, TargetState(color));
    auto descriptor = TargetDescriptor(block, SampleShift);
    descriptor[0] ^= 0x81u;
    const auto loaded = LoadSamples(device, descriptor);
    for (std::size_t index = 0; index < loaded.size(); ++index) Require(loaded[index] == Drawn || loaded[index] == expected[index], "chunked initial upload lost sample values");
    Require(std::equal(before.begin(), before.end(), block.data), "chunked initial upload changed guest contents");
    device.SubmitRecorded(true);
    device.WaitIdle();
    ClearCachedTextures(context.device);
    ClearMultisampleTargets(context.device);
    std::puts("nonuniform initial upload: missing capabilities refuse without ownership or byte changes; 256-byte chunks preserve samples");
    return 0;
}

int NonuniformInitialHook(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (!InitialColorUploadSupported(context)) {
        std::puts("nonuniform initial capture skipped, the device lacks multisampled storage writes and sample rate shading");
        return VulkanTestSkipped;
    }
    Block block(4u * BlockBytes);
    const auto expected = StoreInitial(block, Samples, 0u, false);
    static std::atomic<std::uint32_t> captures{0u};
    struct Reset { ~Reset() { AgcDriver::GuestMemory::SetFlushHook(nullptr); } } reset;
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t, std::size_t) {
        Require(!HoldsMultisampleTargetsLock(), "nonuniform initial capture held the targets lock across a flush callback");
        WithMultisampleTargetsLockForTest([] {});
        captures.fetch_add(1u, std::memory_order_relaxed);
    });
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        PrepareMultisampleTargetForDraw_nid_no_patch(context, DecodeTarget(block));
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    Require(captures.load(std::memory_order_relaxed) == 1u, "nonuniform initial capture read guest contents more than once");
    const auto loaded = LoadSamples(device, block);
    Require(loaded == expected, "nonuniform capture changed initial sample values before a draw");
    device.SubmitRecorded(true);
    device.WaitIdle();
    ClearCachedTextures(context.device);
    ClearMultisampleTargets(context.device);
    std::puts("nonuniform initial upload captures once outside the targets lock");
    return 0;
}

void CheckReadbackSamples(const AgcDriver::Graphics::State& state, const std::vector<std::array<float, 4>>& loaded, const std::array<float, 4>& background, std::span<const std::array<float, 4>> supplied = {}) {
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
    if (state.samples == Samples) {
        Require(std::any_of(pixelsByCoverage.begin() + 1, pixelsByCoverage.end() - 1, [](auto count) { return count != 0; }), "no pixel is partly covered, so the samples of a pixel were not told apart");
        Require(std::adjacent_find(coveredBySample.begin(), coveredBySample.end(), std::not_equal_to<>()) != coveredBySample.end(), "every sample index has the same coverage, so the sample index was not told apart");
    }
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
        AgcDriver::Graphics::PrepareMultisampleTargetForDraw_nid_no_patch(context, color);
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
    std::printf("skipped, the device lacks native color initialization format=%d samples=%u\n", static_cast<int>(format), samples);
    return false;
}

PFN_vkGetPhysicalDeviceImageFormatProperties imageFormatProperties = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL NoReadbackImage(VkPhysicalDevice physical, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    if ((flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT) != 0 && usage == (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return imageFormatProperties(physical, format, type, tiling, usage, flags, properties);
}

std::vector<std::array<float, 4>> LoadColorSamples(AgcDriver::VulkanDevice& device, const Block& block, const AgcDriver::Graphics::ColorTarget& color) {
    auto descriptor = TargetDescriptor(block, std::countr_zero(color.samples));
    descriptor[0] ^= color.pipeBankXor >> 8u;
    return LoadSamples(device, descriptor, color.samples);
}


alignas(256) constexpr std::array<std::uint32_t, 51> HtileHiZPatchCode{
    0xbfa00003u, 0xd7460008u, 0x04010a05u, 0xe0382000u, 0x80000008u, 0xbf8c3f70u, 0x360e048fu, 0x3612068fu,
    0x360c028fu, 0x360a008fu, 0xd54a0004u, 0x001204ffu, 0x00000fffu, 0x7d840ef9u, 0x06868680u, 0x7d8412f9u,
    0x06868880u, 0xd54a0007u, 0x001206ffu, 0x00000fffu, 0x7d840cf9u, 0x06868a80u, 0xd54a0009u, 0x001202ffu,
    0x00000fffu, 0x7d840a80u, 0xd54a000au, 0x001200ffu, 0x00000fffu, 0xd5010006u, 0x001a0902u, 0xd5010007u,
    0x00220f03u, 0xd5010005u, 0x002a1301u, 0x02081500u, 0x7d8a04f9u, 0x06068406u, 0x7d8a0707u, 0x7d8a02f9u,
    0x06068605u, 0x7d8a00f9u, 0x06068804u, 0x88ea6a04u, 0x88ea6a06u, 0x88ea6a08u, 0xbefe046au, 0xbf880002u,
    0xe0782000u, 0x80000408u, 0xbf810000u};

void HtileHiZPatch(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr std::uint32_t records = 64u;
    constexpr std::uint32_t value = 0x12345678u;
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32u, 1u, 1u}, 0u, {true, false, false}, false, 1u};
    const auto patched = [](std::uint32_t word, std::uint32_t bounds) { return (word & 15u) == 0u ? (word & 0xfffu) | (bounds & ~0xfffu) : word; };
    const auto userDataFor = [](const Block& block, std::uint32_t count, std::uint32_t patch) {
        const auto address = block.Address();
        return std::vector<std::uint32_t>{static_cast<std::uint32_t>(address), (static_cast<std::uint32_t>(address >> 32u) & 0xffffu) | (16u << 16u), count, 0x01016facu, patch};
    };
    const std::array<std::uint32_t, 5> packet{0u, records / 32u, 1u, 1u, 1u};
    {
        Block buffer(BlockBytes);
        std::vector<std::uint32_t> original(records * 4u);
        for (std::uint32_t index = 0; index < original.size(); ++index) {
            const auto kind = index % 5u;
            original[index] = kind == 0u ? 0xfffffff0u : kind == 1u ? 0x0000000fu | (index << 12u) : kind == 2u ? index << 4u : kind == 3u ? (value & ~0xfffu) | (index & 0xff0u) : 0xabc00005u;
        }
        std::memcpy(buffer.data, original.data(), original.size() * 4u);
        const auto userData = userDataFor(buffer, records, value);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(HtileHiZPatchCode.data()), std::as_bytes(std::span(HtileHiZPatchCode))}}};
        ShaderRecompiler::RecompileRequest request{{ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(HtileHiZPatchCode.data()), HtileHiZPatchCode, 0u, {}}, {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0u, 0u, 0u, 128u}};
        request.useCache = false;
        device.Dispatch(ShaderRecompiler::Recompile(request), records / 32u, 1u, 1u);
        device.WaitIdle();
        const auto* words = reinterpret_cast<const std::uint32_t*>(buffer.data);
        for (std::uint32_t index = 0; index < original.size(); ++index) Require(words[index] == patched(original[index], value), "the HTILE Hi-Z patch kernel did not write the bounds of cleared tiles only, at word " + std::to_string(index));
        const auto match = MatchHtileClearedHiZPatch(HtileHiZPatchCode, userData, compute, packet, 0u);
        Require(match && match->address == buffer.Address() && match->bytes == records * 16u && match->value == value, "the HTILE Hi-Z patch kernel was not recognized");
        auto altered = HtileHiZPatchCode;
        altered[12] = 0x00000ff0u;
        Require(!MatchHtileClearedHiZPatch(altered, userData, compute, packet, 0u), "a kernel with another bit mask was recognized as the HTILE Hi-Z patch");
        auto strided = userData;
        strided[1] = (strided[1] & 0xffffu) | (8u << 16u);
        Require(!MatchHtileClearedHiZPatch(HtileHiZPatchCode, strided, compute, packet, 0u), "an 8-byte record stride was recognized as the HTILE Hi-Z patch");
        Require(!SkipNoOpHtileHiZPatch(*match), "an HTILE Hi-Z patch with no native depth owner was skipped");
    }
    if ((context.limits.framebufferDepthSampleCounts & VK_SAMPLE_COUNT_2_BIT) == 0u || (context.limits.sampledImageDepthSampleCounts & VK_SAMPLE_COUNT_2_BIT) == 0u) {
        std::puts("HTILE Hi-Z patch kernel decoded; skipping it over native depth not tested: no 2x depth sampling");
        return;
    }
    Block plane(BlockBytes, true);
    Block depth(2u * BlockBytes, true);
    auto* htile = reinterpret_cast<std::uint32_t*>(depth.data + BlockBytes);
    std::fill_n(htile, BlockBytes / 4u, 0xfffffff0u);
    DepthTarget native{depth.Address(), plane.Address(), {Side, Side}, VK_FORMAT_D32_SFLOAT_S8_UINT, 1.0f, 0u, depth.Address() + BlockBytes, false, 2u};
    native.depthSwizzle = 24u;
    native.iterate256 = native.htilePipeAligned = native.htileDepthOnly = true;
    DepthSurfaceView(context, native);
    device.WaitIdle();
    auto depthTexture = TargetDescriptor(depth, 1u, 22u, TileZ64KBX);
    depthTexture[6] = (1u << 10u) | (1u << 19u) | (1u << 21u) | (static_cast<std::uint32_t>(native.htileAddress >> 8u) << 24u);
    depthTexture[7] = static_cast<std::uint32_t>(native.htileAddress >> 16u);
    const auto ones = LoadSamples(device, depthTexture, 2u);
    Require(std::all_of(ones.begin(), ones.end(), [](const auto& sample) { return sample[0] == 1.0f; }), "the native depth fixture did not read its cleared depth");
    const HtileClearedHiZPatch unchanged{native.htileAddress, BlockBytes, 0xfffff123u};
    const HtileClearedHiZPatch lowered{native.htileAddress, BlockBytes, 0x00000123u};
    const bool skipped = SkipNoOpHtileHiZPatch(unchanged);
    const bool skippedChange = SkipNoOpHtileHiZPatch(lowered);
    const bool intact = std::all_of(htile, htile + BlockBytes / 4u, [](std::uint32_t word) { return word == 0xfffffff0u; });
    device.SubmitRecorded(true);
    device.WaitIdle();
    ClearDepthSurfaces(context.device);
    ClearCachedTextures(context.device);
    ClearMultisampleTargets(context.device);
    Require(skipped, "an HTILE Hi-Z patch that writes no byte over a native depth owner was not skipped");
    Require(!skippedChange, "an HTILE Hi-Z patch that changes cleared tiles of a native depth owner was skipped");
    Require(intact, "deciding an HTILE Hi-Z patch changed the native depth owner's HTILE");
    std::puts("the HTILE Hi-Z patch kernel writes the bounds of cleared tiles only; over a native depth owner it is skipped when it writes no byte and kept when it changes a cleared tile");
}

void Readback(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    context.limits.maxStorageBufferRange = 256u;
    std::uint32_t exercised = 0;
    for (const auto fragments : {2u, 4u, 8u}) {
        if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, fragments)) continue;
        Block block(BlockBytes);
        auto color = DecodeTarget(block.Address(), std::countr_zero(fragments));
        RequireReadbackRefused(context, color, "requires an existing resident target");
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            MultisampleTargetView(context, color);
        }
        const auto state = TargetState(color);
        Draw(device, state);
        const auto read = ReadHalfSamples(context, color);
        Require(read == LoadColorSamples(device, block, color), "native readback disagrees with independent MIMG sample reads");
        CheckReadbackSamples(state, read, {});
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
            auto exactColor = DecodeTarget(exact.Address(), std::countr_zero(fragments));
            exactColor.pipeBankXor = 0x8100u;
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
                PrepareMultisampleTargetForDraw_nid_no_patch(context, exactColor);
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
        CheckReadbackSamples(TargetState(color), LoadSamples(device, block), {});
        std::puts("native color readback: unsupported snapshot format refuses readback and retains rendering");
        Block invalidBlock(BlockBytes);
        auto invalid = DecodeTarget(invalidBlock);
        invalid.elementBytes = 4u;
        RequireInitialRefused(context, invalid, "color format or element size is unsupported");
        std::puts("native color readback: mismatched source format and element size is refused before view creation");
    }
    Require(exercised != 0, "no supported native color readback sample count was exercised");
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
    Require(LoadColorSamples(device, block, color) == loaded, "sampling after the native snapshot changed the image contents");
    std::string guard;
    try {
        RequireNoMultisampleTarget(block.Address(), 1u, "snapshot validation");
    } catch (const std::exception& error) {
        guard = error.what();
    }
    Require(guard.find("overlaps multisampled color target") != std::string::npos, "capturing a snapshot removed the native target's guest-access guard");
    Draw(device, TargetState(color));
    Require(LoadColorSamples(device, block, color) == loaded, "attachment reuse after the native snapshot changed untouched samples");
}


void ColorSnapshots(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::uint32_t exercised = 0u;
    for (const auto fragments : {2u, 4u, 8u}) {
        if (!InitialCountSupported(context, VK_FORMAT_R16G16B16A16_SFLOAT, fragments)) continue;
        for (const auto pipeBankXor : {0u, 0x8100u}) {
            Block block(BlockBytes);
            StoreInitial(block, fragments, pipeBankXor, false);
            auto color = DecodeTarget(block.Address(), std::countr_zero(fragments));
            color.pipeBankXor = pipeBankXor;
            const auto state = TargetState(color);
            Draw(device, state);
            CheckColorSnapshot(device, color, block, LoadColorSamples(device, block, color));
            if (fragments == Samples && pipeBankXor == 0u) {
                NoteColorMetadataFill(block.Address(), block.bytes, FillOnes);
                Draw(device, state);
                CheckColorSnapshot(device, color, block, LoadColorSamples(device, block, color), FillOnes);
                StoreWords(block, block.bytes, FillOnes);
                NoteColorMetadataFill(block.Address(), block.bytes, FillOnes);
                Draw(device, state);
                CheckColorSnapshot(device, color, block, LoadColorSamples(device, block, color), FillOnes);
                block.data[BlockBytes - 1u] ^= 1u;
                RequireColorSnapshotRefused(context, color, "no longer matches its initial contents or a known full fill");
                block.data[BlockBytes - 1u] ^= 1u;
                auto wrong = color;
                wrong.mip = 1u;
                RequireColorSnapshotRefused(context, wrong, "subresource does not match the resident target");
            }
            device.SubmitRecorded(true);
            device.WaitIdle();
            ClearCachedTextures(context.device);
            ClearMultisampleTargets(context.device);
            std::printf("native color snapshot: %u fragments xor=0x%x preserves MIMG samples, padding, guest bytes and ownership\n", fragments, pipeBankXor);
            ++exercised;
        }
    }
    Require(exercised != 0u, "no native color snapshot sample count was exercised");
    Block uniform(BlockBytes);
    StoreWords(uniform, uniform.bytes, FillOnes);
    const auto color = DecodeTarget(uniform);
    Draw(device, TargetState(color));
    CheckColorSnapshot(device, color, uniform, LoadSamples(device, uniform));
}

void ColorSnapshotRefusals(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    {
        Block block(BlockBytes);
        const auto color = DecodeTarget(block);
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            MultisampleTargetView(context, color);
        }
        RequireColorSnapshotRefused(context, color, "has no initial footprint");
        Draw(device, TargetState(color));
        CheckReadbackSamples(TargetState(color), ReadHalfSamples(context, color), {});
    }
    {
        Block block(BlockBytes);
        StoreWords(block, block.bytes, FillOnes);
        auto color = DecodeTarget(block);
        color.tileMode = ColorTileMode::Z64KBX;
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            PrepareMultisampleTargetForDraw_nid_no_patch(context, color);
        }
        RequireColorSnapshotRefused(context, color, "unsupported tiling or multisample metadata");
    }
    auto completed = std::make_shared<Block>(BlockBytes);
    const auto completedColor = DecodeTarget(*completed);
    StoreInitial(*completed, Samples, 0u, false);
    Draw(device, TargetState(completedColor));
    auto refusals = std::make_shared<std::array<std::string, 2>>();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        auto* recorder = context.recorder != nullptr ? context.recorder : Recorder::Active();
        Require(recorder != nullptr, "the snapshot fixture has no owning recorder");
        recorder->OnComplete([completed, completedColor, context, refusals] {
            try { static_cast<void>(ReadMultisampleTargetSamples_nid_no_patch(context, completedColor)); }
            catch (const std::exception& error) { (*refusals)[0] = error.what(); }
            try { static_cast<void>(ReadMultisampleTargetColorSnapshot_nid_no_patch(context, completedColor)); }
            catch (const std::exception& error) { (*refusals)[1] = error.what(); }
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
    Require(std::all_of(refusals->begin(), refusals->end(), [](const auto& error) { return error.find("during a recorder completion") != std::string::npos; }), "a native color snapshot recursively completed its producer");
    std::vector<std::byte> expected(completed->bytes);
    for (std::size_t offset = 0; offset < expected.size(); offset += sizeof(FillOnes)) std::memcpy(expected.data() + offset, &FillOnes, sizeof(FillOnes));
    Require(snapshot.original == expected && snapshot.current == expected, "the native snapshot captured its baseline before producer completion");
    std::puts("native color snapshot: missing baseline, tiling, foreign recorder and completion reentry refuse; producer completion precedes current guest capture");
}

void UniformInitialContents(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::uint32_t exercised = 0u;
    for (const auto shift : {1u, 2u, 3u}) {
        const auto fragments = 1u << shift;
        if ((context.limits.framebufferColorSampleCounts & fragments) == 0u || (context.limits.sampledImageColorSampleCounts & fragments) == 0u) continue;
        Block block(4u * BlockBytes);
        StoreWords(block, block.bytes, FillOnes);
        const std::vector<std::uint8_t> before(block.data, block.data + block.bytes);
        const auto color = DecodeTarget(block.Address(), shift);
        const auto state = TargetState(color);
        Draw(device, state);
        const auto loaded = LoadSamples(device, TargetDescriptor(block, shift), fragments);
        std::uint32_t drawn = 0u;
        std::uint32_t untouched = 0u;
        const auto locations = SampleLocations(state);
        for (std::uint32_t y = 0u; y < Side; ++y) {
            for (std::uint32_t x = 0u; x < Side; ++x) {
                for (std::uint32_t sample = 0u; sample < fragments; ++sample) {
                    const auto& texel = loaded[(y * Side + x) * fragments + sample];
                    Require(texel == Drawn || texel == Ones, "a first partial draw lost the uniform initial sample");
                    const auto px = x + static_cast<double>(locations[sample].x);
                    const auto py = y + static_cast<double>(locations[sample].y);
                    const auto distance = std::min({EdgeDistance(px, py, 0u), EdgeDistance(px, py, 1u), EdgeDistance(px, py, 2u)});
                    if (std::fabs(distance) < EdgeMargin) continue;
                    Require((texel == Drawn) == (distance > 0.0), "initial multisample contents and raster coverage disagree");
                    drawn += texel == Drawn;
                    untouched += texel == Ones;
                }
            }
        }
        Require(drawn != 0u && untouched != 0u, "uniform initial contents were not checked on both sides of a partial draw");
        Require(std::equal(before.begin(), before.end(), block.data), "uniform initialization changed guest bytes or padding");
        device.SubmitRecorded(true);
        device.WaitIdle();
        ClearCachedTextures(context.device);
        ClearMultisampleTargets(context.device);
        std::printf("uniform initial contents: %u fragments preserve untouched samples and guest padding\n", fragments);
        ++exercised;
    }
    Require(exercised != 0u, "no uniform multisample initialization case was exercised");
}

void UniformInitialRefusals(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto refuses = [&](const ColorTarget& color, const char* expected, const Context* selected = nullptr) {
        std::string refusal;
        try {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            PrepareMultisampleTargetForDraw_nid_no_patch(selected != nullptr ? *selected : context, color);
        } catch (const std::runtime_error& error) { refusal = error.what(); }
        Require(refusal.find(expected) != std::string::npos, std::string("initial color refusal did not match ") + expected + ": " + refusal);
    };
    Block block(4u * BlockBytes);
    StoreWords(block, block.bytes, FillOnes);
    const auto color = DecodeTarget(block);
    auto invalid = color;
    invalid.address = invalid.surfaceAddress = 0u;
    refuses(invalid, "not fully mapped");
    block.data[BlockBytes - 1u] ^= 1u;
    const std::vector<std::uint8_t> varying(block.data, block.data + block.bytes);
    auto unavailable = context;
    unavailable.storageImageMultisample = false;
    unavailable.sampleRateShading = false;
    refuses(color, "requires storage writes or sample rate shading", &unavailable);
    Require(std::equal(varying.begin(), varying.end(), block.data), "a refused initial import changed guest contents");
    Require(!MultisampleColorTargetOver(color.address, static_cast<std::size_t>(MultisampledColorBytes(color))), "a refused initial import left a resident owner");
    StoreWords(block, block.bytes, FillOnes);
    auto noStorage = context;
    noStorage.storageImageMultisample = false;
    noStorage.storageImageWriteWithoutFormat = false;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        PrepareMultisampleTargetForDraw_nid_no_patch(noStorage, color);
    }
    CheckUniform(LoadSamples(device, block), Ones, "uniform initialization without storage writes");
    const auto original = MultisampleColorTargetOver(color.address, static_cast<std::size_t>(MultisampledColorBytes(color)));
    Require(original && original->active->load(std::memory_order_acquire), "uniform initialization did not retain its owner");
    auto alias = color;
    alias.format = VK_FORMAT_R32G32_SFLOAT;
    refuses(alias, "overlap another native owner");
    const auto current = MultisampleColorTargetOver(color.address, static_cast<std::size_t>(MultisampledColorBytes(color)));
    Require(current && current->active == original->active && original->active->load(std::memory_order_acquire), "refused replacement retired the original owner");
    CheckUniform(LoadSamples(device, block), Ones, "after refused owner replacement");
    Block depthMemory(4u * BlockBytes, true), htile(BlockBytes, true);
    DepthTarget depth{depthMemory.Address(), 0u, {Side, Side}, VK_FORMAT_D32_SFLOAT, 0.0f, 0u, htile.Address(), false, Samples};
    depth.depthSwizzle = 24u;
    depth.iterate256 = depth.htilePipeAligned = depth.htileDepthOnly = true;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto view = DepthSurfaceView(context, depth);
        Require(ResidentDepthGuardOverlaps_nid_no_patch(depth.address, static_cast<std::size_t>(MultisampledDepthBytes_nid_no_patch(depth.extent, 4u, Samples))), "initial color refusal fixture has no resident depth guard");
        NoteDepthSurfaceWrite(context, depth);
        std::string refusal;
        try { PrepareMultisampleTargetForDraw_nid_no_patch(context, DecodeTarget(depthMemory)); }
        catch (const std::runtime_error& error) { refusal = error.what(); }
        Require(DepthSurfaceAt(depth.address), "refused initial color capture retired a written native depth owner");
        Require(refusal.find("publication of native multisample depth is not implemented for this owner") != std::string::npos, "initial color capture did not refuse a resident depth owner it cannot publish");
        Require(DepthSurfaceView(context, depth) == view, "refused color capture replaced the native depth image");
    }
    device.SubmitRecorded(true);
    device.WaitIdle();
    ClearCachedTextures(context.device);
    ClearDepthSurfaces(context.device);
    ClearMultisampleTargets(context.device);
    std::puts("uniform initial refusal checks passed: unreadable memory, differing padding, no storage features, retained owner");
}

void UniformInitialFlushHook(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    static std::atomic<std::uint32_t> captures{0u};
    Block block(4u * BlockBytes);
    StoreWords(block, block.bytes, FillOnes);
    struct Reset { ~Reset() { AgcDriver::GuestMemory::SetFlushHook(nullptr); } } reset;
    AgcDriver::GuestMemory::SetFlushHook([](std::uint64_t, std::size_t) {
        Require(!HoldsMultisampleTargetsLock(), "initial color capture held the targets lock across a flush callback");
        WithMultisampleTargetsLockForTest([] {});
        captures.fetch_add(1u, std::memory_order_relaxed);
    });
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        PrepareMultisampleTargetForDraw_nid_no_patch(context, DecodeTarget(block));
    }
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    Require(captures.load(std::memory_order_relaxed) == 1u, "initial color capture did not run its flush callback exactly once");
    CheckUniform(LoadSamples(device, block), Ones, "after a reentrant initial-capture flush hook");
    device.SubmitRecorded(true);
    device.WaitIdle();
    ClearCachedTextures(context.device);
    ClearMultisampleTargets(context.device);
    std::puts("uniform initial capture flush callback entered the target registry without holding its lock");
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
    info.resources[0].fields = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), stride == 0u ? 16u : 48u, 0x3104dfacu};
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
void CheckBufferRefused(TAction&& action, const char* name, std::vector<std::string>& missing) {
    std::string refusal;
    try {
        action();
    } catch (const std::runtime_error& error) {
        refusal = error.what();
    }
    const bool refused = refusal.find("whose samples are only on the host") != std::string::npos;
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
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::MaterializeMultisampleTargetClear(context, color);
    }
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

alignas(256) constexpr std::array<std::uint32_t, 10> NativeFmaskCode{0x34020084u, 0xe0381000u, 0x80000201u, 0xbf8c3f70u, 0xf0001f08u, 0x00020602u, 0xbf8c3f70u, 0xe0781000u, 0x80010601u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, Threads * 4u> NativeFmaskInput{};
alignas(256) std::array<std::uint32_t, Threads * 4u + 4u> NativeFmaskOutput{};

void MultisampleReadOverSingleSampleDepth(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr VkExtent2D extent{8u * Side, 8u * Side};
    Block depth(BlockBytes, true), htile(BlockBytes, true);
    std::memset(htile.data, 0xff, htile.bytes);
    Block color(static_cast<std::size_t>(extent.width) * extent.height * 4u);
    const std::span<std::byte> pixels(reinterpret_cast<std::byte*>(color.data), color.bytes);
    const auto shaders = DepthFastClearHarness::Compile(device);
    const DepthFastClearHarness::Surface surface{depth.Address(), 0u, htile.Address(), false, VK_FORMAT_D32_SFLOAT, 0u, 0u, TileZ64KBX};
    const auto texture = TargetDescriptor(depth, SampleShift, Format32Float, TileZ64KBX);
    const auto readAs = [&](float value, const char* what) {
        DepthFastClearHarness::DrawOptions options;
        options.depthCompare = VK_COMPARE_OP_ALWAYS;
        options.depthWrite = true;
        options.minDepth = value;
        options.maxDepth = value;
        Require(DepthFastClearHarness::Draw(device, shaders, surface, options, extent, pixels) == extent.width * extent.height, std::string("the single-sample depth draw did not cover the surface ") + what);
        Require(DepthSurfaceAt(depth.Address()), std::string("the single-sample depth surface is not resident ") + what);
        const auto loaded = LoadSamples(device, texture);
        for (const auto& sample : loaded) Require(sample[0] == value, std::string("a four-sample read over a single-sample depth surface did not read the depth it published ") + what + ": " + std::to_string(sample[0]));
    };
    readAs(0.25f, "after the first draw");
    readAs(0.75f, "after the depth was drawn again");
    device.SubmitRecorded(true);
    device.WaitIdle();
    ClearCachedTextures(context.device);
    ClearMultisampleTargets(context.device);
    ClearDepthSurfaces(context.device);
    std::puts("four-sample reads over a single-sample depth surface read the depth it published, before and after it is drawn again");
}

std::array<std::uint32_t, 8> NativeFmaskDescriptor(const Block& fmask, const Block& cmask, std::uint32_t pipeXor, std::uint32_t format) {
    const auto address = fmask.Address() | pipeXor;
    const auto metadata = cmask.Address();
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u) | (format << 20u) | (((Side - 1u) & 3u) << 30u), ((Side - 1u) >> 2u) | ((Side - 1u) << 14u), 4u | (1u << 3u) | (4u << 6u) | (24u << 20u) | (9u << 28u), 0u, 0u, (1u << 19u) | (1u << 21u) | (static_cast<std::uint32_t>(metadata >> 8u) << 24u), static_cast<std::uint32_t>(metadata >> 16u)};
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

void TextureDccTarget(AgcDriver::VulkanDevice& device, std::uint32_t samples) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto shift = static_cast<std::uint32_t>(std::countr_zero(samples));
    const auto clear = [&] {
        device.SubmitRecorded(true);
        device.WaitIdle();
        ClearCachedTextures(context.device);
        ClearMultisampleTargets(context.device);
    };
    struct Cleanup {
        decltype(clear)& action;
        ~Cleanup() { action(); }
    } cleanup{clear};
    const auto bind = [&](const ColorTarget& target) {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        MaterializeMultisampleTargetClear(context, target);
    };
    const auto refused = [&](const ColorTarget& target, const std::string& expected, const std::string& what) {
        std::string refusal;
        try {
            bind(target);
        } catch (const std::exception& error) {
            refusal = error.what();
        }
        Require(refusal.find(expected) != std::string::npos, what + " was not refused: " + refusal);
    };
    for (std::uint32_t mode = 0u; mode < 5u; ++mode) {
        Block color(8u * BlockBytes, true), dcc(BlockBytes, true), cmask(BlockBytes, true), fmask(BlockBytes, true);
        std::memset(color.data, 0xa5, color.bytes);
        std::memset(dcc.data, 0x40, dcc.bytes);
        std::memset(cmask.data, 0xcc, cmask.bytes);
        std::memset(fmask.data, 0x11, fmask.bytes);
        AgcDriver::Registers cx{{0x318, static_cast<std::uint32_t>(color.Address() >> 8u)}, {0x31b, 0u}, {0x31c, Float16x4Info | 0x10006000u}, {0x31d, (shift << 12u) | (shift << 15u)}, {0x31e, 0x28u}, {0x31f, static_cast<std::uint32_t>(cmask.Address() >> 8u)}, {0x321, static_cast<std::uint32_t>(fmask.Address() >> 8u)}, {0x325, static_cast<std::uint32_t>(dcc.Address() >> 8u)}, {0x390, static_cast<std::uint32_t>(color.Address() >> 40u)}, {0x398, static_cast<std::uint32_t>(cmask.Address() >> 40u)}, {0x3a0, static_cast<std::uint32_t>(fmask.Address() >> 40u)}, {0x3a8, static_cast<std::uint32_t>(dcc.Address() >> 40u)}, {0x3b0, ((Side - 1u) << 14u) | (Side - 1u)}, {0x3b8, TiledAttrib3}};
        const auto target = DecodeColorBuffer(cx, 0u);
        Require(target.samples == samples && target.dccAddress == dcc.Address() && target.dccControl == 0x28u && target.cmaskAddress == cmask.Address() && target.fmaskAddress == fmask.Address(), "the register-state fixture lost its decoded target");
        auto words = TargetDescriptor(color, shift);
        words[6] = (1u << 19u) | (1u << 21u) | (target.dccAlphaOnMsb ? 1u << 22u : 0u) | (static_cast<std::uint32_t>(dcc.Address() >> 8u) << 24u);
        words[7] = static_cast<std::uint32_t>(dcc.Address() >> 16u);
        std::array<float, 4> expected{};
        expected[target.dccAlphaOnMsb ? 3u : 0u] = 1.0f;
        CheckUniform(LoadSamples(device, words, samples), expected, "a DCC texture before its first color-target use");
        if (mode == 0u) {
            bind(target);
            CheckUniform(LoadSamples(device, words, samples), expected, "a DCC texture whose owner took CMASK, FMASK and DCC control");
            bind(target);
            CheckUniform(LoadSamples(device, words, samples), expected, "a DCC texture after its color target was reused");
            auto control = target;
            control.dccControl = 0x8u;
            refused(control, "layout or metadata changed", "another DCC control on a color-target owner");
            Require(std::all_of(color.data, color.data + color.bytes, [](auto byte) { return byte == 0xa5u; }) && std::all_of(dcc.data, dcc.data + dcc.bytes, [](auto byte) { return byte == 0x40u; }) && std::all_of(cmask.data, cmask.data + cmask.bytes, [](auto byte) { return byte == 0xccu; }) && std::all_of(fmask.data, fmask.data + fmask.bytes, [](auto byte) { return byte == 0x11u; }), "taking register state changed guest memory");
        } else if (mode == 1u) {
            auto alpha = target;
            alpha.dccAlphaOnMsb = !alpha.dccAlphaOnMsb;
            refused(alpha, "layout or metadata changed", "another alpha placement on a texture DCC owner");
        } else if (mode == 2u) {
            *static_cast<volatile std::uint8_t*>(color.data) = 1u;
            refused(target, "writes to a resident multisampled DCC", "a color store before a texture DCC owner took register state");
        } else if (mode == 3u) {
            *static_cast<volatile std::uint8_t*>(dcc.data) = 0x40u;
            refused(target, "writes to a resident multisampled DCC", "a metadata store before a texture DCC owner took register state");
        } else {
            auto plain = target;
            plain.dccControl = 0u;
            plain.cmaskAddress = 0u;
            plain.cmaskBytes = 0u;
            plain.cmaskFastClear = false;
            plain.fmaskAddress = 0u;
            {
                std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
                Require(MultisampleTargetView(context, plain, nullptr) != VK_NULL_HANDLE, "the texture DCC owner did not serve its own layout as a color target");
            }
            refused(target, "layout or metadata changed", "register state on a drawn texture DCC owner");
        }
        clear();
    }
    std::printf("Texture DCC owner as color target: samples=%u, CMASK, FMASK and DCC control taken; alpha placement, stores, drawn owners and later changes refused\n", samples);
}

void InitialTextureDcc(AgcDriver::VulkanDevice& device, std::uint32_t samples) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto clear = [&] {
        device.SubmitRecorded(true);
        device.WaitIdle();
        ClearCachedTextures(context.device);
        ClearMultisampleTargets(context.device);
    };
    struct Cleanup {
        decltype(clear)& action;
        ~Cleanup() { action(); }
    } cleanup{clear};
    const auto descriptor = [&](const Block& color, const Block& metadata, bool alphaOnMsb) {
        auto words = TargetDescriptor(color, static_cast<std::uint32_t>(std::countr_zero(samples)));
        words[6] = (1u << 19u) | (1u << 21u) | (alphaOnMsb ? 1u << 22u : 0u) | (static_cast<std::uint32_t>(metadata.Address() >> 8u) << 24u);
        words[7] = static_cast<std::uint32_t>(metadata.Address() >> 16u);
        return words;
    };
    for (const auto key : {0x00u, 0x40u, 0x80u, 0xc0u}) {
        for (const bool alphaOnMsb : {false, true}) {
            Block color(8u * BlockBytes, true);
            const bool untouched = key == 0u && !alphaOnMsb;
            Block metadata(BlockBytes, true, !untouched);
            std::memset(color.data, 0xa5, color.bytes);
            if (!untouched) std::memset(metadata.data, key, metadata.bytes);
            std::array<float, 4> expected{};
            for (std::uint32_t channel = 0u; channel < 4u; ++channel) expected[channel] = channel == (alphaOnMsb ? 3u : 0u) ? static_cast<float>(key == 0x40u || key == 0xc0u) : static_cast<float>(key == 0x80u || key == 0xc0u);
            const auto words = descriptor(color, metadata, alphaOnMsb);
            CheckUniform(LoadSamples(device, words, samples), expected, "an initial R_X DCC texture before any draw");
            CheckUniform(LoadSamples(device, words, samples), expected, "a reused initial R_X DCC texture");
            Require(std::all_of(color.data, color.data + color.bytes, [](auto byte) { return byte == 0xa5u; }), "initial DCC sampling changed guest color bytes");
            Require(std::all_of(metadata.data, metadata.data + metadata.bytes, [&](auto byte) { return byte == key; }), "initial DCC sampling changed guest metadata");
            clear();
        }
    }
    for (std::uint32_t mode = 0u; mode < 4u; ++mode) {
        Block color(8u * BlockBytes, true), metadata(BlockBytes, true);
        auto words = descriptor(color, metadata, true);
        std::string expected;
        if (mode == 0u) {
            std::memset(metadata.data, 0x20, metadata.bytes);
            expected = "register clear";
        } else if (mode == 1u) {
            metadata.data[0] = 0x40u;
            expected = "mixed multisampled DCC";
        } else if (mode == 2u) {
            words[6] &= ~(1u << 19u);
            expected = "pipe-aligned";
        } else {
            CheckUniform(LoadSamples(device, words, samples), std::array<float, 4>{}, "an initial DCC texture before a metadata store");
            *static_cast<volatile std::uint8_t*>(metadata.data) = 0u;
            expected = "writes to a resident multisampled DCC";
        }
        RequireRefused(device, words, expected, "an unsupported initial R_X DCC texture");
        clear();
    }
    std::printf("Initial R_X DCC texture: samples=%u, four clear codes, both alpha positions, untouched zero metadata and unsupported-state refusals passed\n", samples);
}

void NativeFmask(std::unique_ptr<AgcDriver::VulkanDevice>& device, std::uint32_t samples) {
    using namespace AgcDriver::Graphics;
    const auto shift = static_cast<std::uint32_t>(std::countr_zero(samples));
    const std::uint32_t format = samples == 2u ? 159u : 166u;
    const std::uint32_t expandedIndices = samples == 2u ? 0x10u : 0x76543210u;
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
        AgcDriver::Registers cx{{0x318, static_cast<std::uint32_t>(colorMemory.Address() >> 8u)}, {0x31b, 0u}, {0x31c, Float16x4Info | 0x10006000u}, {0x31d, (shift << 12u) | (shift << 15u)}, {0x31e, 0u}, {0x31f, static_cast<std::uint32_t>(cmask.Address() >> 8u)}, {0x321, static_cast<std::uint32_t>((fmask.Address() | pipeXor) >> 8u)}, {0x325, static_cast<std::uint32_t>(dcc.Address() >> 8u)}, {0x390, static_cast<std::uint32_t>(colorMemory.Address() >> 40u)}, {0x398, static_cast<std::uint32_t>(cmask.Address() >> 40u)}, {0x3a0, static_cast<std::uint32_t>(fmask.Address() >> 40u)}, {0x3a8, static_cast<std::uint32_t>(dcc.Address() >> 40u)}, {0x3b0, ((Side - 1u) << 14u) | (Side - 1u)}, {0x3b8, TiledAttrib3}};
        const auto color = DecodeColorBuffer(cx, 0u);
        Require(color.samples == samples && color.cmaskAddress == cmask.Address(), "native FMASK fixture lost its decoded target association");
        std::vector<std::array<float, 4>> expected(samples);
        for (std::uint32_t sample = 0u; sample < samples; ++sample) {
            expected[sample] = {static_cast<float>(sample + 1u) / static_cast<float>(samples), 0.25f, 0.5f, 1.0f};
            auto pixel = PixelCode;
            pixel[1] = std::bit_cast<std::uint32_t>(expected[sample][0]);
            pixel[3] = std::bit_cast<std::uint32_t>(expected[sample][1]);
            pixel[5] = std::bit_cast<std::uint32_t>(expected[sample][2]);
            pixel[7] = std::bit_cast<std::uint32_t>(expected[sample][3]);
            auto state = TargetState(color);
            state.quadSampleMask.fill(static_cast<std::uint16_t>(1u << sample));
            Draw(*device, state, 0u, 2u, pixel);
        }
        const auto native = LoadSamples(*device, TargetDescriptor(colorMemory, shift), samples);
        std::vector<std::uint32_t> covered;
        for (std::uint32_t pixel = 0; pixel < Side * Side; ++pixel) {
            bool all = true;
            for (std::size_t sample = 0; sample < samples; ++sample) all = all && native[pixel * samples + sample] == expected[sample];
            if (all) covered.push_back(pixel);
        }
        Require(!covered.empty(), "native FMASK fixture did not render distinct sample colors");
        for (std::uint32_t lane = 0; lane < Threads; ++lane) {
            const auto pixel = covered[lane % covered.size()];
            NativeFmaskInput[lane * 4u] = pixel % Side;
            NativeFmaskInput[lane * 4u + 1u] = pixel / Side;
            NativeFmaskInput[lane * 4u + 2u] = 0u;
            NativeFmaskInput[lane * 4u + 3u] = 0u;
        }
        const auto image = NativeFmaskDescriptor(fmask, cmask, pipeXor, format);
        const auto check = [&] {
            for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                const std::array<std::uint32_t, 4> mapping{expandedIndices, 1u, expandedIndices, 0u};
                for (std::size_t component = 0; component < 4u; ++component) Require(NativeFmaskOutput[lane * 4u + component] == mapping[component], "native FMASK load did not preserve its expanded sample indices and selectors");
                for (std::uint32_t sample = 0; sample < samples; ++sample) {
                    const auto fragment = NativeFmaskOutput[lane * 4u] >> (4u * sample) & 15u;
                    const auto pixel = NativeFmaskInput[lane * 4u + 1u] * Side + NativeFmaskInput[lane * 4u];
                    Require(native[pixel * samples + fragment] == expected[sample], "native FMASK remapped a sampled color to another fragment");
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
        Require(LoadSamples(*device, TargetDescriptor(colorMemory, shift), samples) == native, "FMASK sampling changed the native samples");
        Require(std::all_of(fmask.data, fmask.data + fmask.bytes, [](auto byte) { return byte == 0x11u; }) && std::all_of(cmask.data, cmask.data + cmask.bytes, [](auto byte) { return byte == 0xccu; }), "native FMASK sampling published or changed guest metadata");
        const auto refused = [&](const auto& descriptor, std::string_view reason) {
            NativeFmaskOutput.fill(sentinel);
            std::string error;
            try { DispatchNativeFmask(*device, descriptor, false); } catch (const std::runtime_error& e) { error = e.what(); }
            Require(error.find(reason) != std::string::npos, "native FMASK missing refusal: " + error);
            Require(std::all_of(NativeFmaskOutput.begin(), NativeFmaskOutput.end(), [sentinel](auto word) { return word == sentinel; }), "rejected native FMASK wrote output");
        };
        refused(NativeFmaskDescriptor(unrelated, cmask, pipeXor, format), "descriptor does not match");
        refused(NativeFmaskDescriptor(fmask, unrelated, pipeXor, format), "descriptor does not match");
        refused(NativeFmaskDescriptor(fmask, cmask, pipeXor ^ 0x100u, format), "descriptor does not match");
        refused(NativeFmaskDescriptor(fmask, cmask, pipeXor, samples == 2u ? 166u : 159u), "format does not match");
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
        const auto uncompressed = LoadSamples(*device, TargetDescriptor(colorMemory, shift), samples);
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
        auto recreatedDescriptor = TargetDescriptor(colorMemory, shift);
        recreatedDescriptor[0] |= recreated.pipeBankXor >> 8u;
        const auto recreatedSamples = LoadSamples(*device, recreatedDescriptor, samples);
        Require(std::find(recreatedSamples.begin(), recreatedSamples.end(), Drawn) != recreatedSamples.end(), "former FMASK metadata affected the recreated native target");
        device->SubmitRecorded(true);
        device->WaitIdle();
        ClearMultisampleTargets(device->Device());
        ClearCachedTextures(device->Device());
        std::printf("native FMASK: %u samples, XOR=0x%x, distinct samples, both registration modes, cached reuse, association/extent/write refusals and unchanged guest metadata passed\n", samples, pipeXor);
    }
}

int ColdNativeFmask(AgcDriver::VulkanDevice& device, std::uint32_t samples) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto shift = static_cast<std::uint32_t>(std::countr_zero(samples));
    for (const std::uint8_t stored : {0u, 1u}) {
        Block colorMemory(4u * BlockBytes, true), dcc(BlockBytes, true), cmask(BlockBytes, true), fmask(BlockBytes, true, false);
        std::memset(cmask.data, 0xcc, cmask.bytes);
        AgcDriver::Registers cx{{0x318, static_cast<std::uint32_t>(colorMemory.Address() >> 8u)}, {0x31b, 0u}, {0x31c, Float16x4Info | 0x10006000u}, {0x31d, (shift << 12u) | (shift << 15u)}, {0x31e, 0u}, {0x31f, static_cast<std::uint32_t>(cmask.Address() >> 8u)}, {0x321, static_cast<std::uint32_t>(fmask.Address() >> 8u)}, {0x325, static_cast<std::uint32_t>(dcc.Address() >> 8u)}, {0x390, static_cast<std::uint32_t>(colorMemory.Address() >> 40u)}, {0x398, static_cast<std::uint32_t>(cmask.Address() >> 40u)}, {0x3a0, static_cast<std::uint32_t>(fmask.Address() >> 40u)}, {0x3a8, static_cast<std::uint32_t>(dcc.Address() >> 40u)}, {0x3b0, ((Side - 1u) << 14u) | (Side - 1u)}, {0x3b8, TiledAttrib3}};
        const auto color = DecodeColorBuffer(cx, 0u);
        Require(color.samples == samples && color.cmaskAddress == cmask.Address() && color.fmaskAddress == fmask.Address() && color.dccAddress == dcc.Address(), "the cold native FMASK fixture lost its decoded target association");
        const auto materialize = [&] {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            MaterializeMultisampleTargetClear(context, color);
        };
        materialize();
        Require(AgcDriver::GuestMemory::ImportWatched(fmask.Address(), fmask.bytes, [] { return true; }), "the overlapping watched import failed");
        if (!AgcDriver::GuestMemory::Watched(fmask.Address(), fmask.bytes)) {
            std::printf("cold native FMASK skipped, host imports are compared, not watched: a zero store over zero pages cannot be told apart\n");
            device.SubmitRecorded(true);
            device.WaitIdle();
            ClearCachedTextures(context.device);
            ClearMultisampleTargets(context.device);
            return VulkanTestSkipped;
        }
        materialize();
        materialize();
        *static_cast<volatile std::uint8_t*>(fmask.data) = stored;
        std::string error;
        try {
            materialize();
        } catch (const std::runtime_error& refusal) {
            error = refusal.what();
        }
        Require(error.find("writes to a resident native FMASK") != std::string::npos, std::string(stored == 0u ? "a real zero store over cold native FMASK was not refused: " : "a real nonzero store over cold native FMASK was not refused: ") + error);
        device.SubmitRecorded(true);
        device.WaitIdle();
        ClearCachedTextures(context.device);
        ClearMultisampleTargets(context.device);
    }
    std::printf("Cold native FMASK: %u samples keep a resident target across an overlapping import of untouched FMASK pages, and refuse real zero and nonzero stores\n", samples);
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 3 && (std::string_view(argv[1]) == "--initial-texture-dcc" || std::string_view(argv[1]) == "--texture-dcc-target")) {
            const auto samples = static_cast<std::uint32_t>(std::stoul(argv[2]));
            Require(samples == 2u || samples == 4u || samples == 8u, "unsupported initial texture sample count");
            const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
            if ((context.limits.framebufferColorSampleCounts & samples) == 0u || (context.limits.sampledImageColorSampleCounts & samples) == 0u) return VulkanTestSkipped;
            if (std::string_view(argv[1]) == "--texture-dcc-target") TextureDccTarget(*device, samples);
            else InitialTextureDcc(*device, samples);
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--native-fmask") {
            const auto samples = static_cast<std::uint32_t>(std::stoul(argv[2]));
            {
                const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
                if (!context.storageImageMultisample || (context.limits.framebufferColorSampleCounts & samples) == 0) {
                    std::printf("native FMASK skipped, the device does not render or store %u-sample color\n", samples);
                    return VulkanTestSkipped;
                }
            }
            NativeFmask(device, samples);
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--cold-native-fmask") {
            const auto samples = static_cast<std::uint32_t>(std::stoul(argv[2]));
            Require(samples == 2u || samples == 8u, "the cold native FMASK probe requires two or eight samples");
            const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
            if (!context.storageImageMultisample || (context.limits.framebufferColorSampleCounts & samples) == 0u) {
                std::printf("cold native FMASK skipped, the device does not render or store %u-sample color\n", samples);
                return VulkanTestSkipped;
            }
            if (!BlocksWriteWatched()) {
                std::printf("cold native FMASK skipped, host imports are compared, not watched: a zero store over zero pages cannot be told apart\n");
                return VulkanTestSkipped;
            }
            return ColdNativeFmask(*device, samples);
        }
        if (argc == 2 && std::string_view(argv[1]) == "--native-vertex-input") return NativeVertexInput(device);
        if (argc == 2 && std::string_view(argv[1]) == "--readback") { Readback(*device); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "--snapshot") { ColorSnapshots(*device); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "--snapshot_refusals") { ColorSnapshotRefusals(*device); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "--multisample-read-single-sample-depth") {
            const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
            if ((context.limits.sampledImageColorSampleCounts & Samples) == 0u) return VulkanTestSkipped;
            MultisampleReadOverSingleSampleDepth(*device);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--initial-nonuniform_lifetime") return NonuniformInitialLifetime(*device);
        if (argc == 2 && std::string_view(argv[1]) == "--initial-nonuniform_capabilities") return NonuniformInitialCapabilities(*device);
        if (argc == 2 && std::string_view(argv[1]) == "--initial-nonuniform_hook") return NonuniformInitialHook(*device);
        if (argc == 2 && std::string_view(argv[1]) == "--initial-nonuniform") return NonuniformInitialContents(*device);
        if (argc == 2 && std::string_view(argv[1]) == "--initial-uniform") { UniformInitialContents(*device); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "--initial-uniform_refusals") { UniformInitialRefusals(*device); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "--initial-uniform_hook") { UniformInitialFlushHook(*device); return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "--htile-hiz-patch") { HtileHiZPatch(*device); return 0; }
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
        if (TargetHasCapability(device->Target(), spv::CapabilityComputeDerivativeGroupLinearKHR)) {
            CheckFirstSampleReads(*device, TargetDescriptor(block, SampleShift), drawn, true, "from the drawn 4x target", AdjustedPlaneCode, "image_sample_a");
            std::puts("a DIM 2D image_sample_a read of a 4x color target returns each pixel's sample 0");
        } else {
            std::vector<std::uint32_t> userData;
            std::string refusal;
            try {
                static_cast<void>(CompileProgram(*device, AdjustedPlaneCode, TargetDescriptor(block, SampleShift), userData, PointMipSampler));
            } catch (const std::runtime_error& error) {
                refusal = error.what();
            }
            Require(refusal.find("compute texture derivatives require enabled ComputeDerivativeGroupLinearKHR") != std::string::npos, "a DIM 2D image_sample_a without compute derivatives was not refused: " + refusal);
        }
        Draw(*device, state, 0, 2u, Unorm8PixelCode);
        const auto redrawn = LoadSamples(*device, block);
        Require(redrawn != drawn, "drawing the 4x target again with another color left its samples unchanged");
        CheckFirstSampleReads(*device, TargetDescriptor(block, SampleShift), redrawn, true, "from the 4x target drawn again with another color after a first read");
        Block zeroed(BlockBytes);
        CheckFirstSampleReads(*device, TargetDescriptor(zeroed, SampleShift), std::vector<std::array<float, 4>>(Side * Side * Samples), false, "from zeroed color memory no draw rendered to");
        Block aliased(4u * BlockBytes);
        Draw(*device, TargetState(DecodeSwappedTarget(aliased)), 0, 2u, Unorm8PixelCode);
        const auto originalAliasSamples = LoadSamples(*device, aliased, Format8888Unorm);
        std::string aliasRefusal;
        try { Draw(*device, TargetState(DecodeTarget(aliased))); }
        catch (const std::runtime_error& error) { aliasRefusal = error.what(); }
        Require(aliasRefusal.find("overlap another native owner") != std::string::npos, "an unrelated-format draw did not refuse unpublished native samples");
        Require(LoadSamples(*device, aliased, Format8888Unorm) == originalAliasSamples, "a refused unrelated-format draw changed the original native samples");
        std::puts("DIM 2D image_load, image_sample_lz and image_sample_l reads of a 4x color target return each pixel's sample 0");
        Block swapped(4u * BlockBytes);
        Draw(*device, TargetState(DecodeSwappedTarget(swapped)), 0, 2u, Unorm8PixelCode);
        const auto swappedSamples = LoadSamples(*device, swapped, Format8888Unorm);
        CheckSwappedSamples(swappedSamples);
        CheckFirstSampleReads(*device, TargetDescriptor(swapped, SampleShift, Format8888Unorm), swappedSamples, true, "from a 4x BGRA8 target read through an RGBA8 descriptor");
        std::puts("a 4x BGRA8 color target loaded through an RGBA8 texture descriptor reads its drawn color with red and blue swapped");
        Block packedFloat(4u * BlockBytes);
        Draw(*device, TargetState(DecodePackedFloatTarget(packedFloat)));
        const auto packedSamples = LoadSamples(*device, packedFloat, Format11_11_10Float);
        CheckPackedFloatSamples(packedSamples);
        CheckFirstSampleReads(*device, TargetDescriptor(packedFloat, SampleShift, Format11_11_10Float), packedSamples, true, "from a 4x 10_11_11 float target read through a format 36 descriptor");
        std::puts("a 4x 10_11_11 float color target loaded through a format 36 (11_11_10 float) texture descriptor reads its drawn color");
        Block integer(4u * BlockBytes);
        Block signedFill(BlockBytes);
        if ((AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).limits.sampledImageIntegerSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0) {
            std::puts("DIM 2D reads of 4x 8_8_8_8_UINT and 8_8_8_8_SINT targets not tested: sampledImageIntegerSampleCounts lacks VK_SAMPLE_COUNT_4_BIT");
        } else {
            Draw(*device, TargetState(DecodeUintTarget(integer)), 0, 2u, Uint8PixelCode);
            const auto integerSamples = LoadSamples(*device, integer, Format8888Uint);
            CheckUintSamples(integerSamples);
            CheckFirstSampleReads(*device, TargetDescriptor(integer, SampleShift, Format8888Uint), integerSamples, true, "from a drawn 4x 8_8_8_8_UINT target");
            StoreWords(signedFill, BlockBytes, FilledSint8);
            CheckFirstSampleReads(*device, TargetDescriptor(signedFill, SampleShift, Format8888Sint), std::vector<std::array<float, 4>>(Side * Side * Samples, std::bit_cast<std::array<float, 4>>(FilledSint8Texel)), false, "from 8_8_8_8_SINT color memory no draw rendered to");
            std::puts("DIM 2D reads of a drawn 4x 8_8_8_8_UINT target and of 8_8_8_8_SINT color memory no draw rendered to return each pixel's sample 0 bit for bit");
        }
        CheckOverwrittenTarget(*device);
        CheckClaimedTargets(*device);
        if (!FormatlessMultisampleWrites(AgcDriver::VulkanDeviceTestAccess::ContextOf(*device))) {
            std::puts("multisampled target fills, stores and swizzle modes skipped, the device does not enable multisampled formatless storage writes");
            return VulkanTestSkipped;
        }
        const auto colorBytes = AgcDriver::Graphics::MultisampledColorBytes(color);
        Require(colorBytes == BlockBytes, "the native color target does not occupy exactly one BDA block");
        Block bda(4u * BlockBytes);
        const auto bdaState = TargetState(DecodeTarget(bda));
        CheckBdaSpan(*device, bda, 0, false);
        CheckBdaSpan(*device, bda, BlockBytes, false);
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::MaterializeMultisampleTargetClear(AgcDriver::VulkanDeviceTestAccess::ContextOf(*device), bdaState.color);
        }
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
        std::vector<std::string> missing;
        const auto dispatch = [&](const auto& program) {
            device->Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()));
            device->WaitIdle();
        };
        CheckBufferRefused([&] { dispatch(CompileBufferLoad(*device, block.Address() + 4u, output.Address())); }, "fresh read", missing);
        CheckBufferRefused([&] { dispatch(CompileBufferLoad(*device, block.Address() + colorBytes - 4u, output.Address())); }, "edge read", missing);
        CheckBufferRefused([&] { dispatch(CompileBufferLoad(*device, block.Address() + colorBytes, block.Address())); }, "write", missing);
        CheckBufferRefused([&] { dispatch(warm); }, "warmed dispatch", missing);
        if (prepared != nullptr) {
            CheckBufferRefused([&] {
                device->Dispatch(deferredProgram, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()), prepared);
                device->WaitIdle();
            }, "deferred dispatch", missing);
        }
        bool recipeReused = false;
        if (recipe != nullptr) {
            const auto hit = device->PrepareRecipe(recipe, false);
            Require(hit != nullptr, "the warmed buffer recipe did not retain its template");
            recipeReused = true;
            CheckBufferRefused([&] {
                AgcDriver::VulkanDevice::IndirectOutcome outcome{};
                if (device->DispatchRecipe(warm, 1, 1, 1, 0, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()), hit, outcome) == AgcDriver::RecipeOutcome::Rebuild) {
                    std::puts("native buffer recipe requested rebuild; checking the ordinary dispatch refusal");
                    device->Dispatch(warm, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(BufferLoadCode.data()));
                }
                device->WaitIdle();
            }, "recipe dispatch", missing);
        }
        std::memset(block.data + colorBytes, 0, BlockBytes);
        CheckBufferLoad(*device, CompileBufferLoad(*device, block.Address() + colorBytes, output.Address()), output);
        CheckUniform(LoadSamples(*device, block), Ones, "after the native buffer access refusals and adjacent buffer control");
        std::cout << "native buffer controls: adjacent=passed deferred=" << (prepared != nullptr) << " recipe=" << recipeReused << std::endl;
        std::string failures;
        for (const auto& item : missing) failures += " " + item;
        Require(missing.empty(), "buffer aliases of live native MSAA color memory bypassed the guard:" + failures);
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
        CheckUniform(LoadSamples(*device, mixed), Ones, "from color memory no draw rendered to whose padding alone differs");
        for (const auto& [mode, tileMode] : {std::pair{TileRenderTarget, AgcDriver::Graphics::TextureTileMode::kR64KBX}, std::pair{TileZ64KBX, AgcDriver::Graphics::TextureTileMode::kZ64KBX}}) {
            Block varying(BlockBytes);
            const auto expected = StoreVaryingUnorm(varying, tileMode);
            CheckLoaded(LoadSamples(*device, varying, Format8888Unorm, mode), expected, "from varying color memory of a " + std::to_string(mode) + " swizzle surface no draw rendered to");
        }
        Block varyingD(BlockBytes);
        StoreWords(varyingD, colorBytes, FillOnes);
        std::memcpy(varyingD.data, &odd, sizeof(odd));
        RequireRefused(*device, TargetDescriptor(varyingD, SampleShift, Format16161616Float, TileD64KBX), "non-uniform multisampled guest color tiling is not implemented", "loading varying D_X color memory no draw rendered to");
        std::puts("loads of R_X and Z_X color memory no draw rendered to return each sample's guest color, and varying D_X color memory is refused");
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
        StoreSamples(*device, TargetDescriptor(mixedStore, SampleShift), SampleWrites(oneSample));
        CheckWritten(std::vector<std::array<float, 4>>(Side * Side * Samples, Ones), LoadSamples(*device, mixedStore), oneSample, "into one sample of each pixel of color memory no draw rendered to whose padding alone differs");
        std::puts("stores to one sample of each pixel of a drawn 4x target and to samples of zeroed or non-uniform color memory no draw rendered to reach exactly those samples");
        for (const auto mode : {TileZ64KBX, TileD64KBX}) {
            Block swizzled(BlockBytes);
            StoreWords(swizzled, colorBytes, FillOnes);
            CheckUniform(LoadSamples(*device, swizzled, Format16161616Float, mode), Ones, "from the uniform memory of a " + std::to_string(mode) + " swizzle surface no draw rendered to");
        }
        const auto htileBytes = AgcDriver::Graphics::HtileMetadataBytes({Side, Side});
        Require(htileBytes <= BlockBytes, "the HTILE of the 4x texture does not fit one block");
        Block cleared(2u * BlockBytes);
        StoreWords(cleared, colorBytes, FillOnes);
        const auto clearedHtile = cleared.Address() + BlockBytes;
        CheckUniform(LoadSamples(*device, CompressedDescriptor(cleared, TileZ64KBX, clearedHtile)), {}, "from a Z swizzle surface whose HTILE is all cleared to 0.0 over memory of other values");
        AgcDriver::Graphics::NoteColorMetadataFill(clearedHtile, sizeof(std::uint32_t), 0u);
        RequireRefused(*device, CompressedDescriptor(cleared, TileZ64KBX, clearedHtile), "over the HTILE", "loading a Z swizzle texture after a fill over part of its HTILE");
        AgcDriver::Graphics::NoteColorMetadataFill(clearedHtile, static_cast<std::size_t>(htileBytes), 0u);
        CheckUniform(LoadSamples(*device, CompressedDescriptor(cleared, TileZ64KBX, clearedHtile)), {}, "from a Z swizzle surface after a fill of 0 over its whole HTILE");
        Block expanded(2u * BlockBytes);
        StoreWords(expanded, colorBytes, FillOnes);
        std::fill_n(reinterpret_cast<std::uint32_t*>(expanded.data + BlockBytes), htileBytes / sizeof(std::uint32_t), 0xffffffffu);
        CheckUniform(LoadSamples(*device, CompressedDescriptor(expanded, TileZ64KBX, expanded.Address() + BlockBytes)), Ones, "from the uniform memory of a Z swizzle surface whose HTILE is all expanded");
        Block replaced(2u * BlockBytes);
        StoreWords(replaced, colorBytes, FillOnes);
        const auto replacedHtile = replaced.Address() + BlockBytes;
        CheckUniform(LoadSamples(*device, CompressedDescriptor(replaced, TileZ64KBX, replacedHtile)), {}, "from a Z swizzle surface whose HTILE is all cleared, before its HTILE alone is replaced");
        AgcDriver::Graphics::RetireResidentImagesOver_nid_no_patch(replacedHtile, static_cast<std::size_t>(htileBytes));
        std::fill_n(reinterpret_cast<std::uint32_t*>(replaced.data + BlockBytes), htileBytes / sizeof(std::uint32_t), 0xffffffffu);
        CheckUniform(LoadSamples(*device, CompressedDescriptor(replaced, TileZ64KBX, replacedHtile)), Ones, "from a Z swizzle surface after its HTILE alone was replaced by expanded tiles");
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
        RequireRefused(*device, TargetDescriptor(standard, SampleShift, Format16161616Float, TileS64KBX), "has no modeled multisampled color footprint", "loading an S swizzle multisampled texture no draw rendered to");
        device->WaitIdle();
        AgcDriver::Graphics::ClearMultisampleTargets(device->Device());
        CheckBdaSpan(*device, block, 0, false);
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::MaterializeMultisampleTargetClear(AgcDriver::VulkanDeviceTestAccess::ContextOf(*device), DecodeTarget(block));
        }
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
