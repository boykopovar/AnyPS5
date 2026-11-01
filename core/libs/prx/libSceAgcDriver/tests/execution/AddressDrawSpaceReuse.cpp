#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Recipe.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
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
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
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
using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t VertexCodeOffset = 0;
constexpr std::size_t PixelCodeOffset = 256;
constexpr std::size_t IndirectCodeOffset = 512;
constexpr std::size_t PointerSlots = 1024;
constexpr std::size_t MovedVerticesOffset = 32768;
constexpr std::size_t RingSlotBytes = 256;
constexpr std::size_t RingSlots = 8;
constexpr std::uint32_t PinWaitMs = 4000;

constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 7> PixelCode{
    0x7e000280u,
    0xdc308000u, 0x01000000u,
    0xbf8c3f70u,
    0xf800180fu, 0x01010101u,
    0xbf810000u,
};

constexpr std::array<std::uint32_t, 10> IndirectPixelCode{
    0x7e000280u,
    0xdc348000u, 0x02000000u,
    0xbf8c3f70u,
    0xdc308000u, 0x017d0002u,
    0xbf8c3f70u,
    0xf800180fu, 0x01010101u,
    0xbf810000u,
};

constexpr std::array<std::uint32_t, 12> StoringPixelCode{
    0x7e000280u,
    0xdc308000u, 0x01000000u,
    0xbf8c3f70u,
    0xdc708008u, 0x00000100u,
    0xbf8c3f70u,
    0xf800180fu, 0x01010101u,
    0xbf810000u,
    0xbf810000u,
    0xbf810000u,
};

constexpr std::size_t StoringCodeOffset = 768;
constexpr std::size_t MultisampleTargetOffset = 32768;

alignas(16) constexpr std::array<std::uint32_t, 4> OutsideWords{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};

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
            Require(mapped != MAP_FAILED, "address draw space reuse: cannot map guest pages");
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
        Require(data != nullptr, "address draw space reuse: cannot allocate guest pages");
        std::memset(data, 0, bytes);
    }

    ~Pages() {
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), bytes, nullptr, "test release");
        if (registered) GuestAllocations::Mutation().Remove(data);
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

    void Register(bool writable) {
        Register(writable, bytes);
    }

    void Register(bool writable, std::size_t length) {
        GuestAllocations::Mutation().Add(data, length, true, writable, true);
        registered = true;
    }

    void Unregister() {
        GuestAllocations::Mutation().Remove(data);
        registered = false;
    }

    std::uint64_t Address(std::size_t offset = 0) const { return AddressOf(data + offset); }
    std::uint8_t* Data() { return data; }

private:
    std::size_t bytes;
    bool watched = AgcDriver::GuestMemory::WriteWatched();
    std::uint8_t* data = nullptr;
    bool registered = false;
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

class Unmapped {
public:
    Unmapped() {
#ifdef _WIN32
        data = VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE, PAGE_NOACCESS);
#else
        data = mmap(nullptr, BlockBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (data == MAP_FAILED) data = nullptr;
#endif
        Require(data != nullptr, "address draw space reuse: cannot reserve an inaccessible range");
    }

    ~Unmapped() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, BlockBytes);
#endif
    }

    Unmapped(const Unmapped&) = delete;
    Unmapped& operator=(const Unmapped&) = delete;

    std::uint64_t Address(std::size_t offset) const { return AddressOf(static_cast<const std::uint8_t*>(data) + offset); }

private:
    void* data = nullptr;
};

std::size_t Occurrences(const std::string& text, const std::string& what) {
    std::size_t count = 0;
    for (auto at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size())) ++count;
    return count;
}

std::string FaultAt(std::uint64_t address) {
    char text[64];
    std::snprintf(text, sizeof(text), "address=0x%llx ", static_cast<unsigned long long>(address));
    return text;
}

class Outcomes {
public:
    std::uint64_t operator[](AgcDriver::Graphics::AddressDrawReuse outcome) const {
        return AgcDriver::Graphics::AddressDrawReuseCounters()[static_cast<std::size_t>(outcome)] - before[static_cast<std::size_t>(outcome)];
    }

    std::uint64_t Total() const {
        const auto now = AgcDriver::Graphics::AddressDrawReuseCounters();
        std::uint64_t total = 0;
        for (std::size_t index = 0; index < now.size(); ++index) total += now[index] - before[index];
        return total;
    }

    std::string Text() const {
        const auto now = AgcDriver::Graphics::AddressDrawReuseCounters();
        std::string text;
        for (std::size_t index = 0; index < now.size(); ++index) {
            if (now[index] == before[index]) continue;
            text += (text.empty() ? "" : ", ") + std::string(AgcDriver::Graphics::AddressDrawReuseName(static_cast<AgcDriver::Graphics::AddressDrawReuse>(index))) + " " + std::to_string(now[index] - before[index]);
        }
        return text.empty() ? "nothing" : text;
    }

private:
    AgcDriver::Graphics::AddressDrawReuseCounts before = AgcDriver::Graphics::AddressDrawReuseCounters();
};

std::uint64_t Builds() {
    const auto counters = AgcDriver::Graphics::BdaResources::TableCacheCounters();
    return counters.hits + counters.misses - AgcDriver::Graphics::AddressDrawReuseVerifications();
}

float Level(std::uint8_t value) {
    return static_cast<float>(value) / 255.0f;
}

struct Scene {
    AgcDriver::VulkanDevice& device;
    Pages pixels{TargetLayout.Bytes()};
    Pages vertices{BlockBytes};
    Pages code{BlockBytes};
    Pages constants{BlockBytes};
    Pages spare{BlockBytes};
    Pages ring{BlockBytes};
    bool cachedVertex = false;
    std::deque<ShaderRecompiler::RecompileResult> vertexResults;
    std::uint32_t vertexPush = 0;
    std::deque<ShaderRecompiler::RecompileResult> pixelResults;
    std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{};
    std::vector<AgcDriver::Graphics::GuestMemorySnapshot> snapshots;
    AgcDriver::Graphics::State state{};

    explicit Scene(AgcDriver::VulkanDevice& device) : device(device) {
        pixels.Register(true);
        std::memcpy(vertices.Data(), Triangle.data(), sizeof(Triangle));
        std::memcpy(vertices.Data() + MovedVerticesOffset, Triangle.data(), sizeof(Triangle));
        vertices.Register(false);
        std::memcpy(code.Data() + VertexCodeOffset, VertexCode.data(), sizeof(VertexCode));
        std::memcpy(code.Data() + PixelCodeOffset, PixelCode.data(), sizeof(PixelCode));
        std::memcpy(code.Data() + IndirectCodeOffset, IndirectPixelCode.data(), sizeof(IndirectPixelCode));
        std::memcpy(code.Data() + StoringCodeOffset, StoringPixelCode.data(), sizeof(StoringPixelCode));
        code.Register(false);
        constants.Register(true);
        spare.Register(true);
        ring.Register(true);
        Fetch(0);
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
        Point(constants.Address());
    }

    void Fetch(std::size_t offset) {
        Fetch(vertices, offset);
    }

    void Fetch(Pages& source, std::size_t offset) {
        const auto address = source.Address(offset);
        const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>((BlockBytes - offset) / 16u), 0x01016facu};
        const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{code.Address(VertexCodeOffset), std::as_bytes(std::span(VertexCode))}}};
        ShaderRecompiler::RecompileRequest vertex{
            {ShaderStage::Vertex, code.Address(VertexCodeOffset), VertexCode, 0, {}},
            {64, 0, userData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
            device.Target(),
            {0, 0, 0, 64}
        };
        vertex.useCache = cachedVertex;
        vertexResults.push_back(ShaderRecompiler::Recompile(vertex));
        if (vertexResults.size() > 8) vertexResults.pop_front();
        vertexPush = static_cast<std::uint32_t>(vertexResults.back().pushConstants.size());
        shaders[0] = {ShaderStage::Vertex, &vertexResults.back(), 0};
    }

    void Point(std::uint64_t slot, std::span<const std::uint32_t> program = PixelCode, std::size_t offset = PixelCodeOffset) {
        const auto target = device.Target();
        ShaderRecompiler::ShaderPixelStageInfo pixel{};
        pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
        pixel.posX = true;
        pixel.posY = true;
        pixel.targetOutputMode[0] = 9;
        pixel.targetExportMapping.fill(0xe4u);
        const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(slot >> 32u), 0u, 0u};
        const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{code.Address(offset), std::as_bytes(program)}}};
        ShaderRecompiler::RecompileRequest fragment{
            {ShaderStage::Fragment, code.Address(offset), program, 0, {}},
            {64, 0, userData, std::nullopt, pixel, std::nullopt, pixelMemory},
            target,
            PixelPushLayout(vertexPush, target)
        };
        fragment.useCache = true;
        pixelResults.push_back(ShaderRecompiler::Recompile(fragment));
        if (pixelResults.size() > 8) pixelResults.pop_front();
        const auto& pixelResult = pixelResults.back();
        Require(std::any_of(pixelResult.bindings.begin(), pixelResult.bindings.end(), [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; }), "address draw space reuse: the pixel shader does not read guest memory through the BDA table");
        shaders[1] = {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)};
        snapshots = {
            {code.Address(VertexCodeOffset), std::as_bytes(std::span(reinterpret_cast<const std::uint32_t*>(code.Data() + VertexCodeOffset), VertexCode.size()))},
            {code.Address(offset), std::as_bytes(std::span(reinterpret_cast<const std::uint32_t*>(code.Data() + offset), program.size()))},
        };
    }

    void PointThrough(std::size_t slot, std::uint64_t address) {
        std::memcpy(constants.Data() + slot, &address, sizeof(address));
        Point(constants.Address(slot), IndirectPixelCode, IndirectCodeOffset);
    }

    void Store(Pages& pages, std::size_t offset, std::uint8_t value) {
        const auto level = Level(value);
        std::memcpy(pages.Data() + offset, &level, sizeof(level));
    }

    void Draw(std::shared_ptr<const AgcDriver::DrawRecipe>* recipe = nullptr) {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
        device.Draw(state, draw, shaders, snapshots, recipe);
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
                for (std::size_t channel = 0; channel < 4; ++channel) Require(texels[offset + channel] == first, "address draw space reuse: the full-screen draw left texels of different values");
            }
        }
        return first;
    }

    std::uint8_t DrawnColor() {
        Draw();
        return Color();
    }
};

bool Imported(Scene& scene) {
    scene.Store(scene.constants, 0, 255);
    const auto color = scene.DrawnColor();
    if (AgcDriver::Graphics::HostImportLiveBytes() == 0) return false;
    Require(color == 255, "address draw space reuse: the first draw did not read the guest constant through the BDA table (" + std::to_string(color) + ")");
    return true;
}

void UnchangedInputs(Scene& scene) {
    scene.Store(scene.constants, 0, 200);
    Require(scene.DrawnColor() == 200, "address draw space reuse: the draw before the repeat did not read the guest constant");
    const auto before = Builds();
    const auto color = scene.DrawnColor();
    Require(color == 200, "address draw space reuse: the repeated draw read " + std::to_string(color) + " instead of the guest constant 200");
    Require(Builds() == before, "address draw space reuse: a read-only address-based draw repeated with unchanged inputs over an imported allocation built its resources again (" + std::to_string(Builds() - before) + " builds)");
}

void CpuWrite(Scene& scene) {
    scene.Store(scene.constants, 0, 120);
    Require(scene.DrawnColor() == 120, "address draw space reuse: the draw before the CPU write did not read the guest constant");
    scene.Store(scene.constants, 0, 40);
    const auto before = Builds();
    const auto color = scene.DrawnColor();
    Require(color == 40, "address draw space reuse: a draw after a CPU write into its constant read " + std::to_string(color) + " instead of the written 40");
    Require(Builds() == before, "address draw space reuse: a CPU write into memory the draw reads in place made it build its resources again (" + std::to_string(Builds() - before) + " builds)");
}

void IdleFree(Scene& scene) {
    scene.Store(scene.constants, 0, 90);
    Require(scene.DrawnColor() == 90, "address draw space reuse: the draw before the free did not read the guest constant");
    const auto before = Builds();
    Require(scene.DrawnColor() == 90 && Builds() == before, "address draw space reuse: the draw before the free was not served by the cached build");
    const auto start = std::chrono::steady_clock::now();
    try {
        scene.spare.Unregister();
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("address draw space reuse: a free inside the address space of an idle cached build failed: ") + error.what());
    }
    const auto waited = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    Require(waited < PinWaitMs / 4.0, "address draw space reuse: a free inside the address space of an idle cached build waited " + std::to_string(waited) + " ms");
    const auto rebuilt = Builds();
    Require(scene.DrawnColor() == 90, "address draw space reuse: the draw after the free did not read the guest constant");
    Require(Builds() == rebuilt + 1, "address draw space reuse: the draw after a free inside its address space did not build its resources again");
    scene.spare.Register(true);
}

void NoRecipe(Scene& scene) {
    scene.Store(scene.constants, 0, 150);
    std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
    scene.Draw(&recipe);
    Require(scene.Color() == 150, "address draw space reuse: the draw asked for a recipe did not read the guest constant");
    const auto before = Builds();
    scene.Draw(&recipe);
    Require(scene.Color() == 150 && Builds() == before, "address draw space reuse: the repeated draw asked for a recipe was not served by the cached build");
    Require(recipe == nullptr, "address draw space reuse: a draw recipe was made from a build whose reuse must retake its address space and check its snapshots");
}

void MovedVertices(Scene& scene) {
    scene.Store(scene.constants, 0, 180);
    Require(scene.DrawnColor() == 180, "address draw space reuse: the draw before the vertex buffer moved did not read the guest constant");
    scene.Fetch(MovedVerticesOffset);
    const auto color = scene.DrawnColor();
    Require(color == 180, "address draw space reuse: the draw after its vertex buffer moved read " + std::to_string(color) + " instead of 180");
    const auto start = std::chrono::steady_clock::now();
    try {
        scene.spare.Unregister();
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("address draw space reuse: a free after a draw whose vertex buffer moved failed: ") + error.what());
    }
    const auto waited = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    Require(waited < PinWaitMs / 4.0, "address draw space reuse: a free after a draw whose vertex buffer moved waited " + std::to_string(waited) + " ms");
    scene.spare.Register(true);
    scene.Fetch(0);
    Require(scene.DrawnColor() == 180, "address draw space reuse: the draw after the vertex buffer moved back did not read the guest constant");
}

void FaultPerUse(Scene& scene) {
    const Unmapped unmapped;
    const auto good = scene.constants.Address();
    scene.Store(scene.constants, 0, 210);
    std::size_t slot = PointerSlots;
    scene.PointThrough(slot, good);
    Require(scene.DrawnColor() == 210, "address draw space reuse: the draw before the faulting uses did not read the guest constant through its pointer");
    const auto batch = [&](std::uint64_t first, std::uint64_t second) {
        const auto before = Builds();
        StderrCapture log;
        try {
            scene.PointThrough(slot += 16, first);
            scene.Draw();
            scene.PointThrough(slot += 16, second);
            scene.Draw();
            scene.Wait();
        } catch (...) {
            static_cast<void>(log.Finish());
            throw;
        }
        const auto text = log.Finish();
        Require(Builds() == before, "address draw space reuse: two draws of one build in a batch built their resources again");
        return text;
    };
    const auto once = [](const std::string& text, std::span<const std::uint64_t> faults, const char* what) {
        for (const auto address : faults) Require(Occurrences(text, FaultAt(address)) == 1, std::string("address draw space reuse: ") + what + ": the fault at " + FaultAt(address) + "was not reported once:\n" + text);
        Require(Occurrences(text, "BDA access failed") == faults.size(), std::string("address draw space reuse: ") + what + ": " + std::to_string(Occurrences(text, "BDA access failed")) + " faults were reported for " + std::to_string(faults.size()) + " faulting uses:\n" + text);
    };
    const std::array<std::uint64_t, 4> faults{unmapped.Address(0x100), unmapped.Address(0x200), unmapped.Address(0x300), unmapped.Address(0x400)};
    once(batch(faults[0], good), std::span(faults).subspan(0, 1), "a faulting use before a clean one");
    once(batch(good, faults[1]), std::span(faults).subspan(1, 1), "a clean use before a faulting one");
    once(batch(faults[2], faults[3]), std::span(faults).subspan(2, 2), "two faulting uses");
    once(batch(good, good), {}, "two clean uses after the faults");
    scene.PointThrough(slot += 16, good);
    Require(scene.DrawnColor() == 210, "address draw space reuse: the draw after the faulting uses did not read the guest constant through its pointer");
    scene.Point(good);
}

void Counters(Scene& scene) {
    using AgcDriver::Graphics::AddressDrawReuse;
    const auto good = scene.constants.Address();
    scene.Store(scene.constants, 0, 60);
    scene.Point(good);
    Require(scene.DrawnColor() == 60, "address draw space reuse: the draw before the counted draws did not read the guest constant");
    const Outcomes all;
    std::uint64_t draws = 0;
    const auto counted = [&](AddressDrawReuse outcome, std::size_t repeats, const char* what) {
        const Outcomes step;
        for (std::size_t draw = 0; draw < repeats; ++draw) {
            scene.Draw();
            scene.Wait();
            ++draws;
        }
        Require(step[outcome] == repeats && step.Total() == repeats, std::string("address draw space reuse: ") + what + " counted " + step.Text() + " for " + std::to_string(repeats) + " draws, expected " + AgcDriver::Graphics::AddressDrawReuseName(outcome) + " " + std::to_string(repeats));
    };
    const auto settle = [&] {
        for (int draw = 0; draw < 2; ++draw) {
            scene.Draw();
            scene.Wait();
            ++draws;
        }
    };
    counted(AddressDrawReuse::Reused, 2, "draws with unchanged inputs");
    {
        Pages mapped{BlockBytes};
        mapped.Register(false);
        counted(AddressDrawReuse::SpaceChanged, 1, "a draw after an allocation was mapped");
        counted(AddressDrawReuse::Reused, 1, "the draw after it");
    }
    settle();
    const auto plain = scene.snapshots;
    scene.snapshots.push_back({AddressOf(OutsideWords.data()), std::as_bytes(std::span(OutsideWords))});
    counted(AddressDrawReuse::SnapshotOutside, 1, "a draw with a snapshot outside the address space");
    counted(AddressDrawReuse::NotSpaceOnly, 2, "draws whose build copies that snapshot");
    scene.snapshots = plain;
    counted(AddressDrawReuse::Built, 1, "the draw without it");
    Pages straddle{2 * BlockBytes};
    straddle.Register(false, BlockBytes);
    settle();
    const std::array<std::uint32_t, 4> across{};
    std::memcpy(straddle.Data() + BlockBytes - 8, across.data(), sizeof(across));
    scene.snapshots.push_back({straddle.Address(BlockBytes - 8), std::as_bytes(std::span(across))});
    counted(AddressDrawReuse::PartialOverlap, 1, "a draw with a snapshot across the end of an allocation");
    scene.snapshots = plain;
    settle();
    std::array<std::uint32_t, PixelCode.size()> changed = PixelCode;
    changed.back() ^= 1u;
    scene.snapshots.back() = {scene.code.Address(PixelCodeOffset), std::as_bytes(std::span(changed))};
    counted(AddressDrawReuse::SnapshotChanged, 2, "draws with a snapshot that differs from guest memory");
    scene.snapshots = plain;
    settle();
    scene.Point(good, StoringPixelCode, StoringCodeOffset);
    counted(AddressDrawReuse::NotReadOnly, 2, "draws whose pixel shader stores through addresses");
    scene.Point(good);
    settle();
    Require(all.Total() == draws, "address draw space reuse: " + std::to_string(draws) + " address-based draws counted " + std::to_string(all.Total()) + " outcomes (" + all.Text() + ")");
    Require(scene.DrawnColor() == 60, "address draw space reuse: the draw after the counted draws did not read the guest constant");
}

void PendingSnapshotWrite(Scene& scene) {
    using AgcDriver::Graphics::AddressDrawReuse;
    constexpr std::size_t Slot = PointerSlots + 512;
    constexpr std::size_t Cleared = 64;
    scene.Store(scene.constants, 0, 230);
    scene.Store(scene.constants, Cleared, 0);
    const auto value = scene.constants.Address();
    const std::array<std::uint64_t, 2> captured{value, 0};
    scene.PointThrough(Slot, value);
    const auto plain = scene.snapshots;
    scene.snapshots.push_back({scene.constants.Address(Slot), std::as_bytes(std::span(captured))});
    Require(scene.DrawnColor() == 230 && scene.DrawnColor() == 230, "address draw space reuse: the draws with a captured pointer did not read the guest constant");
    const auto cleared = scene.constants.Address(Cleared);
    const std::array<std::uint32_t, 4> pattern{static_cast<std::uint32_t>(cleared), static_cast<std::uint32_t>(cleared >> 32u), 0u, 0u};
    const Outcomes outcomes;
    const auto built = Builds();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(scene.device.FillBuffer(scene.constants.Address(Slot), 16, pattern), "address draw space reuse: the fill over the captured pointer was not recorded on the GPU");
        const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
        scene.device.Draw(scene.state, draw, scene.shaders, scene.snapshots);
    }
    const auto color = scene.Color();
    Require(color == 0, "address draw space reuse: a draw after a recorded fill rewrote its captured pointer read " + std::to_string(color) + " instead of the new pointer's 0");
    Require(outcomes[AddressDrawReuse::SnapshotChanged] == 1 && outcomes.Total() == 1 && Builds() == built + 1, "address draw space reuse: a draw whose captured snapshot a pending GPU write changed counted " + outcomes.Text() + " and " + std::to_string(Builds() - built) + " builds, expected one refusal for a changed snapshot and a build");
    scene.snapshots = plain;
    scene.Point(value);
}

struct Ring {
    Scene& scene;
    std::size_t next = 0;
    std::vector<AgcDriver::Graphics::GuestMemorySnapshot> plain;

    explicit Ring(Scene& scene) : scene(scene), plain(scene.snapshots) {}
    Ring(const Ring&) = delete;
    Ring& operator=(const Ring&) = delete;

    ~Ring() {
        scene.snapshots = plain;
        scene.Point(scene.constants.Address());
    }

    static std::size_t Slot(std::size_t index) { return (index % RingSlots) * RingSlotBytes; }

    void Write(std::size_t index, std::uint8_t value) {
        scene.Store(scene.ring, Slot(index), value);
    }

    void Point(std::size_t index) {
        scene.Point(scene.ring.Address(Slot(index)));
        scene.snapshots = plain;
        scene.snapshots.push_back({scene.ring.Address(Slot(index)), std::as_bytes(std::span(scene.ring.Data() + Slot(index), 16))});
    }

    std::uint8_t Draw(std::uint8_t value) {
        const auto index = next++;
        Write(index, value);
        Point(index);
        return scene.DrawnColor();
    }
};

void RingSameConstants(Scene& scene) {
    using AgcDriver::Graphics::AddressDrawReuse;
    Ring ring(scene);
    Require(ring.Draw(140) == 140, "address draw space reuse: the first draw of the ring did not read its slot");
    const Outcomes outcomes;
    const auto built = Builds();
    for (std::size_t draw = 1; draw < 2 * RingSlots; ++draw) {
        const auto color = ring.Draw(140);
        Require(color == 140, "address draw space reuse: ring draw " + std::to_string(draw) + " read " + std::to_string(color) + " instead of its slot's 140");
    }
    Require(Builds() == built && outcomes[AddressDrawReuse::Reused] == 2 * RingSlots - 1 && outcomes.Total() == 2 * RingSlots - 1, "address draw space reuse: draws of the same constants in successive ring slots counted " + outcomes.Text() + " and " + std::to_string(Builds() - built) + " builds, expected every one reused");
}

void RingOwnConstants(Scene& scene) {
    using AgcDriver::Graphics::AddressDrawReuse;
    Ring ring(scene);
    Require(ring.Draw(10) == 10, "address draw space reuse: the first draw of the ring did not read its slot");
    const Outcomes outcomes;
    const auto built = Builds();
    for (std::size_t draw = 1; draw < 2 * RingSlots; ++draw) {
        const auto value = static_cast<std::uint8_t>(10 + 15 * draw);
        const auto color = ring.Draw(value);
        Require(color == value, "address draw space reuse: ring draw " + std::to_string(draw) + " read " + std::to_string(color) + " instead of its own " + std::to_string(value));
    }
    Require(Builds() == built && outcomes[AddressDrawReuse::Reused] == 2 * RingSlots - 1 && outcomes.Total() == 2 * RingSlots - 1, "address draw space reuse: draws of different constants in successive ring slots counted " + outcomes.Text() + " and " + std::to_string(Builds() - built) + " builds, expected every one reused");
}

void RingOldSlotWrite(Scene& scene) {
    using AgcDriver::Graphics::AddressDrawReuse;
    Ring ring(scene);
    Require(ring.Draw(50) == 50, "address draw space reuse: the first draw of the ring did not read its slot");
    const Outcomes outcomes;
    for (std::size_t draw = 1; draw < RingSlots; ++draw) {
        const auto value = static_cast<std::uint8_t>(50 + 20 * draw);
        const auto index = ring.next++;
        ring.Write(index, value);
        ring.Point(index);
        scene.Draw();
        ring.Write(index - 1, 255);
        const auto color = scene.Color();
        Require(color == value, "address draw space reuse: ring draw " + std::to_string(draw) + " read " + std::to_string(color) + " after a CPU write to the previous slot, instead of its own " + std::to_string(value));
    }
    Require(outcomes[AddressDrawReuse::Reused] == RingSlots - 1 && outcomes.Total() == RingSlots - 1, "address draw space reuse: ring draws followed by a CPU write to the previous slot counted " + outcomes.Text() + ", expected every one reused");
}

void RingRefusals(Scene& scene) {
    using AgcDriver::Graphics::AddressDrawReuse;
    {
        Ring ring(scene);
        Require(ring.Draw(90) == 90, "address draw space reuse: the first draw of the ring did not read its slot");
        const Outcomes outcomes;
        const auto index = ring.next++;
        ring.Write(index, 90);
        ring.Point(index);
        scene.snapshots.back() = {AddressOf(OutsideWords.data()), std::as_bytes(std::span(OutsideWords))};
        Require(scene.DrawnColor() == 90, "address draw space reuse: the draw whose moved snapshot lies outside the space did not read its slot");
        Require(outcomes[AddressDrawReuse::SnapshotOutside] == 1 && outcomes.Total() == 1, "address draw space reuse: a ring draw whose moved snapshot lies outside the space counted " + outcomes.Text() + ", expected one refusal for a snapshot outside the space");
    }
    scene.Store(scene.constants, 0, 170);
    scene.Point(scene.constants.Address());
    scene.cachedVertex = true;
    scene.Fetch(0);
    for (int warm = 0; warm < 2; ++warm) Require(scene.DrawnColor() == 170, "address draw space reuse: the draw before the vertex buffer moved did not read the guest constant");
    {
        const Outcomes outcomes;
        scene.Fetch(MovedVerticesOffset);
        const auto color = scene.DrawnColor();
        Require(color == 170, "address draw space reuse: the draw after its read-only vertex buffer moved read " + std::to_string(color) + " instead of 170");
        Require(outcomes[AddressDrawReuse::Reused] == 1 && outcomes.Total() == 1, "address draw space reuse: a draw whose read-only vertex buffer moved inside the space counted " + outcomes.Text() + ", expected one reuse with the buffer bound anew");
    }
    {
        std::memcpy(scene.spare.Data(), Triangle.data(), sizeof(Triangle));
        constexpr std::size_t Filled = 64;
        const std::array<std::uint32_t, 4> pattern{};
        const Outcomes outcomes;
        scene.Fetch(scene.spare, 0);
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            Require(scene.device.FillBuffer(scene.spare.Address(Filled), 16, pattern), "address draw space reuse: the fill inside the moved vertex buffer was not recorded on the GPU");
            const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
            scene.device.Draw(scene.state, draw, scene.shaders, scene.snapshots);
        }
        const auto color = scene.Color();
        Require(color == 170, "address draw space reuse: the draw whose moved vertex buffer a pending fill overlaps read " + std::to_string(color) + " instead of 170");
        Require(outcomes[AddressDrawReuse::MovedBuffer] == 1 && outcomes.Total() == 1, "address draw space reuse: a draw whose moved read-only vertex buffer a pending GPU write overlaps counted " + outcomes.Text() + ", expected one refusal for a moved GuestBuffers element");
    }
    scene.Fetch(0);
    scene.cachedVertex = false;
}

bool MultisampleTargetsAvailable(Scene& scene) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(scene.device);
    if (AgcDriver::GuestMemory::WriteWatched() && (context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0) return true;
    Require(std::getenv("ANYPS5_REQUIRE_VULKAN") == nullptr, "address draw space reuse: the multisample target cases require write tracking and 4x color targets");
    std::puts("skipped, the multisample target cases require write tracking and 4x color targets");
    return false;
}

void PlaceMultisampleTarget(Scene& scene, std::uint64_t address) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(scene.device);
    AgcDriver::Graphics::ColorTarget target{};
    target.address = address;
    target.surfaceAddress = address;
    target.extent = {Width, Height};
    target.surfaceExtent = target.extent;
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    target.tileMode = ColorTileMode::RenderTarget;
    target.elementBytes = 4;
    target.bytes = TargetLayout.Bytes();
    target.samples = 4;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Require(AgcDriver::Graphics::MultisampleTargetView(context, target) != VK_NULL_HANDLE, "address draw space reuse: the multisample target has no view");
    scene.device.SubmitRecorded(true);
    scene.device.WaitIdle();
}

void RetireMultisampleTargets(Scene& scene) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(scene.device);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    scene.device.WaitIdle();
    AgcDriver::Graphics::ClearMultisampleTargets(context.device);
}

void MultisampleTargetInside(Scene& scene) {
    using AgcDriver::Graphics::AddressDrawReuse;
    if (!MultisampleTargetsAvailable(scene)) return;
    scene.Store(scene.constants, 0, 130);
    Require(scene.DrawnColor() == 130 && scene.DrawnColor() == 130, "address draw space reuse: the draws before the multisample target did not read the guest constant");
    const Outcomes outcomes;
    const auto built = Builds();
    PlaceMultisampleTarget(scene, scene.constants.Address(MultisampleTargetOffset));
    std::uint8_t color = 0;
    try {
        color = scene.DrawnColor();
    } catch (...) {
        RetireMultisampleTargets(scene);
        throw;
    }
    RetireMultisampleTargets(scene);
    Require(outcomes[AddressDrawReuse::SpaceChanged] == 1 && outcomes.Total() == 1 && Builds() == built + 1, "address draw space reuse: a draw after a resident multisample target appeared inside its address space counted " + outcomes.Text() + " and " + std::to_string(Builds() - built) + " builds, expected one refusal for a space change and a build");
    Require(color == 130, "address draw space reuse: the build after a resident multisample target appeared inside its address space read " + std::to_string(color) + " instead of 130");
}

void MultisampleTargetOutside(Scene& scene) {
    using AgcDriver::Graphics::AddressDrawReuse;
    if (!MultisampleTargetsAvailable(scene)) return;
    scene.Store(scene.constants, 0, 160);
    Require(scene.DrawnColor() == 160 && scene.DrawnColor() == 160, "address draw space reuse: the draws before the multisample target did not read the guest constant");
    Pages outside{BlockBytes};
    const Outcomes outcomes;
    const auto built = Builds();
    PlaceMultisampleTarget(scene, outside.Address());
    std::uint8_t color = 0;
    try {
        color = scene.DrawnColor();
    } catch (...) {
        RetireMultisampleTargets(scene);
        throw;
    }
    RetireMultisampleTargets(scene);
    Require(color == 160, "address draw space reuse: the draw after a resident multisample target appeared outside its space read " + std::to_string(color) + " instead of 160");
    Require(outcomes[AddressDrawReuse::Reused] == 1 && outcomes.Total() == 1 && Builds() == built, "address draw space reuse: a draw after a resident multisample target appeared outside its space counted " + outcomes.Text() + " and " + std::to_string(Builds() - built) + " builds, expected one reuse");
}

void Disabled(Scene& scene) {
    scene.Store(scene.constants, 0, 70);
    Require(scene.DrawnColor() == 70, "address draw space reuse: the first draw with reuse off did not read the guest constant");
    const auto before = Builds();
    Require(scene.DrawnColor() == 70, "address draw space reuse: the repeated draw with reuse off did not read the guest constant");
    Require(Builds() == before + 1, "address draw space reuse: with APS5_NO_ADDRESS_DRAW_REUSE=1 a repeated address-based draw did not build its resources again");
}

void LimitPinWait() {
    if (std::getenv("APS5_PIN_WAIT_MS") != nullptr) return;
    const auto wait = std::to_string(PinWaitMs);
#ifdef _WIN32
    _putenv_s("APS5_PIN_WAIT_MS", wait.c_str());
#else
    setenv("APS5_PIN_WAIT_MS", wait.c_str(), 1);
#endif
}

using Case = void (*)(Scene&);

struct NamedCase {
    const char* name;
    Case run;
};

constexpr std::array<NamedCase, 14> DefaultCases{{
    {"unchanged", UnchangedInputs},
    {"cpu-write", CpuWrite},
    {"idle-free", IdleFree},
    {"no-recipe", NoRecipe},
    {"moved-vertices", MovedVertices},
    {"fault-per-use", FaultPerUse},
    {"counters", Counters},
    {"pending-snapshot-write", PendingSnapshotWrite},
    {"ring-same-constants", RingSameConstants},
    {"ring-own-constants", RingOwnConstants},
    {"ring-old-slot-write", RingOldSlotWrite},
    {"ring-refusals", RingRefusals},
    {"multisample-target-inside", MultisampleTargetInside},
    {"multisample-target-outside", MultisampleTargetOutside},
}};

constexpr std::array<NamedCase, 3> TransitionCases{{
    {"unchanged", UnchangedInputs},
    {"idle-free", IdleFree},
    {"moved-vertices", MovedVertices},
}};

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        const std::string only = mode == "--case" && argc > 2 ? argv[2] : "";
        Require(mode.empty() || mode == "--off" || mode == "--transitions" || mode == "--verify" || !only.empty(), "usage: [--off | --transitions | --verify | --case <name>]");
        Require(mode != "--verify" || std::getenv("APS5_VERIFY_ADDRESS_DRAW_REUSE") != nullptr, "address draw space reuse: --verify needs APS5_VERIFY_ADDRESS_DRAW_REUSE=1");
        LimitPinWait();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        {
            Scene scene(*device);
            if (!Imported(scene)) {
                std::puts("skipped, host imports are unavailable: the cached address space needs one");
                return VulkanTestSkipped;
            }
            const Outcomes outcomes;
            const auto verifiedBefore = AgcDriver::Graphics::AddressDrawReuseVerifications();
            if (mode == "--off") {
                Disabled(scene);
            } else {
                const std::span<const NamedCase> cases = mode == "--transitions" ? std::span<const NamedCase>(TransitionCases) : std::span<const NamedCase>(DefaultCases);
                bool ran = false;
                for (const auto& entry : cases) {
                    if (!only.empty() && only != entry.name) continue;
                    entry.run(scene);
                    ran = true;
                }
                Require(ran, "address draw space reuse: no case is named " + only);
            }
            if (mode == "--verify") {
                const auto verified = AgcDriver::Graphics::AddressDrawReuseVerifications() - verifiedBefore;
                const auto reused = outcomes[AgcDriver::Graphics::AddressDrawReuse::Reused];
                Require(reused != 0 && verified == reused, "address draw space reuse: APS5_VERIFY_ADDRESS_DRAW_REUSE verified " + std::to_string(verified) + " of " + std::to_string(reused) + " served hits");
                std::puts(("address draw space reuse: APS5_VERIFY_ADDRESS_DRAW_REUSE verified " + std::to_string(verified) + " of " + std::to_string(reused) + " served hits").c_str());
            }
            scene.Wait();
        }
        std::printf("address draw space reuse tests passed%s%s\n", mode.empty() ? "" : " ", only.empty() ? mode.c_str() : only.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
