#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "Cover_vert_spv.h"
#include "DrawProofs_spv.h"
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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t Unit = 65536;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Target = 64;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t TextureBytes = std::size_t{Side} * Side * 4u;
constexpr std::size_t TargetOffset = 0;
constexpr std::size_t TextureOffset = Unit;
constexpr std::size_t BlockBytes = TextureOffset + TextureBytes;
constexpr std::size_t Draws = 8;

enum class Mode { Default, Verify, Off };

class Block {
public:
    Block() {
#ifdef _WIN32
        data = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, Unit));
        Require(data != nullptr, "draw proofs: cannot reserve test memory");
        GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* raw = mmap(nullptr, BlockBytes + Unit, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "draw proofs: cannot map test memory");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (begin + Unit - 1) & ~(static_cast<std::uintptr_t>(Unit) - 1);
        if (aligned != begin) munmap(raw, aligned - begin);
        if (aligned + BlockBytes != begin + BlockBytes + Unit) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + Unit - aligned);
        data = reinterpret_cast<std::byte*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
#endif
        std::fill_n(data, BlockBytes, std::byte{0});
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
    }
    ~Block() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes);
#else
        munmap(data, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
#endif
    }
    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;
    std::uint64_t Address(std::size_t offset) const { return reinterpret_cast<std::uintptr_t>(data + offset); }
    std::byte* Bytes(std::size_t offset) const { return data + offset; }

private:
    std::byte* data = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(std::uint64_t address) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint8_t, 4> Unpacked(std::uint32_t value) {
    return {static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8u), static_cast<std::uint8_t>(value >> 16u), static_cast<std::uint8_t>(value >> 24u)};
}

void Fill(const Block& block, std::size_t offset, std::size_t bytes, std::uint32_t value) {
    auto* words = reinterpret_cast<std::uint32_t*>(block.Bytes(offset));
    std::fill_n(words, bytes / 4u, value);
}

void CheckPixels(const ColorTarget& target, const std::array<std::uint8_t, 4>& left, const std::array<std::uint8_t, 4>& right, const std::string& what) {
    std::vector<std::byte> pixels(std::size_t{Target} * Target * 4u);
    ReadColorTarget(target, pixels);
    for (std::uint32_t y = 0; y < Target; ++y) {
        for (std::uint32_t x = 0; x < Target; ++x) {
            const auto& expected = x < Target / 2u ? left : right;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const auto actual = std::to_integer<std::uint8_t>(pixels[(std::size_t{y} * Target + x) * 4u + channel]);
                Require(actual == expected[channel], what + ": pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") channel " + std::to_string(channel) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[channel]));
            }
        }
    }
}

struct Counts {
    DrawProofCounts proofs;
    std::uint64_t refreshes = 0;
};

Counts Take() {
    return {DrawProofCounted(), StorageTexture::RefreshesProved()};
}

DrawProofKindCounts Delta(const Counts& before, const Counts& after, DrawProofKind kind) {
    const auto& from = before.proofs[static_cast<std::size_t>(kind)];
    const auto& to = after.proofs[static_cast<std::size_t>(kind)];
    DrawProofKindCounts delta;
    delta.reused = to.reused - from.reused;
    delta.proved = to.proved - from.proved;
    delta.verified = to.verified - from.verified;
    delta.differing = to.differing - from.differing;
    delta.stored = to.stored - from.stored;
    for (std::size_t miss = 0; miss < delta.missed.size(); ++miss) delta.missed[miss] = to.missed[miss] - from.missed[miss];
    return delta;
}

std::uint64_t Missed(const DrawProofKindCounts& counts, DrawProofMiss miss) {
    return counts.missed[static_cast<std::size_t>(miss)];
}

std::uint64_t Skipped(const DrawProofKindCounts& counts, Mode mode) {
    return mode == Mode::Verify ? counts.verified : counts.reused;
}

alignas(256) constexpr std::array<std::uint32_t, 7> FirstStore{0x7e040300, 0x7e060280, 0x7e0202ff, 0x11aa22bbu, 0xf0200108, 0x00020102, 0xbf810000};
alignas(256) constexpr std::array<std::uint32_t, 7> SecondStore{0x7e040300, 0x7e060280, 0x7e0202ff, 0x33cc44ddu, 0xf0200108, 0x00020102, 0xbf810000};

void StoreTexels(AgcDriver::VulkanDevice& device, const Block& block, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(block.Address(TextureOffset));
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{8, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

void Run(AgcDriver::VulkanDevice& device, const Block& block, Mode mode) {
    AgcDriver::GuestMemory::MarkProofThread();
    const auto program = [](const auto& spirv, std::uint64_t variant) {
        ShaderRecompiler::RecompileResult result;
        result.spirv = std::vector<std::uint32_t>(std::begin(spirv), std::end(spirv));
        result.variantId = variant;
        return result;
    };
    auto vertex = program(COVER_VERT_SPV, 0x7001);
    auto fragment = program(DRAW_PROOFS_SPV, 0x7002);
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::SampledImage;
    binding.role = ShaderRecompiler::DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = 10;
    binding.count = 1;
    binding.imageShape = ShaderRecompiler::DescriptorImageShape::Image2D;
    binding.imageWritten.assign(1, false);
    binding.imageSamplers.assign(1, 0u);
    const auto words = TextureDescriptor(block.Address(TextureOffset));
    binding.guestDescriptor.assign(words.begin(), words.end());
    fragment.bindings = {binding};
    const std::array shaders{CompiledShader{ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderStage::Fragment, &fragment, 0}};
    State state{};
    state.stages.path = ShaderPath::Vertex;
    state.color = {block.Address(TargetOffset), {Target, Target}, VK_FORMAT_R8G8B8A8_UNORM, Unit, 0xe4u, ColorTileMode::RenderTarget, 4};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {Target, Target};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(Target), static_cast<float>(Target), -static_cast<float>(Target), 0, 1};
    state.scissor = {{0, 0}, {Target, Target}};
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    const AgcDriver::Pm4::DrawParameters triangle{0, 3, 0, 1, 0, false};
    const auto draw = [&](const State& drawn) {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        device.Draw(drawn, triangle, shaders);
    };
    const bool proofs = mode != Mode::Off;

    constexpr std::uint32_t first = 0xff2040c0u;
    Fill(block, TextureOffset, TextureBytes, first);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    auto before = Take();
    for (std::size_t index = 0; index < Draws; ++index) draw(state);
    auto after = Take();
    const auto resources = Delta(before, after, DrawProofKind::Resources);
    const auto targets = Delta(before, after, DrawProofKind::Targets);
    std::cout << "pass of " << Draws << " draws: " << DrawProofText(after.proofs, before.proofs) << "; target refreshes " << after.refreshes - before.refreshes << '\n';
    if (proofs) {
        Require(Skipped(resources, mode) >= Draws - 3u && resources.proved <= 2u, "a pass of identical draws did not prove its resources once and reuse the proof");
        Require(Skipped(targets, mode) >= Draws - 3u && targets.proved <= 3u, "a pass of identical draws did not refresh its target once and reuse the proof");
        if (mode == Mode::Default) Require(after.refreshes - before.refreshes <= 3u, "draws reusing the target proof still refreshed the target");
        if (mode == Mode::Verify) Require(after.refreshes - before.refreshes >= Draws - 2u, "the verification did not refresh the target beside the reused proofs");
    } else {
        Require(resources.reused == 0 && resources.proved == 0 && targets.reused == 0 && targets.proved == 0 && resources.verified == 0 && targets.verified == 0, "APS5_NO_DRAW_PROOFS still reused or took proofs");
        Require(Missed(resources, DrawProofMiss::None) >= Draws - 1u && Missed(targets, DrawProofMiss::None) >= Draws, "APS5_NO_DRAW_PROOFS did not run the full check on every draw");
        Require(after.refreshes - before.refreshes >= Draws - 2u, "APS5_NO_DRAW_PROOFS did not refresh the target on every draw");
    }
    if (mode == Mode::Verify) Require(resources.differing == 0 && resources.stored == 0 && targets.differing == 0 && targets.stored == 0, "the full checks beside the reused proofs found a difference");
    CheckPixels(state.color, Unpacked(first), Unpacked(first), "pass of identical draws");

    for (std::size_t index = 0; index < 3; ++index) draw(state);
    constexpr std::uint32_t second = 0x80604020u;
    Fill(block, TextureOffset, TextureBytes, second);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    before = Take();
    draw(state);
    after = Take();
    if (proofs) Require(Missed(Delta(before, after, DrawProofKind::Resources), DrawProofMiss::Epoch) == 1u && Delta(before, after, DrawProofKind::Resources).reused == 0, "a CPU store to the texture behind an ordering point did not refuse the resource proof");
    CheckPixels(state.color, Unpacked(second), Unpacked(second), "CPU store to the texture between two draws");

    for (std::size_t index = 0; index < 3; ++index) draw(state);
    constexpr std::uint32_t third = 0x10305070u;
    std::vector<std::uint32_t> stored(TextureBytes / 4u, third);
    AgcDriver::GuestMemory::Write(block.Address(TextureOffset), std::as_bytes(std::span(stored)));
    before = Take();
    draw(state);
    after = Take();
    if (proofs) Require(Missed(Delta(before, after, DrawProofKind::Resources), DrawProofMiss::Serial) == 1u && Delta(before, after, DrawProofKind::Resources).reused == 0, "a driver store to the texture inside the epoch did not refuse the resource proof");
    CheckPixels(state.color, Unpacked(third), Unpacked(third), "driver store to the texture between two draws");

    for (std::size_t index = 0; index < 3; ++index) draw(state);
    before = Take();
    StoreTexels(device, block, FirstStore);
    draw(state);
    StoreTexels(device, block, SecondStore);
    draw(state);
    after = Take();
    const auto dispatched = Delta(before, after, DrawProofKind::Resources);
    std::cout << "draws behind dispatches: " << DrawProofText(after.proofs, before.proofs) << '\n';
    std::uint64_t checked = 0;
    for (const auto count : dispatched.missed) checked += count;
    if (proofs) Require(Skipped(dispatched, mode) == 0 && checked >= 2u, "a draw behind a dispatch writing its texture reused the resource proof");
    CheckPixels(state.color, Unpacked(0x33cc44ddu), Unpacked(0x33cc44ddu), "draws behind dispatches writing the texture");

    for (std::size_t index = 0; index < 3; ++index) draw(state);
    constexpr std::uint32_t written = 0x44332211u;
    Fill(block, TargetOffset, Unit, written);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    auto half = state;
    half.scissor = {{0, 0}, {Target / 2u, Target}};
    before = Take();
    draw(half);
    after = Take();
    if (proofs) Require(Missed(Delta(before, after, DrawProofKind::Targets), DrawProofMiss::Epoch) == 1u && Delta(before, after, DrawProofKind::Targets).reused == 0, "a CPU store to the target between two draws did not refuse the target proof");
    CheckPixels(state.color, Unpacked(0x33cc44ddu), Unpacked(written), "target written between two draws");

    const auto total = DrawProofCounted();
    for (std::size_t kind = 0; kind < total.size(); ++kind) Require(total[kind].differing == 0 && total[kind].stored == 0, "a full check beside a reused proof found a difference");
    device.WaitIdle();
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view argument = argc == 2 ? std::string_view(argv[1]) : std::string_view();
        const auto mode = argument == "--verify" ? Mode::Verify : argument == "--off" ? Mode::Off : Mode::Default;
        Require(mode != Mode::Verify || std::getenv("APS5_VERIFY_DRAW_PROOFS") != nullptr, "--verify needs APS5_VERIFY_DRAW_PROOFS");
        Require(mode != Mode::Off || std::getenv("APS5_NO_DRAW_PROOFS") != nullptr, "--off needs APS5_NO_DRAW_PROOFS");
        Require(mode == Mode::Off || DrawProofsEnabled(), "draw proofs are disabled in this environment");
#ifdef _WIN32
        _putenv_s("APS5_NO_HOST_IMPORT", "1");
#else
        setenv("APS5_NO_HOST_IMPORT", "1", 1);
#endif
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Block block;
        if (!AgcDriver::GuestMemory::Watched(block.Address(0), BlockBytes)) {
            std::cout << "skipped, the test memory is not write-watched\n";
            return VulkanTestSkipped;
        }
        Run(*device, block, mode);
        std::cout << "draw proofs tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
