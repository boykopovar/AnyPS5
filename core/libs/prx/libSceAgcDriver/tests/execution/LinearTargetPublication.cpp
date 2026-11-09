#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <chrono>
#include <thread>
#include <limits>
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
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

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 1088;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;
constexpr std::uint32_t NoInput = 0xffffffffu;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 16>, 8> Programs{};
std::size_t programCount = 0;

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::span<const std::uint32_t> Program(std::uint32_t fixedVgpr, std::uint32_t stippleVgpr, bool blue = false) {
    Require(programCount < Programs.size(), "too many test programs");
    auto& code = Programs[programCount++];
    std::size_t at = 0;
    code[at++] = 0x36000000u | (20u << 17u) | (fixedVgpr << 9u) | 0xffu;
    code[at++] = 0x0000ffffu;
    code[at++] = 0x2c000000u | (21u << 17u) | (fixedVgpr << 9u) | 0x90u;
    code[at++] = 0x7e000c00u | (20u << 17u) | (256u + 20u);
    code[at++] = 0x7e000c00u | (21u << 17u) | (256u + 21u);
    code[at++] = stippleVgpr == NoInput ? (blue ? 0x7e0002f2u : 0x7e000280u) | (22u << 17u) : 0x7e000c00u | (22u << 17u) | (256u + stippleVgpr);
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

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t inputAddr, std::span<const std::uint32_t> pixelCode, std::uint32_t scissorWidth = Width, std::uint32_t scissorX = 0, std::uint32_t writeMask = 0xf, bool sparse = false) {
    const auto target = device.Target();
    constexpr std::uint32_t waveSize = 64;
    std::vector<std::uint32_t> vertexUserData(4, 0u);
    const std::array<std::array<float, 4>, 3> small{{{-1.0f, -1.0f, 0.5f, 1.0f}, {-0.5f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};
    const auto vertexBuffer = BufferDescriptor(sparse ? small.data() : Triangle.data(), 16u, static_cast<std::uint32_t>(Triangle.size()));
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
    blend.blendEnable = VK_FALSE;
    blend.colorWriteMask = writeMask;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{static_cast<std::int32_t>(scissorX), 0}, {scissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
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

bool Expect(AgcDriver::VulkanDevice& device, const std::string& mode) {
    using namespace AgcDriver::Graphics;
    constexpr std::size_t bytes = std::size_t{Width} * Height * 16u;
    Block block(bytes);
    std::memset(block.data, 0xff, bytes);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (HostImportFor(context, block.Address(), bytes) == nullptr) return false;
    const auto color = DecodeTarget(block, Rgba32FloatInfo);
    const auto input = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionFixedPoint);
    Draw(device, color, input, Program(0u, NoInput));
    device.WaitIdle();
    Check(std::vector<std::uint8_t>(block.data, block.data + bytes), "initial publication");
    if (mode == "same") {
        auto original = std::make_shared<Buffer>(context, Width / 2u * Height * 16u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        for (std::uint32_t y = 0; y < Height; ++y) {
            std::memcpy(original->Bytes().data() + std::size_t{y} * Width / 2u * 16u,
                        block.data + std::size_t{y} * Width * 16u, Width / 2u * 16u);
        }
        std::shared_ptr<StorageTexture> live;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            live = StorageTexture::FindLive(block.Address(), bytes);
            Require(live != nullptr, "the same-value target has no resident image");
            live->Refresh();
            std::memset(block.data, 0x42, bytes);
            live->PrepareLinearDraw();
            auto* recorder = Recorder::Active();
            Require(recorder != nullptr, "the same-value draw has no recorder");
            recorder->Keep(original);
            recorder->Keep(live);
            const auto commands = recorder->Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {Width / 2u, Height, 1};
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, original->Handle(), live->Image(), VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            live->MarkDirty();
            live->FinishLinearDraw();
        }
        device.WaitIdle();
        for (std::uint32_t y = 0; y < Height; ++y) {
            const auto* row = block.data + std::size_t{y} * Width * 16u;
            Require(std::memcmp(row, original->Bytes().data() + std::size_t{y} * Width / 2u * 16u, Width / 2u * 16u) == 0,
                    "a GPU write equal to stale image contents did not replace the newer guest bytes");
            Require(std::all_of(row + Width / 2u * 16u, row + Width * 16u, [](auto byte) { return byte == 0x42u; }),
                    "a same-value GPU write overwrote CPU bytes outside its region");
        }
        std::puts("linear publication same-value write after refresh: passed");
        return true;
    }
    const bool queued = mode == "queued";
    const bool masked = mode == "mask";
    const bool geometry = mode == "geometry";
    const bool reader = mode == "reader";
    const bool eviction = mode == "eviction";
    const bool alias = mode == "alias";
    Require(queued || masked || geometry || reader || eviction || alias, "unknown linear publication case");
    if (alias) {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const auto prior = StorageTexture::FindLive(block.Address(), bytes);
        Require(prior != nullptr, "the initial target has no resident image");
        auto descriptor = prior->Descriptor();
        descriptor.width /= 2u;
        descriptor.height *= 2u;
        auto overlapping = CachedStorageSurface(context, descriptor);
        Require(overlapping != prior && overlapping->GuestBytes() == bytes, "the reshaped storage alias was not created");
        const std::array<std::uint32_t, 4> zero{0u, 0u, 0u, 0x3f800000u};
        const char* refusal = nullptr;
        Require(overlapping->FillClear(zero, 0u, refusal), std::string("the alias clear was refused: ") + (refusal != nullptr ? refusal : "unknown"));
        Require(StorageTexture::AnyPending(), "the alias clear has no pending result");
    }
    Draw(device, color, input, Program(0u, NoInput, true), queued ? Width / 4u : (masked || geometry ? Width : Width / 2u), 0u, masked ? 4u : 0xfu, geometry);
    if (queued) Draw(device, color, input, Program(0u, NoInput), Width / 4u, Width / 2u);
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr && !recorder->Idle(), "the tested linear draw did not remain recorded");
    if (eviction) {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const auto live = StorageTexture::FindLive(block.Address(), bytes);
        Require(live != nullptr && live->Cached(), "the target was not cached before eviction");
        live->Flush();
        ClearCachedTextures(device.Device());
        Require(!live->Cached(), "the target survived the cache clear");
        Require(!recorder->Idle(), "cache eviction unexpectedly completed the pending draw");
    }
    const auto inside = [&](std::uint32_t x, std::uint32_t y) {
        if (geometry) return static_cast<double>(x) + 0.5 < 8.0 + 8.0 * (static_cast<double>(y) + 0.5) / Height;
        if (queued) return x < Width / 4u || (x >= Width / 2u && x < 3u * Width / 4u);
        return x < Width / 2u;
    };
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            auto* pixel = block.data + (std::size_t{y} * Width + x) * 16u;
            if (masked) {
                std::memset(pixel, 0x42, 8u);
                std::memset(pixel + 12u, 0x42, 4u);
            } else if (!inside(x, y)) std::memset(pixel, 0x42, 16u);
        }
    }
    const auto check = [&](const std::uint8_t* data) {
        std::size_t wrongGpu = 0, wrongCpu = 0;
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto* pixel = data + (std::size_t{y} * Width + x) * 16u;
                if (masked) {
                    float blue = 0;
                    std::memcpy(&blue, pixel + 8u, 4u);
                    wrongGpu += blue != 1.0f;
                    for (std::size_t byte = 0; byte < 16u; ++byte) if (byte < 8u || byte >= 12u) wrongCpu += pixel[byte] != 0x42u;
                } else if (inside(x, y)) {
                    std::array<float, 4> texel{};
                    std::memcpy(texel.data(), pixel, sizeof(texel));
                    wrongGpu += texel != std::array<float, 4>{static_cast<float>(x), static_cast<float>(y), queued && x >= Width / 2u ? 0.0f : 1.0f, 1.0f};
                } else {
                    for (std::size_t byte = 0; byte < 16u; ++byte) wrongCpu += pixel[byte] != 0x42u;
                }
            }
        }
        std::printf("linear publication %s: bytes=%zu, wrong GPU pixels=%zu, wrong CPU bytes=%zu\n", mode.c_str(), bytes, wrongGpu, wrongCpu);
        Require(wrongGpu == 0 && wrongCpu == 0, "linear publication lost GPU results or CPU bytes");
    };
    if (reader) {
        Block destination(bytes);
        Block label(BlockBytes);
        std::memset(destination.data, 0xa5, bytes);
        std::memset(label.data, 0, BlockBytes);
        constexpr std::uint32_t ready = 0x8152u;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto copied = device.CopyBuffer(destination.Address(), block.Address(), bytes, 0, std::numeric_limits<std::size_t>::max(), 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
            Require(copied.path == 1 || copied.path == 3, "the dependent copy did not use a GPU path");
            Require(device.WriteLabelOnGpu(label.Address(), std::as_bytes(std::span(&ready, 1)), 1, 0, false) == 0, "the completion label was not GPU-visible");
            std::printf("dependent GPU copy path=%d\n", copied.path);
            device.SubmitRecorded(false);
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        const auto* observed = reinterpret_cast<volatile const std::uint32_t*>(label.data);
        while (*observed != ready) {
            Require(std::chrono::steady_clock::now() < deadline, "the GPU completion label did not land");
            std::this_thread::yield();
        }
        check(destination.data);
        check(block.data);
        device.WaitIdle();
    } else {
        device.WaitIdle();
        check(block.data);
    }
    return true;
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!Expect(*device, argc > 1 ? argv[1] : "queued")) return VulkanTestSkipped;
        std::puts("linear target publication tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
