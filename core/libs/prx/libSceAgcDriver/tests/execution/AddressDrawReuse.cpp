#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
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
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
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

using AgcDriver::Graphics::ColorTargetLayout;
using AgcDriver::Graphics::ColorTileMode;
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t PageBytes = 4096;
constexpr std::uint32_t PointerOffset = 0;
constexpr std::uint32_t StoreOffset = 8;
constexpr std::uint32_t ValueOffset = 16;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr float One = 1.0f;
constexpr float Zero = 0.0f;

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000, 0x80000005, 0xbf8c3f70, 0xf80008cf, 0x03020100, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 14> PixelCode{
    0x7e000280u,
    0xdc348000u, 0x02000000u,
    0xbf8c3f70u,
    0xdc308000u, 0x017d0002u,
    0xbf8c3f70u,
    0xdc708008u, 0x00000100u,
    0xf800180fu, 0x01010101u,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 13> FullScreenVertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};

alignas(256) std::array<std::uint32_t, 128> SubmittedCode{};

struct ShaderHeader {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) ShaderHeader SubmittedPixelHeader{};
alignas(8) ShaderHeader SubmittedVertexHeader{};

constexpr std::array<std::array<float, 4>, 3> Triangle{{
    {-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}
}};

const ColorTargetLayout TargetLayout(Width, Height, ColorTileMode::RenderTarget, 4);

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class Pages {
public:
    explicit Pages(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* mapped = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED, "address draw reuse: cannot map guest pages");
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "address draw reuse: cannot allocate guest pages");
        std::memset(data, 0, bytes);
    }

    ~Pages() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        for (const auto offset : registered) GuestAllocations::Mutation().Remove(data + offset);
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

    Pages(const Pages&) = delete;
    Pages& operator=(const Pages&) = delete;

    void Register(std::size_t offset, std::size_t length, bool writable) {
        GuestAllocations::Mutation().Add(data + offset, length, true, writable, true);
        registered.push_back(offset);
    }

    void Unregister(std::size_t offset) {
        GuestAllocations::Mutation().Remove(data + offset);
        registered.erase(std::remove(registered.begin(), registered.end(), offset), registered.end());
    }

    std::uint64_t Address(std::size_t offset = 0) const { return AddressOf(data + offset); }
    std::uint8_t* Data() { return data; }

private:
    std::size_t bytes;
    bool watched = AgcDriver::GuestMemory::WriteWatched();
    std::uint8_t* data = nullptr;
    std::vector<std::size_t> registered;
};

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

std::uint64_t Builds() {
    const auto counters = AgcDriver::Graphics::BdaResources::TableCacheCounters();
    return counters.hits + counters.misses;
}

std::uint64_t Counted(const AgcDriver::Graphics::AddressReuseStats& stats, AgcDriver::Graphics::AddressReuse outcome) {
    return stats.outcomes[static_cast<std::size_t>(outcome)];
}

std::uint64_t Refusals(const AgcDriver::Graphics::AddressReuseStats& stats) {
    std::uint64_t refusals = 0;
    for (const auto count : stats.outcomes) refusals += count;
    return refusals - Counted(stats, AgcDriver::Graphics::AddressReuse::Reused);
}

struct Scene {
    AgcDriver::VulkanDevice& device;
    Pages pixels{TargetLayout.Bytes()};
    Pages vertices{BlockBytes};
    Pages guest{BlockBytes};
    Pages spare{BlockBytes};
    Pages extra;
    std::size_t ranges;
    std::vector<std::uint32_t> vertexUserData;
    std::vector<std::uint32_t> pixelUserData;
    std::array<std::uint32_t, PixelCode.size()> pixelProgram = PixelCode;
    ShaderRecompiler::RecompileResult vertexResult;
    ShaderRecompiler::RecompileResult pixelResult;
    std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{};
    std::array<AgcDriver::Graphics::GuestMemorySnapshot, 2> snapshots{};
    AgcDriver::Graphics::State state{};

    Scene(AgcDriver::VulkanDevice& device, std::size_t ranges, std::uint32_t vertexRecords = static_cast<std::uint32_t>(BlockBytes / 16u)) : device(device), extra(std::max<std::size_t>(ranges, 1) * 2 * PageBytes), ranges(ranges) {
        pixels.Register(0, TargetLayout.Bytes(), true);
        std::memcpy(vertices.Data(), Triangle.data(), sizeof(Triangle));
        vertices.Register(0, BlockBytes, false);
        guest.Register(0, BlockBytes, true);
        for (std::size_t range = 0; range < ranges; ++range) extra.Register(range * 2 * PageBytes, PageBytes, false);
        Point(guest.Address(ValueOffset));
        Store(guest, ValueOffset, One);
        const auto target = device.Target();
        const auto vertexAddress = vertices.Address();
        vertexUserData = {static_cast<std::uint32_t>(vertexAddress), static_cast<std::uint32_t>((vertexAddress >> 32u) & 0xffffu) | (16u << 16u), vertexRecords, 0x01016facu};
        const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{AddressOf(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
        ShaderRecompiler::RecompileRequest vertex{
            {ShaderStage::Vertex, AddressOf(VertexCode.data()), VertexCode, 0, {}},
            {64, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
            target,
            {0, 0, 0, 64}
        };
        vertex.useCache = false;
        vertexResult = ShaderRecompiler::Recompile(vertex);
        const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
        ShaderRecompiler::ShaderPixelStageInfo pixel{};
        pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
        pixel.posX = true;
        pixel.posY = true;
        pixel.targetOutputMode[0] = 9;
        pixel.targetExportMapping.fill(0xe4u);
        const auto pointer = guest.Address();
        pixelUserData = {static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u), 0u, 0u};
        const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{AddressOf(PixelCode.data()), std::as_bytes(std::span(PixelCode))}}};
        ShaderRecompiler::RecompileRequest fragment{
            {ShaderStage::Fragment, AddressOf(PixelCode.data()), PixelCode, 0, {}},
            {64, 0, pixelUserData, std::nullopt, pixel, std::nullopt, pixelMemory},
            target,
            PixelPushLayout(vertexPush, target)
        };
        fragment.useCache = false;
        pixelResult = ShaderRecompiler::Recompile(fragment);
        Require(std::any_of(pixelResult.bindings.begin(), pixelResult.bindings.end(), [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; }), "address draw reuse: the pixel shader does not reach guest memory through the BDA table");
        shaders = {{{ShaderStage::Vertex, &vertexResult, 0}, {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}}};
        snapshots = {{{AddressOf(VertexCode.data()), std::as_bytes(std::span(VertexCode))}, {AddressOf(PixelCode.data()), std::as_bytes(std::span(pixelProgram))}}};
        state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
        state.color = {pixels.Address(), {Width, Height}, VK_FORMAT_R8G8B8A8_UNORM, TargetLayout.Bytes(), 0xe4u, ColorTileMode::RenderTarget};
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
    }

    static void Store(Pages& pages, std::size_t offset, float value) {
        std::memcpy(pages.Data() + offset, &value, sizeof(value));
    }

    void Point(std::uint64_t address) {
        std::memcpy(guest.Data() + PointerOffset, &address, sizeof(address));
    }

    void Clear() {
        std::memcpy(guest.Data() + StoreOffset, &Sentinel, sizeof(Sentinel));
    }

    std::uint32_t Stored() {
        std::uint32_t value = 0;
        std::memcpy(&value, guest.Data() + StoreOffset, sizeof(value));
        return value;
    }

    double Draw() {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
        const auto start = std::chrono::steady_clock::now();
        device.Draw(state, draw, shaders, snapshots);
        return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
    }

    void Wait() {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.WaitIdle();
    }

    std::uint8_t Color() {
        AgcDriver::Graphics::StorageTexture::FlushPending(pixels.Address(), TargetLayout.Bytes(), nullptr, "test read-back");
        Wait();
        std::vector<std::uint8_t> texels(TargetLayout.Bytes());
        AgcDriver::GuestMemory::Read(pixels.Address(), std::as_writable_bytes(std::span(texels)), 1);
        const auto first = texels[TargetLayout.Offset(0, 0)];
        for (std::uint32_t y = 0; y < Height; ++y) {
            for (std::uint32_t x = 0; x < Width; ++x) {
                const auto offset = TargetLayout.Offset(x, y);
                for (std::size_t channel = 0; channel < 4; ++channel) Require(texels[offset + channel] == first, "address draw reuse: the full-screen draw left texels of different values");
            }
        }
        return first;
    }
};

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

struct Timing {
    std::uint64_t builds = 0;
    double firstUs = 0;
    double averageUs = 0;
};

Timing Repeat(Scene& scene, std::size_t draws, bool waited) {
    Timing timing;
    const auto before = Builds();
    double total = 0;
    for (std::size_t draw = 0; draw < draws; ++draw) {
        const auto us = scene.Draw();
        if (draw == 0) timing.firstUs = us;
        else total += us;
        if (waited) scene.Wait();
    }
    scene.Wait();
    timing.builds = Builds() - before;
    timing.averageUs = draws > 1 ? total / static_cast<double>(draws - 1) : 0.0;
    return timing;
}

void Report(const char* what, std::size_t draws, const Timing& timing) {
    std::printf("[address-reuse] %s: %zu draws, %llu builds, %llu reuses; first draw %.1f us, then %.1f us per draw\n", what, draws, static_cast<unsigned long long>(timing.builds), static_cast<unsigned long long>(draws - timing.builds), timing.firstUs, timing.averageUs);
}

bool Imported(Scene& scene) {
    scene.Draw();
    scene.Wait();
    if (AgcDriver::Graphics::HostImportLiveBytes() == 0) return false;
    Require(scene.Stored() == Bits(One), "address draw reuse: the first draw's store through the BDA table did not reach guest memory");
    return true;
}

void Synthetic(Scene& scene, std::size_t draws) {
    const auto waited = Repeat(scene, draws, true);
    Report("each draw waited for", draws, waited);
    const auto backToBack = Repeat(scene, draws, false);
    Report("back to back", draws, backToBack);
    Require(scene.Color() == 255 && scene.Stored() == Bits(One), "address draw reuse: repeated draws did not read and store the guest value");
    Require(waited.builds == 0, "address draw reuse: " + std::to_string(waited.builds) + " of " + std::to_string(draws) + " repeated address-based draws with unchanged inputs built their resources again");
}

void ReuseCounters(Scene& scene) {
    using AgcDriver::Graphics::AddressReuse;
    using AgcDriver::Graphics::AddressReuseCounters;
    constexpr std::uint64_t Repeats = 4;
    scene.Clear();
    scene.Draw();
    scene.Wait();
    auto before = AddressReuseCounters();
    for (std::uint64_t draw = 0; draw < Repeats; ++draw) {
        scene.Draw();
        scene.Wait();
    }
    auto after = AddressReuseCounters();
    const auto reused = Counted(after, AddressReuse::Reused) - Counted(before, AddressReuse::Reused);
    Require(reused == Repeats && after.proofs - before.proofs == Repeats && Refusals(after) == Refusals(before), "address draw reuse: " + std::to_string(Repeats) + " repeated draws with unchanged inputs counted " + std::to_string(reused) + " reuses, " + std::to_string(after.proofs - before.proofs) + " proofs and " + std::to_string(Refusals(after) - Refusals(before)) + " refusals");
    before = after;
    scene.Draw();
    scene.Draw();
    scene.Wait();
    after = AddressReuseCounters();
    Require(Counted(after, AddressReuse::InUse) - Counted(before, AddressReuse::InUse) == 1 && Counted(after, AddressReuse::Reused) - Counted(before, AddressReuse::Reused) == 1 && Refusals(after) - Refusals(before) == 1, "address draw reuse: a draw repeated before the previous one completed was not counted as one refusal in use");
    before = after;
    scene.pixelProgram.back() ^= 1u;
    scene.Draw();
    scene.Wait();
    after = AddressReuseCounters();
    Require(Counted(after, AddressReuse::Snapshots) - Counted(before, AddressReuse::Snapshots) == 1 && Refusals(after) - Refusals(before) == 1 && Counted(after, AddressReuse::Reused) == Counted(before, AddressReuse::Reused), "address draw reuse: a draw after the guest rewrote its pixel program was not counted as one refusal for a changed snapshot");
    scene.pixelProgram = PixelCode;
    scene.Draw();
    scene.Wait();
}

void GuestWrite(Scene& scene) {
    scene.Clear();
    scene.Draw();
    Require(scene.Color() == 255 && scene.Stored() == Bits(One), "address draw reuse: the draw before the guest write did not read the guest value");
    Scene::Store(scene.guest, ValueOffset, Zero);
    scene.Clear();
    const auto before = Builds();
    const auto counted = AgcDriver::Graphics::AddressReuseCounters();
    scene.Draw();
    Require(scene.Color() == 0, "address draw reuse: a draw after a guest write rendered the stale value");
    Require(scene.Stored() == Bits(Zero), "address draw reuse: a draw after a guest write stored the stale value through the BDA table");
    Require(Builds() == before, "address draw reuse: a guest write into memory the draw reads in place made it build its resources again");
    const auto after = AgcDriver::Graphics::AddressReuseCounters();
    Require(Counted(after, AgcDriver::Graphics::AddressReuse::Reused) == Counted(counted, AgcDriver::Graphics::AddressReuse::Reused) + 1 && Refusals(after) == Refusals(counted), "address draw reuse: a draw after a guest write into memory it reads in place was not counted as one reuse");
    Scene::Store(scene.guest, ValueOffset, One);
    for (int draw = 0; draw < 3; ++draw) {
        scene.Clear();
        scene.Draw();
        scene.Wait();
        Require(scene.Stored() == Bits(One), "address draw reuse: reused draw " + std::to_string(draw) + " did not store through the BDA table");
    }
}

void PendingSnapshotWrite(Scene& scene) {
    using AgcDriver::Graphics::AddressReuse;
    constexpr std::size_t RewrittenOffset = 32;
    const std::array<std::uint64_t, 1> captured{scene.guest.Address(ValueOffset)};
    const std::array<AgcDriver::Graphics::GuestMemorySnapshot, 3> snapshots{{
        scene.snapshots[0],
        scene.snapshots[1],
        {scene.guest.Address(PointerOffset), std::as_bytes(std::span(captured))},
    }};
    const auto draw = [&] {
        const AgcDriver::Pm4::DrawParameters parameters{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
        scene.device.Draw(scene.state, parameters, scene.shaders, snapshots);
    };
    Scene::Store(scene.guest, RewrittenOffset, Zero);
    scene.Point(scene.guest.Address(ValueOffset));
    for (int warm = 0; warm < 2; ++warm) {
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            draw();
        }
        scene.Wait();
    }
    Require(scene.Color() == 255, "address draw reuse: the draw with a captured pointer did not read the guest value");
    const auto rewritten = scene.guest.Address(RewrittenOffset);
    const std::array<std::uint32_t, 4> pattern{static_cast<std::uint32_t>(rewritten), static_cast<std::uint32_t>(rewritten >> 32u), 0u, 0u};
    const auto before = AgcDriver::Graphics::AddressReuseCounters();
    const auto built = Builds();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(scene.device.FillBuffer(scene.guest.Address(PointerOffset), 16, pattern), "address draw reuse: the fill over the captured pointer was not recorded on the GPU");
        draw();
    }
    const auto color = scene.Color();
    const auto after = AgcDriver::Graphics::AddressReuseCounters();
    const auto reused = Counted(after, AddressReuse::Reused) - Counted(before, AddressReuse::Reused);
    Require(color == 0, "address draw reuse: a draw after a recorded fill rewrote its captured pointer read the old pointer");
    Require(Counted(after, AddressReuse::Snapshots) == Counted(before, AddressReuse::Snapshots) + 1 && reused == 0 && Builds() == built + 1, "address draw reuse: a draw whose captured snapshot a pending GPU write changed was not refused for a changed snapshot and rebuilt (" + std::to_string(reused) + " reuses, " + std::to_string(Builds() - built) + " builds)");
    scene.Point(scene.guest.Address(ValueOffset));
    scene.Clear();
    scene.Draw();
    Require(scene.Color() == 255 && scene.Stored() == Bits(One), "address draw reuse: the draw after the captured pointer was restored did not read the guest value");
}

void Fault(Scene& scene) {
    void* reserved = nullptr;
#ifdef _WIN32
    reserved = VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE, PAGE_NOACCESS);
#else
    reserved = mmap(nullptr, BlockBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED) reserved = nullptr;
#endif
    Require(reserved != nullptr, "address draw reuse: cannot reserve an inaccessible range");
    const auto unmapped = AddressOf(reserved);
    scene.Clear();
    scene.Draw();
    scene.Wait();
    scene.Point(unmapped);
    scene.Clear();
    const auto before = Builds();
    StderrCapture log;
    std::string text;
    try {
        scene.Draw();
        scene.Wait();
    } catch (...) {
        static_cast<void>(log.Finish());
        throw;
    }
    text = log.Finish();
    const auto reused = Builds() == before;
    char address[64];
    std::snprintf(address, sizeof(address), "address=0x%llx", static_cast<unsigned long long>(unmapped));
    Require(text.find("BDA access failed") != std::string::npos && text.find(address) != std::string::npos && text.find("reason=1") != std::string::npos, "address draw reuse: a reused draw reading an unmapped pointer did not report its BDA fault:\n" + text);
    Require(reused, "address draw reuse: the faulting draw built its resources again instead of reusing them");
    scene.Point(scene.guest.Address(ValueOffset));
    scene.Clear();
    const auto afterFault = Builds();
    const auto counted = AgcDriver::Graphics::AddressReuseCounters();
    scene.Draw();
    Require(scene.Color() == 255 && scene.Stored() == Bits(One), "address draw reuse: the draw after a fault did not read the guest value");
    Require(Builds() == afterFault + 1, "address draw reuse: a build whose draw faulted served the next draw");
    const auto refused = AgcDriver::Graphics::AddressReuseCounters();
    Require(Counted(refused, AgcDriver::Graphics::AddressReuse::NotReusable) == Counted(counted, AgcDriver::Graphics::AddressReuse::NotReusable) + 1 && Refusals(refused) == Refusals(counted) + 1, "address draw reuse: the draw after a fault was not counted as one refusal of a build that is no longer reusable");
#ifdef _WIN32
    VirtualFree(reserved, 0, MEM_RELEASE);
#else
    munmap(reserved, BlockBytes);
#endif
}

void Remap(Scene& scene) {
    scene.Clear();
    scene.Draw();
    scene.Wait();
    Scene::Store(scene.spare, ValueOffset, Zero);
    scene.spare.Register(0, BlockBytes, false);
    scene.Point(scene.spare.Address(ValueOffset));
    scene.Clear();
    auto before = Builds();
    scene.Draw();
    Require(scene.Color() == 0 && scene.Stored() == Bits(Zero), "address draw reuse: a draw reading a newly mapped range read something else");
    Require(Builds() == before + 1, "address draw reuse: a draw reused resources built before a range was mapped");
    scene.Point(scene.guest.Address(ValueOffset));
    scene.spare.Unregister(0);
    scene.Clear();
    before = Builds();
    scene.Draw();
    Require(scene.Color() == 255 && scene.Stored() == Bits(One), "address draw reuse: a draw after a range was unmapped did not read the guest value");
    Require(Builds() == before + 1, "address draw reuse: a draw reused resources built before a range was unmapped");
    if (scene.ranges == 0) return;
    scene.Clear();
    scene.Draw();
    scene.Wait();
    scene.extra.Unregister(0);
    scene.Clear();
    before = Builds();
    scene.Draw();
    Require(scene.Color() == 255 && scene.Stored() == Bits(One), "address draw reuse: a draw after another range was unmapped did not read the guest value");
    Require(Builds() == before + 1, "address draw reuse: a draw reused resources whose BDA table maps an unmapped range");
}

void OversizedVertexBuffer(AgcDriver::VulkanDevice& device) {
    Scene scene(device, 0, 0xffffffffu);
    Pages later{TargetLayout.Bytes()};
    const bool pixelsAbove = scene.pixels.Address() > scene.vertices.Address();
    auto& target = pixelsAbove ? scene.pixels : later;
    if (!pixelsAbove) later.Register(0, TargetLayout.Bytes(), true);
    Require(target.Address() >= scene.vertices.Address() + BlockBytes, "address draw reuse: no render target lies above the vertex buffer");
    scene.state.color.address = target.Address();
    scene.state.colors = {scene.state.color};
    scene.Clear();
    scene.Draw();
    scene.Wait();
    Require(scene.Stored() == Bits(One), "address draw reuse: the draw whose vertex buffer descriptor reaches past its allocation did not store through the BDA table");
    const auto before = Builds();
    scene.Clear();
    scene.Draw();
    scene.Wait();
    Require(scene.Stored() == Bits(One), "address draw reuse: the reused draw whose vertex buffer descriptor reaches past its allocation did not store through the BDA table");
    Require(Builds() == before, "address draw reuse: a draw whose vertex buffer descriptor reaches past its allocation built its resources again");
}

void ThrowingProof(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    if ((context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0) {
        std::puts("address draw reuse: a throwing reuse proof not tested, the device has no 4x color targets");
        return;
    }
    constexpr std::uint32_t format32UInt = 20;
    constexpr std::uint32_t type2D = 9;
    Pages surface{BlockBytes};
    surface.Register(0, BlockBytes, true);
    struct ClearTargets {
        AgcDriver::VulkanDevice& device;
        VkDevice handle;
        ~ClearTargets() {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            device.SubmitRecorded(true);
            device.WaitIdle();
            AgcDriver::Graphics::ClearMultisampleTargets(handle);
            AgcDriver::Graphics::ClearCachedTextures(handle);
        }
    } clearTargets{device, context.device};
    const auto address = surface.Address();
    ShaderRecompiler::DescriptorBinding image{};
    image.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    image.role = ShaderRecompiler::DescriptorRole::GuestImages;
    image.binding = ShaderRecompiler::RuntimeAbi::FirstImageBinding + AgcDriver::Graphics::PlaceholderDimensionsPerClass + 2u;
    image.count = 1;
    image.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    image.imageSamplers = {0u};
    image.guestDescriptor = {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format32UInt << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u), 0xfacu | (type2D << 28u), 0u, 0u, 0u, 0u};
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    fragment.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    fragment.bindings = {
        {ShaderRecompiler::DescriptorKind::StorageBuffer, ShaderRecompiler::DescriptorRole::BdaPagetable, 0, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::BdaPagetable), 1, {}, false},
        {ShaderRecompiler::DescriptorKind::StorageBuffer, ShaderRecompiler::DescriptorRole::FaultBuffer, 0, static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::FaultBuffer), 1, {}, false},
        image,
    };
    const std::array<AgcDriver::Graphics::CompiledShader, 2> stages{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Fragment, &fragment, 0}}};
    AgcDriver::Graphics::ColorTarget target{};
    target.address = address;
    target.surfaceAddress = address;
    target.extent = {Width, Height};
    target.surfaceExtent = target.extent;
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    target.tileMode = ColorTileMode::RenderTarget;
    target.elementBytes = 4;
    target.bytes = BlockBytes / 4;
    target.samples = 4;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    AgcDriver::Graphics::ShaderResources resources(context, stages, AgcDriver::Graphics::ColorTarget{}, 0, 0, 0, {});
    Require(resources.UsesAddressTable() && resources.Reusable(), "address draw reuse: an address-based draw build that samples an image is not reusable");
    resources.EndUse();
    Require(AgcDriver::Graphics::MultisampleTargetView(context, target) != VK_NULL_HANDLE, "address draw reuse: the multisampled target over the sampled image has no view");
    device.SubmitRecorded(true);
    device.WaitIdle();
    bool threw = false;
    bool reused = false;
    const auto counted = AgcDriver::Graphics::AddressReuseCounters();
    try {
        reused = resources.Revalidate(stages, nullptr, {});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(!threw, "address draw reuse: the reuse proof of an address-based draw build whose sampled image's memory became a multisampled target threw instead of refusing the reuse");
    Require(!reused, "address draw reuse: an address-based draw build whose sampled image's memory became a multisampled target was reused");
    const auto refused = AgcDriver::Graphics::AddressReuseCounters();
    Require(refused.proofs == counted.proofs + 1 && Counted(refused, AgcDriver::Graphics::AddressReuse::Threw) == Counted(counted, AgcDriver::Graphics::AddressReuse::Threw) + 1 && Refusals(refused) == Refusals(counted) + 1 && Counted(refused, AgcDriver::Graphics::AddressReuse::Reused) == Counted(counted, AgcDriver::Graphics::AddressReuse::Reused), "address draw reuse: a reuse proof that threw was not counted as one refusal that threw");
    resources.WriteBack();
}

void RegisterShader(ShaderHeader& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(ShaderHeader);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> SubmittedDraw(const std::uint32_t* vertexCode, const std::uint32_t* pixelCode, std::uint64_t color, std::uint64_t pointer, std::uint64_t label) {
    const auto vertex = AddressOf(vertexCode);
    const auto pixel = AddressOf(pixelCode);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000u | (static_cast<std::uint32_t>(ColorTileMode::RenderTarget) << 14u)}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, Bits(32.0f)}, {0x110, Bits(32.0f)},
        {0x111, Bits(-2.0f)}, {0x112, Bits(2.0f)},
        {0x113, Bits(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, Bits(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 0}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 2u << 1u},
        {0x00c, static_cast<std::uint32_t>(pointer)}, {0x00d, static_cast<std::uint32_t>(pointer >> 32u)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {3, 2});
    Append(commands, 0x49, {0x14u | (5u << 8u), 1u << 29u, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32u), 1u, 0u, 0u});
    return commands;
}

void DecodeCacheHit() {
    auto* pixelCode = SubmittedCode.data();
    auto* vertexCode = SubmittedCode.data() + SubmittedCode.size() / 2;
    std::memcpy(pixelCode, PixelCode.data(), sizeof(PixelCode));
    std::memcpy(vertexCode, FullScreenVertexCode.data(), sizeof(FullScreenVertexCode));
    RegisterShader(SubmittedPixelHeader, pixelCode, sizeof(PixelCode), 1);
    RegisterShader(SubmittedVertexHeader, vertexCode, sizeof(FullScreenVertexCode), 2);
    Pages color{BlockBytes};
    color.Register(0, BlockBytes, true);
    Pages guest{BlockBytes};
    guest.Register(0, BlockBytes, true);
    const auto value = guest.Address(ValueOffset);
    std::memcpy(guest.Data() + PointerOffset, &value, sizeof(value));
    Scene::Store(guest, ValueOffset, One);
    constexpr std::size_t labelWord = 8;
    auto commands = SubmittedDraw(vertexCode, pixelCode, color.Address(), guest.Address(), guest.Address(labelWord * sizeof(std::uint32_t)));
    const auto submit = [&] {
        std::memcpy(guest.Data() + StoreOffset, &Sentinel, sizeof(Sentinel));
        std::memset(guest.Data() + labelWord * sizeof(std::uint32_t), 0, sizeof(std::uint32_t));
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "address draw reuse: the submitted draw failed");
        AgcDriverWaitIdle_nid_postfix();
        std::array<std::uint32_t, labelWord + 1> words{};
        AgcDriver::GuestMemory::Read(guest.Address(), std::as_writable_bytes(std::span(words)), 4);
        Require(words[labelWord] == 1u, "address draw reuse: the label after the submitted draw did not land");
        Require(words[StoreOffset / sizeof(std::uint32_t)] == Bits(One), "address draw reuse: the submitted draw did not store through the BDA table");
    };
    auto& driver = AgcDriver::DriverDetail::Driver::Get();
    submit();
    const auto built = Builds();
    submit();
    Require(Builds() == built, "address draw reuse: a second decode miss of a submitted draw built its resources again");
    const auto before = driver.DrawCacheCounters();
    submit();
    const auto after = driver.DrawCacheCounters();
    if (after.hits == before.hits) {
        std::puts("address draw reuse: reuse across a decode-cache hit not tested, the third submitted draw did not hit the decode cache");
        return;
    }
    Require(Builds() == built, "address draw reuse: a submitted draw that hit the decode cache built its resources again after a decode miss of the same draw");
}

}

int main(int argc, char** argv) {
    try {
        const std::size_t ranges = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 512;
        const std::size_t draws = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 64;
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            {
                Scene scene(*device, ranges);
                if (!Imported(scene)) {
                    std::puts("skipped, host imports are unavailable: a store through the BDA table needs one");
                    return VulkanTestSkipped;
                }
                Synthetic(scene, draws);
                ReuseCounters(scene);
                GuestWrite(scene);
                PendingSnapshotWrite(scene);
                Fault(scene);
                Remap(scene);
                scene.Wait();
            }
            OversizedVertexBuffer(*device);
            ThrowingProof(*device);
        }
        DecodeCacheHit();
        AgcDriverShutdown_nid_postfix();
        std::printf("address draw reuse tests passed (%zu extra registered ranges)\n", ranges);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
