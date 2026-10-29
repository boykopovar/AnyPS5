#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
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
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t Rgba8Info = 0x8028u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;
constexpr std::uint32_t NoInput = 0xffffffffu;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

constexpr std::array<std::array<float, 4>, 6> LeftHalf{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {0.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f},
    {0.0f, -1.0f, 0.5f, 1.0f}, {0.0f, 1.0f, 0.5f, 1.0f}, {-1.0f, 1.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 16>, 15> Programs{};
std::size_t programCount = 0;

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::span<const std::uint32_t> Program(std::uint32_t fixedVgpr, std::uint32_t stippleVgpr, std::uint32_t blueConstant = 0x80u) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    code[at++] = 0x36000000u | (20u << 17u) | (fixedVgpr << 9u) | 0xffu;
    code[at++] = 0x0000ffffu;
    code[at++] = 0x2c000000u | (21u << 17u) | (fixedVgpr << 9u) | 0x90u;
    code[at++] = 0x7e000c00u | (20u << 17u) | (256u + 20u);
    code[at++] = 0x7e000c00u | (21u << 17u) | (256u + 21u);
    code[at++] = stippleVgpr == NoInput ? 0x7e000200u | blueConstant | (22u << 17u) : 0x7e000c00u | (22u << 17u) | (256u + stippleVgpr);
    code[at++] = 0x7e0002f2u | (23u << 17u);
    code[at++] = 0xf800180fu;
    code[at++] = 0x17161514u;
    code[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(code.data(), at);
}

struct Block {
    explicit Block(std::size_t bytes) : bytes(bytes), watched(AgcDriver::GuestMemory::WriteWatched()) {
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
        Require(!watched || AgcDriver::GuestMemory::Watched(Address(), bytes), "the color target is not write-watched");
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block, std::uint32_t info) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = info;
    cx[0x31d] = 0;
    cx[0x323] = 0;
    cx[0x324] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    color.exportIndex = 0;
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t inputAddr, std::span<const std::uint32_t> pixelCode,
          std::uint32_t scissorWidth = Width, VkColorComponentFlags colorWriteMask = 0xf, std::span<const std::array<float, 4>> vertices = Triangle, bool halveDestination = false, bool recipe = false) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const auto vertexBuffer = BufferDescriptor(vertices.data(), 16u, static_cast<std::uint32_t>(vertices.size()));
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
    pixel.inputAddr = inputAddr;
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
    blend.blendEnable = halveDestination ? VK_TRUE : VK_FALSE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = colorWriteMask;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {scissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {0.5f, 0.5f, 0.5f, 0.5f};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(vertices.size()), 0, 1, 0, false};
    if (!recipe) {
        device.Draw(state, draw, shaders);
        return;
    }
    std::shared_ptr<const AgcDriver::DrawRecipe> captured;
    device.Draw(state, draw, shaders, {}, &captured);
    Require(captured != nullptr, "the linear draw did not produce a reusable recipe");
    device.WaitIdle();
    Require(device.DrawFromRecipe(state, draw, shaders, {}, captured) == AgcDriver::RecipeOutcome::Recorded,
            "the linear draw did not execute its cached recipe");
}


void Check(const std::vector<std::uint8_t>& stored, const std::string& what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            std::array<float, 4> texel{};
            std::memcpy(texel.data(), stored.data() + (static_cast<std::size_t>(y) * Width + x) * 16u, sizeof(texel));
            const std::array<float, 4> expected{static_cast<float>(x), static_cast<float>(y), 0.0f, 1.0f};
            Require(texel == expected, what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") holds (" + std::to_string(texel[0]) + ", " + std::to_string(texel[1]) + ", " + std::to_string(texel[2]) + ", " + std::to_string(texel[3]) + ")");
        }
    }
}

enum class Coverage { Scissor, BlueOnly, Geometry };

constexpr std::size_t TargetBytes = std::size_t{Width} * Height * 16u;

bool Expect(AgcDriver::VulkanDevice& device, Block& block, Coverage coverage, bool seed = false, bool recipe = false) {
    using ShaderRecompiler::PixelInput;
    using ShaderRecompiler::PixelInputBit;
    constexpr std::size_t bytes = TargetBytes;
    std::fill(block.data, block.data + bytes, std::uint8_t{0xff});
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), bytes) == nullptr) return false;
    const auto color = DecodeTarget(block, Rgba32FloatInfo);
    Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && color.bytes == bytes, "the target is not a linear R32G32B32A32 target");
    Draw(device, color, PixelInputBit(PixelInput::PositionFixedPoint), Program(0u, NoInput));
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    const bool resident = std::getenv("APS5_NO_RESIDENT_LINEAR") == nullptr;
    Require(!resident || (recorder != nullptr && !recorder->Idle()), "the draw into an imported linear target waited for the GPU instead of being recorded");
    device.WaitIdle();
    std::vector<std::uint8_t> stored(bytes);
    std::memcpy(stored.data(), block.data, bytes);
    Check(stored, "after the recorded draw, raw guest read");
    constexpr std::uint32_t drawn = Width / 2u;
    constexpr std::size_t rowBytes = std::size_t{Width} * 16u;
    constexpr std::size_t drawnBytes = std::size_t{drawn} * 16u;
    const bool masked = coverage == Coverage::BlueOnly;
    const bool geometry = coverage == Coverage::Geometry;
    const std::string name = recipe ? "cached recipe" : seed ? "seeded scissor" : masked ? "blue-only mask" : geometry ? "partial geometry" : "unseeded scissor";
    if (seed) {
        for (std::uint32_t y = 0; y < Height; ++y) std::memset(block.data + y * rowBytes, 0x5a, drawnBytes);
    }
    std::unique_ptr<Block> recipeVertices;
    auto vertices = geometry ? std::span<const std::array<float, 4>>(LeftHalf) : std::span<const std::array<float, 4>>(Triangle);
    if (recipe) {
        recipeVertices = std::make_unique<Block>(BlockBytes);
        std::memcpy(recipeVertices->data, vertices.data(), vertices.size_bytes());
        Require(AgcDriver::Graphics::HostImportFor(context, recipeVertices->Address(), vertices.size_bytes()) != nullptr,
                "the recipe fixture requires host-imported vertex data");
        vertices = {reinterpret_cast<const std::array<float, 4>*>(recipeVertices->data), vertices.size()};
    }
    Draw(device, color, PixelInputBit(PixelInput::PositionFixedPoint), Program(0u, NoInput, 0xf4u),
         geometry ? Width : drawn, masked ? VK_COLOR_COMPONENT_B_BIT : 0xf, vertices, false, recipe);
    recorder = AgcDriver::Graphics::Recorder::Active();
    Require(!resident || (recorder != nullptr && !recorder->Idle()), name + ": the second draw waited for the GPU instead of being recorded");
    for (std::uint32_t y = 0; y < Height; ++y) {
        auto* row = block.data + y * rowBytes;
        std::memset(row + drawnBytes, 0x42, rowBytes - drawnBytes);
        if (masked) {
            for (std::uint32_t x = 0; x < drawn; ++x) {
                std::memset(row + x * 16u, 0x42, 8u);
                std::memset(row + x * 16u + 12u, 0x42, 4u);
            }
        }
    }
    device.WaitIdle();
    for (std::uint32_t y = 0; y < Height; ++y) {
        const auto* row = block.data + y * rowBytes;
        for (std::uint32_t x = 0; x < drawn; ++x) {
            std::array<float, 4> texel{};
            std::memcpy(texel.data(), row + x * 16u, sizeof(texel));
            constexpr float cpu = std::bit_cast<float>(0x42424242u);
            const std::array<float, 4> expected{masked ? cpu : static_cast<float>(x), masked ? cpu : static_cast<float>(y), 2.0f, masked ? cpu : 1.0f};
            Require(texel == expected, name + ": the second draw or the CPU's masked channels were lost at (" + std::to_string(x) + ", " + std::to_string(y) + ")");
        }
        Require(std::all_of(row + drawnBytes, row + rowBytes, [](std::uint8_t byte) { return byte == 0x42u; }),
                name + ": write-back overwrote the CPU's untouched right half in row " + std::to_string(y));
    }
    return true;
}

bool ExpectSubwordWrites(AgcDriver::VulkanDevice& device, Block& block) {
    using ShaderRecompiler::PixelInput;
    using ShaderRecompiler::PixelInputBit;
    constexpr std::size_t bytes = std::size_t{Width} * Height * 4u;
    constexpr std::uint32_t drawn = Width / 2u;
    std::fill(block.data, block.data + bytes, std::uint8_t{0xff});
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), bytes) == nullptr) return false;
    const auto color = DecodeTarget(block, Rgba8Info);
    Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && color.format == VK_FORMAT_R8G8B8A8_UNORM && color.bytes == bytes,
            "the target is not a linear R8G8B8A8_UNORM target");
    Draw(device, color, PixelInputBit(PixelInput::PositionFixedPoint), Program(0u, NoInput));
    device.WaitIdle();
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto* texel = block.data + (std::size_t{y} * Width + x) * 4u;
            Require(texel[0] == (x == 0 ? 0u : 255u) && texel[1] == (y == 0 ? 0u : 255u) && texel[2] == 0u && texel[3] == 255u,
                    "RGBA8 initial draw did not reach guest memory");
        }
    }
    Draw(device, color, PixelInputBit(PixelInput::PositionFixedPoint), Program(0u, NoInput, 0xf2u), drawn, VK_COLOR_COMPONENT_B_BIT);
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    const bool resident = std::getenv("APS5_NO_RESIDENT_LINEAR") == nullptr;
    Require(!resident || (recorder != nullptr && !recorder->Idle()), "RGBA8 masked draw waited for the GPU instead of being recorded");
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            auto* texel = block.data + (std::size_t{y} * Width + x) * 4u;
            texel[0] = texel[1] = texel[3] = 0x42u;
            if (x >= drawn) texel[2] = 0x42u;
        }
    }
    const auto check = [&](std::uint8_t channels, const std::string& name) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto* texel = block.data + (std::size_t{y} * Width + x) * 4u;
                Require(texel[0] == channels && texel[1] == channels && texel[2] == (x < drawn ? 255u : 0x42u) && texel[3] == channels,
                        name + ": unexpected pixel at (" + std::to_string(x) + ", " + std::to_string(y) + ")");
            }
        }
    };
    device.WaitIdle();
    check(0x42u, "RGBA8 GPU and CPU writes sharing a dword");
    Draw(device, color, PixelInputBit(PixelInput::PositionFixedPoint), Program(0u, NoInput), Width,
         VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT, Triangle, true);
    recorder = AgcDriver::Graphics::Recorder::Active();
    Require(!resident || (recorder != nullptr && !recorder->Idle()), "RGBA8 destination blend waited for the GPU instead of being recorded");
    device.WaitIdle();
    check(0x21u, "RGBA8 destination blend must read the CPU's preserved channels");
    return true;
}

bool ExpectQueuedDraws(AgcDriver::VulkanDevice& device, Block& block) {
    using ShaderRecompiler::PixelInput;
    using ShaderRecompiler::PixelInputBit;
    std::fill(block.data, block.data + TargetBytes, std::uint8_t{0xff});
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), TargetBytes) == nullptr) return false;
    const auto color = DecodeTarget(block, Rgba32FloatInfo);
    Draw(device, color, PixelInputBit(PixelInput::PositionFixedPoint), Program(0u, NoInput));
    device.WaitIdle();
    Check(std::vector<std::uint8_t>(block.data, block.data + TargetBytes), "queued draw baseline");
    const bool resident = std::getenv("APS5_NO_RESIDENT_LINEAR") == nullptr;
    for (const auto [width, blue] : {std::pair{Width / 2u, 0xf4u}, std::pair{Width / 4u, 0xf2u}}) {
        Draw(device, color, PixelInputBit(PixelInput::PositionFixedPoint), Program(0u, NoInput, blue), width, VK_COLOR_COMPONENT_B_BIT);
        const auto* recorder = AgcDriver::Graphics::Recorder::Active();
        Require(!resident || (recorder != nullptr && !recorder->Idle()), "an overlapping linear draw waited for the GPU instead of being recorded");
    }
    constexpr float cpu = 64.0f;
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            auto* texel = block.data + (std::size_t{y} * Width + x) * 16u;
            for (const auto channel : {0u, 1u, 3u}) std::memcpy(texel + channel * sizeof(float), &cpu, sizeof(cpu));
            if (x >= Width / 2u) std::memcpy(texel + 2u * sizeof(float), &cpu, sizeof(cpu));
        }
    }
    const auto check = [&](float channels, const std::string& name) {
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                std::array<float, 4> texel{};
                std::memcpy(texel.data(), block.data + (std::size_t{y} * Width + x) * 16u, sizeof(texel));
                const std::array<float, 4> expected{channels, channels, x < Width / 4u ? 1.0f : x < Width / 2u ? 2.0f : cpu, channels};
                Require(texel == expected, name + ": unexpected pixel at (" + std::to_string(x) + ", " + std::to_string(y) + ")");
            }
        }
    };
    device.WaitIdle();
    check(cpu, "queued overlapping draws and untouched CPU bytes");
    Draw(device, color, PixelInputBit(PixelInput::PositionFixedPoint), Program(0u, NoInput), Width,
         VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT, Triangle, true);
    device.WaitIdle();
    check(cpu / 2.0f, "queued draw refresh must upload preserved CPU channels");
    return true;
}

void ExpectCpuSnapshot(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    context.hostImportAlignment = 0;
    TextureDetiler detiler(context);
    context.detiler = &detiler;
    Block block(2 * BlockBytes);
    std::memset(block.data, 0x11, block.bytes);
    GuestTextureResource resource{};
    resource.baseAddress = block.Address();
    resource.width = 63;
    resource.height = 256;
    resource.depthOrLastArray = 1;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2DArray;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    const auto& mip = geometry.mips.front();
    Require(geometry.imageLayers == 2 && geometry.layerBytes == BlockBytes && geometry.guestBytes == block.bytes && mip.pitchBytes == 256,
            "CPU snapshot fixture does not contain two padded 64 KiB layers");
    Require(HostImportFor(context, block.Address(), block.bytes) == nullptr, "CPU snapshot fixture unexpectedly imported guest memory");
    auto image = StorageTexture::Create(context, detiler, resource, 0);
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "CPU snapshot fixture has no recorder");
    constexpr std::uint32_t drawn = 31;
    auto upload = std::make_shared<Buffer>(context, drawn * resource.height * 4u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    std::fill(upload->Bytes().begin(), upload->Bytes().end(), std::byte{0x22});
    image->PrepareLinearDraw();
    recorder->Keep(image);
    recorder->Keep(upload);
    const auto commands = recorder->Commands();
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy write{};
    write.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    write.imageExtent = {drawn, resource.height, 1};
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, upload->Handle(), image->Image(), VK_IMAGE_LAYOUT_GENERAL, 1, &write);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    for (std::uint32_t row = 0; row < resource.height; ++row) {
        auto* pixels = block.data + row * mip.pitchBytes;
        std::memset(pixels + drawn * 4u, 0x42, (resource.width - drawn) * 4u);
        std::memset(pixels + resource.width * 4u, 0x55, mip.pitchBytes - resource.width * 4u);
    }
    std::memset(block.data + geometry.layerBytes, 0x66, static_cast<std::size_t>(geometry.layerBytes));
    std::vector<std::uint8_t> expected(block.data, block.data + block.bytes);
    for (std::uint32_t row = 0; row < resource.height; ++row) std::memset(expected.data() + row * mip.pitchBytes, 0x22, drawn * 4u);
    image->MarkSliceDirty(0);
    image->FinishLinearDraw();
    recorder->Sync();
    Require(std::equal(expected.begin(), expected.end(), block.data), "CPU snapshot write-back lost GPU bytes, CPU bytes, padding, or the untouched layer");
    image->Refresh();
    const auto rowBytes = resource.width * 4u;
    const auto layerBytes = rowBytes * resource.height;
    Buffer readback(context, static_cast<std::size_t>(layerBytes) * 2u, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    CommandBatch batch(context);
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy read{};
    read.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 2};
    read.imageExtent = {resource.width, resource.height, 1};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(batch.Handle(), image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &read);
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    batch.SubmitAndWait();
    for (std::uint32_t layer = 0; layer < 2; ++layer) {
        for (std::uint32_t row = 0; row < resource.height; ++row) {
            Require(std::memcmp(readback.Bytes().data() + layer * layerBytes + row * rowBytes,
                                expected.data() + geometry.GuestLayerOffset(layer) + row * mip.pitchBytes, rowBytes) == 0,
                    "CPU snapshot refresh did not upload preserved CPU texels");
        }
    }
    std::puts("linear target CPU snapshot, padded array layers, and refresh: passed");
}

void ExpectChangedRetile(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    TextureDetiler detiler(context);
    for (const auto elementBytes : {1u, 2u, 4u, 8u, 16u}) {
        for (std::uint32_t shift = 0; shift < 4; ++shift) {
            TileMipLayout layout{};
            layout.width = 19;
            layout.height = 3;
            layout.pitchBytes = layout.width * elementBytes + 7u;
            layout.linearSize = layout.tiledSize = layout.pitchBytes * layout.height;
            const std::uint32_t currentOffset = 19u + shift, originalOffset = 2046u + shift, destinationOffset = 39u + shift;
            Buffer source(context, 4096, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            Buffer destination(context, 2048, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            auto sourceBytes = source.Bytes();
            std::fill(sourceBytes.begin(), sourceBytes.end(), std::byte{0x11});
            std::fill_n(sourceBytes.begin() + currentOffset, static_cast<std::size_t>(layout.linearSize), std::byte{0x22});
            std::array<std::byte, 2048> expected{};
            expected.fill(std::byte{0x42});
            std::copy(expected.begin(), expected.end(), destination.Bytes().begin());
            const bool windowed = shift % 2u != 0;
            DetileWindow window{};
            if (windowed) {
                window.rangeBegin = layout.pitchBytes + elementBytes;
                window.rangeEnd = layout.pitchBytes + 18u * elementBytes;
                window.columnBegin = 1; window.columnEnd = 18;
                window.rowBegin = 1; window.rowEnd = 2;
            }
            for (std::uint32_t y = 0; y < layout.height; ++y) {
                for (std::uint32_t x = 0; x < layout.width * elementBytes; ++x) {
                    const auto offset = y * layout.pitchBytes + x;
                    const auto before = static_cast<std::byte>((offset * 29u + 11u) & 0xffu);
                    sourceBytes[originalOffset + offset] = sourceBytes[currentOffset + offset] = before;
                    if ((x + y) % 3u != 0) continue;
                    const auto changed = before ^ std::byte{0x80};
                    sourceBytes[currentOffset + offset] = changed;
                    if (!windowed || (y == 1 && x >= elementBytes && x < 18u * elementBytes)) expected[destinationOffset + offset] = changed;
                }
            }
            detiler.BeginBatch();
            CommandBatch batch(context);
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            detiler.Dispatch(batch.Handle(), TextureTileMode::kLinear, elementBytes, source.Handle(), currentOffset, destination.Handle(), destinationOffset, layout, true, 0, false, window, originalOffset);
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            batch.SubmitAndWait();
            destination.Invalidate();
            Require(std::equal(expected.begin(), expected.end(), destination.Bytes().begin()),
                    "changed linear retile corrupted GPU bytes, CPU bytes, or padding for element size " + std::to_string(elementBytes) + " and offset shift " + std::to_string(shift));
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::printf("linear target resident device: %s\n", device->DeviceName().c_str());
        if (argc == 2 && std::string(argv[1]) == "--recipe") {
            Block recipe(BlockBytes);
            Require(Expect(*device, recipe, Coverage::Scissor, false, true), "the recipe fixture requires a host-imported linear target");
            std::puts("linear target cached recipe tests passed");
            return 0;
        }
        ExpectChangedRetile(*device);
        ExpectCpuSnapshot(*device);
        Block unseeded(BlockBytes);
        Block seeded(BlockBytes);
        Block masked(BlockBytes);
        Block geometry(BlockBytes);
        Block subword(BlockBytes);
        Block queued(BlockBytes);
        if (!Expect(*device, unseeded, Coverage::Scissor) || !Expect(*device, seeded, Coverage::Scissor, true) ||
            !Expect(*device, masked, Coverage::BlueOnly) || !Expect(*device, geometry, Coverage::Geometry) ||
            !ExpectSubwordWrites(*device, subword) || !ExpectQueuedDraws(*device, queued)) {
            std::puts("linear target resident test skipped: no host import on this device");
            return VulkanTestSkipped;
        }
        std::puts("linear target resident tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
