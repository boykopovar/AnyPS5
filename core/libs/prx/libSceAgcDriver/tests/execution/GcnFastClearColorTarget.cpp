#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using Color = std::array<float, 4>;
using Texel = std::array<std::uint8_t, 4>;
using Triangle = std::array<std::array<float, 4>, 3>;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
constexpr std::uint32_t Format8888 = 10u;
constexpr std::uint32_t BlendClamp = 1u << 15u;
constexpr std::uint32_t FastClear = 1u << 13u;
constexpr std::uint32_t Compression = 1u << 14u;
constexpr std::uint32_t OneFragmentSamples = 4;
constexpr std::uint32_t Thin1D = 13u;
constexpr std::size_t ColorBytes = Width * Height * 4u;
constexpr std::size_t CmaskBytes = 128u;
constexpr std::size_t Slot = 0x10000;
constexpr std::size_t FmaskBytes = Width * Height;
constexpr std::size_t BlockBytes = 24u * Slot;
constexpr std::size_t Vertices = 23u * Slot;
constexpr std::uint64_t GcnAddressLimit = std::uint64_t{1} << 40u;
constexpr std::uint32_t RedClear = 0xff0000ffu;
constexpr Texel Red{0xff, 0x00, 0x00, 0xff};
constexpr Texel Green{0x00, 0xff, 0x00, 0xff};
constexpr Texel Blue{0x00, 0x00, 0xff, 0xff};

alignas(256) constexpr std::array<std::uint32_t, 6> VertexCode{
    0xe0382000u, 0x80000000u, 0xbf8c0f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u,
};

constexpr Triangle Lower{{{-1.0f, -1.0f, 0.5f, 1.0f}, {0.9f, -1.0f, 0.5f, 1.0f}, {-1.0f, 0.7f, 0.5f, 1.0f}}};

alignas(256) std::array<std::uint32_t, 64> GreenProgram{};

std::span<const std::uint32_t> Program(const Color& color) {
    std::size_t at = 0;
    for (std::uint32_t channel = 0; channel < 4; ++channel) {
        GreenProgram[at++] = 0x7e0002ffu | ((4u + channel) << 17u);
        GreenProgram[at++] = std::bit_cast<std::uint32_t>(color[channel]);
    }
    GreenProgram[at++] = 0xf800180fu;
    GreenProgram[at++] = 0x07060504u;
    GreenProgram[at++] = 0xbf810000u;
    return std::span<const std::uint32_t>(GreenProgram.data(), at);
}

class LowBlock {
public:
    LowBlock() {
#if defined(_WIN32) || defined(__APPLE__)
        data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
#ifdef _WIN32
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uint32_t ReadWriteProtection = 0x04;
        if (data != nullptr) GuestArena::GuestArenaCommit_nid_postfix(data, BlockBytes, ReadWriteProtection, BlockBytes);
#endif
#else
#ifdef ANYPS5_ENABLE_ASAN
        constexpr std::uintptr_t FirstHint = 0x60000000;
#else
        constexpr std::uintptr_t FirstHint = std::uintptr_t{1} << 32u;
#endif
        for (std::uintptr_t hint = FirstHint; data == nullptr && hint < GcnAddressLimit; hint += std::uintptr_t{1} << 32u) {
            void* mapped = mmap(reinterpret_cast<void*>(hint), BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (mapped != MAP_FAILED) data = static_cast<std::uint8_t*>(mapped);
        }
        if (data != nullptr && AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, BlockBytes);
#endif
        if (data == nullptr) return;
        std::memset(data, 0, BlockBytes);
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(Address(), BlockBytes);
    }

    ~LowBlock() {
        if (data == nullptr) return;
        AgcDriver::Graphics::StorageTexture::FlushPending(Address(), BlockBytes, nullptr, "test release");
        GuestAllocations::Mutation().Remove(data);
#if defined(_WIN32) || defined(__APPLE__)
        GuestArena::GuestArenaReset_nid_postfix(data, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(data, BlockBytes);
#else
        munmap(data, BlockBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, BlockBytes);
#endif
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    bool Usable() const { return data != nullptr && Address() + BlockBytes <= GcnAddressLimit; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }
    std::uint64_t At(std::size_t offset) const { return Address() + offset; }

private:
    std::uint8_t* data = nullptr;
};

struct Case {
    std::size_t color;
    std::size_t cmask;
    std::size_t fmask = 0;
    std::uint32_t slice = 0;
};

constexpr Case Cleared{0u * Slot, 1u * Slot};
constexpr Case Expanded{2u * Slot, 3u * Slot};
constexpr Case Mixed{4u * Slot, 5u * Slot};
constexpr Case Eliminated{6u * Slot, 7u * Slot};
constexpr Case OneCleared{8u * Slot, 9u * Slot, 10u * Slot};
constexpr Case OneExpanded{11u * Slot, 12u * Slot, 13u * Slot};
constexpr Case OneUnknownCode{14u * Slot, 15u * Slot, 16u * Slot};
constexpr Case SliceCleared{17u * Slot, 18u * Slot, 0u, 1u};
constexpr Case OneSliceCleared{19u * Slot, 20u * Slot, 21u * Slot, 1u};

AgcDriver::Graphics::ColorTarget DecodeTarget(const LowBlock& block, const Case& at) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(block.At(at.color) >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = at.slice | (at.slice << 13u);
    const bool oneFragment = at.fmask != 0u;
    cx[0x31c] = (Format8888 << 2u) | BlendClamp | FastClear | (oneFragment ? Compression : 0u);
    cx[0x31d] = Thin1D | (oneFragment ? static_cast<std::uint32_t>(std::countr_zero(OneFragmentSamples)) << 12u : 0u);
    cx[0x31f] = static_cast<std::uint32_t>(block.At(at.cmask) >> 8u);
    cx[0x320] = static_cast<std::uint32_t>(CmaskBytes / 128u - 1u);
    cx[0x321] = oneFragment ? static_cast<std::uint32_t>(block.At(at.fmask) >> 8u) : 0u;
    cx[0x322] = Width * Height / 64u - 1u;
    cx[0x323] = RedClear;
    cx[0x324] = 0u;
    const auto target = AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
    Require(target.samples == 1u && target.cmaskFastClear && target.cmaskAddress == block.At(at.cmask) + at.slice * CmaskBytes && target.cmaskBytes == CmaskBytes, "the fast-cleared GCN target did not decode with the CMASK of its slice");
    Require(!oneFragment || (target.coverageSamples == OneFragmentSamples && target.fmaskAddress == block.At(at.fmask) + at.slice * FmaskBytes && target.gcnFmaskBytes == FmaskBytes), "the fast-cleared 4-sample 1-fragment GCN target did not decode with its coverage samples and the FMASK of its slice");
    Require(target.format == VK_FORMAT_R8G8B8A8_UNORM && target.address == block.At(at.color) + at.slice * ColorBytes && target.bytes == ColorBytes && target.tileMode == AgcDriver::Graphics::ColorTileMode::GcnThin1D, "the 8_8_8_8 unorm GCN target did not decode as a thin 1D RGBA8 target at its slice");
    return target;
}

void Fill(std::uint64_t address, std::size_t bytes, std::span<const std::uint8_t> pattern) {
    std::vector<std::byte> data(bytes);
    for (std::size_t offset = 0; offset < bytes; ++offset) data[offset] = static_cast<std::byte>(pattern[offset % pattern.size()]);
    AgcDriver::GuestMemory::Write(address, data, 256);
}

std::vector<std::uint8_t> Stored(std::uint64_t address, std::size_t bytes) {
    AgcDriver::Graphics::StorageTexture::FlushPending(address, bytes, nullptr, "test read-back");
    std::vector<std::uint8_t> stored(bytes);
    AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)), 1);
    return stored;
}

std::size_t Count(std::span<const std::uint8_t> texels, const Texel& value) {
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + 4u <= texels.size(); offset += 4u) {
        if (std::equal(value.begin(), value.end(), texels.begin() + static_cast<std::ptrdiff_t>(offset))) ++count;
    }
    return count;
}

ShaderRecompiler::ShaderPixelStageInfo PixelInfo() {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionX) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PositionY);
    pixel.posX = true;
    pixel.posY = true;
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping.fill(0xe4u);
    return pixel;
}

void Draw(AgcDriver::VulkanDevice& device, const LowBlock& block, const AgcDriver::Graphics::ColorTarget& color, std::span<const std::uint32_t> pixelCode) {
    const auto target = device.Target();
    std::vector<std::byte> vertices(sizeof(Lower));
    std::memcpy(vertices.data(), Lower.data(), sizeof(Lower));
    AgcDriver::GuestMemory::Write(block.At(Vertices), vertices, 256);
    const auto address = block.At(Vertices);
    const std::vector<std::uint32_t> vertexUserData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), static_cast<std::uint32_t>(sizeof(Lower)), 0xfacu | (7u << 12u) | (14u << 15u)};
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(VertexCode.data()), std::as_bytes(std::span(VertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(VertexCode.data()), VertexCode, 0, {}},
        {64u, 0, vertexUserData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        target,
        {0, 0, 0, 64}
    };
    vertex.useCache = false;
    const auto vertexResult = ShaderRecompiler::Recompile(vertex);
    const auto vertexPush = static_cast<std::uint32_t>(vertexResult.pushConstants.size());
    const std::vector<std::uint32_t> pixelUserData(8, 0u);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(pixelCode)}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, pixelUserData, std::nullopt, PixelInfo(), std::nullopt, pixelMemory},
        target,
        PixelPushLayout(vertexPush, target)
    };
    fragment.useCache = false;
    const auto pixelResult = ShaderRecompiler::Recompile(fragment);
    const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{
        {ShaderStage::Vertex, &vertexResult, 0},
        {ShaderStage::Fragment, &pixelResult, PixelPushOffset(vertexPush, target)}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
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
    blend.colorWriteMask = 0xf;
    state.blends = {blend};
    state.blend = blend;
    state.blendConstants = {};
    state.samples = 1u;
    const AgcDriver::Pm4::DrawParameters draw{0, static_cast<std::uint32_t>(Lower.size()), 0, 1, 0, false};
    device.Draw(state, draw, shaders);
    device.WaitIdle();
}

bool AllBytes(std::span<const std::uint8_t> bytes, std::uint8_t value) {
    return std::all_of(bytes.begin(), bytes.end(), [&](std::uint8_t entry) { return entry == value; });
}

bool Guarded(std::uint64_t address) {
    try {
        AgcDriver::Graphics::RequireNoMultisampleColorTarget(address, 256u, "a test read");
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Usable()) {
            std::printf("skipped, no guest block below the 40-bit GCN color base range (0x%llx)\n", static_cast<unsigned long long>(block.Address()));
            return VulkanTestSkipped;
        }
        const auto green = Program({0.0f, 1.0f, 0.0f, 1.0f});
        constexpr auto texels = Width * Height;

        Fill(block.At(Cleared.cmask), CmaskBytes, std::array<std::uint8_t, 1>{0x00});
        Draw(*device, block, DecodeTarget(block, Cleared), green);
        const auto cleared = Stored(block.At(Cleared.color), ColorBytes);
        const auto clearedRed = Count(cleared, Red);
        const auto clearedGreen = Count(cleared, Green);
        Require(clearedRed > 0u && clearedGreen > 0u && clearedRed + clearedGreen == texels, "a draw over a GCN target whose CMASK holds the fast clear code did not land on the clear color everywhere else: " + std::to_string(clearedRed) + " red and " + std::to_string(clearedGreen) + " green of " + std::to_string(texels) + " texels");
        Require(AllBytes(Stored(block.At(Cleared.cmask), CmaskBytes), 0xffu), "the CMASK of the materialized GCN fast clear was not expanded");

        Fill(block.At(Expanded.cmask), CmaskBytes, std::array<std::uint8_t, 1>{0xff});
        Fill(block.At(Expanded.color), ColorBytes, Blue);
        Draw(*device, block, DecodeTarget(block, Expanded), green);
        const auto expanded = Stored(block.At(Expanded.color), ColorBytes);
        const auto expandedBlue = Count(expanded, Blue);
        const auto expandedGreen = Count(expanded, Green);
        Require(expandedBlue > 0u && expandedGreen > 0u && expandedBlue + expandedGreen == texels && Count(expanded, Red) == 0u, "a draw over a GCN target whose CMASK is expanded did not keep its texels: " + std::to_string(expandedBlue) + " blue and " + std::to_string(expandedGreen) + " green of " + std::to_string(texels) + " texels");
        Require(clearedGreen == expandedGreen, "the same triangle covered a different number of texels over the cleared and the expanded GCN target");

        Fill(block.At(Eliminated.cmask), CmaskBytes, std::array<std::uint8_t, 1>{0x00});
        AgcDriver::Graphics::ColorMetadataPass pass{};
        pass.mode = AgcDriver::Graphics::ColorMetadataPass::Mode::EliminateFastClear;
        pass.targets = {DecodeTarget(block, Eliminated)};
        device->ColorMetadataPass(pass);
        device->WaitIdle();
        Require(Count(Stored(block.At(Eliminated.color), ColorBytes), Red) == texels, "the fast clear eliminate of a GCN target did not store the clear color in every texel");
        Require(AllBytes(Stored(block.At(Eliminated.cmask), CmaskBytes), 0xffu), "the fast clear eliminate of a GCN target did not expand its CMASK");

        Fill(block.At(OneCleared.cmask), CmaskBytes, std::array<std::uint8_t, 1>{0x00});
        Draw(*device, block, DecodeTarget(block, OneCleared), green);
        const auto oneCleared = Stored(block.At(OneCleared.color), ColorBytes);
        Require(Count(oneCleared, Red) + Count(oneCleared, Green) == texels && Count(oneCleared, Green) == clearedGreen, "a draw over a 4-sample 1-fragment GCN target whose CMASK holds the fast clear code did not land on the clear color everywhere else");
        Require(AllBytes(Stored(block.At(OneCleared.cmask), CmaskBytes), 0xffu), "the CMASK of the materialized one-fragment GCN fast clear was not expanded");

        Fill(block.At(OneExpanded.cmask), CmaskBytes, std::array<std::uint8_t, 1>{0xcc});
        Fill(block.At(OneExpanded.color), ColorBytes, Blue);
        Draw(*device, block, DecodeTarget(block, OneExpanded), green);
        const auto oneExpanded = Stored(block.At(OneExpanded.color), ColorBytes);
        Require(Count(oneExpanded, Blue) + Count(oneExpanded, Green) == texels && Count(oneExpanded, Red) == 0u, "a draw over a 4-sample 1-fragment GCN target whose CMASK holds code 0xc on every tile did not keep its texels");

        std::array<std::uint8_t, CmaskBytes> mixed{};
        mixed[CmaskBytes / 2u] = 0x0fu;
        Fill(block.At(Mixed.cmask), CmaskBytes, mixed);
        std::string refusal;
        try {
            Draw(*device, block, DecodeTarget(block, Mixed), green);
        } catch (const std::runtime_error& error) {
            refusal = error.what();
        }
        Require(refusal.find("fast clears only some tiles") != std::string::npos, "a GCN CMASK with mixed codes was not refused by name: " + refusal);

        Fill(block.At(OneUnknownCode.cmask), CmaskBytes, std::array<std::uint8_t, 1>{0x55});
        std::string unknown;
        try {
            Draw(*device, block, DecodeTarget(block, OneUnknownCode), green);
        } catch (const std::runtime_error& error) {
            unknown = error.what();
        }
        Require(unknown.find("CMASK code 0x5 of a one-fragment GCN color target is not implemented") != std::string::npos, "an unknown CMASK code of a one-fragment GCN target was not refused by name: " + unknown);

        std::array<std::uint8_t, 2u * CmaskBytes> sliceMasks{};
        std::fill_n(sliceMasks.begin(), CmaskBytes, std::uint8_t{0xff});
        for (const auto& at : {SliceCleared, OneSliceCleared}) {
            const auto kind = std::string(at.fmask != 0u ? "a 4-sample 1-fragment" : "a single-sample");
            Fill(block.At(at.color), 2u * ColorBytes, Blue);
            Fill(block.At(at.cmask), sliceMasks.size(), sliceMasks);
            Draw(*device, block, DecodeTarget(block, at), green);
            const auto drawn = Stored(block.At(at.color) + ColorBytes, ColorBytes);
            Require(Count(drawn, Red) + Count(drawn, Green) == texels && Count(drawn, Green) == clearedGreen, "a draw into slice 1 of " + kind + " GCN array whose slice 1 CMASK holds the fast clear code did not land on the clear color everywhere else: " + std::to_string(Count(drawn, Red)) + " red, " + std::to_string(Count(drawn, Green)) + " green and " + std::to_string(Count(drawn, Blue)) + " blue of " + std::to_string(texels) + " texels");
            Require(Count(Stored(block.At(at.color), ColorBytes), Blue) == texels, "a draw into slice 1 of " + kind + " fast-cleared GCN array changed slice 0");
            Require(AllBytes(Stored(block.At(at.cmask), 2u * CmaskBytes), 0xffu), "the CMASK of slice 1 of " + kind + " GCN array was not expanded where that slice's CMASK starts");
        }
        Require(Guarded(block.At(OneSliceCleared.fmask) + FmaskBytes) && !Guarded(block.At(OneSliceCleared.fmask)), "the FMASK guard of slice 1 of a 4-sample 1-fragment GCN array does not cover that slice's FMASK alone");

        std::printf("GCN single-sample fast clear tests passed: a cleared CMASK materializes the clear color under a draw (%zu clear, %zu drawn texels), an expanded CMASK keeps the texels, mixed codes are refused, an eliminate stores the clear color, a 4-sample 1-fragment target clears under code 0, keeps its texels under code 0xc and refuses code 0x5, and a draw into slice 1 of a single-sample or 1-fragment array clears that slice through its own CMASK and keeps slice 0\n", clearedRed, clearedGreen);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
