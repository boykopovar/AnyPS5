#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "ShaderCacheDirectory.hpp"
#include "../NarrowConstantStoreFixture.hpp"
#include "Recompiler.hpp"
#include "ShaderDiskCache.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Color1555Info = 0x8044u;
constexpr std::uint32_t LinearAttrib3 = 0x09000000u;
constexpr std::uint32_t Float32Export = 9;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

alignas(256) std::array<std::uint32_t, 64> PixelProgram{};
std::size_t pixelProgramWords = 0;

std::span<const std::uint32_t> Program(const std::array<float, 4>& color) {
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        PixelProgram[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        PixelProgram[at++] = std::bit_cast<std::uint32_t>(color[channel]);
    }
    PixelProgram[at++] = 0xf800180fu;
    PixelProgram[at++] = 0x07060504u;
    PixelProgram[at++] = 0xbf810000u;
    pixelProgramWords = at;
    return std::span<const std::uint32_t>(PixelProgram.data(), at);
}

class Block {
public:
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
            Require(raw != MAP_FAILED, "cannot map the guest block");
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
        Require(data != nullptr, "cannot allocate the guest block");
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

AgcDriver::Graphics::ColorTarget DecodeTarget(const Block& block) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.Address() >> 8u);
    cx[0x31b] = 0;
    cx[0x31c] = Color1555Info;
    cx[0x31d] = 0;
    cx[0x3b0] = ((Width - 1u) << 14u) | (Height - 1u);
    cx[0x3b8] = LinearAttrib3;
    cx[0x390] = static_cast<std::uint32_t>(block.Address() >> 40u);
    auto color = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(color.format == VK_FORMAT_A1R5G5B5_UNORM_PACK16 && color.bytes == block.bytes, "the test target did not decode as an A1R5G5B5 surface");
    return color;
}

void Draw(AgcDriver::VulkanDevice& device, const AgcDriver::Graphics::ColorTarget& color, std::span<const std::uint32_t> pixelCode, std::uint32_t colorWriteMask = 0xf) {
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
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = colorWriteMask;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};

    const auto codeAddress = reinterpret_cast<std::uintptr_t>(pixelCode.data());
    const AgcDriver::Registers context{{0x1b6, 0x8000}, {0x1b3, 0}, {0x1b4, 0}, {0x203, 0}, {0x1c5, Float32Export}};
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
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
}

void ExpectTexel(AgcDriver::VulkanDevice& device, const Block& block, std::uint16_t texel, const char* what) {
    AgcDriver::Graphics::StorageTexture::FlushPending(block.Address(), block.bytes, nullptr, "test read-back");
    device.WaitIdle();
    std::vector<std::uint16_t> stored(block.bytes / 2u);
    AgcDriver::GuestMemory::Read(block.Address(), std::as_writable_bytes(std::span(stored)), 1);
    const ColorTargetLayout layout(Width, Height, ColorTileMode::Linear, 2);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto index = layout.Offset(x, y) / 2u;
            if (stored[index] == texel) continue;
            char detail[160];
            std::snprintf(detail, sizeof(detail), ": pixel (%u, %u) holds 0x%04x, expected 0x%04x", x, y, stored[index], texel);
            throw std::runtime_error(std::string(what) + detail);
        }
    }
}

// The constant-store compute program with its two stored literals replaced: `salt` 0 is the
// fixture's program, every other value a program of its own with its own module.
std::vector<std::uint32_t> ComputeCode(std::uint32_t salt) {
    std::vector<std::uint32_t> code(NarrowConstantStoreFixture::Code.begin(), NarrowConstantStoreFixture::Code.end());
    if (salt != 0) {
        code[3] = 0x00a10200u ^ (salt * 0x01010101u);
        code[5] = 0x0000fec3u ^ (salt * 0x00010001u);
    }
    return code;
}

ShaderRecompiler::RecompileResult CompileCompute(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& code, std::span<const std::uint32_t> userData, std::uint32_t waveSize) {
    using namespace ShaderRecompiler;
    const std::span<const std::uint32_t> words(code);
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(words.data()), std::as_bytes(words)}}};
    const ShaderComputeStageInfo compute{{NarrowConstantStoreFixture::Threads, 1, 1}, 0, {false, false, false}, false, 1, {}};
    RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(words.data()), words, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0, 0, 0, 128}, std::nullopt, false,
    };
    return Recompile(request);
}

void ExpectConstantStore(const Block& block, const char* what) {
    using namespace NarrowConstantStoreFixture;
    for (std::size_t offset = 0; offset < Threads * LaneBytes; ++offset) {
        const auto expected = ExpectedLane[offset % LaneBytes];
        Require(block.data[offset] == expected, std::string(what) + ": byte " + std::to_string(offset) + " is " + std::to_string(block.data[offset]) + ", expected " + std::to_string(expected));
    }
}

std::array<std::uint32_t, 2> AddressWords(const Block& block) {
    const auto address = block.Address();
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
}

void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

struct Totals {
    AgcDriver::Graphics::PipelineJournalCounters journal;
    AgcDriver::Graphics::PipelineStoreCounters store;
};

Totals Snapshot() {
    return {AgcDriver::Graphics::PipelineJournalTotals(), AgcDriver::Graphics::PipelineStoreTotals()};
}

void JournalTests() {
    using namespace NarrowConstantStoreFixture;
    Require(AgcDriver::Graphics::PipelineJournalEnabled(), "the pipeline journal is disabled although the shader cache directory is set");
    const auto journalPath = AgcDriver::Graphics::PipelineJournalPath();
    Block compute(BlockBytes);
    Block target(ColorTargetLayout(Width, Height, ColorTileMode::Linear, 2).Bytes());
    const auto computeCode = ComputeCode(0);
    const std::array<float, 4> red{1.0f, 0.0f, 0.0f, 1.0f};
    constexpr std::uint16_t redTexel = 0x801f;
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the first device is required once the second opened");
        device->WaitForPipelineJournalReplay();
        const auto before = Snapshot();
        Require(before.journal.loadedRecipes == 0 && before.journal.replayedGraphics == 0 && before.journal.replayedCompute == 0, "an empty journal replayed pipelines");
        std::memset(compute.data, Fill, compute.bytes);
        const auto words = AddressWords(compute);
        const auto result = CompileCompute(*device, computeCode, words, 32);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(computeCode.data()));
        device->WaitIdle();
        ExpectConstantStore(compute, "first run: the constant store dispatch");
        std::memset(target.data, 0, target.bytes);
        Draw(*device, DecodeTarget(target), Program(red));
        ExpectTexel(*device, target, redTexel, "first run: the draw");
        Require(device->ComputePipelinesCreated() == 1, "the first run did not build exactly one compute pipeline");
        AgcDriver::Graphics::FlushPipelineJournal();
        const auto after = Snapshot();
        Require(after.store.misses == before.store.misses + 1 && after.store.hits == before.store.hits, "the first draw did not miss the pipeline store exactly once");
        Require(after.journal.recordedGraphics == 1 && after.journal.recordedCompute == 1 && after.journal.recordedModules == 3 && after.journal.writeFailures == 0, "the first run did not record one graphics and one compute pipeline with three modules");
        Draw(*device, DecodeTarget(target), Program(red));
        device->WaitIdle();
        AgcDriver::Graphics::FlushPipelineJournal();
        const auto repeated = Snapshot();
        Require(repeated.store.hits == after.store.hits + 1 && repeated.store.misses == after.store.misses, "a repeated draw did not hit the pipeline store");
        Require(repeated.journal.recordedGraphics == 1 && repeated.journal.recordedModules == 3, "a repeated draw recorded its pipeline again");
    }
    Require(std::filesystem::exists(journalPath) && std::filesystem::file_size(journalPath) > 0, "the journal file was not written");
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the second device could not be opened");
        device->WaitForPipelineJournalReplay();
        const auto replayed = Snapshot();
        Require(replayed.journal.loadedRecipes == 2 && replayed.journal.loadedModules == 3 && replayed.journal.droppedRecords == 0, "the second run did not load the two recorded pipelines and their three modules");
        Require(replayed.journal.replayedGraphics == 1 && replayed.journal.replayedCompute == 1 && replayed.journal.replayRejected == 0 && replayed.journal.replayFailed == 0 && replayed.journal.replayPresent == 0, "the replay did not build one graphics and one compute pipeline");
        Require(device->ComputePipelinesCreated() == 1, "the replay did not build the compute pipeline on the new device");
        std::memset(compute.data, Fill, compute.bytes);
        const auto words = AddressWords(compute);
        const auto result = CompileCompute(*device, computeCode, words, 32);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(computeCode.data()));
        device->WaitIdle();
        ExpectConstantStore(compute, "second run: the constant store dispatch");
        Require(device->ComputePipelinesCreated() == 1, "the second run's dispatch built a compute pipeline the replay had built");
        std::memset(target.data, 0, target.bytes);
        Draw(*device, DecodeTarget(target), Program(red));
        ExpectTexel(*device, target, redTexel, "second run: the draw through the replayed pipeline");
        const auto drawn = Snapshot();
        Require(drawn.store.hits == replayed.store.hits + 1 && drawn.store.misses == replayed.store.misses, "the second run's draw did not find the replayed pipeline");
        AgcDriver::Graphics::FlushPipelineJournal();
        const auto final = Snapshot();
        Require(final.journal.recordedGraphics == 1 && final.journal.recordedCompute == 1 && final.journal.recordedModules == 3, "the second run recorded pipelines the journal already held");
    }
}

void HeldModuleTests() {
    using namespace NarrowConstantStoreFixture;
    constexpr std::uint32_t programs = 16;
    Block compute(BlockBytes);
    const auto words = AddressWords(compute);
    std::vector<std::vector<std::uint32_t>> codes;
    for (std::uint32_t salt = 1; salt <= programs; ++salt) codes.push_back(ComputeCode(salt));
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "no device to record the compute programs");
        device->WaitForPipelineJournalReplay();
        std::vector<ShaderRecompiler::RecompileResult> results;
        for (const auto& code : codes) results.push_back(CompileCompute(*device, code, words, 32));
        for (std::size_t index = 0; index < results.size(); ++index) device->Dispatch(results[index], 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(codes[index].data()));
        device->WaitIdle();
        AgcDriver::Graphics::FlushPipelineJournal();
        std::uint64_t encoded = 0;
        for (const auto& result : results) {
            std::vector<std::byte> bytes;
            ShaderRecompiler::ShaderDiskCache::EncodeResult(result, bytes);
            encoded += bytes.size();
        }
        const auto journalBytes = std::filesystem::file_size(AgcDriver::Graphics::PipelineJournalPath());
        Require(journalBytes < encoded, "the journal is " + std::to_string(journalBytes) + " bytes with 16 of its modules alone encoding to " + std::to_string(encoded) + " bytes, so it does not store them compressed");
    }
    SetEnvironment("APS5_PIPELINE_JOURNAL_THREADS", "1");
    const auto before = Snapshot();
    const auto device = OpenVulkanTestDevice();
    Require(device != nullptr, "no device to replay the compute programs");
    device->WaitForPipelineJournalReplay();
    const auto replayed = Snapshot();
    SetEnvironment("APS5_PIPELINE_JOURNAL_THREADS", "2");
    Require(replayed.journal.loadedRecipes == programs + 2 && replayed.journal.loadedModules == programs + 3 && replayed.journal.droppedRecords == 0, "the journal does not hold the 16 compute programs beside the first two pipelines");
    Require(replayed.journal.replayedCompute - before.journal.replayedCompute == programs + 1 && replayed.journal.replayedGraphics - before.journal.replayedGraphics == 1 && replayed.journal.replayRejected == before.journal.replayRejected && replayed.journal.replayFailed == before.journal.replayFailed, "the one-thread replay did not build every recorded pipeline");
    const auto held = replayed.journal.replayHeldPeakBytes;
    Require(held > 0 && held * 4 < replayed.journal.loadedBytes, "the one-thread replay held " + std::to_string(held) + " bytes of modules at once from a " + std::to_string(replayed.journal.loadedBytes) + "-byte journal");
}

// --benchmark [count]: `count` distinct compute programs, dispatched for the first time on three
// devices in turn. The first device starts without the driver's pipeline cache file (a driver with
// an on-disk shader cache of its own, lavapipe's Mesa cache here, still serves from it unless that
// is disabled too, MESA_SHADER_CACHE_DISABLE=true, where the driver's compile dominates); it leaves
// the driver's cache and the journal warm. The second replays the journal before its dispatches.
// The third dispatches with the driver's cache warm but the journal file removed: what a warm run
// pays at each first use without the journal. Wall times only; no pass threshold.
void Benchmark(std::uint32_t count) {
    using namespace NarrowConstantStoreFixture;
    Require(AgcDriver::Graphics::PipelineJournalEnabled(), "the pipeline journal is disabled");
    Block compute(BlockBytes);
    const auto words = AddressWords(compute);
    std::vector<std::vector<std::uint32_t>> codes;
    for (std::uint32_t salt = 1; salt <= count; ++salt) codes.push_back(ComputeCode(salt));
    struct Run {
        double openMs = 0;
        double dispatchMs = 0;
        std::uint64_t created = 0;
        AgcDriver::Graphics::PipelineJournalCounters journal;
    };
    const auto run = [&](const char* what) {
        Run measured;
        const auto openedAt = std::chrono::steady_clock::now();
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, std::string("no device for the benchmark's ") + what);
        device->WaitForPipelineJournalReplay();
        measured.openMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - openedAt).count();
        measured.journal = AgcDriver::Graphics::PipelineJournalTotals();
        std::vector<ShaderRecompiler::RecompileResult> results;
        for (const auto& code : codes) results.push_back(CompileCompute(*device, code, words, 32));
        const auto createdBefore = device->ComputePipelinesCreated();
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t index = 0; index < results.size(); ++index) device->Dispatch(results[index], 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(codes[index].data()));
        device->WaitIdle();
        measured.dispatchMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        measured.created = device->ComputePipelinesCreated() - createdBefore;
        AgcDriver::Graphics::FlushPipelineJournal();
        return measured;
    };
    const auto cold = run("cold run");
    const auto replayed = run("replayed run");
    std::error_code error;
    std::filesystem::remove(AgcDriver::Graphics::PipelineJournalPath(), error);
    Require(!error, "cannot remove the journal for the benchmark's last run");
    const auto warm = run("warm run without the journal");
    std::printf("pipeline journal benchmark: %u compute programs, %u replay threads\n", count, replayed.journal.replayThreads);
    std::printf("  cold driver cache, no journal:  %llu pipelines built at their dispatches, %u dispatches in %.1f ms (%.2f ms each)\n", static_cast<unsigned long long>(cold.created), count, cold.dispatchMs, cold.dispatchMs / count);
    std::printf("  warm driver cache, no journal:  %llu pipelines built at their dispatches, %u dispatches in %.1f ms (%.2f ms each)\n", static_cast<unsigned long long>(warm.created), count, warm.dispatchMs, warm.dispatchMs / count);
    std::printf("  warm driver cache, journal:     %llu pipelines built by the replay in %.1f ms of replay work (%.1f ms from the device opening), then %llu built at their dispatches, %u dispatches in %.1f ms (%.2f ms each)\n", static_cast<unsigned long long>(replayed.journal.replayedCompute), replayed.journal.replayMs, replayed.openMs, static_cast<unsigned long long>(replayed.created), count, replayed.dispatchMs, replayed.dispatchMs / count);
}

struct JournalRecordHeader {
    std::uint32_t kind;
    std::uint32_t reserved;
    std::uint64_t payloadBytes;
    std::uint64_t payloadHash;
};
static_assert(sizeof(JournalRecordHeader) == 24);

constexpr std::uint32_t JournalModuleRecord = 1u;
constexpr std::uint32_t JournalComputeRecord = 3u;
constexpr std::uint32_t JournalHeaderBytes = 32u;

void AppendJournalRecord(std::vector<std::byte>& file, std::uint32_t kind, const std::vector<std::byte>& payload) {
    const JournalRecordHeader header{kind, 0u, payload.size(), ShaderRecompiler::HashBytes(payload)};
    const auto offset = file.size();
    file.resize(offset + sizeof(header) + payload.size());
    std::memcpy(file.data() + offset, &header, sizeof(header));
    std::memcpy(file.data() + offset + sizeof(header), payload.data(), payload.size());
    while ((file.size() - offset - sizeof(header)) % 8u != 0) file.push_back(std::byte{0});
}

std::vector<ShaderRecompiler::PipelineIdentity> ReadJournalComputeIdentities(const std::filesystem::path& path) {
    std::vector<std::byte> file;
    std::ifstream input(path, std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    file.reserve(bytes.size());
    for (const auto byte : bytes) file.push_back(static_cast<std::byte>(byte));
    std::vector<ShaderRecompiler::PipelineIdentity> identities;
    std::size_t cursor = JournalHeaderBytes;
    while (cursor + sizeof(JournalRecordHeader) <= file.size()) {
        JournalRecordHeader header{};
        std::memcpy(&header, file.data() + cursor, sizeof(header));
        const auto payload = cursor + sizeof(header);
        if (header.payloadBytes > file.size() - payload) break;
        if (header.kind == JournalComputeRecord && header.payloadBytes >= 17u) {
            ShaderRecompiler::PipelineIdentity identity;
            std::memcpy(&identity.low, file.data() + payload, sizeof(identity.low));
            std::memcpy(&identity.high, file.data() + payload + 8, sizeof(identity.high));
            identities.push_back(identity);
        }
        cursor = payload + header.payloadBytes;
        while (cursor % 8u != 0) ++cursor;
    }
    return identities;
}

void NewestEntriesTests() {
    using namespace NarrowConstantStoreFixture;
    Block compute(BlockBytes);
    const auto words = AddressWords(compute);
    const auto recorded = Snapshot();
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the newest entries test could not be opened");
        device->WaitForPipelineJournalReplay();
        for (std::uint32_t salt = 1; salt <= 3; ++salt) {
            const auto code = ComputeCode(salt);
            const auto result = CompileCompute(*device, code, words, 32);
            device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        }
        device->WaitIdle();
        AgcDriver::Graphics::FlushPipelineJournal();
        const auto after = Snapshot();
        Require(after.journal.recordedCompute == recorded.journal.recordedCompute + 3, "the three compute pipelines were not recorded");
    }
    SetEnvironment("APS5_PIPELINE_JOURNAL_ENTRIES", "2");
    const auto before = Snapshot();
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the newest entries replay could not be opened");
        device->WaitForPipelineJournalReplay();
        const auto replayed = Snapshot();
        Require(replayed.journal.replayedCompute == before.journal.replayedCompute + 2, "the replay did not build the two newest entries: built " + std::to_string(replayed.journal.replayedCompute - before.journal.replayedCompute) + " compute and " + std::to_string(replayed.journal.replayedGraphics - before.journal.replayedGraphics) + " graphics, " + std::to_string(replayed.journal.replayRejected - before.journal.replayRejected) + " rejected, " + std::to_string(replayed.journal.replayPresent - before.journal.replayPresent) + " present, " + std::to_string(replayed.journal.replaySkipped - before.journal.replaySkipped) + " skipped");
        Require(replayed.journal.replayedGraphics == before.journal.replayedGraphics, "the replay built an entry that is not among the newest");
        const auto created = device->ComputePipelinesCreated();
        const auto newest = ComputeCode(3);
        const auto result = CompileCompute(*device, newest, words, 32);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(newest.data()));
        device->WaitIdle();
        Require(device->ComputePipelinesCreated() == created, "the newest pipeline was not built by the replay");
        const auto oldest = ComputeCode(0);
        const auto old = CompileCompute(*device, oldest, words, 32);
        device->Dispatch(old, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(oldest.data()));
        device->WaitIdle();
        Require(device->ComputePipelinesCreated() == created + 1, "the oldest pipeline was built by the replay");
    }
    SetEnvironment("APS5_PIPELINE_JOURNAL_ENTRIES", "1024");
}

void EvictionTests() {
    using namespace NarrowConstantStoreFixture;
    const auto path = AgcDriver::Graphics::PipelineJournalPath();
    AgcDriver::Graphics::FlushPipelineJournal();
    constexpr std::size_t Modules = 24;
    constexpr std::size_t ModuleBytes = 64u * 1024u;
    std::vector<std::byte> file;
    for (std::size_t index = 0; index < Modules; ++index) {
        std::vector<std::byte> module(20u + ModuleBytes, std::byte{0x5a});
        const std::uint64_t low = index + 1u;
        const std::uint64_t high = 0u;
        std::memcpy(module.data(), &low, sizeof(low));
        std::memcpy(module.data() + 8, &high, sizeof(high));
        AppendJournalRecord(file, JournalModuleRecord, module);
        std::vector<std::byte> compute(17u, std::byte{0});
        std::memcpy(compute.data(), &low, sizeof(low));
        std::memcpy(compute.data() + 8, &high, sizeof(high));
        AppendJournalRecord(file, JournalComputeRecord, compute);
    }
    {
        std::ofstream out(path, std::ios::binary | std::ios::app);
        Require(static_cast<bool>(out), "the synthetic journal records could not be written");
        out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
        out.flush();
        Require(!out.fail(), "the synthetic journal records could not be written");
    }
    Require(std::filesystem::file_size(path) > 1024u * 1024u, "the journal is smaller than the cap");
    const auto before = Snapshot();
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the eviction test could not be opened");
        device->WaitForPipelineJournalReplay();
        Block compute(BlockBytes);
        Block target(ColorTargetLayout(Width, Height, ColorTileMode::Linear, 2).Bytes());
        const auto words = AddressWords(compute);
        const auto code = ComputeCode(7);
        const auto result = CompileCompute(*device, code, words, 32);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        std::memset(target.data, 0, target.bytes);
        Draw(*device, DecodeTarget(target), Program(std::array<float, 4>{1.0f, 0.0f, 0.0f, 1.0f}));
        device->WaitIdle();
        AgcDriver::Graphics::FlushPipelineJournal();
    }
    const auto after = Snapshot();
    Require(after.journal.evictedRecords > before.journal.evictedRecords, "the journal was not rewritten when it reached its cap: " + std::to_string(after.journal.evictedRecords) + " records evicted, " + std::to_string(after.journal.writeFailures - before.journal.writeFailures) + " write failures");
    const auto size = std::filesystem::file_size(path);
    Require(size <= 1024u * 1024u + 64u * 1024u, "the rewritten journal is larger than the cap");
    const auto identities = ReadJournalComputeIdentities(path);
    Require(!identities.empty(), "the rewritten journal holds no compute pipeline");
    bool newest = false;
    for (const auto& identity : identities) {
        if (identity.low == Modules) newest = true;
        Require(identity.low != 1u, "the oldest pipeline survived the rewrite");
    }
    Require(newest, "the newest pipeline did not survive the rewrite");
}

void EvictionWatermarkTests() {
    using namespace NarrowConstantStoreFixture;
    const auto path = AgcDriver::Graphics::PipelineJournalPath();
    const auto before = Snapshot();
    Block compute(BlockBytes);
    const auto words = AddressWords(compute);
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the eviction watermark test could not be opened");
        device->WaitForPipelineJournalReplay();
        for (std::uint32_t salt = 8; salt <= 10; ++salt) {
            auto result = CompileCompute(*device, ComputeCode(salt), words, 32);
            result.shaderDataUserWords.assign(16u * 1024u, salt);
            AgcDriver::Graphics::RecordComputePipeline(result, true);
        }
        AgcDriver::Graphics::FlushPipelineJournal();
    }
    const auto after = Snapshot();
    Require(after.journal.recordedCompute == before.journal.recordedCompute + 3, "the records after the rewrite were not recorded");
    Require(after.journal.writeFailures == before.journal.writeFailures, "the records after the rewrite failed to write");
    Require(after.journal.evictedRecords == before.journal.evictedRecords, "three consecutive records after a rewrite triggered another rewrite");
    Require(std::filesystem::file_size(path) <= 1024u * 1024u, "the journal exceeded its cap after the consecutive records");
}

void EvictionKeepsPendingModules() {
    using namespace NarrowConstantStoreFixture;
    const auto path = AgcDriver::Graphics::PipelineJournalPath();
    Block target(ColorTargetLayout(Width, Height, ColorTileMode::Linear, 2).Bytes());
    const auto color = DecodeTarget(target);
    const std::array<float, 4> red{1.0f, 0.0f, 0.0f, 1.0f};
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the pending module test could not be opened");
        device->WaitForPipelineJournalReplay();
        std::memset(target.data, 0, target.bytes);
        Draw(*device, color, Program(red));
        device->WaitIdle();
        AgcDriver::Graphics::FlushPipelineJournal();
    }
    constexpr std::size_t FillerBytes = 600u * 1024u;
    {
        std::vector<std::byte> file;
        std::vector<std::byte> module(20u + FillerBytes, std::byte{0x5a});
        const std::uint64_t low = 0x5a5au;
        const std::uint64_t high = 0u;
        std::memcpy(module.data(), &low, sizeof(low));
        std::memcpy(module.data() + 8, &high, sizeof(high));
        AppendJournalRecord(file, JournalModuleRecord, module);
        std::vector<std::byte> compute(17u + FillerBytes, std::byte{0});
        std::memcpy(compute.data(), &low, sizeof(low));
        std::memcpy(compute.data() + 8, &high, sizeof(high));
        AppendJournalRecord(file, JournalComputeRecord, compute);
        std::ofstream out(path, std::ios::binary | std::ios::app);
        Require(static_cast<bool>(out), "the filler records could not be written");
        out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
        out.flush();
        Require(!out.fail(), "the filler records could not be written");
    }
    Require(std::filesystem::file_size(path) > 1024u * 1024u, "the journal is smaller than the cap");
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the pending module test could not be opened");
        device->WaitForPipelineJournalReplay();
        std::memset(target.data, 0, target.bytes);
        Draw(*device, color, Program(red), 0x7);
        device->WaitIdle();
        AgcDriver::Graphics::FlushPipelineJournal();
    }
    const auto before = Snapshot();
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the pending module replay could not be opened");
        device->WaitForPipelineJournalReplay();
        const auto replayed = Snapshot();
        Require(replayed.journal.replayedGraphics == before.journal.replayedGraphics + 1, "the record written after the eviction was not replayed: " + std::to_string(replayed.journal.replayedGraphics - before.journal.replayedGraphics) + " graphics built, " + std::to_string(replayed.journal.replayRejected - before.journal.replayRejected) + " rejected");
        Require(replayed.journal.replayRejected == before.journal.replayRejected, "the record written after the eviction was rejected: the eviction dropped a module it needed");
    }
}

void OversizedRecordKeepsTheJournal() {
    using namespace NarrowConstantStoreFixture;
    const auto path = AgcDriver::Graphics::PipelineJournalPath();
    Block compute(BlockBytes);
    const auto words = AddressWords(compute);
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the oversized record test could not be opened");
        device->WaitForPipelineJournalReplay();
        const auto code = ComputeCode(0);
        const auto result = CompileCompute(*device, code, words, 32);
        device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device->WaitIdle();
        AgcDriver::Graphics::FlushPipelineJournal();
    }
    const auto before = Snapshot();
    const auto sizeBefore = std::filesystem::file_size(path);
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the oversized record test could not be opened");
        device->WaitForPipelineJournalReplay();
        const auto code = ComputeCode(2);
        auto result = CompileCompute(*device, code, words, 32);
        result.shaderDataUserWords.resize(400000u);
        std::uint32_t noise = 0x9e3779b9u;
        for (auto& word : result.shaderDataUserWords) word = noise = noise * 1664525u + 1013904223u;
        AgcDriver::Graphics::RecordComputePipeline(result, false);
        AgcDriver::Graphics::FlushPipelineJournal();
    }
    const auto after = Snapshot();
    Require(after.journal.writeFailures == before.journal.writeFailures + 1, "the oversized record was not counted as a write failure");
    Require(std::filesystem::file_size(path) == sizeBefore, "the oversized record emptied the journal: " + std::to_string(sizeBefore) + " bytes became " + std::to_string(std::filesystem::file_size(path)));
    const auto before2 = Snapshot();
    {
        const auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "the device for the oversized record replay could not be opened");
        device->WaitForPipelineJournalReplay();
        const auto replayed = Snapshot();
        Require(replayed.journal.replayedCompute == before2.journal.replayedCompute + 1, "the record that was in the journal did not survive the oversized one");
    }
}

}

int main(int argc, char** argv) {
    try {
        const auto directory = std::filesystem::temp_directory_path() / ("aps5-pipeline-journal-test-" + std::to_string(std::random_device{}()));
        std::filesystem::create_directories(directory);
        SetEnvironment("ANYPS5_NO_SHADER_CACHE", "0");
        SetEnvironment("ANYPS5_SHADER_CACHE_DIR", directory.string());
        SetEnvironment("APS5_NO_PIPELINE_JOURNAL", "0");
        SetEnvironment("APS5_VERIFY_SET_LAYOUTS", "1");
        SetEnvironment("APS5_PIPELINE_JOURNAL_THREADS", "2");
        SetEnvironment("APS5_PIPELINE_JOURNAL_MAX_MIB", "1");
        struct Cleanup {
            std::filesystem::path directory;
            ~Cleanup() {
                std::error_code error;
                std::filesystem::remove_all(directory, error);
            }
        } cleanup{directory};
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        if (argc >= 2 && std::string(argv[1]) == "--benchmark") {
            Benchmark(argc >= 3 ? static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10)) : 24u);
            return 0;
        }
        JournalTests();
        NewestEntriesTests();
        EvictionTests();
        EvictionWatermarkTests();
        EvictionKeepsPendingModules();
        OversizedRecordKeepsTheJournal();
        HeldModuleTests();
        std::puts("pipeline journal record and replay passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
