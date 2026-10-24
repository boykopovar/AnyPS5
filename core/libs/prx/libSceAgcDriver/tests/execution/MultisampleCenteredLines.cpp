#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
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
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Side = 16;
constexpr std::uint32_t Samples = 4;
constexpr std::uint32_t SampleShift = 2;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t Float16x4Info = 0x0730u;
constexpr std::uint32_t MultisampledAttrib = (SampleShift << 12u) | (SampleShift << 15u);
constexpr std::uint32_t TiledAttrib3 = 0x4dc6c000u;
constexpr std::uint32_t AaConfig4x = 0x0020c002u;
constexpr std::uint32_t MsaaEnableClear = 2;
constexpr std::uint32_t StandardLocations4x = 0x622ae6aeu;
constexpr std::uint32_t Format16161616Float = 71;
constexpr std::uint32_t Type2DMsaa = 14;
constexpr std::uint32_t TileRenderTarget = 0x1b;
constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 32;
constexpr std::uint32_t LoadResult = 12;
constexpr std::array<std::uint32_t, 2> Primitives{2u, 3u};
constexpr std::array<float, 4> Drawn{0.25f, 0.5f, 0.125f, 1.0f};
constexpr std::array<std::byte, 4> DrawnBytes{std::byte{64}, std::byte{128}, std::byte{32}, std::byte{255}};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelCode{
    0x7e0802ff, 0x3e800000, 0x7e0a02ff, 0x3f000000, 0x7e0c02ff, 0x3e000000, 0x7e0e02ff, 0x3f800000, 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 11> Unorm8PixelCode{
    0x7e0802ff, std::bit_cast<std::uint32_t>(64.0f / 255.0f), 0x7e0a02ff, std::bit_cast<std::uint32_t>(128.0f / 255.0f), 0x7e0c02ff, std::bit_cast<std::uint32_t>(32.0f / 255.0f), 0x7e0e02ff, std::bit_cast<std::uint32_t>(1.0f), 0xf800180f, 0x07060504, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 26> LoadCode{
    0x34020087, 0xe0301000, 0x80000201, 0xe0301004, 0x80000301, 0xe0301008, 0x80000401, 0xe030100c,
    0x80000501, 0xe0301010, 0x80000601, 0xe0301014, 0x80000701, 0xbf8c3f70, 0xf0001f30, 0x00011005,
    0xbf8c3f70, 0xe0701030, 0x80001001, 0xe0701034, 0x80001101, 0xe0701038, 0x80001201, 0xe070103c,
    0x80001301, 0xbf810000,
};

alignas(256) constexpr std::array<std::array<float, 4>, 4> Lines{{
    {-0.828125f, -0.4375f, 0.5f, 1.0f}, {0.90625f, 0.140625f, 0.5f, 1.0f},
    {-0.265625f, 0.90625f, 0.5f, 1.0f}, {0.34375f, -0.921875f, 0.5f, 1.0f},
}};

alignas(256) std::array<std::uint32_t, Threads * Words> Buffer{};
alignas(256) std::array<std::array<std::byte, 4096>, Primitives.size()> LinePixels{};

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
#endif
        Require(data != nullptr, "cannot allocate the multisampled color target");
        std::memset(data, 0, bytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }
    std::size_t bytes;
    std::uint8_t* data = nullptr;
};

AgcDriver::QueueState LineQueue(const Block& block, std::uint32_t primitive) {
    const auto half = std::bit_cast<std::uint32_t>(static_cast<float>(Side) / 2.0f);
    const auto extent = (Side << 16u) | Side;
    AgcDriver::QueueState queue;
    queue.userConfig[0x242] = primitive;
    queue.context = {
        {0x2d5, 0x2000}, {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, AaConfig4x}, {0x292, MsaaEnableClear}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4},
        {0x318, static_cast<std::uint32_t>(block.Address() >> 8u)}, {0x390, static_cast<std::uint32_t>(block.Address() >> 40u)},
        {0x31b, 0}, {0x31c, Float16x4Info}, {0x31d, MultisampledAttrib},
        {0x3b0, ((Side - 1u) << 14u) | (Side - 1u)}, {0x3b8, TiledAttrib3}, {0x1e0, 0},
        {0xc, 0}, {0xd, extent}, {0x81, 0x80000000}, {0x82, extent},
        {0x90, 0x80000000}, {0x91, extent}, {0x94, 0x80000000}, {0x95, extent},
        {0x10f, half}, {0x110, half}, {0x111, std::bit_cast<std::uint32_t>(-static_cast<float>(Side) / 2.0f)}, {0x112, half},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0}, {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}
    };
    for (std::uint32_t offset = 0x2fe; offset <= 0x30d; ++offset) queue.context[offset] = (offset - 0x2feu) % 4u == 0 ? StandardLocations4x : 0u;
    queue.shader[0x008] = 0x100;
    queue.shader[0x009] = 0;
    return queue;
}

AgcDriver::Graphics::State DecodeLines(const Block& block, std::uint32_t primitive) {
    const auto state = AgcDriver::Graphics::DecodeState(LineQueue(block, primitive));
    const auto& color = state.color;
    Require(state.topology == (primitive == 2u ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_LINE_STRIP), "VGT_PRIMITIVE_TYPE " + std::to_string(primitive) + " did not decode as a line list or strip");
    Require(state.colors.size() == 1 && color.address == block.Address() && color.format == VK_FORMAT_R16G16B16A16_SFLOAT && color.samples == Samples && color.extent.width == Side && color.extent.height == Side, "the target did not decode as a 16x16 RGBA16F surface of 4 samples");
    Require(state.samples == Samples && !state.depth && !state.perSampleShading && !state.alphaToCoverage, "the lines did not decode as a 4x draw without depth, sample shading or alpha-to-coverage");
    const auto locations = AgcDriver::Graphics::SampleLocations(state);
    for (std::uint32_t sample = 0; sample < Samples; ++sample) Require(locations[sample].x == 0.5f && locations[sample].y == 0.5f, "the decoded 4x line state does not put every sample at the pixel center");
    Require(state.viewport.x == 0.0f && state.viewport.y == static_cast<float>(Side) && state.viewport.width == static_cast<float>(Side) && state.viewport.height == -static_cast<float>(Side) && state.scissor.offset.x == 0 && state.scissor.offset.y == 0 && state.scissor.extent.width == Side && state.scissor.extent.height == Side, "the decoded viewport or scissor does not cover the 16x16 target");
    return state;
}

AgcDriver::Graphics::State OneSample(AgcDriver::Graphics::State state, std::span<std::byte> pixels, const AgcDriver::Graphics::ColorTargetLayout& linear) {
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Side, Side}, VK_FORMAT_R8G8B8A8_UNORM, linear.Bytes(), 0xe4u};
    state.colors = {state.color};
    state.samples = 1;
    state.customSampleLocations = false;
    state.sampleLocations.fill(0);
    state.sampleLocationGrid = false;
    return state;
}

void DrawLines(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::State& state, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto address = reinterpret_cast<std::uintptr_t>(Lines.data());
    const std::array<std::uint32_t, 4> vertexBuffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Lines.size()), 0x01016facu};
    std::copy(vertexBuffer.begin(), vertexBuffer.end(), vertexUserData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {WaveSize, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
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
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{codeAddress, std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, codeAddress, pixelCode, 0, {}},
        {WaveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Lines.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

std::vector<std::array<float, 4>> LoadSamples(AgcDriver::VulkanDevice& device, const Block& block) {
    const auto address = block.Address();
    const std::array<std::uint32_t, 8> texture{
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format16161616Float << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (SampleShift << 16u) | (TileRenderTarget << 20u) | (Type2DMsaa << 28u),
        0u,
        SampleShift << 4u,
        0u,
        0u,
    };
    const auto bufferAddress = reinterpret_cast<std::uintptr_t>(Buffer.data());
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(bufferAddress), static_cast<std::uint32_t>((bufferAddress >> 32u) & 0xffffu), static_cast<std::uint32_t>(Buffer.size() * 4u), 0x31016facu};
    std::vector<std::uint32_t> userData(16, 0u);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(LoadCode.data()), std::as_bytes(std::span(LoadCode))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(LoadCode.data()), LoadCode, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto program = ShaderRecompiler::Recompile(request);
    std::vector<std::array<float, 4>> loaded(Side * Side * Samples);
    for (std::uint32_t first = 0; first < loaded.size(); first += Threads) {
        Buffer.fill(0xdeadbeefu);
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto index = first + tid;
            const auto pixel = index / Samples;
            const std::array<std::uint32_t, 6> words{0u, 0u, 0u, pixel % Side, pixel / Side, index % Samples};
            std::copy(words.begin(), words.end(), &Buffer[tid * Words]);
        }
        device.Dispatch(program, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(LoadCode.data()));
        device.WaitIdle();
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            for (std::uint32_t component = 0; component < 4u; ++component) loaded[first + tid][component] = std::bit_cast<float>(Buffer[tid * Words + LoadResult + component]);
        }
    }
    return loaded;
}

bool NearEndpoint(std::uint32_t x, std::uint32_t y) {
    return std::any_of(Lines.begin(), Lines.end(), [&](const std::array<float, 4>& vertex) {
        const double px = (static_cast<double>(vertex[0]) + 1.0) * 0.5 * Side;
        const double py = Side - (static_cast<double>(vertex[1]) + 1.0) * 0.5 * Side;
        return std::fabs(x + 0.5 - px) < 1.5 && std::fabs(y + 0.5 - py) < 1.5;
    });
}

void CompareLines(AgcDriver::VulkanDevice& device, const Block& block, std::uint32_t primitive, std::span<std::byte> pixels) {
    const auto decoded = DecodeLines(block, primitive);
    DrawLines(device, decoded, PixelCode);
    const auto loaded = LoadSamples(device, block);
    const AgcDriver::Graphics::ColorTargetLayout linear(Side, Side, AgcDriver::Graphics::ColorTileMode::Linear, 4u);
    Require(linear.Bytes() <= pixels.size(), "the one-sample line target does not fit its memory");
    std::fill(pixels.begin(), pixels.end(), std::byte{0});
    DrawLines(device, OneSample(decoded, pixels, linear), Unorm8PixelCode);
    std::uint32_t covered = 0;
    std::uint32_t single = 0;
    std::uint32_t compared = 0;
    std::uint32_t differing = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            char where[80];
            std::snprintf(where, sizeof(where), "VGT_PRIMITIVE_TYPE %u: pixel (%u, %u)", primitive, x, y);
            std::uint32_t drawn = 0;
            for (std::uint32_t sample = 0; sample < Samples; ++sample) {
                const auto& texel = loaded[(y * Side + x) * Samples + sample];
                Require(texel == Drawn || texel == std::array<float, 4>{}, std::string(where) + " of the 4x target holds neither the line color nor the cleared value");
                drawn += texel == Drawn ? 1u : 0u;
            }
            Require(drawn == 0u || drawn == Samples, std::string(where) + " has " + std::to_string(drawn) + " of its 4 samples covered by a line with every sample at the pixel center");
            const auto bytes = pixels.begin() + static_cast<std::ptrdiff_t>(linear.Offset(x, y));
            const bool oneSample = std::equal(DrawnBytes.begin(), DrawnBytes.end(), bytes);
            Require(oneSample || std::all_of(bytes, bytes + 4, [](std::byte value) { return value == std::byte{0}; }), std::string(where) + " of the one-sample target holds neither the line color nor zero");
            covered += drawn == Samples ? 1u : 0u;
            single += oneSample ? 1u : 0u;
            if (NearEndpoint(x, y)) continue;
            ++compared;
            differing += (drawn == Samples) != oneSample ? 1u : 0u;
            Require(!oneSample || drawn == Samples, std::string(where) + " is drawn by the one-sample line but not covered in the 4x target");
        }
    }
    Require(covered >= 20u && covered <= 2u * single && compared >= 200u, "VGT_PRIMITIVE_TYPE " + std::to_string(primitive) + ": the 4x lines covered " + std::to_string(covered) + " pixels and the one-sample lines " + std::to_string(single) + ", with " + std::to_string(compared) + " pixels compared");
    std::printf("VGT_PRIMITIVE_TYPE %u: 4x lines with every sample at the pixel center covered %u pixels in full and none in part; the one-sample lines drew %u; %u of the %u pixels away from the endpoints differ\n", primitive, covered, single, differing, compared);
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).storageImageMultisample) {
            std::puts("skipped, the device has no shaderStorageImageMultisample to load the samples of a 4x target");
            return VulkanTestSkipped;
        }
        if ((AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).sampleLocationSampleCounts & Samples) == 0u) {
            std::puts("skipped, the device has no VK_EXT_sample_locations at 4 samples to put every sample at the pixel center");
            return VulkanTestSkipped;
        }
        Block listTarget(4u * BlockBytes);
        Block stripTarget(4u * BlockBytes);
        struct ReleaseDevice {
            std::unique_ptr<AgcDriver::VulkanDevice>& value;
            ~ReleaseDevice() { value.reset(); }
        } releaseDevice{device};
        const std::array<const Block*, Primitives.size()> targets{&listTarget, &stripTarget};
        for (std::size_t index = 0; index < Primitives.size(); ++index) CompareLines(*device, *targets[index], Primitives[index], LinePixels[index]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
