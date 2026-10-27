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
#include <atomic>
#include <chrono>
#include <thread>
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
constexpr std::uint32_t Height = 4096;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Rgba32FloatInfo = 0x8738u;
constexpr std::uint32_t Rgba8UnormInfo = 0x8028u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32AbgrExport = 9;
constexpr std::uint32_t NoInput = 0xffffffffu;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::array<std::uint32_t, 16>, 4> Programs{};
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

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::uint32_t inputAddr, std::span<const std::uint32_t> pixelCode, std::uint32_t scissorWidth = Width, std::uint32_t writeMask = 0xfu) {
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
    state.scissor = {{0, 0}, {scissorWidth, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}


struct Hammer {
    std::uint8_t* data;
    std::size_t pixels;
    std::atomic<bool> stop{false};
    std::atomic<std::uint32_t> passes{0};
    std::atomic<std::size_t> regressions{0};
    std::atomic<std::size_t> alphaCorruptions{0};
    std::atomic<std::size_t> lostUpdates{0};
    std::atomic<std::size_t> staleValues{0};
    std::atomic<std::uint32_t> passLimit{60000u};
    std::array<std::uint64_t, 8> firstRegressions{};
    std::uint32_t pass = 0;
    void Run() {
        while (!stop.load(std::memory_order_relaxed) && pass < passLimit.load(std::memory_order_relaxed)) {
            ++pass;
            const auto expected = static_cast<std::uint16_t>(pass - 1u);
            for (std::size_t index = 0; index < pixels; ++index) {
                auto* pixel = data + index * 4u;
                std::uint16_t seen = 0;
                std::memcpy(&seen, pixel, 2u);
                if (seen != expected) {
                    if (static_cast<std::uint16_t>(expected - seen) == 1u) lostUpdates.fetch_add(1, std::memory_order_relaxed);
                    else staleValues.fetch_add(1, std::memory_order_relaxed);
                    const auto slot = regressions.fetch_add(1, std::memory_order_relaxed);
                    if (slot < firstRegressions.size()) firstRegressions[slot] = (static_cast<std::uint64_t>(pass) << 48u) | (static_cast<std::uint64_t>(seen) << 32u) | static_cast<std::uint64_t>(index);
                }
                if (pixel[3] != 0x77u) alphaCorruptions.fetch_add(1, std::memory_order_relaxed);
                const auto value = static_cast<std::uint16_t>(pass);
                pixel[0] = static_cast<std::uint8_t>(value & 0xffu);
                pixel[1] = static_cast<std::uint8_t>(value >> 8u);
                pixel[3] = 0x77u;
            }
            passes.store(pass, std::memory_order_release);
        }
    }
};

bool Expect(AgcDriver::VulkanDevice& device) {
    using ShaderRecompiler::PixelInput;
    using ShaderRecompiler::PixelInputBit;
    constexpr std::size_t pixels = std::size_t{Width} * Height;
    constexpr std::size_t bytes = pixels * 4u;
    Block block(bytes);
    for (std::size_t index = 0; index < pixels; ++index) {
        auto* pixel = block.data + index * 4u;
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0x11u;
        pixel[3] = 0x77u;
    }
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if (AgcDriver::Graphics::HostImportFor(context, block.Address(), bytes) == nullptr) return false;
    const auto color = DecodeTarget(block, Rgba8UnormInfo);
    Require(color.tileMode == AgcDriver::Graphics::ColorTileMode::Linear && color.bytes == bytes && color.format == VK_FORMAT_R8G8B8A8_UNORM, "the target is not a linear R8G8B8A8 target");
    const auto input = PixelInputBit(PixelInput::PositionFixedPoint);
    const bool synchronousControl = std::getenv("APS5_HAMMER_SYNC") != nullptr;
    const bool sequential = std::getenv("APS5_HAMMER_SEQUENTIAL") != nullptr;
    Hammer hammer{block.data, pixels};
    if (sequential) hammer.passLimit.store(3u);
    std::thread worker([&] { hammer.Run(); });
    while (hammer.passes.load(std::memory_order_acquire) < 2u) std::this_thread::yield();
    if (sequential) worker.join();
    const auto before = std::chrono::steady_clock::now();
    Draw(device, color, input, Program(0u, NoInput, true), Width, 4u);
    const auto* recorder = AgcDriver::Graphics::Recorder::Active();
    const bool wasRecorded = recorder != nullptr && !recorder->Idle();
    if (!synchronousControl) Require(wasRecorded, "the blue-only draw into the imported linear target was not recorded");
    const auto recorded = std::chrono::steady_clock::now();
    if (sequential) {
        hammer.passLimit.store(5u);
        hammer.Run();
    }
    device.WaitIdle();
    const auto done = std::chrono::steady_clock::now();
    const auto passesAtDone = hammer.passes.load(std::memory_order_acquire);
    if (!sequential) {
        while (hammer.passes.load(std::memory_order_acquire) < passesAtDone + 2u) std::this_thread::yield();
        hammer.stop.store(true, std::memory_order_relaxed);
        worker.join();
    }
    const auto lastPass = static_cast<std::uint16_t>(hammer.passes.load());
    std::size_t wrongBlue = 0, wrongCounter = 0, wrongAlpha = 0;
    for (std::size_t index = 0; index < pixels; ++index) {
        const auto* pixel = block.data + index * 4u;
        std::uint16_t counter = 0;
        std::memcpy(&counter, pixel, 2u);
        wrongBlue += pixel[2] != 0xffu;
        wrongCounter += counter != lastPass;
        wrongAlpha += pixel[3] != 0x77u;
    }
    std::printf("linear target concurrent bytes: pixels=%zu recorded=%d passes=%u (at wait end %u) draw ms=%.2f wait ms=%.2f\n", pixels, wasRecorded, hammer.passes.load(), passesAtDone, std::chrono::duration<double, std::milli>(recorded - before).count(), std::chrono::duration<double, std::milli>(done - recorded).count());
    std::printf("linear target concurrent bytes: mode=%s counter regressions seen by the CPU writer=%zu (lost updates by one pass=%zu, other stale values=%zu) alpha corruptions seen=%zu; final wrong blue=%zu wrong counter=%zu wrong alpha=%zu\n", sequential ? "sequential" : "concurrent", hammer.regressions.load(), hammer.lostUpdates.load(), hammer.staleValues.load(), hammer.alphaCorruptions.load(), wrongBlue, wrongCounter, wrongAlpha);
    for (std::size_t slot = 0; slot < std::min<std::size_t>(hammer.regressions.load(), hammer.firstRegressions.size()); ++slot) {
        const auto packed = hammer.firstRegressions[slot];
        std::printf("  regression: pass=%llu saw=%llu at pixel=%llu (expected %llu)\n", static_cast<unsigned long long>(packed >> 48u), static_cast<unsigned long long>((packed >> 32u) & 0xffffu), static_cast<unsigned long long>(packed & 0xffffffffu), static_cast<unsigned long long>((packed >> 48u) - 1u));
    }
    Require(wrongBlue == 0, "the GPU's blue channel did not reach guest memory");
    Require(hammer.regressions.load() == 0 && hammer.alphaCorruptions.load() == 0 && wrongCounter == 0 && wrongAlpha == 0, "CPU byte stores to channels the draw masked out were lost or reverted during publication");
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!Expect(*device)) {
            std::puts("linear target concurrent bytes skipped: no host import on this device");
            return VulkanTestSkipped;
        }
        std::puts("linear target concurrent bytes passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
