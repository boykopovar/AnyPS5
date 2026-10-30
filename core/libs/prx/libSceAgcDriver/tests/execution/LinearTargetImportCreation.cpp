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
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 16;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t SettledBytes = 16 * BlockBytes;
constexpr std::size_t ResidentBytes = 32 * BlockBytes;
constexpr std::size_t TargetBytes = std::size_t{Width} * Height * 16u;
constexpr std::size_t RowBytes = std::size_t{Width} * 16u;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;
constexpr VkColorComponentFlags RedGreen = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::uint32_t, 16> PixelCode{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t stride, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), count, 0x01016facu};
}

std::span<const std::uint32_t> Program() {
    std::size_t at = 0;
    PixelCode[at++] = 0x36000000u | (20u << 17u) | 0xffu;
    PixelCode[at++] = 0x0000ffffu;
    PixelCode[at++] = 0x2c000000u | (21u << 17u) | 0x90u;
    PixelCode[at++] = 0x7e000c00u | (20u << 17u) | (256u + 20u);
    PixelCode[at++] = 0x7e000c00u | (21u << 17u) | (256u + 21u);
    PixelCode[at++] = 0x7e000280u | (22u << 17u);
    PixelCode[at++] = 0x7e0002f2u | (23u << 17u);
    PixelCode[at++] = 0xf800180fu;
    PixelCode[at++] = 0x17161514u;
    PixelCode[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(PixelCode.data(), at);
}

struct Block {
    Block(std::size_t bytes, std::uintptr_t at, bool direct = false, std::size_t granule = 0, bool writable = true) : bytes(bytes), arena(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (arena && direct) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 0x200000));
            Require(data != nullptr, "cannot allocate the direct memory of the color target");
            section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_RESERVE, 0, static_cast<DWORD>(bytes), nullptr);
            Require(section != nullptr, "cannot reserve the backing of the color target's direct memory");
            GuestArena::GuestArenaMapDirect_nid_postfix(data, bytes, section, 0, PAGE_READWRITE);
        } else if (arena) {
            data = static_cast<std::uint8_t*>(at != 0 ? GuestArena::GuestArenaAllocateAtOrAbove_nid_postfix(at, bytes, BlockBytes) : GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, granule != 0 ? granule : bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        static_cast<void>(direct);
        static_cast<void>(granule);
        if (arena && at != 0) {
            void* raw = mmap(reinterpret_cast<void*>(at), bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map the color target");
            data = static_cast<std::uint8_t*>(raw);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else if (arena) {
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
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, writable, true);
    }
    ~Block() {
        StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
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
        if (section != nullptr) {
            GuestArena::GuestArenaForgetSection_nid_postfix(section);
            CloseHandle(section);
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
    bool Watched() const { return arena && AgcDriver::GuestMemory::Watched(Address(), bytes); }
    std::size_t bytes;
    bool arena;
    std::uint8_t* data = nullptr;
#ifdef _WIN32
    HANDLE section = nullptr;
#endif
};

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Rgba32FloatInfo;
    cx[0x31d] = 0;
    cx[0x323] = 0;
    cx[0x324] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    color.exportIndex = 0;
    Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && color.bytes == TargetBytes, "the target is not a linear R32G32B32A32 target");
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const Block& block) {
    const auto color = DecodeTarget(block);
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

    const auto pixelCode = Program();
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
    blend.colorWriteMask = RedGreen;
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, waveSize, waveSize, std::nullopt, std::nullopt};
    state.colors = {color};
    state.color = color;
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

void Seed(Block& block, const std::array<float, 4>& texel) {
    for (std::size_t offset = 0; offset < TargetBytes; offset += sizeof(texel)) std::memcpy(block.data + offset, texel.data(), sizeof(texel));
}

void Check(const Block& block, const std::array<float, 4>& seed, const std::string& what) {
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            std::array<float, 4> texel{};
            std::memcpy(texel.data(), block.data + static_cast<std::size_t>(y) * RowBytes + static_cast<std::size_t>(x) * 16u, sizeof(texel));
            const std::array<float, 4> expected{static_cast<float>(x), static_cast<float>(y), seed[2], seed[3]};
            Require(texel == expected, what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") holds (" + std::to_string(texel[0]) + ", " + std::to_string(texel[1]) + ", " + std::to_string(texel[2]) + ", " + std::to_string(texel[3]) + ")");
        }
    }
}

void ExpectRecorded(const AgcDriver::Graphics::Context& context, const Block& block, const std::string& what) {
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr && !recorder->Idle(), what + ": the draw into a linear target in registered, write-watched memory without a host import waited for the GPU instead of being recorded");
    Require(StorageTexture::FindPending(block.Address(), TargetBytes) != nullptr, what + ": the drawn linear target has no pending resident image");
    Require(AgcDriver::Graphics::ExistingHostImportFor(context, block.Address(), TargetBytes) != nullptr, what + ": the draw made no host import of the linear target");
    Require(block.Watched(), what + ": the linear target's host import took its memory out of the write watch");
}

void Created(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr std::array<float, 4> seed{100.0f, 200.0f, 300.0f, 400.0f};
    Block block(SettledBytes, 0);
    Seed(block, seed);
    Require(block.Watched(), "import created: the color target is not write-watched");
    Require(AgcDriver::Graphics::ExistingHostImportFor(context, block.Address(), TargetBytes) == nullptr, "import created: the color target has a host import before the draw");
    Draw(device, block);
    ExpectRecorded(context, block, "import created");
    device.WaitIdle();
    Require(block.Watched(), "import created: the color target left the write watch after its store");
    Check(block, seed, "import created, after the device wait");
}

bool Reused(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr std::array<float, 4> first{100.0f, 200.0f, 300.0f, 400.0f};
    constexpr std::array<float, 4> second{500.0f, 600.0f, 700.0f, 800.0f};
    std::uint64_t address = 0;
    {
        Block block(SettledBytes, 0);
        address = block.Address();
        Seed(block, first);
        Draw(device, block);
        ExpectRecorded(context, block, "reuse, first block");
        device.WaitIdle();
        Check(block, first, "reuse, first block");
    }
    Block block(SettledBytes, static_cast<std::uintptr_t>(address));
    if (block.Address() != address) {
        std::puts("linear target import reuse skipped: the second block was not mapped at the first one's address");
        return false;
    }
    Require(block.Watched(), "reuse: the block allocated at the freed block's address is not write-watched");
    Seed(block, second);
    Require(block.Watched(), "reuse: the CPU's store took the second block out of the write watch");
    Draw(device, block);
    ExpectRecorded(context, block, "reuse, second block");
    device.WaitIdle();
    Check(block, second, "reuse, the draw into the second block did not start from the CPU's bytes");
    return true;
}

#ifdef _WIN32
AgcDriver::Graphics::HostImportRequesterCounts ColorTargetCounts() {
    return AgcDriver::Graphics::HostImportCounters().requesters[static_cast<std::size_t>(AgcDriver::Graphics::ImportRequester::ColorTarget)];
}

void Direct(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr std::array<float, 4> seed{1300.0f, 1400.0f, 1500.0f, 1600.0f};
    Block block(ResidentBytes, 0, true);
    Seed(block, seed);
    Require(block.Watched(), "direct memory: the color target is not write-watched");
    Require(AgcDriver::Graphics::ExistingHostImportFor(context, block.Address(), TargetBytes) == nullptr, "direct memory: the color target has a host import before the draw");
    Draw(device, block);
    ExpectRecorded(context, block, "direct memory");
    device.WaitIdle();
    Require(block.Watched(), "direct memory: the color target left the write watch after its store");
    Check(block, seed, "direct memory, after the device wait");
}

void PrivateGate(AgcDriver::VulkanDevice& device) {
    using AgcDriver::Graphics::ImportRequester;
    using AgcDriver::Graphics::ImportWatch;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    Block direct(ResidentBytes / 4, 0, true);
    Block flexible(2 * BlockBytes, 0);
    GuestArena::GuestArenaCommit_nid_postfix(flexible.data + BlockBytes, BlockBytes, PAGE_READONLY, BlockBytes);
    const auto decided = AgcDriver::Graphics::PrepareImportWatch(context);
    const auto before = ColorTargetCounts();
    AgcDriver::Graphics::SetImportWatch(context, ImportWatch::Unwatch);
    const auto* compared = AgcDriver::Graphics::HostImportFor(context, direct.Address(), TargetBytes, ImportRequester::ColorTarget);
    AgcDriver::Graphics::SetImportWatch(context, ImportWatch::Watch);
    const auto* readOnly = AgcDriver::Graphics::HostImportFor(context, flexible.Address(), TargetBytes, ImportRequester::ColorTarget);
    const auto refused = ColorTargetCounts();
    const auto* watched = AgcDriver::Graphics::HostImportFor(context, direct.Address(), TargetBytes, ImportRequester::ColorTarget);
    const auto made = ColorTargetCounts();
    AgcDriver::Graphics::SetImportWatch(context, decided);
    Require(compared == nullptr, "private gate: a color target's import of direct memory under the settle floor was made where it would be compared");
    Require(readOnly == nullptr && refused.made == before.made && refused.refused == before.refused && refused.failed == before.failed, "private gate: a color target's import was made of private memory committed read-only while registered writable, or a refused color target import was counted");
    Require(watched != nullptr && made.made == before.made + 1 && made.failed == before.failed, "private gate: a color target's import of direct memory registered writable was not made where imports stay watched");
}
#endif

void OutsideWatch(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr std::array<float, 4> seed{1700.0f, 1800.0f, 1900.0f, 2000.0f};
    Block block(SettledBytes, 0);
    Seed(block, seed);
    AgcDriver::GuestMemory::Unwatch(block.Address() + SettledBytes - BlockBytes, BlockBytes);
    Require(AgcDriver::GuestMemory::Watched(block.Address(), TargetBytes) && !block.Watched(), "run outside the write watch: the target is not watched, or the rest of its block still is");
    Draw(device, block);
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder == nullptr || recorder->Idle(), "run outside the write watch: a linear target whose import run has a block outside the write watch was drawn into a resident image");
    Require(AgcDriver::Graphics::ExistingHostImportFor(context, block.Address(), TargetBytes) == nullptr, "run outside the write watch: the draw made a host import over a block outside the write watch");
    Require(AgcDriver::GuestMemory::Watched(block.Address(), TargetBytes), "run outside the write watch: the draw took the target out of the write watch");
    device.WaitIdle();
    Check(block, seed, "run outside the write watch, after the device wait");
}

void ReadOnlyRegistry(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr std::array<float, 4> seed{2500.0f, 2600.0f, 2700.0f, 2800.0f};
    Block block(SettledBytes, 0, false, 0, false);
    Seed(block, seed);
    Require(block.Watched(), "range registered read-only: the color target is not write-watched");
    Draw(device, block);
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder == nullptr || recorder->Idle(), "range registered read-only: a linear target in memory the guest registered read-only was drawn into a resident image");
    Require(AgcDriver::Graphics::ExistingHostImportFor(context, block.Address(), TargetBytes) == nullptr, "range registered read-only: the draw made a host import of memory the guest registered read-only");
    device.WaitIdle();
    Check(block, seed, "range registered read-only, after the device wait");
}

#ifdef _WIN32
void Rewatched(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr std::array<float, 4> seed{2100.0f, 2200.0f, 2300.0f, 2400.0f};
    Block block(SettledBytes, 0, false, BlockBytes);
    Seed(block, seed);
    auto* last = block.data + SettledBytes - BlockBytes;
    AgcDriver::GuestMemory::Unwatch(block.Address() + SettledBytes - BlockBytes, BlockBytes);
    Draw(device, block);
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder == nullptr || recorder->Idle(), "run back in the write watch: the draw before the block came back was drawn into a resident image");
    device.WaitIdle();
    GuestArena::GuestArenaReset_nid_postfix(last, BlockBytes);
    GuestArena::GuestArenaCommit_nid_postfix(last, BlockBytes, PAGE_READWRITE, BlockBytes);
    Require(block.Watched(), "run back in the write watch: committing the block again did not bring it back into the write watch");
    Draw(device, block);
    ExpectRecorded(context, block, "run back in the write watch, a refused run whose block came back");
    device.WaitIdle();
    Check(block, seed, "run back in the write watch, after the device wait");
}
#endif

void Compared(AgcDriver::VulkanDevice& device) {
    using AgcDriver::Graphics::ImportWatch;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    constexpr std::array<float, 4> seed{900.0f, 1000.0f, 1100.0f, 1200.0f};
    const auto decided = AgcDriver::Graphics::PrepareImportWatch(context);
    AgcDriver::Graphics::SetImportWatch(context, ImportWatch::Unwatch);
    Block block(BlockBytes, 0);
    Seed(block, seed);
    Require(block.Watched(), "compared import: the color target is not write-watched");
    Draw(device, block);
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder == nullptr || recorder->Idle(), "compared import: a linear target whose host import would be compared was drawn into a resident image");
    Require(AgcDriver::Graphics::ExistingHostImportFor(context, block.Address(), TargetBytes) == nullptr, "compared import: the draw made a host import that leaves the target's memory compared");
    Require(StorageTexture::FindPending(block.Address(), TargetBytes) == nullptr, "compared import: the drawn linear target has a pending resident image");
    Require(block.Watched(), "compared import: the draw took the color target out of the write watch");
    device.WaitIdle();
    Check(block, seed, "compared import, after the device wait");
    AgcDriver::Graphics::SetImportWatch(context, decided);
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!AgcDriver::GuestMemory::WriteWatched()) {
            std::puts("skipped, guest memory is not write-watched");
            return VulkanTestSkipped;
        }
        const std::string_view only = argc > 1 ? argv[1] : "";
#ifndef _WIN32
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const auto decided = AgcDriver::Graphics::PrepareImportWatch(context);
        AgcDriver::Graphics::SetImportWatch(context, AgcDriver::Graphics::ImportWatch::Watch);
        struct Restore {
            const AgcDriver::Graphics::Context& context;
            AgcDriver::Graphics::ImportWatch decided;
            ~Restore() { AgcDriver::Graphics::SetImportWatch(context, decided); }
        } restore{context, decided};
#endif
        Block probe(SettledBytes, 0);
        if (AgcDriver::Graphics::HostImportFor(AgcDriver::VulkanDeviceTestAccess::ContextOf(*device), probe.Address(), probe.bytes) == nullptr) {
            std::puts("skipped, host imports are unavailable");
            return VulkanTestSkipped;
        }
        const bool watched = probe.Watched();
#ifndef _WIN32
        Require(watched, "a host import of write-watched memory left the write watch although imports are set to stay watched");
#endif
        if (!watched) std::puts("linear target import creation: a host import of write-watched memory does not stay watched on this device; only the compared cases run");
        if (watched && (only.empty() || only == "--created")) Created(*device);
        if (watched && (only.empty() || only == "--reused") && Reused(*device)) std::puts("linear target import reuse passed");
#ifdef _WIN32
        if (watched && (only.empty() || only == "--direct")) Direct(*device);
        if (only.empty() || only == "--private-gate") PrivateGate(*device);
#endif
        if (watched && (only.empty() || only == "--outside-watch")) OutsideWatch(*device);
        if (watched && (only.empty() || only == "--read-only-registry")) ReadOnlyRegistry(*device);
#ifdef _WIN32
        if (watched && (only.empty() || only == "--rewatched")) Rewatched(*device);
#endif
        if (only.empty() || only == "--compared") Compared(*device);
        std::puts("linear target import creation tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
