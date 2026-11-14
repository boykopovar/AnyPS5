#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
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
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Recorder;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Triangles;
using DepthFastClearHarness::Width;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t CopiedBytes = Lanes * 4u;
constexpr std::size_t EmptyOffset = 4096;
constexpr std::size_t LateOffset = 8192;
constexpr std::size_t KeptOffset = 12288;
constexpr std::size_t All = std::size_t{Width} * Height * 4u;
const ColorTargetLayout TargetLayout(Width, Height, ColorTileMode::RenderTarget, 4);

alignas(256) constexpr std::array<std::uint32_t, 4> VertexCode{0xbe822100, 0xf80008cf, 0x0b0a0908, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 4> CoverCode{0x7e0002f2, 0xf800180f, 0x00000000, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 7> CopyAcross{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0xbf810000u};

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
            Require(raw != MAP_FAILED, "draw input GPU copy: cannot map a block");
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
        Require(data != nullptr, "draw input GPU copy: cannot allocate a block");
        std::memset(data, 0, bytes);
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

    std::uint64_t Address(std::size_t offset = 0) const { return reinterpret_cast<std::uintptr_t>(data) + offset; }

    std::size_t bytes;
    bool watched;
    std::uint8_t* data = nullptr;
};

struct Program {
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult pixel;
    std::uint32_t vertexPush = 0;
};

Program CompileDraw(AgcDriver::VulkanDevice& device, std::uint64_t vertices, std::uint64_t variant) {
    const auto target = device.Target();
    Program program;
    const std::vector<std::uint32_t> vertexUserData{0x10000000u, 1u, 0u, 0u};
    ShaderRecompiler::ShaderVertexStageInfo info{};
    info.resourcesNum = 1;
    info.resources[0].fields = {static_cast<std::uint32_t>(vertices), static_cast<std::uint32_t>((vertices >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(Triangles().size()), 0x3104dfacu};
    info.resourcesDst[0] = {8, 4, 0, 0};
    info.fetchAttribReg = 8;
    info.fetchBufferReg = 10;
    info.fetchEmbedded = true;
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64, 0, vertexUserData, std::nullopt, std::nullopt, info, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    program.vertex = ShaderRecompiler::Recompile(vertex);
    Require(program.vertex.vertexAttributes.size() == 1, "draw input GPU copy: the vertex program's fetch did not become a vertex attribute");
    program.vertexPush = static_cast<std::uint32_t>(program.vertex.pushConstants.size());
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(CoverCode.data()), std::as_bytes(std::span(CoverCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(CoverCode.data()), CoverCode, 0, {}},
        {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
        target,
        PixelPushLayout(program.vertexPush, target)
    };
    fragment.useCache = false;
    program.pixel = ShaderRecompiler::Recompile(fragment);
    program.vertex.variantId = variant;
    program.pixel.variantId = variant + 1u;
    return program;
}

std::array<std::uint32_t, 4> ByteDescriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

ShaderRecompiler::RecompileResult CompileCopy(AgcDriver::VulkanDevice& device, std::uint64_t from, std::uint64_t to) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto source = ByteDescriptor(from, CopiedBytes);
    const auto destination = ByteDescriptor(to, CopiedBytes);
    std::copy(source.begin(), source.end(), userData.begin());
    std::copy(destination.begin(), destination.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(CopyAcross.data()), std::as_bytes(std::span(CopyAcross))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(CopyAcross.data()), CopyAcross, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

struct Scene {
    AgcDriver::VulkanDevice& device;
    const Block& vertices;
    const Program& reader;
    const ShaderRecompiler::RecompileResult& full;
    const ShaderRecompiler::RecompileResult& empty;
    bool copies;
    bool recorded;
};

AgcDriver::Graphics::State StateFor(const Block& target) {
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = {target.Address(), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, TargetLayout.Bytes(), 0xe4u, ColorTileMode::RenderTarget};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Height), static_cast<float>(Width), -static_cast<float>(Height), 0, 1};
    state.negativeOneToOne = false;
    state.scissor = {{0, 0}, {Width, Height}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    state.blendConstants = {};
    return state;
}

std::array<AgcDriver::Graphics::CompiledShader, 2> ShadersOf(const Scene& scene) {
    return {{
        {ShaderStage::Vertex, &scene.reader.vertex, 0},
        {ShaderStage::Fragment, &scene.reader.pixel, PixelPushOffset(scene.reader.vertexPush, scene.device.Target())}
    }};
}

const AgcDriver::Pm4::DrawParameters Triangle{0, 3, 0, 1, 0, false};

void Draw(const Scene& scene, const Block& target) {
    const auto shaders = ShadersOf(scene);
    scene.device.Draw(StateFor(target), Triangle, shaders);
}

void Draw(const Scene& scene, const Program& program, const Block& target) {
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &program.vertex, 0},
        {ShaderStage::Fragment, &program.pixel, PixelPushOffset(program.vertexPush, scene.device.Target())}
    }};
    scene.device.Draw(StateFor(target), Triangle, shaders);
}

void Fill(const Scene& scene) {
    scene.device.Dispatch(scene.full, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyAcross.data()));
}

void Clear(const Scene& scene) {
    scene.device.Dispatch(scene.empty, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyAcross.data()));
}

std::size_t Covered(const Scene& scene, const Block& target) {
    AgcDriver::Graphics::StorageTexture::FlushPending(target.Address(), target.bytes, nullptr, "test read-back");
    scene.device.WaitIdle();
    std::vector<std::uint8_t> stored(TargetLayout.Bytes());
    AgcDriver::GuestMemory::Read(target.Address(), std::as_writable_bytes(std::span(stored)), 1);
    return static_cast<std::size_t>(std::count(stored.begin(), stored.end(), std::uint8_t{0xff}));
}

struct Marks {
    Recorder::DrawInputCopyStatistics counts = Recorder::DrawInputCopyCounts();
    std::uint64_t submissions = Submissions();

    static std::uint64_t Submissions() {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "draw input GPU copy: no active recorder");
        return recorder->Submissions();
    }

    std::uint64_t Copies() const { return Recorder::DrawInputCopyCounts().copies - counts.copies; }
    std::uint64_t Hits() const { return Recorder::DrawInputCopyCounts().hits - counts.hits; }
    std::uint64_t Refused(Recorder::DrawInputCopyRefusal reason) const { return Recorder::DrawInputCopyCounts().refused[static_cast<std::size_t>(reason)] - counts.refused[static_cast<std::size_t>(reason)]; }
    bool Submitted() const { return Submissions() != submissions; }
};

bool Settled(const AgcDriver::VulkanDevice& device, const Block& vertices) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    return AgcDriver::Graphics::WatchedAfterSettle(context, vertices.Address(), CopiedBytes);
}

bool Pending(const Block& vertices) {
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    return recorder != nullptr && recorder->PendingWriteOverlaps(vertices.Address(), Triangles().size() * 16u);
}

void Expect(const Scene& scene, const Marks& marks, std::uint64_t copies, const std::string& what) {
    if (!scene.copies) {
        Require(marks.Copies() == 0, "draw input GPU copy, " + what + ": " + std::to_string(marks.Copies()) + " draw inputs were copied on the GPU although the copies are off");
        return;
    }
    Require(marks.Copies() == copies, "draw input GPU copy, " + what + ": " + std::to_string(marks.Copies()) + " draw inputs were copied on the GPU, expected " + std::to_string(copies));
    if (scene.recorded) Require(!marks.Submitted() && Pending(scene.vertices), "draw input GPU copy, " + what + ": the draws waited for the dispatch that wrote their vertices");
}

void DispatchThenDraws(const Scene& scene, const Block& target) {
    const Marks marks;
    Fill(scene);
    Draw(scene, target);
    Draw(scene, target);
    Expect(scene, marks, 1, "a dispatch then two draws");
    if (scene.copies && scene.recorded) Require(marks.Hits() >= 1, "draw input GPU copy: the second draw did not reuse the first draw's copy");
    if (!scene.copies && scene.recorded) Require(marks.Submitted(), "draw input GPU copy: a draw behind a dispatch recorded no wait with the copies off, so the test cannot tell a wait");
    Require(Covered(scene, target) == All, "draw input GPU copy: a draw did not cover its target with the triangle a dispatch wrote into its vertex buffer");
}

void SecondDispatch(const Scene& scene, const Block& first, const Block& second, const Block& third) {
    const Marks marks;
    Clear(scene);
    Draw(scene, first);
    Fill(scene);
    Draw(scene, second);
    Clear(scene);
    Draw(scene, third);
    Expect(scene, marks, 3, "three dispatches with a draw after each");
    Require(Covered(scene, first) == 0, "draw input GPU copy: a draw behind a dispatch that emptied its vertex buffer still drew the previous triangle");
    Require(Covered(scene, second) == All, "draw input GPU copy: a draw behind a second dispatch read the vertices the first one wrote");
    Require(Covered(scene, third) == 0, "draw input GPU copy: a draw behind a third dispatch read the vertices the second one wrote");
}

void CpuStore(const Scene& scene, const Block& before, const Block& after, const Block& direct) {
    Fill(scene);
    Draw(scene, before);
    const std::array<std::byte, 48> zeros{};
    AgcDriver::GuestMemory::Write(scene.vertices.Address(), zeros);
    Draw(scene, after);
    Require(Covered(scene, before) == All, "draw input GPU copy: the draw before a CPU store lost its triangle");
    Require(Covered(scene, after) == 0, "draw input GPU copy: a draw after a CPU store over its vertex buffer still drew the copied triangle");
    Fill(scene);
    Draw(scene, direct);
    scene.device.WaitIdle();
    std::memset(scene.vertices.data, 0, zeros.size());
    Draw(scene, after);
    Require(Covered(scene, direct) == All, "draw input GPU copy: the draw before a guest store lost its triangle");
    Require(Covered(scene, after) == 0, "draw input GPU copy: a draw after a guest store over its vertex buffer still drew the copied triangle");
}

void Recipe(const Scene& scene, const Block& target) {
    if (!scene.recorded) return;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(scene.device);
    const auto shaders = ShadersOf(scene);
    const auto state = StateFor(target);
    const Marks marks;
    Clear(scene);
    std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::Draw(context, state, Triangle, shaders, {}, &recipe);
    }
    Require(recipe != nullptr, "draw input GPU copy: a recorded draw produced no recipe");
    Fill(scene);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto hit = AgcDriver::Graphics::DrawWithRecipe(context, state, Triangle, shaders, {}, *recipe);
        Require(hit.recorded, std::string("draw input GPU copy: the recipe of the draw was refused: ") + AgcDriver::Graphics::DrawRecipeMissName(hit.miss));
    }
    Expect(scene, marks, 2, "a draw and its recipe behind a dispatch each");
    Require(Covered(scene, target) == All, "draw input GPU copy: a draw recorded from its recipe did not read the vertices the dispatch before it wrote");
}

void MissInsidePass(const Scene& scene, const Program& still, const Program& late, const ShaderRecompiler::RecompileResult& fillLate, const Block& target) {
    Draw(scene, still, target);
    scene.device.WaitIdle();
    const Marks marks;
    scene.device.Dispatch(fillLate, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyAcross.data()));
    Draw(scene, still, target);
    Draw(scene, late, target);
    if (scene.copies && scene.recorded) {
        Require(marks.Copies() == 1, "draw input GPU copy: a draw input first read inside a render pass was copied " + std::to_string(marks.Copies()) + " times, expected 1");
        const auto ended = Recorder::DrawInputCopyCounts().passBreaks - marks.counts.passBreaks;
        Require(ended == 1 && !marks.Submitted(), "draw input GPU copy: a draw input first read inside a render pass ended " + std::to_string(ended) + " render passes, expected 1, or waited for its dispatch");
    }
    Require(Covered(scene, target) == All, "draw input GPU copy: a draw whose input was copied inside a render pass did not read the vertices the dispatch wrote");
}

void PassKeptAcrossCopy(const Scene& scene, const Program& whole, const Program& late, const ShaderRecompiler::RecompileResult& clearLate, const Block& target) {
    const Marks marks;
    scene.device.Dispatch(clearLate, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(CopyAcross.data()));
    Draw(scene, whole, target);
    Draw(scene, late, target);
    if (scene.copies && scene.recorded) {
        const auto ended = Recorder::DrawInputCopyCounts().passBreaks - marks.counts.passBreaks;
        Require(marks.Copies() == 1 && ended == 1, "draw input GPU copy: a second draw input first read inside a render pass was copied " + std::to_string(marks.Copies()) + " times and ended " + std::to_string(ended) + " render passes, expected 1 and 1");
    }
    Require(Covered(scene, target) == All, "draw input GPU copy: the render pass a draw input copy ended lost what it had drawn");
}

void RefusedAllocation(const Scene& scene, const Block& before, const Block& after) {
    const Marks marks;
    Clear(scene);
    Draw(scene, before);
    AgcDriver::Graphics::SetDeviceLocalFullForTests(true);
    Fill(scene);
    Draw(scene, after);
    AgcDriver::Graphics::SetDeviceLocalFullForTests(false);
    if (scene.copies) Require(marks.Copies() == 1 && marks.Refused(Recorder::DrawInputCopyRefusal::Allocation) == 1, "draw input GPU copy: a draw input refused device memory was not left to the CPU copy once");
    Require(Covered(scene, before) == 0, "draw input GPU copy: the draw before the refused allocation drew a triangle");
    Require(Covered(scene, after) == All, "draw input GPU copy: a draw whose input was refused device memory did not read the vertices the dispatch wrote");
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, std::string("draw input GPU copy: cannot set ") + name);
#else
    Require(setenv(name, value, 1) == 0, std::string("draw input GPU copy: cannot set ") + name);
#endif
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        Require(mode.empty() || mode == "--off" || mode == "--staged" || mode == "--synchronous", "draw input GPU copy: unknown mode " + mode);
        SetEnvironment("APS5_NO_DRAW_INPUTS_IN_PLACE", "1");
        if (mode == "--off") SetEnvironment("APS5_NO_DRAW_INPUT_GPU_COPY", "1");
        if (mode == "--synchronous") SetEnvironment("APS5_SYNC_DRAWS", "1");
        if (mode == "--staged") {
            SetEnvironment("APS5_WRITTEN_SHADOW_MIN_KIB", "0");
            SetEnvironment("APS5_NO_CHANGED_COPY_BACK", "1");
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(!device->DrawInputsInPlace() && Recorder::DrawInputCopiesEnabled() == (mode != "--off"), "draw input GPU copy: the environment does not match the run");
        if (AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).hostImportAlignment == 0 || !AgcDriver::GuestMemory::WriteWatched()) {
            std::puts("skipped, host imports or write watching are unavailable, so no draw input lies behind a GPU write into guest memory");
            return VulkanTestSkipped;
        }
        {
            const Block source(BlockBytes);
            const Block vertices(BlockBytes);
            std::vector<std::unique_ptr<Block>> targets;
            for (int target = 0; target < 13; ++target) targets.push_back(std::make_unique<Block>(TargetLayout.Bytes()));
            std::memcpy(source.data, Triangles().data(), Triangles().size() * 16u);
            const auto reader = CompileDraw(*device, vertices.Address(), 0x51a7e001u);
            const auto still = CompileDraw(*device, source.Address(EmptyOffset), 0x51a7e011u);
            const auto late = CompileDraw(*device, vertices.Address(LateOffset), 0x51a7e021u);
            const auto fillLate = CompileCopy(*device, source.Address(), vertices.Address(LateOffset));
            const auto whole = CompileDraw(*device, source.Address(), 0x51a7e031u);
            const auto kept = CompileDraw(*device, vertices.Address(KeptOffset), 0x51a7e041u);
            const auto clearKept = CompileCopy(*device, source.Address(EmptyOffset), vertices.Address(KeptOffset));
            const auto full = CompileCopy(*device, source.Address(), vertices.Address());
            const auto empty = CompileCopy(*device, source.Address(EmptyOffset), vertices.Address());
            const Scene scene{*device, vertices, reader, full, empty, mode != "--off", mode != "--synchronous"};
            const auto landed = Recorder::HandoverCounts().landed;
            Fill(scene);
            device->WaitIdle();
            if (mode == "--staged" && Recorder::HandoverCounts().landed == landed) {
                std::puts("skipped, the device stages no written buffer in device memory, so no copy-back is pending when a draw input is copied");
                return VulkanTestSkipped;
            }
            if (!Settled(*device, vertices)) {
                std::puts("skipped, the device does not keep host imports write-watched, so a copied draw input cannot be proved unchanged");
                return VulkanTestSkipped;
            }
            Require(std::memcmp(vertices.data, Triangles().data(), Triangles().size() * 16u) == 0, "draw input GPU copy: the dispatch did not write the triangle into the vertex buffer");
            Clear(scene);
            device->WaitIdle();
            for (const auto& target : targets) Draw(scene, *target);
            device->WaitIdle();
            DispatchThenDraws(scene, *targets[1]);
            std::puts("draw input GPU copy: draws read the vertices a dispatch wrote");
            SecondDispatch(scene, *targets[2], *targets[3], *targets[4]);
            std::puts("draw input GPU copy: every dispatch between draws reached the next draw");
            CpuStore(scene, *targets[5], *targets[6], *targets[7]);
            std::puts("draw input GPU copy: CPU stores between draws reached the next draw");
            Recipe(scene, *targets[8]);
            std::puts(scene.recorded ? "draw input GPU copy: a recipe read the vertices a dispatch wrote" : "draw input GPU copy: no recipe, the draws are synchronous");
            MissInsidePass(scene, still, late, fillLate, *targets[11]);
            std::puts("draw input GPU copy: a draw input first read inside a render pass was copied there");
            PassKeptAcrossCopy(scene, whole, kept, clearKept, *targets[12]);
            std::puts("draw input GPU copy: the render pass a copy ended kept what it had drawn");
            RefusedAllocation(scene, *targets[9], *targets[10]);
            std::puts("draw input GPU copy: a draw input refused device memory was copied by the CPU");
            const auto counts = Recorder::DrawInputCopyCounts();
            std::printf("draw input GPU copy: %llu copies of %llu bytes, %llu reused, %llu render passes ended\n", static_cast<unsigned long long>(counts.copies), static_cast<unsigned long long>(counts.bytes), static_cast<unsigned long long>(counts.hits), static_cast<unsigned long long>(counts.passBreaks));
            device->WaitIdle();
        }
        device->WaitIdle();
        std::puts(mode == "--off" ? "draw input GPU copy tests passed with the copies off" : mode == "--staged" ? "draw input GPU copy tests passed behind staged dispatches" : mode == "--synchronous" ? "draw input GPU copy tests passed with synchronous draws" : "draw input GPU copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
