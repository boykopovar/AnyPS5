#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Recipe.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
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
constexpr std::size_t CapturedOffset = 1024;
constexpr std::size_t RecordsOffset = 2048;
constexpr std::size_t MovedVerticesOffset = 32768;
constexpr std::uint32_t NoLocation = 0x280u;
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
        GuestAllocations::Mutation().Add(data, bytes, true, writable, true);
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

std::uint64_t Builds() {
    const auto counters = AgcDriver::Graphics::BdaResources::TableCacheCounters();
    return counters.hits + counters.misses;
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
    Pages alias{BlockBytes};
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
        code.Register(false);
        constants.Register(true);
        spare.Register(true);
        alias.Register(true);
        Fetch(vertices, 0);
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
        vertex.useCache = true;
        vertexResults.push_back(ShaderRecompiler::Recompile(vertex));
        if (vertexResults.size() > 8) vertexResults.pop_front();
        vertexPush = static_cast<std::uint32_t>(vertexResults.back().pushConstants.size());
        shaders[0] = {ShaderStage::Vertex, &vertexResults.back(), 0};
    }

    void Point(std::uint64_t slot) {
        const auto target = device.Target();
        ShaderRecompiler::ShaderPixelStageInfo pixel{};
        pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
        pixel.posX = true;
        pixel.posY = true;
        pixel.targetOutputMode[0] = 9;
        pixel.targetExportMapping.fill(0xe4u);
        const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(slot >> 32u), 0u, 0u};
        const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{code.Address(PixelCodeOffset), std::as_bytes(std::span(PixelCode))}}};
        ShaderRecompiler::RecompileRequest fragment{
            {ShaderStage::Fragment, code.Address(PixelCodeOffset), PixelCode, 0, {}},
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
            {code.Address(PixelCodeOffset), std::as_bytes(std::span(reinterpret_cast<const std::uint32_t*>(code.Data() + PixelCodeOffset), PixelCode.size()))},
        };
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

    void DrawWithoutRecords() {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Pm4::DrawParameters draw{0, 0, 0, 0, 0, false, 0, 0};
        draw.indirect = AgcDriver::Pm4::DrawParameters::IndirectDraw{constants.Address(RecordsOffset), 0x24u, 16u, 16u, 0u, false, 0u, NoLocation, NoLocation, NoLocation, false, 0u};
        device.Draw(state, draw, shaders, snapshots);
    }

    AgcDriver::RecipeOutcome DrawFromRecipe(const std::shared_ptr<const AgcDriver::DrawRecipe>& recipe) {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Triangle.size()), 0, 1, 0, false};
        return device.DrawFromRecipe(state, draw, shaders, snapshots, recipe);
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

    void RequireFreeAtOnce(const std::string& after) {
        Wait();
        const auto start = std::chrono::steady_clock::now();
        try {
            spare.Unregister();
        } catch (const std::exception& error) {
            throw std::runtime_error("address draw space reuse: a free inside the address space " + after + " failed: " + error.what());
        }
        const auto waited = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        spare.Register(true);
        Require(waited < PinWaitMs / 4.0, "address draw space reuse: a free inside the address space " + after + " waited " + std::to_string(waited) + " ms");
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
    scene.RequireFreeAtOnce("while the cached build is idle");
    const auto rebuilt = Builds();
    Require(scene.DrawnColor() == 90, "address draw space reuse: the draw after the free did not read the guest constant");
    Require(Builds() == rebuilt + 1, "address draw space reuse: the draw after a free inside its address space did not build its resources again");
}

void RecipeAfterSnapshotChange(Scene& scene) {
    scene.Store(scene.constants, 0, 150);
    const auto plain = scene.snapshots;
    std::array<std::byte, 16> captured{};
    std::memcpy(captured.data(), scene.constants.Data() + CapturedOffset, captured.size());
    scene.snapshots.push_back({scene.constants.Address(CapturedOffset), captured});
    std::shared_ptr<const AgcDriver::DrawRecipe> recipe;
    scene.Draw(&recipe);
    Require(scene.Color() == 150, "address draw space reuse: the draw asked for a recipe did not read the guest constant");
    scene.constants.Data()[CapturedOffset] ^= 0xffu;
    const auto before = Builds();
    if (recipe != nullptr) {
        const bool recorded = scene.DrawFromRecipe(recipe) == AgcDriver::RecipeOutcome::Recorded;
        const auto color = scene.Color();
        Require(!recorded, "address draw space reuse: a draw recipe made from a build whose reuse retakes its address space recorded the draw (read " + std::to_string(color) + ") after a snapshot inside that space changed, with " + std::to_string(Builds() - before) + " builds, where the draw path builds anew");
    }
    const auto color = scene.DrawnColor();
    const auto built = Builds() - before;
    scene.constants.Data()[CapturedOffset] ^= 0xffu;
    scene.snapshots = plain;
    Require(color == 150 && built == 1, "address draw space reuse: the draw after a snapshot inside the address space changed read " + std::to_string(color) + " with " + std::to_string(built) + " builds, expected 150 and a build");
    Require(recipe == nullptr, "address draw space reuse: a draw recipe was made from a build whose reuse must retake its address space and check its snapshots");
}

void ThrowingHit(Scene& scene) {
    scene.Store(scene.constants, 0, 110);
    for (int warm = 0; warm < 2; ++warm) Require(scene.DrawnColor() == 110, "address draw space reuse: the draw before the aliasing draw did not read the guest constant");
    const auto color = scene.state.color;
    scene.state.color.address = scene.alias.Address();
    scene.state.colors = {scene.state.color};
    scene.Fetch(scene.alias, 0);
    const auto before = Builds();
    std::string refused;
    try {
        scene.Draw();
    } catch (const std::exception& error) {
        refused = error.what();
    }
    const auto built = Builds() - before;
    scene.state.color = color;
    scene.state.colors = {color};
    scene.Fetch(scene.vertices, 0);
    Require(!refused.empty(), "address draw space reuse: a draw whose vertex buffer aliases its render target was recorded");
    Require(built == 0, "address draw space reuse: the draw whose vertex buffer aliases its render target built its resources (" + std::to_string(built) + " builds) instead of hitting the cached build");
    scene.RequireFreeAtOnce("after a hit that threw before it was served (" + refused + ")");
    Require(scene.DrawnColor() == 110, "address draw space reuse: the draw after the refused aliasing draw did not read the guest constant");
}

void MovedVertices(Scene& scene) {
    scene.Store(scene.constants, 0, 170);
    for (int warm = 0; warm < 2; ++warm) Require(scene.DrawnColor() == 170, "address draw space reuse: the draw before the vertex buffer moved did not read the guest constant");
    const auto before = Builds();
    scene.Fetch(scene.vertices, MovedVerticesOffset);
    const auto color = scene.DrawnColor();
    scene.Fetch(scene.vertices, 0);
    Require(color == 170, "address draw space reuse: the draw after its read-only vertex buffer moved read " + std::to_string(color) + " instead of 170");
    const bool transitions = std::getenv("APS5_DRAW_TRANSITIONS") != nullptr;
    Require(transitions || Builds() == before, "address draw space reuse: the draw whose read-only vertex buffer moved inside the space built its resources (" + std::to_string(Builds() - before) + " builds) instead of binding the moved buffer to the cached build");
    scene.RequireFreeAtOnce(transitions ? "after the transition path built a draw whose vertex buffer moved anew" : "after a draw whose vertex buffer moved inside it");
}

void NoRecords(Scene& scene) {
    scene.Store(scene.constants, 0, 60);
    for (int warm = 0; warm < 2; ++warm) Require(scene.DrawnColor() == 60, "address draw space reuse: the draw before the indirect draw did not read the guest constant");
    const auto before = Builds();
    scene.DrawWithoutRecords();
    Require(Builds() == before, "address draw space reuse: the indirect draw without records built its resources (" + std::to_string(Builds() - before) + " builds) instead of hitting the cached build");
    scene.RequireFreeAtOnce("after an indirect draw without records ended after its resource-cache hit");
    Require(scene.DrawnColor() == 60, "address draw space reuse: the draw after the indirect draw did not read the guest constant");
}

void Disabled(Scene& scene) {
    scene.Store(scene.constants, 0, 70);
    Require(scene.DrawnColor() == 70, "address draw space reuse: the first draw with the reuse off did not read the guest constant");
    const auto before = Builds();
    Require(scene.DrawnColor() == 70, "address draw space reuse: the repeated draw with the reuse off did not read the guest constant");
    Require(Builds() == before + 1, "address draw space reuse: without APS5_GPU_HEAP a repeated address-based draw did not build its resources again");
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

constexpr std::array<NamedCase, 7> Cases{{
    {"unchanged", UnchangedInputs},
    {"cpu-write", CpuWrite},
    {"idle-free", IdleFree},
    {"recipe-snapshot", RecipeAfterSnapshotChange},
    {"throwing-hit", ThrowingHit},
    {"moved-vertices", MovedVertices},
    {"no-records", NoRecords},
}};

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        const std::string only = mode == "--case" && argc > 2 ? argv[2] : "";
        Require(mode.empty() || !only.empty(), "usage: [--case <name>]");
        LimitPinWait();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool reuse = !AgcDriver::Graphics::ShaderResources::SpaceReuseDisabled();
        {
            Scene scene(*device);
            if (!Imported(scene)) {
                std::puts("skipped, host imports are unavailable: the cached address space needs one");
                return VulkanTestSkipped;
            }
            try {
                if (!reuse) {
                    Require(only.empty(), "address draw space reuse: --case needs APS5_GPU_HEAP set");
                    Disabled(scene);
                } else {
                    bool ran = false;
                    for (const auto& entry : Cases) {
                        if (!only.empty() && only != entry.name) continue;
                        entry.run(scene);
                        ran = true;
                    }
                    Require(ran, "address draw space reuse: no case is named " + only);
                }
                scene.Wait();
            } catch (const std::exception& error) {
                std::cerr << error.what() << std::endl;
                std::_Exit(1);
            }
        }
        std::printf("address draw space reuse tests passed%s%s\n", reuse ? "" : " with the reuse off", only.empty() ? "" : (" " + only).c_str());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
