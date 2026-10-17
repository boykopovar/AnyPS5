#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
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
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <memory>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t TextureBytes = Width * Height * 4;
constexpr std::uint32_t Stride = 48;
constexpr std::uint32_t Records = 48;
constexpr std::uint32_t Moved = 64;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t TexturesOffset = 65536;
constexpr std::size_t BlockBytes = TexturesOffset + 2 * 65536;
constexpr std::size_t UnimportableBytes = BlockBytes - 256;
#ifdef _WIN32
constexpr std::uint64_t CpuWriteResolutions = 2;
#else
constexpr std::uint64_t CpuWriteResolutions = 1;
#endif

alignas(256) std::array<std::uint32_t, Records> Keys{};
alignas(256) std::array<std::uint32_t, Lanes * 4> Output{};
alignas(256) std::array<std::uint32_t, 32> Srt{};

alignas(256) constexpr std::array<std::uint32_t, 28> TableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu,
    0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u,
    0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e020d00u, 0x082802f7u, 0x7e3c0300u, 0x7e3e0280u, 0xf0201108u, 0x0001141eu, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 4> Image(const void* texels) {
    const auto address = AddressOf(texels);
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Width - 1u) & 3u) << 30u), ((Width - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (Type2D << 28u)};
}

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t bytes) {
    const auto address = AddressOf(base);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

class GuestBlock {
public:
    explicit GuestBlock(std::size_t registered = BlockBytes) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "image table reuse: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, registered, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    std::uint32_t* Table() const { return reinterpret_cast<std::uint32_t*>(block); }
    float* Texels(std::uint32_t texture) const { return reinterpret_cast<float*>(block + TexturesOffset + static_cast<std::size_t>(texture) * TextureBytes); }

private:
    std::uint8_t* block = nullptr;
};

void SetRecord(const GuestBlock& block, std::uint32_t record, std::uint32_t texture) {
    auto* words = block.Table() + record * (Stride / 4u);
    std::fill(words, words + Stride / 4u, 0u);
    std::copy(PointClamp.begin(), PointClamp.end(), words);
    const auto image = Image(block.Texels(texture));
    std::copy(image.begin(), image.end(), words + 4);
}

void Fill(const GuestBlock& block, std::uint32_t texture, float bias) {
    for (std::uint32_t texel = 0; texel < Width * Height; ++texel) block.Texels(texture)[texel] = bias + static_cast<float>(texture) * 100.0f + static_cast<float>(texel);
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    const auto srtAddress = AddressOf(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::span<const std::uint32_t> code(TableCode);
    const std::span<const std::uint32_t> table(block.Table(), Records * Stride / 4u);
    const std::array<ShaderRecompiler::MemoryRegion, 4> memory{{
        {AddressOf(code.data()), std::as_bytes(code)},
        {srtAddress, std::as_bytes(std::span(Srt))},
        {AddressOf(table.data()), std::as_bytes(table)},
        {AddressOf(Keys.data()), std::as_bytes(std::span(Keys))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {true, false, false}, false, 1};
    const ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, AddressOf(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    auto result = ShaderRecompiler::Recompile(request);
    const auto binding = std::find_if(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& candidate) { return candidate.role == ShaderRecompiler::DescriptorRole::ImageTable; });
    Require(binding != result.bindings.end() && binding->guestDescriptor.size() == static_cast<std::size_t>(Records) * 8u, "image table reuse: the shader does not bind the whole table");
    return result;
}

void SetOutput(const void* output) {
    const auto view = Buffer(output, static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(view.begin(), view.end(), Srt.begin() + 8);
}

void Store(AgcDriver::VulkanDevice& device, const GuestBlock& block, std::uint32_t texture) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto image = Image(block.Texels(texture));
    std::copy(image.begin(), image.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(StoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{AddressOf(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, AddressOf(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, AddressOf(code.data()));
    device.WaitIdle();
}

std::vector<std::uint32_t> Dispatch(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& result, std::uint32_t record) {
    Keys[0] = record;
    std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
    device.Dispatch(result, 1, 1, 1, {}, AddressOf(TableCode.data()));
    device.WaitIdle();
    std::vector<std::uint32_t> red(Lanes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) red[lane] = Output[lane * 4u];
    return red;
}

std::vector<std::uint32_t> Row(const float* texels) {
    std::vector<std::uint32_t> row(Lanes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) row[lane] = std::bit_cast<std::uint32_t>(texels[lane % Width]);
    return row;
}

struct Step {
    std::uint64_t reused;
    std::uint64_t resolved;
};

Step Since(const AgcDriver::Graphics::ImageTableReuseCounts& before) {
    const auto now = AgcDriver::Graphics::ImageTableReuses();
    return {now.reused - before.reused, now.resolved - before.resolved};
}

void Expect(const AgcDriver::Graphics::ImageTableReuseCounts& before, std::uint64_t reused, std::uint64_t resolved, const std::string& what) {
    const auto step = Since(before);
    Require(step.reused == reused && step.resolved == resolved, "image table reuse: " + what + ": " + std::to_string(step.reused) + " reused and " + std::to_string(step.resolved) + " resolved tables, expected " + std::to_string(reused) + " and " + std::to_string(resolved));
}

void RunImageStore(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    for (std::uint32_t texture = 0; texture < Records; ++texture) {
        Fill(block, texture, 0.125f);
        SetRecord(block, texture, texture);
    }
    const auto srt = Buffer(block.Table(), Records * Stride);
    std::copy(srt.begin(), srt.end(), Srt.begin());
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 25) == Row(block.Texels(25)), "image table reuse: the table before an image store sampled the wrong entry");
    Require(Dispatch(device, result, 27) == Row(block.Texels(27)), "image table reuse: the table resolved again before an image store sampled the wrong entry");
    Require(Dispatch(device, result, 23) == Row(block.Texels(23)), "image table reuse: the table reused before an image store sampled the wrong entry");
    Store(device, block, 23);
    std::vector<float> stored(Width);
    for (std::uint32_t x = 0; x < Width; ++x) stored[x] = -4.0f - static_cast<float>(x);
    Require(Dispatch(device, result, 23) == Row(stored.data()), "image table reuse: an entry an image store left results pending over kept its old texels");
    Expect(before, 3 - CpuWriteResolutions, CpuWriteResolutions + 1, "CPU writes to a new table's 64 KiB block, then an image store leaving results pending over an entry");
    AgcDriver::Graphics::StorageTexture::FlushPending(AddressOf(block.Texels(23)), TextureBytes, nullptr, "test");
    device.WaitIdle();
    Require(Row(block.Texels(23)) == Row(stored.data()), "image table reuse: the image store wrote the wrong values");
}

void RunTests(AgcDriver::VulkanDevice& device, const GuestBlock& block, const GuestBlock& stores) {
    for (std::uint32_t texture = 0; texture < Records; ++texture) {
        Fill(block, texture, 0.25f);
        SetRecord(block, texture, texture);
    }
    Fill(block, Moved, 0.5f);
    for (std::uint32_t key = 0; key < Records; ++key) Keys[key] = key;
    const auto srt = Buffer(block.Table(), Records * Stride);
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    std::copy(srt.begin(), srt.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4);
    SetOutput(Output.data());
    const auto result = Compile(device, block);
    const bool watched = AgcDriver::GuestMemory::WriteWatched();

    std::vector<std::uint32_t> unbumped[2];
    auto before = AgcDriver::Graphics::ImageTableReuses();
    std::thread thread([&] {
        unbumped[0] = Dispatch(device, result, 3);
        unbumped[1] = Dispatch(device, result, 3);
    });
    thread.join();
    Require(unbumped[0] == Row(block.Texels(3)) && unbumped[1] == Row(block.Texels(3)), "image table reuse: a thread without a collect epoch sampled the wrong entry");
    Expect(before, 0, 2, "a thread without a collect epoch");

    AgcDriver::GuestMemory::BumpCollectEpoch();
    before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 5) == Row(block.Texels(5)), "image table reuse: the first dispatch of the epoch sampled the wrong entry");
    Require(Dispatch(device, result, 9) == Row(block.Texels(9)), "image table reuse: a dispatch reusing the table sampled the wrong entry");
    Require(Dispatch(device, result, 5) == Row(block.Texels(5)), "image table reuse: the third dispatch of the epoch sampled the wrong entry");
    if (!watched) {
        Expect(before, 0, 3, "guest memory without a write watch");
        std::puts("guest memory has no write watch: image table reuse is not tested");
        return;
    }
    Expect(before, 2, 1, "three dispatches of one table in one epoch");

    before = AgcDriver::Graphics::ImageTableReuses();
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
    Require(Dispatch(device, result, 7) == Row(block.Texels(7)), "image table reuse: the table after the texture cache was cleared sampled the wrong entry");
    Require(Dispatch(device, result, 9) == Row(block.Texels(9)), "image table reuse: the table resolved after the texture cache was cleared sampled the wrong entry");
    Expect(before, 1, 1, "a kept table whose textures the texture cache dropped");

    before = AgcDriver::Graphics::ImageTableReuses();
    std::vector<float> stored(Width * Height);
    for (std::uint32_t texel = 0; texel < Width * Height; ++texel) stored[texel] = -1.0f - static_cast<float>(texel);
    AgcDriver::GuestMemory::Write(AddressOf(block.Texels(11)), std::as_bytes(std::span(stored)));
    Require(Dispatch(device, result, 11) == Row(stored.data()), "image table reuse: an entry the driver stored over in the epoch kept its old texels");
    Require(Dispatch(device, result, 11) == Row(stored.data()), "image table reuse: the table resolved after a driver store sampled the wrong entry");
    Expect(before, 1, 1, "a driver store over an entry");

    before = AgcDriver::Graphics::ImageTableReuses();
    Fill(block, 17, 0.75f);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(Dispatch(device, result, 17) == Row(block.Texels(17)), "image table reuse: an entry the CPU wrote before the epoch kept its old texels");
    Require(Dispatch(device, result, 3) == Row(block.Texels(3)), "image table reuse: an entry stamped by another entry's collect sampled the wrong texels");
    Require(Dispatch(device, result, 17) == Row(block.Texels(17)), "image table reuse: the table of the new epoch sampled the wrong entry");
    Expect(before, 3 - CpuWriteResolutions, CpuWriteResolutions, "a CPU write collected in a new epoch, then two dispatches");

    before = AgcDriver::Graphics::ImageTableReuses();
    SetRecord(block, 19, Moved);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto moved = Compile(device, block);
    Require(Dispatch(device, moved, 19) == Row(block.Texels(Moved)), "image table reuse: a record pointed at another texture sampled the old one");
    Require(Dispatch(device, result, 19) == Row(block.Texels(19)), "image table reuse: the original table sampled the moved record");
    Require(Dispatch(device, moved, 19) == Row(block.Texels(Moved)), "image table reuse: the moved table sampled the wrong entry when reused");
    Require(Dispatch(device, result, 21) == Row(block.Texels(21)), "image table reuse: the original table sampled the wrong entry when reused");
    Expect(before, 2, 2, "two tables that differ in one record");

    SetOutput(block.Texels(13));
    const auto writer = Compile(device, block);
    SetOutput(Output.data());
    before = AgcDriver::Graphics::ImageTableReuses();
    static_cast<void>(Dispatch(device, writer, 7));
    const auto sampled = Dispatch(device, result, 13);
    const std::vector<float> written(block.Texels(13), block.Texels(13) + Width * Height);
    Require(std::bit_cast<std::uint32_t>(written[0]) == Row(block.Texels(7))[0], "image table reuse: the dispatch writing into an entry's texels stored the wrong values");
    Require(sampled == Row(written.data()), "image table reuse: an entry a dispatch wrote in the epoch kept its old texels");
    Expect(before, 1, 1, "a dispatch writing an entry's texels");
    Require(Dispatch(device, result, 15) == Row(block.Texels(15)), "image table reuse: the table after a dispatch wrote an entry sampled the wrong entry");

    RunImageStore(device, stores);
}

#ifdef _WIN32
namespace TrackedPending {
alignas(256) constexpr std::array<std::uint32_t, 30> PendingTableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0xf4280a02u, 0x24000020u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu,
    0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c0f08u, 0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u,
    0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};
constexpr std::size_t PendingOutputWords = Lanes * 4 + 2;
constexpr std::size_t PPage = 65536, PBytes = 2 * PPage, PReservationBytes = 4 * PPage;
constexpr std::uint32_t PHeight = 512, PWords = PBytes / 4, PGuard = 0x6a39c5e7u;
alignas(256) const auto PCodeLow = [] { auto code = PendingTableCode; code[20] = std::bit_cast<std::uint32_t>(0.5f / PHeight); return code; }();
alignas(256) const auto PCodeHigh = [] { auto code = PendingTableCode; code[20] = std::bit_cast<std::uint32_t>(256.5f / PHeight); return code; }();

class Surface {
public:
    Surface() {
        base = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(PReservationBytes, PPage));
        Require(base != nullptr, "tracked pending: reserve failed");
        GuestArena::GuestArenaCommit_nid_postfix(base, PReservationBytes, PAGE_READWRITE, PPage);
        GuestAllocations::Mutation().Add(base, PReservationBytes - 256, true, true, true);
        std::fill_n(reinterpret_cast<std::uint32_t*>(base), PPage / 4, PGuard);
        std::fill_n(reinterpret_cast<std::uint32_t*>(base + 3 * PPage), PPage / 4, PGuard);
    }
    ~Surface() {
        GuestAllocations::Mutation().Remove(base);
        GuestArena::GuestArenaReset_nid_postfix(base, PReservationBytes);
        GuestArena::GuestArenaRelease_nid_postfix(base, PReservationBytes);
    }
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    float* Data() const { return reinterpret_cast<float*>(base + PPage); }
    void Check(const std::vector<float>& expected) const {
        for (std::size_t word = 0; word < PWords; ++word)
            Require(std::bit_cast<std::uint32_t>(Data()[word]) == std::bit_cast<std::uint32_t>(expected[word]), "tracked pending: complete surface word mismatch");
        for (std::size_t word = 0; word < PPage / 4; ++word)
            Require(reinterpret_cast<const std::uint32_t*>(base)[word] == PGuard && reinterpret_cast<const std::uint32_t*>(base + 3 * PPage)[word] == PGuard,
                    "tracked pending: outer surface guard changed");
    }
private:
    std::uint8_t* base = nullptr;
};

std::array<std::uint32_t, 8> Descriptor(const Surface& surface) {
    std::array<std::uint32_t, 8> words{};
    const auto image = Image(surface.Data());
    std::copy(image.begin(), image.end(), words.begin());
    words[2] = ((Width - 1u) >> 2u) | ((PHeight - 1u) << 14u) | (1u << 31u);
    const auto decoded = AgcDriver::Graphics::DecodeTextureResource(words);
    Require(decoded.baseAddress == AddressOf(surface.Data()) && decoded.width == Width && decoded.height == PHeight,
            "tracked pending: address or dimensions did not round-trip through descriptor");
    Require(AgcDriver::Graphics::DescribeSurface(decoded).guestBytes == PBytes,
            "tracked pending: descriptor size differs from independent geometry");
    return words;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const GuestBlock& table,
    std::span<const std::uint32_t> code, std::span<const std::uint32_t> keys) {
    const auto srtAddress = AddressOf(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const auto descriptors = std::span(table.Table(), Records * Stride / 4u);
    const std::array<ShaderRecompiler::MemoryRegion, 4> memory{{
        {AddressOf(code.data()), std::as_bytes(code)}, {srtAddress, std::as_bytes(std::span(Srt))},
        {AddressOf(descriptors.data()), std::as_bytes(descriptors)}, {AddressOf(keys.data()), std::as_bytes(keys)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{{ShaderStage::Compute, AddressOf(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory}, device.Target(), {0, 0, 0, 128}};
    request.useCache = false;
    auto result = ShaderRecompiler::Recompile(request);
    const auto binding = std::find_if(result.bindings.begin(), result.bindings.end(), [](const auto& candidate) {
        return candidate.role == ShaderRecompiler::DescriptorRole::ImageTable;
    });
    Require(binding != result.bindings.end() && binding->guestDescriptor.size() == Records * 8, "tracked pending: all48 descriptors were not materialized");
    for (unsigned record = 0; record < Records; ++record) for (unsigned word = 0; word < 8; ++word)
        Require(binding->guestDescriptor[record * 8 + word] == table.Table()[record * (Stride / 4) + 4 + word], "tracked pending: descriptor word changed");
    return result;
}

int RunTrackedPendingTable(bool mixedProof, bool disjoint) {
    Require(!(mixedProof && disjoint), "tracked pending: cases must remain distinct");
    Require(std::getenv("APS5_NO_RESOURCE_CACHE") != nullptr,
            "tracked pending: forced-resource-build diagnostic flags required");
    GuestBlock table, io;
    std::fill_n(table.Table(), BlockBytes / 4, PGuard);
    std::fill_n(io.Table(), BlockBytes / 4, PGuard);
    std::vector<std::unique_ptr<Surface>> surfaces;
    std::vector<std::vector<float>> expected(Records + (disjoint ? 1u : 0u), std::vector<float>(PWords, 0.0f));
    for (unsigned record = 0; record < expected.size(); ++record) {
        surfaces.push_back(std::make_unique<Surface>());
        for (unsigned i = 0; i < PWords; ++i)
            surfaces.back()->Data()[i] = expected[record][i] = 0.25f + record * 32768.0f + i;
        if (record == Records) continue;
        auto* descriptor = table.Table() + record * (Stride / 4);
        std::fill_n(descriptor, Stride / 4, 0u);
        std::copy(PointClamp.begin(), PointClamp.end(), descriptor);
        const auto image = Descriptor(*surfaces.back());
        std::copy(image.begin(), image.end(), descriptor + 4);
    }
    auto* output = io.Table(); auto* keys = io.Table() + 1024; auto* writerKeys = io.Table() + 2048;
    for (unsigned i = 0; i < Records; ++i) keys[i] = writerKeys[i] = i;
    const auto tableView = Buffer(table.Table(), Records * Stride), keyView = Buffer(keys, Records * 4), writerKeyView = Buffer(writerKeys, Records * 4);
    std::copy(tableView.begin(), tableView.end(), Srt.begin());
    std::copy(keyView.begin(), keyView.end(), Srt.begin() + 4); SetOutput(output);
    const auto descriptors = std::vector<std::uint32_t>(table.Table(), table.Table() + Records * Stride / 4);
    const auto device = OpenVulkanTestDevice();
    if (!device) return VulkanTestSkipped;
    Require(AgcDriver::GuestMemory::WriteWatched(), "tracked pending: write watch unavailable");
    const auto low = Compile(*device, table, PCodeLow, {keys, Records});
    const auto high = Compile(*device, table, PCodeHigh, {keys, Records});
    constexpr unsigned Queued = 13;
    const unsigned destination = disjoint ? Records : Queued;
    std::copy(writerKeyView.begin(), writerKeyView.end(), Srt.begin() + 4); SetOutput(surfaces[destination]->Data());
    const auto writer = Compile(*device, table, PCodeLow, {writerKeys, Records});
    std::copy(keyView.begin(), keyView.end(), Srt.begin() + 4); SetOutput(output);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    if (mixedProof) AgcDriver::GuestMemory::Unwatch(AddressOf(surfaces.back()->Data()), PBytes);
    for (unsigned record = 0; record < expected.size(); ++record) {
        const auto address = AddressOf(surfaces[record]->Data());
        const auto topology = AgcDriver::GuestMemory::CommittedRanges(address, PBytes);
        Require(topology.size() == 1 && topology[0] == std::pair{address, address + PBytes}, "tracked pending: committed topology mismatch");
        const auto whole = AgcDriver::GuestMemory::CollectWrites(address, PBytes);
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        const auto committed = AgcDriver::GuestMemory::CollectCommittedWrites(address, PBytes, ranges);
        if (mixedProof && record == Records - 1) {
            Require(whole == 0 && committed == 0 && !AgcDriver::GuestMemory::Watched(address, PBytes) &&
                    AgcDriver::GuestMemory::UntrackedHint(address, PBytes) != 0, "tracked pending: partial table lacks its hinted control");
        } else {
            Require(whole != 0 && committed != 0 && AgcDriver::GuestMemory::Watched(address, PPage) &&
                    AgcDriver::GuestMemory::Watched(address + PPage, PPage), "tracked pending: whole surface is not tracked");
        }
    }
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto check = [&] {
        Require(std::equal(descriptors.begin(), descriptors.end(), table.Table()), "tracked pending: descriptor input changed");
        for (unsigned record = 0; record < expected.size(); ++record) surfaces[record]->Check(expected[record]);
        for (std::size_t word = descriptors.size(); word < BlockBytes / 4; ++word) Require(table.Table()[word] == PGuard, "tracked pending: table guard changed");
        for (std::size_t word = 0; word < BlockBytes / 4; ++word) {
            if (word < PendingOutputWords || (word >= 1024 && word < 1024 + Records) || (word >= 2048 && word < 2048 + Records)) continue;
            Require(io.Table()[word] == PGuard, "tracked pending: IO guard changed");
        }
        for (unsigned i = 1; i < Records; ++i) Require(keys[i] == i, "tracked pending: consumer key changed");
        for (unsigned i = 0; i < Records; ++i) Require(writerKeys[i] == i, "tracked pending: immutable writer key changed");
    };
    const auto sample = [&](unsigned record, bool upper, const char* phase) {
        keys[0] = record; std::fill_n(output, PendingOutputWords, 0xdeadbeefu);
        const auto& program = upper ? high : low;
        device->Dispatch(program, 1, 1, 1, {}, AddressOf(upper ? PCodeHigh.data() : PCodeLow.data()));
        if (disjoint && std::string_view(phase) == "queued-consumer")
            Require(AgcDriver::Graphics::Recorder::SnapshotWriteOverlaps(AddressOf(surfaces[destination]->Data()), PBytes),
                    "tracked pending: disjoint consumer unnecessarily published the pending writer");
        device->WaitIdle();
        for (unsigned lane = 0; lane < Lanes; ++lane) {
            const auto index = (upper ? PPage / 4 : 0) + lane;
            const std::array words{std::bit_cast<std::uint32_t>(expected[record][index]), 0u, 0u, 0x3f800000u};
            for (unsigned channel = 0; channel < 4; ++channel)
                Require(output[lane * 4 + channel] == words[channel], "tracked pending: independent RGBA mismatch " + std::string(phase) + " lane=" + std::to_string(lane));
        }
        Require(output[Lanes * 4] == 0xdeadbeefu && output[Lanes * 4 + 1] == 0xdeadbeefu && keys[0] == record, "tracked pending: output/key sentinel changed");
        std::printf("tracked_pending_sample=PASS phase=%s record=%u row=%u\n", phase, record, upper ? 256u : 0u);
    };
    sample(0, false, "cold-low"); sample(0, true, "known-upper"); sample(Records - 1, false, "last-low");
    sample(Records - 1, true, "last-known-upper");
    for (unsigned i = 0; i < 4; ++i) sample(i, false, "stable");
    check();
    for (unsigned i = 0; i < Width; ++i) surfaces[3]->Data()[i] = expected[3][i] = -3000.5f - i;
    AgcDriver::GuestMemory::BumpCollectEpoch(); sample(3, false, "cpu-write"); check();
    for (unsigned i = 0; i < Width; ++i) expected[7][i] = 70000.25f + i;
    AgcDriver::GuestMemory::Write(AddressOf(surfaces[7]->Data()), std::as_bytes(std::span(expected[7]).first(Width)));
    sample(7, false, "driver-write"); check();
    sample(Queued, false, "queued-warm");
    const auto epoch = AgcDriver::GuestMemory::ThreadCollectEpoch();
    const auto ownership = std::array<std::uint64_t, 5>{epoch, AgcDriver::GuestMemory::UnwatchSerial(), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), AgcDriver::Graphics::DepthSurfacesSerial(), AgcDriver::GuestMemory::ForgetSerial()};
    device->Dispatch(writer, 1, 1, 1, {}, AddressOf(PCodeLow.data()));
    Require(AgcDriver::Graphics::Recorder::SnapshotWriteOverlaps(AddressOf(surfaces[destination]->Data()), PBytes), "tracked pending: queued buffer write not observed");
    Require(AgcDriver::GuestMemory::ThreadCollectEpoch() == epoch, "tracked pending: queued writer changed epoch");
    for (unsigned lane = 0; lane < Lanes; ++lane) {
        expected[destination][lane * 4] = expected[0][lane]; expected[destination][lane * 4 + 1] = 0.0f;
        expected[destination][lane * 4 + 2] = 0.0f; expected[destination][lane * 4 + 3] = 1.0f;
    }
    Require(std::array<std::uint64_t, 5>{AgcDriver::GuestMemory::ThreadCollectEpoch(), AgcDriver::GuestMemory::UnwatchSerial(), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), AgcDriver::Graphics::DepthSurfacesSerial(), AgcDriver::GuestMemory::ForgetSerial()} == ownership,
            "tracked pending: queued writer changed ownership proof state");
    const auto beforeConsumer = AgcDriver::Graphics::ImageTableReuses();
    sample(Queued, false, "queued-consumer");
    if (disjoint) Expect(beforeConsumer, 1, 0, "disjoint queued writer leaves table reusable");
    else Require(Since(beforeConsumer).resolved != 0, "tracked pending: overlapping write was reused without resolution");
    Require(!AgcDriver::Graphics::Recorder::SnapshotWriteOverlaps(AddressOf(surfaces[destination]->Data()), PBytes), "tracked pending: queued writer remained unpublished");
    check();
    AgcDriver::Graphics::ClearCachedTextures(device->Device());
    sample(Queued, false, "retired"); sample(5, true, "retired-upper"); check();
    std::printf("tracked_pending_table=PASS mixed=%u disjoint=%u full_RGBA_sources_guards_mutations_retirement=PASS\n", mixedProof ? 1u : 0u, disjoint ? 1u : 0u);
    return 0;
}
}
#endif

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && (std::string_view(argv[1]) == "--tracked-pending-write" || std::string_view(argv[1]) == "--tracked-pending-write-mixed" || std::string_view(argv[1]) == "--tracked-pending-write-disjoint")) {
#ifdef _WIN32
            return TrackedPending::RunTrackedPendingTable(std::string_view(argv[1]) == "--tracked-pending-write-mixed", std::string_view(argv[1]) == "--tracked-pending-write-disjoint");
#else
            std::puts("skipped, tracked pending snapshot control requires Windows write watching");
            return VulkanTestSkipped;
#endif
        }
        GuestBlock block;
        GuestBlock stores(UnimportableBytes);
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunTests(*device, block, stores);
        std::puts("image table reuse tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
