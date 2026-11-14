#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
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

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t RegionBytes = 128 * BlockBytes;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;
constexpr std::uint8_t Initial = 0x3f;
constexpr std::uint8_t Sentinel = 0x37;
constexpr std::uint8_t Gap = 0x29;
constexpr std::size_t GapBytes = 0x100;

struct Surface {
    std::size_t offset;
    std::uint32_t width;
    std::uint32_t height;
    std::size_t RowBytes() const { return std::size_t{width} * 16u; }
    std::size_t Bytes() const { return RowBytes() * height; }
};

constexpr Surface Tall{0, 64, 256};
constexpr Surface Large{0, 512, 600};
constexpr std::size_t PageBytes = 4096;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 16>, 2> Programs{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::span<const std::uint32_t> Program(std::size_t index, bool blue) {
    auto& code = Programs.at(index);
    std::size_t at = 0;
    code[at++] = 0x36000000u | (20u << 17u) | 0xffu;
    code[at++] = 0x0000ffffu;
    code[at++] = 0x2c000000u | (21u << 17u) | 0x90u;
    code[at++] = 0x7e000c00u | (20u << 17u) | (256u + 20u);
    code[at++] = 0x7e000c00u | (21u << 17u) | (256u + 21u);
    code[at++] = (blue ? 0x7e0002f2u : 0x7e000280u) | (22u << 17u);
    code[at++] = 0x7e0002f2u | (23u << 17u);
    code[at++] = 0xf800180fu;
    code[at++] = 0x17161514u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes), arena(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (arena) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (arena) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "write-back guest newer: cannot map the color targets");
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
        Require(data != nullptr, "write-back guest newer: cannot allocate the color targets");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~Block() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (arena) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (arena) {
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
    bool arena;
    std::uint8_t* data = nullptr;
};

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, const Surface& surface) {
    const auto address = block.Address() + surface.offset;
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(address >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Rgba32FloatInfo;
    cx[0x31d] = 0;
    cx[0x323] = 0;
    cx[0x324] = 0;
    cx[0x3b0] = ((surface.width - 1u) << 14u) | (surface.height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(address >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    color.exportIndex = 0;
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, const Surface& surface, std::span<const std::uint32_t> pixelCode, VkRect2D scissor) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(Triangle.data(), 16u, static_cast<std::uint32_t>(Triangle.size()));
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

    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionFixedPoint);
    pixel.targetOutputMode.at(0) = Float32AbgrExport;
    pixel.targetExportMapping.fill(0xe4u);
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {waveSize, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};

    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_FALSE;
    blend.colorWriteMask = 0xf;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {surface.width, surface.height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(surface.height), static_cast<float>(surface.width), -static_cast<float>(surface.height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = scissor;
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

bool Inside(const VkRect2D& rect, std::uint32_t x, std::uint32_t y) {
    return x >= static_cast<std::uint32_t>(rect.offset.x) && x < static_cast<std::uint32_t>(rect.offset.x) + rect.extent.width && y >= static_cast<std::uint32_t>(rect.offset.y) && y < static_cast<std::uint32_t>(rect.offset.y) + rect.extent.height;
}

std::uint8_t* Pixel(const Block& block, const Surface& surface, std::uint32_t x, std::uint32_t y) {
    return block.data + surface.offset + static_cast<std::size_t>(y) * surface.RowBytes() + static_cast<std::size_t>(x) * 16u;
}

bool Holds(const std::uint8_t* pixel, std::uint8_t value) {
    return std::all_of(pixel, pixel + 16, [&](std::uint8_t byte) { return byte == value; });
}

bool HoldsOutput(const std::uint8_t* pixel, std::uint32_t x, std::uint32_t y, float z) {
    std::array<float, 4> texel{};
    std::memcpy(texel.data(), pixel, sizeof(texel));
    return texel == std::array<float, 4>{static_cast<float>(x), static_cast<float>(y), z, 1.0f};
}

enum class Write { None, BeforeDraw, AfterDraw };

struct Result {
    std::size_t wrongDrawn = 0;
    std::size_t wrongUntouched = 0;
    bool sentinelKept = false;
    bool sentinelDrawn = false;
};

std::size_t PageOf(const Surface& surface, std::uint32_t x, std::uint32_t y) {
    return (surface.offset + static_cast<std::size_t>(y) * surface.RowBytes() + static_cast<std::size_t>(x) * 16u) / PageBytes;
}

bool Run(AgcDriver::VulkanDevice& device, const Surface& surface, Write write, Result& result) {
    Block block(RegionBytes);
    std::fill(block.data, block.data + block.bytes, Initial);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), block.bytes) == nullptr) return false;
    const auto target = DecodeTarget(block, surface);
    Require(target.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && target.bytes == surface.Bytes(), "write-back guest newer: the target is not a linear R32G32B32A32 target");
    const VkRect2D seed{{static_cast<std::int32_t>(surface.width - 1u), static_cast<std::int32_t>(surface.height - 1u)}, {1, 1}};
    const VkRect2D drawn{{8, 4}, {16, 16}};
    constexpr std::uint32_t sentinelX = 12;
    constexpr std::uint32_t sentinelY = 9;
    Draw(device, target, surface, Program(0, false), seed);
    device.WaitIdle();
    if (write == Write::BeforeDraw) std::memset(Pixel(block, surface, sentinelX, sentinelY), Sentinel, 16);
    Draw(device, target, surface, Program(0, false), drawn);
    if (write == Write::AfterDraw) std::memset(Pixel(block, surface, sentinelX, sentinelY), Sentinel, 16);
    device.WaitIdle();
    const auto sentinelPage = PageOf(surface, sentinelX, sentinelY);
    const bool pageFallback = surface.Bytes() > (std::size_t{1} << 20u);
    for (std::uint32_t y = 0; y < surface.height; ++y) {
        for (std::uint32_t x = 0; x < surface.width; ++x) {
            const auto* pixel = Pixel(block, surface, x, y);
            if (x == sentinelX && y == sentinelY) {
                result.sentinelKept = Holds(pixel, Sentinel);
                result.sentinelDrawn = HoldsOutput(pixel, x, y, 0.0f);
                continue;
            }
            if (Inside(drawn, x, y) || Inside(seed, x, y)) {
                if (write == Write::AfterDraw && pageFallback && PageOf(surface, x, y) == sentinelPage) continue;
                result.wrongDrawn += HoldsOutput(pixel, x, y, 0.0f) ? 0u : 1u;
            } else {
                result.wrongUntouched += Holds(pixel, Initial) ? 0u : 1u;
            }
        }
    }
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!AgcDriver::GuestMemory::WriteWatched()) {
            std::puts("write-back guest newer test skipped: guest memory is not write-watched");
            return VulkanTestSkipped;
        }
        struct Case {
            const char* name;
            const Surface* surface;
            Write write;
            bool control;
        };
        const std::array<Case, 6> cases{{
            {"control: no guest write, a target small enough for a pending snapshot", &Tall, Write::None, true},
            {"control: the guest wrote inside the drawn rectangle before the draw (the draw wins)", &Tall, Write::BeforeDraw, true},
            {"control: no guest write, a target too large for a pending snapshot", &Large, Write::None, true},
            {"control: the guest wrote inside the drawn rectangle of a large target before the draw (the draw wins)", &Large, Write::BeforeDraw, true},
            {"the guest wrote inside the drawn rectangle after the draw, small target (byte by byte against the snapshot)", &Tall, Write::AfterDraw, false},
            {"the guest wrote inside the drawn rectangle after the draw, large target (by whole pages)", &Large, Write::AfterDraw, false},
        }};
        std::vector<std::string> controlFailures, failures;
        for (const auto& test : cases) {
            Result result;
            if (!Run(*device, *test.surface, test.write, result)) {
                std::puts("write-back guest newer test skipped: no host import on this device");
                return VulkanTestSkipped;
            }
            const bool sentinelRight = test.write == Write::AfterDraw ? result.sentinelKept : result.sentinelDrawn;
            const bool right = sentinelRight && result.wrongDrawn == 0 && result.wrongUntouched == 0;
            std::printf("%s: %s (sentinel pixel %s, wrong drawn pixels %zu, changed untouched pixels %zu)\n", test.name, right ? "correct" : "WRONG", result.sentinelKept ? "holds the guest's bytes" : result.sentinelDrawn ? "holds the draw's output" : "holds neither", result.wrongDrawn, result.wrongUntouched);
            if (!right) (test.control ? controlFailures : failures).push_back(test.name);
        }
        if (!controlFailures.empty()) {
            std::string all;
            for (const auto& name : controlFailures) all += "\n  " + name;
            throw std::runtime_error("write-back guest newer: the controls are wrong, so the test itself is broken:" + all);
        }
        if (!failures.empty()) {
            std::string all;
            for (const auto& name : failures) all += "\n  " + name;
            throw std::runtime_error("write-back guest newer: a drawn target's write-back overwrote bytes the guest wrote after the draw:" + all);
        }
        std::puts("write-back guest newer: bytes the guest wrote after a draw survive the target's write-back, and the draw's other pixels land");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
