#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "Cover_vert_spv.h"
#include "Cover_frag_spv.h"
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
#include <atomic>
#include <chrono>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

constexpr int VulkanTestSkipped = 77;
constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 32;
constexpr std::uint32_t LeftWidth = 16;
constexpr std::uint32_t Old = 0x5a5a5a5au;
constexpr std::uint32_t New = 0x12345678u;
constexpr std::uint32_t Clear = 0x11223344u;
constexpr std::uint32_t Drawn = 0xffffffffu;
constexpr VkRect2D Left{{0, 0}, {LeftWidth, Height}};
constexpr VkRect2D Right{{Width - LeftWidth, 0}, {LeftWidth, Height}};
constexpr VkRect2D Whole{{0, 0}, {Width, Height}};
constexpr std::size_t PlaneBytes = 65536;
constexpr std::size_t HtileBytes = ((Width + 1023u) / 1024u) * ((Height + 511u) / 512u) * 32768u;

alignas(4096) std::array<std::byte, 2 * PlaneBytes> SurfaceBlock{};
alignas(4096) std::array<std::byte, 2 * PlaneBytes> DepthBlock{};
const std::span<std::byte, PlaneBytes> SurfaceStorage{reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(SurfaceBlock.data()) + PlaneBytes - 1u) & ~std::uintptr_t{PlaneBytes - 1u}), PlaneBytes};
const std::span<std::byte, PlaneBytes> DepthStorage{reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(DepthBlock.data()) + PlaneBytes - 1u) & ~std::uintptr_t{PlaneBytes - 1u}), PlaneBytes};
alignas(4096) std::array<std::uint8_t, 4096> Keys{};
alignas(4096) std::array<std::uint8_t, 4096> Cmask{};
alignas(4096) std::array<std::uint32_t, HtileBytes / 4u> Htile{};

alignas(4096) std::array<std::byte, 3 * PlaneBytes> OuterBlock{};
const std::span<std::byte, 2 * PlaneBytes> OuterStorage{reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(OuterBlock.data()) + PlaneBytes - 1u) & ~std::uintptr_t{PlaneBytes - 1u}), 2 * PlaneBytes};
alignas(4096) std::array<std::byte, 2 * PlaneBytes> SourceBlock{};
alignas(4096) std::array<std::byte, 4 * PlaneBytes> MultisampleBlock{};
const std::span<std::byte, PlaneBytes> SourceStorage{reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(SourceBlock.data()) + PlaneBytes - 1u) & ~std::uintptr_t{PlaneBytes - 1u}), PlaneBytes};
const std::span<std::byte, 2 * PlaneBytes> MultisampleStorage{reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(MultisampleBlock.data()) + PlaneBytes - 1u) & ~std::uintptr_t{PlaneBytes - 1u}), 2 * PlaneBytes};

std::uint64_t address(const void* data) {
    return reinterpret_cast<std::uintptr_t>(data);
}

ColorTarget colorTarget(bool dcc = false, bool cmask = false, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM) {
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 4);
    ColorTarget color{};
    color.address = address(SurfaceStorage.data());
    color.extent = {Width, Height};
    color.format = format;
    color.bytes = layout.Bytes();
    color.componentMapping = 0xe4u;
    color.tileMode = ColorTileMode::RenderTarget;
    color.elementBytes = 4;
    color.clearWords = {Clear, 0};
    color.surfaceAddress = color.address;
    color.surfaceExtent = color.extent;
    if (dcc) color.dccAddress = address(Keys.data());
    if (cmask) {
        color.cmaskAddress = address(Cmask.data());
        color.cmaskBytes = CmaskLayout(Width, Height).Bytes();
    }
    return color;
}

DepthTarget depthTarget(bool mirrored) {
    DepthTarget depth{address(DepthStorage.data()), 0, {Width, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, mirrored ? 0u : address(Htile.data()), false};
    depth.depthSwizzle = mirrored ? XorSwizzleMode(TextureTileMode::kZ64KBX) : 0u;
    return depth;
}

struct DrawOptions {
    VkRect2D scissor = Left;
    std::optional<DepthTarget> depth;
    bool depthWrite = false;
    VkCompareOp compare = VK_COMPARE_OP_LESS;
    float depthValue = 0.5f;
    std::uint32_t colorWriteMask = 15;
    bool prepareOnly = false;
    bool changePadding = false;
};

void setDescriptorPadding(ColorTarget& color, DepthTarget* depth, std::uint8_t padding) {
    constexpr auto colorBegin = offsetof(ColorTarget, componentMapping) + sizeof(ColorTarget::componentMapping);
    constexpr auto colorEnd = offsetof(ColorTarget, tileMode);
    constexpr auto depthBegin = offsetof(DepthTarget, clearStencil) + sizeof(DepthTarget::clearStencil);
    constexpr auto depthEnd = offsetof(DepthTarget, htileAddress);
    static_assert(colorBegin < colorEnd && depthBegin < depthEnd);
    std::memset(reinterpret_cast<std::byte*>(&color) + colorBegin, padding, colorEnd - colorBegin);
    if (depth != nullptr) std::memset(reinterpret_cast<std::byte*>(depth) + depthBegin, padding, depthEnd - depthBegin);
}

State drawState(const ColorTarget& color, const DrawOptions& options) {
    State state{};
    state.stages = {ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = color;
    state.colors = {color};
    state.hasColorTarget = true;
    state.depth = options.depth;
    if (options.changePadding) {
        static std::uint8_t padding = 0;
        ++padding;
        setDescriptorPadding(state.colors.front(), state.depth ? &*state.depth : nullptr, padding);
    }
    state.depthTest = options.depth.has_value();
    state.depthWrite = options.depthWrite;
    state.depthCompare = options.compare;
    state.renderExtent = {Width, Height};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, 0, static_cast<float>(Width), static_cast<float>(Height), options.depthValue, options.depthValue};
    state.negativeOneToOne = false;
    state.samples = color.samples;
    state.scissor = options.scissor;
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = options.colorWriteMask;
    state.blends = {state.blend};
    state.blendConstants = {};
    return state;
}

void draw(AgcDriver::VulkanDevice& device, const ColorTarget& color, const DrawOptions& options = {}) {
    ShaderRecompiler::RecompileResult vertex;
    vertex.spirv = std::vector<std::uint32_t>(std::begin(COVER_VERT_SPV), std::end(COVER_VERT_SPV));
    ShaderRecompiler::RecompileResult fragment;
    fragment.spirv = std::vector<std::uint32_t>(std::begin(COVER_FRAG_SPV), std::end(COVER_FRAG_SPV));
    const std::array<CompiledShader, 2> shaders{{
        {ShaderRecompiler::ShaderStage::Vertex, &vertex, 0},
        {ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}
    }};
    const auto state = drawState(color, options);
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    if (options.prepareOnly) SetDrawStopAfterResolveForTest(DrawStopForTest::ReturnAfterResolve);
    device.Draw(state, {0, 3, 0, 1, 0, false}, shaders);
}

bool memoResolves() {
    static const bool resolves = std::getenv("APS5_NO_TARGET_MEMO") != nullptr || std::getenv("APS5_VERIFY_TARGET_MEMO") != nullptr || std::getenv("APS5_DRAW_PIPELINE") == nullptr;
    return resolves;
}

std::uint64_t lookups() {
    const auto& counts = ThreadLookupOutcomes().counts;
    return std::accumulate(counts.begin(), counts.end(), std::uint64_t{0});
}

bool memoServes() {
    static const bool serves = std::getenv("APS5_NO_TARGET_MEMO") == nullptr && std::getenv("APS5_NO_TEXTURE_CACHE") == nullptr && std::getenv("APS5_DRAW_PIPELINE") != nullptr;
    return serves;
}

bool memoCounted() {
    static const bool counted = std::getenv("APS5_PROFILE_TARGET_MEMO") != nullptr || std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return counted;
}

struct MemoEvents {
    std::array<std::uint64_t, static_cast<std::size_t>(TargetMemoEvent::Count)> counts{};
    MemoEvents() {
        for (std::size_t index = 0; index < counts.size(); ++index) counts[index] = TargetMemoEvents(static_cast<TargetMemoEvent>(index));
    }
    std::uint64_t Moved(TargetMemoEvent event) const { return TargetMemoEvents(event) - counts[static_cast<std::size_t>(event)]; }
    std::string Text() const {
        std::string text;
        for (std::size_t index = 0; index < counts.size(); ++index) {
            const auto event = static_cast<TargetMemoEvent>(index);
            if (Moved(event) != 0) text += std::string(text.empty() ? "" : ", ") + TargetMemoEventName(event) + " " + std::to_string(Moved(event));
        }
        return text.empty() ? "none" : text;
    }
};

void requireMemoEvent(const MemoEvents& before, TargetMemoEvent event, std::uint64_t expected, const std::string& what) {
    Require(before.Moved(event) == expected, what + ": " + TargetMemoEventName(event) + " counted " + std::to_string(before.Moved(event)) + ", expected " + std::to_string(expected) + " (" + before.Text() + ")");
}

void arm(AgcDriver::VulkanDevice& device, const ColorTarget& color, const DrawOptions& options = {}) {
    for (int repeat = 0; repeat < 3; ++repeat) draw(device, color, options);
    const auto before = lookups();
    const MemoEvents events;
    draw(device, color, options);
    Require(!LookupOutcomes::Profiled() || (lookups() != before) == memoResolves(), memoResolves() ? "a draw without the memo did not look up its targets" : "a draw that repeats the previous draw's targets looked them up");
    const auto served = memoServes() && memoCounted() ? 1u : 0u;
    requireMemoEvent(events, TargetMemoEvent::Hit, served, "a repeated draw's target memo count");
    requireMemoEvent(events, TargetMemoEvent::Accept, served, "a repeated draw's target memo count");
    requireMemoEvent(events, TargetMemoEvent::VerifyChecked, std::getenv("APS5_VERIFY_TARGET_MEMO") != nullptr ? served : 0u, "a repeated draw's target memo count");
}

struct ProofFields {
    std::uint64_t epoch = AgcDriver::GuestMemory::CollectEpoch();
    std::uint64_t proofs = AgcDriver::GuestMemory::ProofSerial();
    std::uint64_t pending = StorageTexture::PendingSerial();
    std::uint64_t unwatch = AgcDriver::GuestMemory::UnwatchSerial();
    std::uint64_t allocations = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    std::uint64_t surfaces = DepthSurfacesSerial();
    std::uint64_t released = DepthSurfacesReleased();
};

std::string moved(const ProofFields& before) {
    const ProofFields now;
    std::string names;
    const auto note = [&](bool changed, const char* name) {
        if (changed) names += std::string(names.empty() ? "" : " ") + name;
    };
    note(now.epoch != before.epoch, "epoch");
    note(now.proofs != before.proofs, "proofs");
    note(now.pending != before.pending, "pending");
    note(now.unwatch != before.unwatch, "unwatch");
    note(now.allocations != before.allocations, "allocations");
    note(now.surfaces != before.surfaces, "surfaces");
    note(now.released != before.released, "released");
    return names;
}

void requireMoved(const ProofFields& before, const std::string& expected, const std::string& what) {
    const auto names = moved(before);
    Require(names == expected, what + " moved [" + names + "], expected [" + expected + "]");
}

void mustRepeat(AgcDriver::VulkanDevice& device, const ColorTarget& color, const std::string& what) {
    const auto before = lookups();
    draw(device, color);
    Require(!LookupOutcomes::Profiled() || (lookups() != before) == memoResolves(), what);
}

void mustLookUp(AgcDriver::VulkanDevice& device, const ColorTarget& color, const DrawOptions& options, const std::string& what) {
    const auto before = lookups();
    draw(device, color, options);
    Require(!LookupOutcomes::Profiled() || lookups() != before, what);
}

void fillSurface(std::uint32_t value) {
    const auto* end = SurfaceStorage.data() + SurfaceStorage.size();
    for (auto* at = SurfaceStorage.data(); at != end; at += 4) std::memcpy(at, &value, 4);
}

void reset(AgcDriver::VulkanDevice& device, const ColorTarget& color) {
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, PlaneBytes, nullptr, "test");
        device.WaitIdle();
    }
    std::vector<std::byte> texels(PlaneBytes);
    for (std::size_t offset = 0; offset < texels.size(); offset += 4) std::memcpy(texels.data() + offset, &Old, 4);
    AgcDriver::GuestMemory::Write(color.address, texels);
    if (color.dccAddress != 0) AgcDriver::GuestMemory::Write(color.dccAddress, std::vector<std::byte>(Keys.size(), std::byte{0xff}));
    if (color.cmaskAddress != 0) AgcDriver::GuestMemory::Write(color.cmaskAddress, std::vector<std::byte>(Cmask.size(), std::byte{0xff}));
    AgcDriver::GuestMemory::BumpCollectEpoch();
}

void expectTexels(AgcDriver::VulkanDevice& device, const ColorTarget& color, const std::function<std::uint32_t(std::uint32_t, std::uint32_t)>& texel, const std::string& what) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
    device.WaitIdle();
    const ColorTargetLayout layout(Width, Height, color.tileMode, 4);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            std::uint32_t stored = 0;
            std::memcpy(&stored, SurfaceStorage.data() + layout.Offset(x, y), 4);
            Require(stored == texel(x, y), what + ": texel (" + std::to_string(x) + ", " + std::to_string(y) + ") holds " + std::to_string(stored));
        }
    }
}

void expect(AgcDriver::VulkanDevice& device, const ColorTarget& color, const std::function<std::uint32_t(std::uint32_t)>& texel, const std::string& what) {
    expectTexels(device, color, [&](std::uint32_t x, std::uint32_t) { return texel(x); }, what);
}

std::uint32_t leftOver(std::uint32_t x, std::uint32_t other) {
    return x < LeftWidth ? Drawn : other;
}

void repeatedDraws(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Old); }, "repeated draws");
    reset(device, color);
    arm(device, color);
    draw(device, color, {.scissor = Right});
    expect(device, color, [](std::uint32_t x) { return x < LeftWidth || x >= Width - LeftWidth ? Drawn : Old; }, "a repeated target with another scissor lost a draw");
}

void cpuWrite(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    fillSurface(New);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    draw(device, color);
    expect(device, color, [](std::uint32_t x) { return leftOver(x, New); }, "a CPU write between repeated draws");
}

void driverStore(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
        std::vector<std::byte> texels(PlaneBytes);
        for (std::size_t offset = 0; offset < texels.size(); offset += 4) std::memcpy(texels.data() + offset, &New, 4);
        AgcDriver::GuestMemory::Write(color.address, texels);
    }
    draw(device, color);
    expect(device, color, [](std::uint32_t x) { return leftOver(x, New); }, "a driver store between repeated draws");
}

void driverStoreOverPendingResults(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    AgcDriver::GuestMemory::Write(color.address, std::vector<std::byte>(4, std::byte{0x12}));
    mustLookUp(device, color, {}, "a draw after a driver store repeated its targets");
}

void resultsFlushed(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
    }
    mustLookUp(device, color, {}, "a draw after the target's results were flushed repeated its targets");
}

void storedStamp(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
        device.WaitIdle();
    }
    arm(device, color);
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
        device.WaitIdle();
    }
    AgcDriver::GuestMemory::BumpCollectEpoch();
    arm(device, color);
    const ProofFields before;
    fillSurface(New);
    AgcDriver::GuestMemory::CollectWrites(color.address, PlaneBytes);
    AgcDriver::GuestMemory::MarkWritten(color.address, PlaneBytes);
    requireMoved(before, "proofs", "a GPU store stamp");
    mustLookUp(device, color, {}, "a draw after a GPU store stamp repeated its targets");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, New); }, "a GPU store stamp between repeated draws");
}

void pendingSerialOnly(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    const ProofFields before;
    StorageTexture::BumpPendingSerial();
    requireMoved(before, "pending", "a change of pending results");
    mustLookUp(device, color, {}, "a draw after a change of pending results repeated its targets");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Old); }, "a change of pending results between repeated draws");
}

struct BesideTargets {
    ColorTarget inner;
    ColorTarget outer;
};

BesideTargets besideTargets() {
    auto inner = colorTarget();
    inner.address = address(OuterStorage.data()) + PlaneBytes;
    inner.surfaceAddress = inner.address;
    auto outer = colorTarget();
    outer.address = address(OuterStorage.data());
    outer.surfaceAddress = outer.address;
    outer.extent = {4 * Width, 4 * Height};
    outer.surfaceExtent = outer.extent;
    outer.bytes = ColorTargetLayout(outer.extent.width, outer.extent.height, ColorTileMode::RenderTarget, 4).Bytes();
    Require(outer.bytes == OuterStorage.size(), "the outer image does not cover both blocks");
    return {inner, outer};
}

void armBesidePending(AgcDriver::VulkanDevice& device, const BesideTargets& targets) {
    reset(device, targets.inner);
    reset(device, targets.outer);
    draw(device, targets.outer);
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(targets.inner.address, PlaneBytes, nullptr, "test");
        device.WaitIdle();
    }
    arm(device, targets.inner);
}

void flushBeside(AgcDriver::VulkanDevice& device, const BesideTargets& targets) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushPending(targets.outer.address, OuterStorage.size(), nullptr, "test");
    device.WaitIdle();
}

void otherResultsOutsideTarget(AgcDriver::VulkanDevice& device) {
    const auto targets = besideTargets();
    armBesidePending(device, targets);
    expect(device, targets.inner, [](std::uint32_t x) { return leftOver(x, Old); }, "a target beside another image's pending results");
    flushBeside(device, targets);
}

void otherResultsGrowIntoTarget(AgcDriver::VulkanDevice& device) {
    const auto targets = besideTargets();
    armBesidePending(device, targets);
    const auto outer = StorageTexture::FindPending(targets.outer.address, OuterStorage.size());
    Require(outer != nullptr, "the outer image has no results pending beside the target");
    const ProofFields before;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        outer->MarkDirty();
    }
    requireMoved(before, "pending", "another image's results growing into the target");
    mustLookUp(device, targets.inner, {}, "a draw after another image's results grew into its target repeated its targets");
    flushBeside(device, targets);
}

void labelStoredBeforeHit(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    const ProofFields before;
    std::thread presenter([] { AgcDriverStoreGuestLabel_nid_postfix(reinterpret_cast<std::uint64_t*>(SourceStorage.data()), 1); });
    presenter.join();
    requireMoved(before, "proofs", "a label stored on another thread");
    mustLookUp(device, color, {}, "a draw after another thread's label store repeated its targets");
    mustRepeat(device, color, "a draw after the lookup that took another thread's label store did not repeat");
}

void labelStoredOverTarget(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    std::thread presenter([&] {
        const auto word = (std::uint64_t{New} << 32u) | New;
        for (std::size_t offset = 0; offset < PlaneBytes; offset += 8) AgcDriverStoreGuestLabel_nid_postfix(reinterpret_cast<std::uint64_t*>(color.address + offset), word);
    });
    presenter.join();
    mustLookUp(device, color, {}, "a draw after another thread's label stores over its target repeated its targets");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, New); }, "another thread's label stores over a repeated target");
}

void labelStoredOnAnotherThread(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    const auto before = lookups();
    constexpr int draws = 4000;
    {
        std::atomic<bool> stop{false};
        std::atomic<int> drawn{0};
        std::thread presenter([&] {
            auto* label = reinterpret_cast<std::uint64_t*>(SourceStorage.data());
            int stored = 0;
            for (std::uint64_t value = 1; !stop.load(std::memory_order_relaxed); ++value) {
                while (drawn.load(std::memory_order_relaxed) < stored + 3 && !stop.load(std::memory_order_relaxed)) {}
                stored = drawn.load(std::memory_order_relaxed);
                const auto next = std::chrono::steady_clock::now() + std::chrono::microseconds(value % 64u);
                while (std::chrono::steady_clock::now() < next) {}
                AgcDriverStoreGuestLabel_nid_postfix(label, value);
            }
        });
        struct Join {
            std::atomic<bool>& stop;
            std::thread& thread;
            ~Join() {
                stop.store(true, std::memory_order_relaxed);
                thread.join();
            }
        } join{stop, presenter};
        for (int repeat = 0; repeat < draws; ++repeat) {
            draw(device, color);
            drawn.fetch_add(1, std::memory_order_relaxed);
        }
    }
    Require(!LookupOutcomes::Profiled() || memoResolves() || lookups() - before < draws, "no draw repeated while labels were stored on another thread");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Old); }, "a label stored on another thread between repeated draws");
}

void storeThroughAlias(AgcDriver::VulkanDevice& device, const ColorTarget& color) {
    alignas(256) static constexpr std::array<std::uint32_t, 7> code{0x7e040300u, 0x7e060280u, 0x7e0202ffu, New, 0xf0200108u, 0x00020102u, 0xbf810000u};
    constexpr std::uint32_t format32Uint = 20u;
    constexpr std::uint32_t tileRenderTarget = 0x1bu;
    constexpr std::uint32_t type2D = 9u;
    std::vector<std::uint32_t> userData(16, 0u);
    userData[8] = static_cast<std::uint32_t>(color.address >> 8u);
    userData[9] = static_cast<std::uint32_t>((color.address >> 40u) & 0xffu) | (format32Uint << 20u) | (((Width - 1u) & 3u) << 30u);
    userData[10] = ((Width - 1u) >> 2u) | ((Height - 1u) << 14u);
    userData[11] = 0xfacu | (tileRenderTarget << 20u) | (type2D << 28u);
    const std::span<const std::uint32_t> kernel(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{address(kernel.data()), std::as_bytes(kernel)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Width, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, address(kernel.data()), kernel, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, address(kernel.data()));
}

void computeThroughAlias(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    const ProofFields before;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        storeThroughAlias(device, color);
    }
    Require(!moved(before).empty(), "a compute write through an alias of the target moved no proof field");
    std::cout << "compute write moved [" << moved(before) << "]\n";
    mustLookUp(device, color, {}, "a draw after a compute write repeated its targets");
    expectTexels(device, color, [](std::uint32_t x, std::uint32_t y) { return x < LeftWidth ? Drawn : y == 0u ? New : Old; }, "a compute write through an alias between identical draws");
}

void copyBetween(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    for (std::size_t offset = 0; offset < PlaneBytes; offset += 4) std::memcpy(SourceStorage.data() + offset, &New, 4);
    AgcDriver::GuestMemory::CollectWrites(address(SourceStorage.data()), PlaneBytes);
    const ProofFields before;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const auto outcome = device.CopyBuffer(color.address, address(SourceStorage.data()), PlaneBytes, 0, std::numeric_limits<std::size_t>::max(), 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
        std::cout << "copy took path " << outcome.path << " and moved [" << moved(before) << "]\n";
    }
    Require(!moved(before).empty(), "a copy over the target moved no proof field");
    mustLookUp(device, color, {}, "a draw after a copy repeated its targets");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, New); }, "a copy between identical draws");
}

void resolveBetween(AgcDriver::VulkanDevice& device) {
    auto multisampled = colorTarget();
    multisampled.address = address(MultisampleStorage.data());
    multisampled.surfaceAddress = multisampled.address;
    multisampled.samples = 4;
    multisampled.coverageSamples = 4;
    const auto color = colorTarget();
    reset(device, color);
    // Draw the whole multisampled target: with a scissor starting at x = 48, llvmpipe leaves two of the four
    // samples of texel 47 drawn, and the scissor edge is not what this test is about.
    draw(device, multisampled, {.scissor = Whole});
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        device.WaitIdle();
    }
    arm(device, color);
    const ProofFields before;
    ColorMetadataPass pass{};
    pass.mode = ColorMetadataPass::Mode::Resolve;
    pass.source = multisampled;
    pass.targets = {color};
    device.ColorMetadataPass(pass);
    const auto fields = moved(before);
    std::cout << "resolve moved [" << fields << "]\n";
    if (fields.empty()) mustRepeat(device, color, "a draw after a resolve into its own image did not repeat");
    else mustLookUp(device, color, {}, "a draw after a resolve repeated its targets");
    expect(device, color, [](std::uint32_t) { return Drawn; }, "a resolve between identical draws");
}

void committerMoved(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    const auto epoch = AgcDriver::GuestMemory::CollectEpoch();
    const ProofFields before;
    std::thread other([&] {
        AgcDriver::GuestMemory::MarkProofThread();
        AgcDriver::GuestMemory::BumpCollectEpoch();
        draw(device, color, {.scissor = Right});
    });
    other.join();
    Require(AgcDriver::GuestMemory::CollectEpoch() == epoch, "another committer moved this thread's epoch");
    std::cout << "another committer moved [" << moved(before) << "]\n";
    mustLookUp(device, color, {}, "a draw after another thread's commit repeated its targets");
    expect(device, color, [](std::uint32_t x) { return x < LeftWidth || x >= Width - LeftWidth ? Drawn : Old; }, "another thread's commit between identical draws");
}

void depthDrawnByAnotherCommitter(AgcDriver::VulkanDevice& device) {
    auto other = colorTarget();
    other.address = address(SourceStorage.data());
    other.surfaceAddress = other.address;
    auto depth = depthTarget(true);
    depth.address = address(SurfaceStorage.data());
    const auto color = colorTarget();
    reset(device, color);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    draw(device, other, {.depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS});
    arm(device, color);
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
        device.WaitIdle();
    }
    arm(device, color);
    const ProofFields before;
    std::thread committer([&] {
        AgcDriver::GuestMemory::MarkProofThread();
        AgcDriver::GuestMemory::BumpCollectEpoch();
        draw(device, other, {.depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS, .depthValue = 0.75f});
    });
    committer.join();
    std::cout << "a depth draw on another committer moved [" << moved(before) << "]\n";
    mustLookUp(device, color, {.scissor = Right}, "a draw after another thread's depth draw repeated its targets");
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
        device.WaitIdle();
    }
    float stored = 0.0f;
    std::memcpy(&stored, SurfaceStorage.data(), 4);
    Require(stored == 0.75f, "a depth plane drawn on another thread did not reach the target over it: " + std::to_string(stored));
}

void remap(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    GuestAllocations::Mutation().Remove(SurfaceStorage.data());
    fillSurface(New);
    GuestAllocations::Mutation().Add(SurfaceStorage.data(), SurfaceStorage.size(), true, true, true);
    draw(device, color);
    expect(device, color, [](std::uint32_t x) { return leftOver(x, New); }, "a remap between repeated draws");
}

void gpuWriteUnderAlias(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    const auto alias = colorTarget(false, false, VK_FORMAT_B8G8R8A8_UNORM);
    reset(device, color);
    arm(device, color);
    mustLookUp(device, alias, {.scissor = Right}, "a draw through an alias repeated the previous format's targets");
    arm(device, alias, {.scissor = Right});
    mustLookUp(device, color, {}, "a draw back through the first format repeated the alias's targets");
    expect(device, color, [](std::uint32_t x) { return x < LeftWidth || x >= Width - LeftWidth ? Drawn : Old; }, "a draw through an alias between repeated draws");
}


void cmaskClear(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget(false, true);
    reset(device, color);
    for (int repeat = 0; repeat < 3; ++repeat) draw(device, color);
    std::fill(Cmask.begin(), Cmask.end(), std::uint8_t{0});
    AgcDriver::GuestMemory::BumpCollectEpoch();
    draw(device, color);
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Clear); }, "a CMASK cleared between repeated draws");
}

std::byte* allocateWatched(std::size_t bytes) {
    constexpr std::size_t block = 65536;
#ifdef _WIN32
    void* memory = GuestArena::GuestArenaAllocate_nid_postfix(bytes, block);
    GuestArena::GuestArenaCommit_nid_postfix(memory, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(raw != MAP_FAILED, "cannot map a watched block");
    void* memory = reinterpret_cast<void*>((reinterpret_cast<std::uintptr_t>(raw) + block - 1) & ~static_cast<std::uintptr_t>(block - 1));
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, bytes);
#endif
    GuestAllocations::Mutation().Add(memory, bytes, true, true, true);
    return static_cast<std::byte*>(memory);
}

struct HitCase {
    const char* name;
    bool metadata;
    bool depth;
};

void memoHitPath(AgcDriver::VulkanDevice& device) {
    static const char* const benchRepeats = std::getenv("APS5_BENCH_MEMO_HIT");
    const bool bench = benchRepeats != nullptr && benchRepeats[0] != '\0';
    const std::size_t repeats = bench ? std::strtoull(benchRepeats, nullptr, 10) : 1000u;
    const std::size_t samples = bench ? 8u : 2u;
    const bool disabled = !DrawPipelineEnabled() || std::getenv("APS5_NO_TARGET_MEMO") != nullptr || std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    for (const HitCase hitCase : {HitCase{"color", false, false}, HitCase{"color+depth", false, true}, HitCase{"dcc+depth", true, true}}) {
        auto color = colorTarget(hitCase.metadata);
        if (hitCase.metadata) {
            auto* keys = allocateWatched(Keys.size());
            std::memset(keys, 0xff, Keys.size());
            color.dccAddress = address(keys);
        }
        DrawOptions options;
        if (hitCase.depth) options.depth = depthTarget(true);
        reset(device, color);
        arm(device, color, options);
        const auto state = drawState(color, options);
        std::array<std::vector<double>, 2> costs;
        for (std::size_t sample = 0; sample < samples * 2u; ++sample) {
            const std::size_t arm = (sample % 4u == 1u || sample % 4u == 2u) ? 1u : 0u;
            RuntimeSwitches::Assign(RuntimeSwitch::NoTargetMemoOverlapCache, arm == 1u);
            std::size_t hits = 0;
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto nanoseconds = TargetMemoHitNanosecondsForTest(context, state, repeats, hits);
            Require(hits == (disabled ? 0u : repeats), std::string("the memo hit path of ") + hitCase.name + " hit " + std::to_string(hits) + " of " + std::to_string(repeats));
            costs[arm].push_back(static_cast<double>(nanoseconds) / static_cast<double>(repeats));
        }
        RuntimeSwitches::Assign(RuntimeSwitch::NoTargetMemoOverlapCache, false);
        if (!bench) continue;
        for (std::size_t arm = 0; arm < 2; ++arm) {
            auto& sorted = costs[arm];
            std::ranges::sort(sorted);
            std::printf("bench memo-hit %-12s %-18s ns/draw: median %.1f  Q1 %.1f  Q3 %.1f  min %.1f  max %.1f  (%zu samples of %zu)\n", hitCase.name, arm == 0 ? "cached overlap" : "computed overlap", sorted[sorted.size() / 2], sorted[sorted.size() / 4], sorted[sorted.size() * 3 / 4], sorted.front(), sorted.back(), sorted.size(), repeats);
        }
    }
}

void dccClear(AgcDriver::VulkanDevice& device) {
    auto color = colorTarget(true);
    auto* keys = allocateWatched(Keys.size());
    color.dccAddress = address(keys);
    reset(device, color);
    arm(device, color);
    std::fill(keys, keys + Keys.size(), std::byte{0x20});
    AgcDriver::GuestMemory::BumpCollectEpoch();
    mustLookUp(device, color, {}, "a draw after DCC keys were cleared repeated its targets");
}

void cmaskClearRecorded(AgcDriver::VulkanDevice& device) {
    auto color = colorTarget(false, true);
    auto* cmask = allocateWatched(65536);
    color.cmaskAddress = address(cmask);
    std::memset(cmask, 0xff, color.cmaskBytes);
    reset(device, color);
    for (int repeat = 0; repeat < 3; ++repeat) draw(device, color);
    std::memset(cmask, 0, color.cmaskBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    bool repeated = false;
    for (int repeat = 0; repeat < 64 && !repeated; ++repeat) {
        const auto before = lookups();
        draw(device, color);
        repeated = lookups() == before;
    }
    // A CMASK the write watch does not cover (no userfaultfd write protection on this host) cannot be proved
    // unwritten, so no draw may repeat over it.
    const bool watched = GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(color.cmaskAddress, color.cmaskBytes);
    std::cout << "the CMASK is " << (watched ? "" : "not ") << "covered by the write watch; a draw " << (repeated ? "repeated" : "did not repeat") << "\n";
    Require(!LookupOutcomes::Profiled() || memoResolves() || repeated || !watched, "no draw repeated its targets over a recorded CMASK clear");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Clear); }, "repeated draws over a recorded CMASK clear");
}

void htileClear(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    Htile.fill(0xffffffffu);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto depth = depthTarget(false);
    arm(device, color, {.depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS, .colorWriteMask = 0});
    NoteDepthMetadataFill(depth.htileAddress, HtileBytes, 0u);
    draw(device, color, {.depth = depth, .compare = VK_COMPARE_OP_LESS});
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Old); }, "a draw behind an HTILE clear between repeated draws");
}

float storedDepth(AgcDriver::VulkanDevice& device, const DepthTarget& depth, std::uint32_t x = 0) {
    static_cast<void>(FlushDepthPlanes(depth.address, PlaneBytes, DepthPlaneScope::Whole));
    device.WaitIdle();
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(depth.address, PlaneBytes, nullptr, "test");
        device.WaitIdle();
    }
    float stored = 0.0f;
    const ColorTargetLayout layout(Width, Height, ColorTileMode::RenderTarget, 4);
    std::memcpy(&stored, reinterpret_cast<const void*>(depth.address + layout.Offset(x, 0)), 4);
    return stored;
}

void depthPlaneStoredBetween(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    const auto depth = depthTarget(true);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    arm(device, color, {.depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS, .depthValue = 0.25f});
    Require(storedDepth(device, depth) == 0.25f, "depth written by repeated draws did not reach guest memory");
    arm(device, color, {.depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS});
    Require(storedDepth(device, depth) == 0.5f, "depth written after a plane store did not reach guest memory");
}

void proofsBreakOnceStored(AgcDriver::VulkanDevice&) {
    auto* word = reinterpret_cast<std::uint64_t*>(SourceStorage.data());
    std::uint64_t whileStoring = 0;
    AgcDriver::GuestMemory::StoreOwnBytes(address(SourceStorage.data()), sizeof(*word), [&] {
        whileStoring = AgcDriver::GuestMemory::ProofSerial();
        *word = New;
    });
    Require(AgcDriver::GuestMemory::ProofSerial() != whileStoring, "a proof read while a store was landing outlived the store");
    const auto beforeMark = AgcDriver::GuestMemory::ProofSerial();
    static_cast<void>(AgcDriver::GuestMemory::MarkWritten(address(SourceStorage.data()), sizeof(*word)));
    Require(AgcDriver::GuestMemory::ProofSerial() - beforeMark >= 2u, "a write mark did not break proofs after stamping");
}

void depthPlaneWrittenByCpu(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    auto depth = depthTarget(true);
    auto* plane = allocateWatched(2 * PlaneBytes);
    depth.address = address(plane);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    arm(device, color, {.depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS});
    const float written = 0.25f;
    for (std::size_t offset = 0; offset + sizeof(written) <= 2 * PlaneBytes; offset += sizeof(written)) std::memcpy(plane + offset, &written, sizeof(written));
    AgcDriver::GuestMemory::BumpCollectEpoch();
    mustLookUp(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS}, "a draw after a CPU write to its depth plane repeated its targets");
    arm(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS});
    Require(storedDepth(device, depth) == written, "a CPU write to a repeated depth plane was lost");
}

void cmaskPaddingBesideExpandedTiles(AgcDriver::VulkanDevice& device) {
    auto color = colorTarget(false, true);
    auto* cmask = reinterpret_cast<std::uint8_t*>(allocateWatched(Cmask.size()));
    color.cmaskAddress = address(cmask);
    reset(device, color);
    const CmaskLayout layout(Width, Height);
    std::fill(cmask, cmask + Cmask.size(), std::uint8_t{0});
    for (std::uint32_t tileY = 0; tileY < layout.TilesY(); ++tileY) {
        for (std::uint32_t tileX = 0; tileX < layout.TilesX(); ++tileX) {
            const auto nibble = layout.Nibble(tileX, tileY);
            cmask[nibble / 2u] |= static_cast<std::uint8_t>(0xfu << ((nibble % 2u) * 4u));
        }
    }
    const std::span tiles(cmask, color.cmaskBytes);
    Require(std::ranges::find(tiles, std::uint8_t{0}) != tiles.end(), "the CMASK has no padding beside its tiles");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    bool repeated = false;
    for (int repeat = 0; repeat < 64 && !repeated; ++repeat) {
        const auto before = lookups();
        draw(device, color);
        repeated = lookups() == before;
    }
    Require(!LookupOutcomes::Profiled() || memoResolves() || repeated || !GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(color.cmaskAddress, color.cmaskBytes), "no draw over expanded tiles with zero padding repeated its targets");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Old); }, "draws over expanded tiles with zero padding");
}

void arrayLayerOutsideTheScope(AgcDriver::VulkanDevice& device) {
    auto color = colorTarget();
    color.address = color.surfaceAddress = color.arrayAddress = address(OuterStorage.data());
    color.arrayLastSlice = 1;
    color.layers = 1;
    const auto flush = [&] {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, OuterStorage.size(), nullptr, "test");
        device.WaitIdle();
    };
    flush();
    AgcDriver::GuestMemory::BumpCollectEpoch();
    arm(device, color);
    const auto resident = StorageTexture::FindLive(color.address, OuterStorage.size());
    Require(resident != nullptr, "the array target has no resident image");
    flush();
    const auto byteOf = [&](std::uint32_t layer) { return OuterStorage.data() + layer * PlaneBytes + 16; };
    const auto kept = *byteOf(1);
    *byteOf(1) = ~kept;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(resident->RefreshObligation() != nullptr, "the whole image's obligation missed a changed layer");
    Require(resident->RefreshObligation(0, 1) == nullptr, "the selected layer's obligation counted a layer outside it");
    Require(resident->RefreshObligation(1, 1) != nullptr, "the changed layer's obligation missed it");
    draw(device, color);
    draw(device, color);
    mustRepeat(device, color, "a draw into the unchanged layer did not repeat");
    *byteOf(1) = kept;
}

void hitTargetKeepsItsResidency(AgcDriver::VulkanDevice& device) {
    if (memoResolves()) return;
    constexpr std::uint32_t Extent = 1024;
    constexpr std::uint32_t Images = 8;
    const ColorTargetLayout layout(Extent, Extent, ColorTileMode::RenderTarget, 4);
    const auto stride = (static_cast<std::size_t>(layout.Bytes()) + 65535u) & ~std::size_t{65535u};
    auto* block = allocateWatched(Images * stride);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto color = colorTarget();
    reset(device, color);
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        ClearCachedTextures(device.Device());
    }
    arm(device, color);
    const auto resident = StorageTexture::FindLive(color.address, color.bytes);
    Require(resident != nullptr && resident->Cached(), "the repeated target has no cached image");
    std::shared_ptr<StorageTexture> oldest;
    std::uint32_t repeats = 0;
    for (std::uint32_t image = 0; image < Images; ++image) {
        auto other = colorTarget();
        other.address = other.surfaceAddress = address(block + image * stride);
        other.extent = other.surfaceExtent = {Extent, Extent};
        other.bytes = layout.Bytes();
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            auto inserted = CachedStorageSurface(context, SurfaceForTarget(other));
            if (image == 0) oldest = std::move(inserted);
        }
        const auto before = lookups();
        draw(device, color);
        if (lookups() == before) ++repeats;
    }
    Require(!LookupOutcomes::Profiled() || repeats >= Images / 2u, "too few draws between insertions repeated their targets");
    Require(oldest != nullptr && !oldest->Cached(), "the insertions never reached the storage budget");
    Require(resident->Cached(), "the storage cache evicted a target every draw repeated");
}

void surfaceCreatedElsewhere(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    const ProofFields before;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        auto other = depthTarget(false);
        other.address = address(MultisampleStorage.data());
        other.htileAddress = 0;
        static_cast<void>(DepthSurfaceView(AgcDriver::VulkanDeviceTestAccess::ContextOf(device), other));
    }
    requireMoved(before, "surfaces", "a depth surface created elsewhere");
    mustLookUp(device, color, {}, "a draw after a new depth surface repeated its targets");
}

void depthWriteOnRepeatedTarget(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    const auto depth = depthTarget(true);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    arm(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS});
    const auto before = lookups();
    draw(device, color, {.depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS, .depthValue = 0.75f});
    Require(!LookupOutcomes::Profiled() || (lookups() != before) == memoResolves(), "a draw that first writes a repeated depth target did not repeat it");
    Require(storedDepth(device, depth) == 0.75f, "a depth write on a repeated target did not reach guest memory");
}

void colorMemoDepthDescriptorChange(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    auto depth = depthTarget(true);
    arm(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS});
    depth.clearDepth = 0.375f;
    const auto before = lookups();
    draw(device, color, {.scissor = Right, .depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS, .depthValue = 0.75f});
    Require(!LookupOutcomes::Profiled() || (lookups() != before) == memoResolves(), "a depth descriptor miss unnecessarily resolved unchanged colors");
    Require(storedDepth(device, depth, Width - LeftWidth) == 0.75f, "depth fallback lost its depth write");
    expect(device, color, [](std::uint32_t x) { return x < LeftWidth || x >= Width - LeftWidth ? Drawn : Old; }, "colors reused over a depth descriptor miss");
}

void colorMemoWithMsaaDepth(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    auto depth = depthTarget(false);
    depth.address = address(MultisampleStorage.data());
    depth.samples = 4;
    depth.htileAddress = 0;
    arm(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS, .prepareOnly = true});
    const auto before = lookups();
    draw(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS, .prepareOnly = true});
    Require(!LookupOutcomes::Profiled() || (lookups() != before) == memoResolves(), "unchanged colors were not reused while MSAA depth took its full path");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    Require(RepeatDepthSurfaceView(context, depth, false, false) == VK_NULL_HANDLE, "MSAA depth acquired a repeat certificate");
}

void colorMemoProofBreak(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    auto depth = depthTarget(true);
    arm(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS});
    const ProofFields before;
    AgcDriver::GuestMemory::BreakProofs();
    requireMoved(before, "proofs", "an explicit proof break");
    depth.clearDepth = 0.625f;
    const MemoEvents events;
    mustLookUp(device, color, {.scissor = Right, .depth = depth, .compare = VK_COMPARE_OP_ALWAYS}, "a proof break allowed color reuse while depth fell back");
    requireMemoEvent(events, TargetMemoEvent::Proofs, memoServes() && memoCounted() ? 1u : 0u, "an explicit proof break");
    requireMemoEvent(events, TargetMemoEvent::Hit, 0u, "an explicit proof break");
    expect(device, color, [](std::uint32_t x) { return x < LeftWidth || x >= Width - LeftWidth ? Drawn : Old; }, "proof-break fallback");
}

void cachedDepthOverlap(AgcDriver::VulkanDevice& device) {
    const bool disabled = !DrawPipelineEnabled() || std::getenv("APS5_NO_TARGET_MEMO") != nullptr || std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    const auto color = colorTarget();
    const auto disjoint = depthTarget(true);
    auto onSurface = disjoint;
    onSurface.address = color.address;
    auto stencilOnSurface = disjoint;
    stencilOnSurface.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    stencilOnSurface.stencilAddress = color.address;
    auto htileOnSurface = disjoint;
    htileOnSurface.htileAddress = color.address;
    auto sliced = disjoint;
    sliced.lastSlice = 1;
    const std::array<std::pair<const char*, DepthTarget>, 4> overlapping{{
        {"depth plane", onSurface}, {"stencil plane", stencilOnSurface}, {"HTILE", htileOnSurface}, {"sliced", sliced}
    }};
    const DrawOptions armed{.depth = disjoint, .compare = VK_COMPARE_OP_ALWAYS};
    for (const auto& [name, depth] : overlapping) {
        reset(device, color);
        arm(device, color, armed);
        const auto before = lookups();
        draw(device, color, armed);
        Require(!LookupOutcomes::Profiled() || (lookups() != before) == memoResolves(), std::string("a repeated draw over a disjoint depth did not reuse its targets (") + name + ")");
        Require(DepthTargetOverlaps(depth, color.address, color.bytes), std::string("the ") + name + " variant does not overlap the color");
        if (!disabled) {
            const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
            const auto hitsWith = [&](const DrawOptions& options) {
                std::size_t hits = 0;
                std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                TargetMemoHitNanosecondsForTest(context, drawState(color, options), 1, hits);
                return hits;
            };
            Require(hitsWith(armed) == 1, std::string("the memo did not hit the depth it was armed with (") + name + ")");
            Require(hitsWith({.depth = depth, .compare = VK_COMPARE_OP_ALWAYS}) == 0, std::string("a ") + name + " over the color hit the memo proved against another depth");
            Require(hitsWith(armed) == 1, std::string("a miss over a ") + name + " dropped the memo of the disjoint depth");
        }
        mustLookUp(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS, .prepareOnly = true}, std::string("a ") + name + " over the color reused the targets proved against another depth");
    }
    const bool surfaceLower = address(SurfaceStorage.data()) < address(SourceStorage.data());
    auto lower = colorTarget();
    auto higher = lower;
    lower.address = lower.surfaceAddress = address((surfaceLower ? SurfaceStorage : SourceStorage).data());
    higher.address = higher.surfaceAddress = address((surfaceLower ? SourceStorage : SurfaceStorage).data());
    auto small = depthTarget(true);
    small.address = lower.address;
    auto grown = small;
    grown.extent = {16384u, 16384u};
    Require(!DepthTargetOverlaps(small, higher.address, higher.bytes) && DepthTargetOverlaps(grown, higher.address, higher.bytes), "the depth plane at the lower block does not reach the higher block only by growing its extent");
    reset(device, higher);
    arm(device, higher, {.depth = small, .compare = VK_COMPARE_OP_ALWAYS});
    if (disabled) return;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto hitsWith = [&](const DepthTarget& depth) {
        std::size_t hits = 0;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        TargetMemoHitNanosecondsForTest(context, drawState(higher, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS}), 1, hits);
        return hits;
    };
    Require(hitsWith(small) == 1, "the memo did not hit the depth it was armed with (extent)");
    Require(hitsWith(grown) == 0, "a depth plane grown over the color by its extent alone hit the memo proved against the smaller plane");
}

void colorMetadataDepthAlias(AgcDriver::VulkanDevice& device) {
    auto color = colorTarget(true);
    auto* metadata = allocateWatched(PlaneBytes);
    std::memset(metadata, 0xff, PlaneBytes);
    color.dccAddress = address(metadata);
    reset(device, color);
    auto depth = depthTarget(true);
    depth.htileAddress = color.dccAddress;
    arm(device, color);
    Require(DepthTargetOverlaps(depth, color.dccAddress, Keys.size()), "DCC/HTILE overlap was missed");
    mustLookUp(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS, .prepareOnly = true}, "DCC metadata aliased by HTILE allowed color reuse");
    Require(DepthTargetOverlaps(depth, std::numeric_limits<std::uint64_t>::max() - 1u, 4u), "overflowing color dependency was considered disjoint");
    auto overflowing = depth;
    overflowing.address = std::numeric_limits<std::uint64_t>::max() - 1u;
    Require(DepthTargetOverlaps(overflowing, color.address, color.bytes), "overflowing depth dependency was considered disjoint");
}

void colorMemoResidentEviction(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto resident = StorageTexture::FindLive(color.address, color.bytes);
    Require(resident != nullptr, "no resident to evict");
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
        device.WaitIdle();
    }
    arm(device, color, {.prepareOnly = true});
    const ProofFields before;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        ClearCachedTextures(context.device);
    }
    requireMoved(before, "", "evicting a clean retained color resident");
    Require(!resident->Cached(), "the negative control did not evict the resident");
    const MemoEvents events;
    mustLookUp(device, color, {}, "an evicted color resident was reused");
    requireMemoEvent(events, TargetMemoEvent::ResidentEvicted, memoServes() && memoCounted() ? 1u : 0u, "an evicted color resident");
    requireMemoEvent(events, TargetMemoEvent::Hit, 0u, "an evicted color resident");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Old); }, "resident eviction");
}

void depthFallbackInvalidatesColor(AgcDriver::VulkanDevice& device) {
#ifdef _WIN32
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    auto* block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(PlaneBytes, PlaneBytes));
    const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, PlaneBytes, nullptr);
    Require(section != nullptr, "cannot allocate shared color backing");
    GuestArena::GuestArenaMap_nid_postfix(block, PlaneBytes, section, 0, PAGE_READWRITE);
    GuestAllocations::Mutation().Add(block, PlaneBytes, true, true, true);
    auto* alias = GuestArena::GuestArenaMapAlias_nid_postfix(address(block), PlaneBytes);
    Require(alias != nullptr, "cannot alias color memory for the depth fallback");
    GuestAllocations::Mutation().Add(alias, PlaneBytes, true, true, true);
    auto color = colorTarget();
    color.address = color.surfaceAddress = address(block);
    auto depth = depthTarget(false);
    depth.htileAddress = address(alias);
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    AgcDriver::GuestMemory::Write(address(alias), std::vector<std::byte>(PlaneBytes, std::byte{0xff}));
    static_cast<void>(DepthSurfaceView(context, depth, false));
    AgcDriver::GuestMemory::Write(address(alias), std::vector<std::byte>(PlaneBytes, std::byte{0}));
    AgcDriver::GuestMemory::BumpCollectEpoch();
    arm(device, color, {.prepareOnly = true});
    Require(!DepthTargetOverlaps(depth, color.address, color.bytes), "the alias negative control overlaps virtually");
    const auto resident = StorageTexture::FindLive(color.address, color.bytes);
    Require(resident != nullptr, "no color resident before depth fallback");
    const auto proof = AgcDriver::GuestMemory::ProofSerial();
    mustLookUp(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS, .prepareOnly = true}, "a depth-path write through an alias retained stale color candidates");
    Require(AgcDriver::GuestMemory::ProofSerial() != proof, "the depth fallback did not store through its HTILE alias");
    std::uint32_t expanded = 0;
    std::memcpy(&expanded, block, sizeof(expanded));
    Require(expanded == 0xfffc000fu, "the depth fallback did not expand its aliased HTILE");
    const auto current = StorageTexture::FindLive(color.address, color.bytes);
    Require(current != nullptr && current->RefreshObligation() == nullptr, "the color lookup did not reconcile the depth-path write");
    current->MarkDirty();
    StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test depth fallback alias");
    device.WaitIdle();
    const ColorTargetLayout layout(Width, Height, color.tileMode, color.elementBytes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = layout.Offset(x, y);
            std::uint32_t stored = 0;
            std::memcpy(&stored, block + offset, sizeof(stored));
            Require(stored == (offset < HtileBytes ? 0xfffc000fu : 0u), "the reconciled color image lost the aliased depth write on readback");
        }
    }
    ClearDepthSurfaces(context.device);
    ClearCachedTextures(context.device);
    GuestAllocations::Mutation().Remove(alias);
    GuestArena::GuestArenaUnmapAlias_nid_postfix(alias);
    GuestAllocations::Mutation().Remove(block);
    GuestArena::GuestArenaReset_nid_postfix(block, PlaneBytes);
    GuestArena::GuestArenaRelease_nid_postfix(block, PlaneBytes);
    GuestArena::GuestArenaForgetSection_nid_postfix(section);
    CloseHandle(section);
#else
    const auto color = colorTarget();
    reset(device, color);
    arm(device, color, {.prepareOnly = true});
    const auto depth = depthTarget(true);
    ClearDepthSurfaces(device.Device());
    arm(device, color, {.prepareOnly = true});
    mustLookUp(device, color, {.depth = depth, .prepareOnly = true}, "depth surface creation did not invalidate color candidates");
#endif
}

void validRefreshBaselines(AgcDriver::VulkanDevice& device) {
    auto color = colorTarget();
    auto* memory = allocateWatched(PlaneBytes);
    color.address = color.surfaceAddress = address(memory);
    AgcDriver::GuestMemory::Unwatch(color.address, color.bytes);
    reset(device, color);
    arm(device, color);
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushPending(color.address, color.bytes, nullptr, "test");
    device.WaitIdle();
    const auto resident = StorageTexture::FindLive(color.address, color.bytes);
    Require(resident != nullptr && resident->OriginalSnapshotBytes() != 0, "the baseline fixture has no saved bytes");
    const auto kept = memory[16];
    memory[16] = ~kept;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(resident->RefreshObligation() != nullptr, "a valid saved baseline missed changed guest bytes");
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
#ifdef _WIN32
    const std::pair<std::uint64_t, std::uint64_t> range{color.address, color.address + color.bytes};
    AgcDriver::GuestMemory::Rewatch(std::span(&range, 1));
#else
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, color.bytes);
#endif
    Require(AgcDriver::GuestMemory::Watched(color.address, color.bytes) && HostImportFor(context, color.address, color.bytes) != nullptr, "the baseline fixture cannot directly upload watched bytes");
    resident->Refresh();
    Require(resident->GuestCopyCapacity() == 0, "a direct upload of watched bytes kept a guest-byte copy no unit vouches for");
    Require(resident->RefreshObligation() == nullptr, "a released saved baseline rejected current image bytes");
    arm(device, color);
}

void heldBaselineUnits(AgcDriver::VulkanDevice& device) {
    auto color = colorTarget();
    auto* memory = allocateWatched(2 * PlaneBytes);
    color.address = color.surfaceAddress = color.arrayAddress = address(memory);
    color.arrayLastSlice = 1;
    color.layers = 1;
    AgcDriver::GuestMemory::Unwatch(color.address, 2 * PlaneBytes);
    reset(device, color);
    arm(device, color);
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    StorageTexture::FlushPending(color.address, 2 * PlaneBytes, nullptr, "test");
    device.WaitIdle();
    const auto resident = StorageTexture::FindLive(color.address, 2 * PlaneBytes);
    Require(resident != nullptr && resident->GuestCopyCapacity() != 0, "the two-layer baseline fixture has no saved bytes");
#ifdef _WIN32
    const std::pair<std::uint64_t, std::uint64_t> range{color.address, color.address + 2 * PlaneBytes};
    AgcDriver::GuestMemory::Rewatch(std::span(&range, 1));
#else
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, 2 * PlaneBytes);
#endif
    resident->MarkUnitsWritten({true, false});
    StorageTexture::FlushPending(color.address, PlaneBytes, nullptr, "test");
    device.WaitIdle();
    Require(resident->GuestCopyCapacity() != 0, "a layer that still vouches for its saved bytes lost the guest-byte copy");
    const auto byteOf = [&](std::uint32_t layer) { return memory + layer * PlaneBytes + 16; };
    const auto written = *byteOf(0);
    *byteOf(0) = ~written;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(resident->RefreshObligation(0, 1) == nullptr, "a layer that no longer vouches for its saved bytes was compared with them");
    const auto held = *byteOf(1);
    *byteOf(1) = ~held;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(resident->RefreshObligation(1, 1) != nullptr, "a held layer's saved bytes missed its changed guest bytes");
    *byteOf(0) = written;
    *byteOf(1) = held;
}

void depthProofAfterMetadataWrite(AgcDriver::VulkanDevice& device) {
    const auto color = colorTarget();
    reset(device, color);
    Htile.fill(0xffffffffu);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    auto depth = depthTarget(false);
    depth.clearDepth = 0.0f;
    arm(device, color, {.depth = depth, .depthWrite = true, .compare = VK_COMPARE_OP_ALWAYS, .depthValue = 0.75f, .colorWriteMask = 0});
    AgcDriver::GuestMemory::StoreOwnBytes(address(Htile.data()), HtileBytes, [] { Htile.fill(0u); });
    AgcDriver::GuestMemory::BumpCollectEpoch();
    draw(device, color, {.depth = depth, .compare = VK_COMPARE_OP_LESS});
    expect(device, color, [](std::uint32_t) { return Old; }, "a raw HTILE write reused a stale depth proof");
}

void claimAfterAbsorbedFill(AgcDriver::VulkanDevice& device) {
    auto multisampled = colorTarget();
    multisampled.address = address(MultisampleStorage.data());
    multisampled.surfaceAddress = multisampled.address;
    multisampled.samples = 4;
    multisampled.coverageSamples = 4;
    const auto color = colorTarget();
    reset(device, color);
    draw(device, multisampled, {.scissor = Right});
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        device.WaitIdle();
    }
    auto depth = depthTarget(true);
    depth.address = multisampled.address;
    arm(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS});
    Require(MultisampledColorFillAbsorbs(multisampled.address, static_cast<std::size_t>(MultisampledColorBytes(multisampled)), 0u), "a fill over the aliased multisampled target was not taken as its clear");
    draw(device, color, {.depth = depth, .compare = VK_COMPARE_OP_ALWAYS});
    std::string refusal;
    try {
        RequireNoMultisampleColorTarget(depth.address, 4096, "a read of the depth plane");
    } catch (const std::exception& error) {
        refusal = error.what();
    }
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        device.WaitIdle();
        ClearMultisampleTargets(AgcDriver::VulkanDeviceTestAccess::ContextOf(device).device);
    }
    Require(refusal.empty(), "a depth draw after a fill over an aliased multisampled target did not claim its memory: " + refusal);
}

void untrackedMetadataClear(AgcDriver::VulkanDevice& device, bool dcc) {
    const auto color = colorTarget(dcc, !dcc);
    Require(!AgcDriver::GuestMemory::Watched(dcc ? color.dccAddress : color.cmaskAddress, dcc ? Keys.size() : Cmask.size()), "the untracked metadata fixture is write-watched");
    reset(device, color);
    for (int repeat = 0; repeat < 4; ++repeat) draw(device, color);
    const ProofFields before;
    if (dcc) std::fill(Keys.begin(), Keys.end(), std::uint8_t{0x20});
    else std::fill(Cmask.begin(), Cmask.end(), std::uint8_t{0});
    requireMoved(before, "", "a CPU write to untracked metadata");
    mustLookUp(device, color, {}, dcc ? "a target with untracked DCC keys repeated its targets" : "a target with an untracked CMASK repeated its targets");
    expect(device, color, [](std::uint32_t x) { return leftOver(x, Clear); }, dcc ? "a repeat skipped a fast clear written to untracked DCC keys" : "a repeat skipped a fast clear written to an untracked CMASK");
    std::ranges::fill(Keys, std::uint8_t{0xff});
    std::ranges::fill(Cmask, std::uint8_t{0xff});
}

void untrackedDccClear(AgcDriver::VulkanDevice& device) {
    untrackedMetadataClear(device, true);
}

void untrackedCmaskClear(AgcDriver::VulkanDevice& device) {
    untrackedMetadataClear(device, false);
}

void residentDccClear(AgcDriver::VulkanDevice& device) {
    const auto keyed = colorTarget(true);
    const auto plain = colorTarget();
    Require(!AgcDriver::GuestMemory::Watched(keyed.dccAddress, Keys.size()), "the untracked metadata fixture is write-watched");
    reset(device, keyed);
    draw(device, keyed);
    for (int repeat = 0; repeat < 4; ++repeat) draw(device, plain);
    std::fill(Keys.begin(), Keys.end(), std::uint8_t{0x20});
    expect(device, plain, [](std::uint32_t x) { return leftOver(x, Old); }, "a target without DCC took the fast clear of a resident image's DCC keys");
    std::ranges::fill(Keys, std::uint8_t{0xff});
}

void descriptorIdentity() {
    auto color = colorTarget();
    auto depth = depthTarget(false);
    auto colorOther = color;
    auto depthOther = depth;
    setDescriptorPadding(color, &depth, 0);
    setDescriptorPadding(colorOther, &depthOther, 0xa5);
    Require(color == colorOther, "color descriptor identity includes padding");
    Require(depth == depthOther, "depth descriptor identity includes padding");
    depth.clearDepth = 0.0f;
    depthOther.clearDepth = -0.0f;
    Require(depth != depthOther, "depth descriptor identity lost float bits");
    depth.clearDepth = depthOther.clearDepth = std::bit_cast<float>(0x7fc00001u);
    Require(depth == depthOther, "depth descriptor identity compares NaNs numerically");
}

void unchangedMemo(AgcDriver::VulkanDevice& device) {
    descriptorIdentity();
    const auto color = colorTarget();
    reset(device, color);
    for (const auto samples : {0u, 1u, 4u}) {
        DrawOptions options{};
        options.changePadding = true;
        if (samples != 0u) {
            options.depth = depthTarget(false);
            options.depth->samples = samples;
            if (samples != 1u) {
                options.depth->address = address(MultisampleStorage.data());
                options.depth->htileAddress = 0;
                options.prepareOnly = true;
            }
        }
        options.compare = VK_COMPARE_OP_ALWAYS;
        arm(device, color, options);
        const auto builds = TargetMemoBuildsForTest();
        const ProofFields proof;
        for (int repeat = 0; repeat < 4; ++repeat) draw(device, color, options);
        requireMoved(proof, "", "unchanged repeat");
        const bool enabled = std::getenv("APS5_NO_TARGET_MEMO") == nullptr;
        Require(!enabled || TargetMemoBuildsForTest() == builds, "an unchanged successful repeat rebuilt the memo");
        AgcDriver::GuestMemory::BreakProofs();
        draw(device, color, options);
        Require(!enabled || TargetMemoBuildsForTest() > builds, "a broken proof retained the old memo");
    }
}

const std::array<std::pair<const char*, void (*)(AgcDriver::VulkanDevice&)>, 14> R3Scenarios{{
    {"untracked-dcc", untrackedDccClear},
    {"untracked-cmask", untrackedCmaskClear},
    {"resident-dcc", residentDccClear},
    {"claim", claimAfterAbsorbedFill},
    {"depth-proof", depthProofAfterMetadataWrite},
    {"baseline", validRefreshBaselines},
    {"held-baseline", heldBaselineUnits},
    {"unchanged", unchangedMemo},
    {"fallback", depthFallbackInvalidatesColor},
    {"descriptor", colorMemoDepthDescriptorChange},
    {"msaa", colorMemoWithMsaaDepth},
    {"proof", colorMemoProofBreak},
    {"metadata", colorMetadataDepthAlias},
    {"eviction", colorMemoResidentEviction}
}};

const std::array<std::pair<const char*, void (*)(AgcDriver::VulkanDevice&)>, 33> Scenarios{{
    {"repeated draws", repeatedDraws},
    {"CPU write", cpuWrite},
    {"driver store", driverStore},
    {"driver store over pending results", driverStoreOverPendingResults},
    {"results flushed", resultsFlushed},
    {"GPU store stamp", storedStamp},
    {"pending serial alone", pendingSerialOnly},
    {"another image's results outside the target", otherResultsOutsideTarget},
    {"another image's results growing into the target", otherResultsGrowIntoTarget},
    {"label stored before a hit", labelStoredBeforeHit},
    {"label stored over the target", labelStoredOverTarget},
    {"label stored on another thread", labelStoredOnAnotherThread},
    {"compute write through an alias", computeThroughAlias},
    {"copy between identical draws", copyBetween},
    {"resolve between identical draws", resolveBetween},
    {"committer moved threads", committerMoved},
    {"depth drawn by another committer", depthDrawnByAnotherCommitter},
    {"remap", remap},
    {"GPU write under an alias", gpuWriteUnderAlias},
    {"CMASK clear", cmaskClear},
    {"DCC clear", dccClear},
    {"HTILE clear", htileClear},
    {"depth plane stored between draws", depthPlaneStoredBetween},
    {"proofs break once stored", proofsBreakOnceStored},
    {"depth plane written by the CPU", depthPlaneWrittenByCpu},
    {"CMASK clear recorded before repeated draws", cmaskClearRecorded},
    {"CMASK padding beside expanded tiles", cmaskPaddingBesideExpandedTiles},
    {"array layer outside the scope", arrayLayerOutsideTheScope},
    {"hit target keeps its residency", hitTargetKeepsItsResidency},
    {"depth surface created elsewhere", surfaceCreatedElsewhere},
    {"depth write on a repeated target", depthWriteOnRepeatedTarget},
    {"cached depth overlap", cachedDepthOverlap},
    {"memo hit path", memoHitPath}
}};

}

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        Require(_putenv_s("APS5_TEXTURE_CACHE_MIB", "16") == 0, "cannot set the test cache budget");
#else
        Require(setenv("APS5_TEXTURE_CACHE_MIB", "16", 1) == 0, "cannot set the test cache budget");
#endif
        std::unique_ptr<AgcDriver::VulkanDevice> device;
        try {
            device = std::make_unique<AgcDriver::VulkanDevice>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        Keys.fill(0xffu);
        Cmask.fill(0xffu);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(SurfaceStorage.data(), SurfaceStorage.size(), true, true, true);
            mutation.Add(DepthStorage.data(), DepthStorage.size(), true, true, true);
            mutation.Add(Keys.data(), Keys.size(), true, true, true);
            mutation.Add(Cmask.data(), Cmask.size(), true, true, true);
            mutation.Add(Htile.data(), HtileBytes, true, true, true);
            mutation.Add(SourceStorage.data(), SourceStorage.size(), true, true, true);
            mutation.Add(OuterStorage.data(), OuterStorage.size(), true, true, true);
            mutation.Add(MultisampleStorage.data(), MultisampleStorage.size(), true, true, true);
        }
        AgcDriver::GuestMemory::MarkProofThread();
        AgcDriver::GuestMemory::BumpCollectEpoch();
        if (argc == 3 && std::string(argv[1]) == "--r3") {
            bool found = false;
            for (const auto& [name, scenario] : R3Scenarios) {
                if (std::string(argv[2]) != name) continue;
                found = true;
                std::cout << name << '\n' << std::flush;
                scenario(*device);
            }
            Require(found, "unknown R3 scenario");
        } else {
            for (const auto& [name, scenario] : Scenarios) {
                std::cout << name << '\n' << std::flush;
                scenario(*device);
            }
        }
        device->WaitIdle();
        device.reset();
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(SurfaceStorage.data());
            mutation.Remove(DepthStorage.data());
            mutation.Remove(Keys.data());
            mutation.Remove(Cmask.data());
            mutation.Remove(Htile.data());
            mutation.Remove(SourceStorage.data());
            mutation.Remove(OuterStorage.data());
            mutation.Remove(MultisampleStorage.data());
        }
        std::cout << "target state memo tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
