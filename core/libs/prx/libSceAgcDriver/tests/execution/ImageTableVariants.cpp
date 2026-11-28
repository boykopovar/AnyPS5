#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "ImageTableAbi.hpp"
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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::CompiledShader;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::ShaderResources;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Wide = 64;
constexpr std::uint32_t Narrow = 32;
constexpr std::uint32_t Spanning = 512;
constexpr std::uint32_t TailWidth = 256;
constexpr std::uint32_t Height = 4;
constexpr std::uint32_t TextureBytes = Wide * Height * 4;
constexpr std::uint32_t Stride = 48;
constexpr std::uint32_t Records = 48;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::size_t TexturesOffset = 65536;
constexpr std::size_t BlockBytes = TexturesOffset + 4 * 65536;
constexpr std::uint32_t Largest = 60;
constexpr std::uint32_t Written = 61;
constexpr std::uint32_t Depth = 62;
constexpr std::uint32_t Sized = 59;
constexpr std::uint32_t Tied = 58;
constexpr std::uint32_t Proved = 56;
constexpr std::uint32_t Swizzled = 55;
constexpr std::uint32_t Flagged = 54;
constexpr std::uint32_t Compressed = 64;
constexpr std::uint32_t Format1555Unorm = 134;
constexpr std::uint32_t Format32_32_32Float = 74;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::size_t KeysOffset = 32768;
constexpr std::size_t KeyCount = 65536 / 256;
constexpr std::size_t ArgumentsWord = 4096;

alignas(256) std::array<std::uint32_t, Records> Keys{};
alignas(256) std::array<std::uint32_t, Lanes * 4> Output{};
alignas(256) std::array<std::uint32_t, 32> Srt{};

alignas(256) constexpr std::array<std::uint32_t, 28> TableCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e020d01u, 0x060202f0u, 0x100202ffu,
    0x3c800000u, 0x7e0402ffu, 0x3e000000u, 0xf09c8f08u, 0x01090401u, 0x7e120202u, 0x34101286u, 0x4a101100u,
    0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

constexpr auto FullDescriptorTableCodeWords = [] {
    auto code = TableCode;
    code[10] = 0xf4300802u;
    code[19] = 0xf09c0f08u;
    return code;
}();
alignas(256) constexpr std::array<std::uint32_t, 28> FullDescriptorTableCode = FullDescriptorTableCodeWords;

alignas(256) constexpr std::array<std::uint32_t, 23> FetchCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0xf0008f08u, 0x00090401u,
    0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 23> SizeCode{
    0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080700u, 0xfa000020u, 0x8f108202u, 0xf4200444u,
    0x20000000u, 0x9312b011u, 0xf42c0802u, 0x24000000u, 0x360200bfu, 0x7e040280u, 0xf0388308u, 0x00090402u,
    0x7e120202u, 0x34101286u, 0x4a101100u, 0x34101084u, 0xe0781000u, 0x80070408u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e020d00u, 0x082802f7u, 0x7e3c0300u, 0x7e3e0280u, 0xf0201108u, 0x0001141eu, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 6> KeyStoreCode{
    0x34020082u, 0x7e0002c1u, 0xe0701000u, 0x80000001u, 0xbf810000u, 0xbf810000u,
};

constexpr std::array<std::uint32_t, 4> PointClamp{0x92u, (4u * 256u) << 12u, 0u, 0u};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 4> Image(const void* texels, std::uint32_t width, std::uint32_t format = Format32Float) {
    const auto address = AddressOf(texels);
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u), ((width - 1u) >> 2u) | ((Height - 1u) << 14u) | (1u << 31u), 0xfacu | (Type2D << 28u)};
}

std::array<std::uint32_t, 4> Buffer(const void* base, std::uint32_t bytes) {
    const auto address = AddressOf(base);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

class Unreachable {
public:
    Unreachable() {
#ifdef _WIN32
        memory = VirtualAlloc(nullptr, TextureBytes, MEM_RESERVE, PAGE_NOACCESS);
#else
        void* mapped = mmap(nullptr, TextureBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        memory = mapped == MAP_FAILED ? nullptr : mapped;
#endif
        Require(memory != nullptr, "image table variants: cannot reserve the unreachable surface");
    }

    ~Unreachable() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, TextureBytes);
#endif
    }

    const float* Texels() const { return static_cast<const float*>(memory); }

private:
    void* memory = nullptr;
};

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
        Require(block != nullptr, "image table variants: cannot allocate the guest block");
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
    std::uint8_t* KeyBytes() const { return block + KeysOffset; }

private:
    std::uint8_t* block = nullptr;
};

void SetRecord(const GuestBlock& table, std::uint32_t record, const float* texels, std::uint32_t width, std::uint32_t format = Format32Float) {
    auto* words = table.Table() + record * (Stride / 4u);
    std::fill(words, words + Stride / 4u, 0u);
    std::copy(PointClamp.begin(), PointClamp.end(), words);
    const auto image = Image(texels, width, format);
    std::copy(image.begin(), image.end(), words + 4);
}

std::uint32_t* RecordImage(const GuestBlock& table, std::uint32_t record) {
    return table.Table() + record * (Stride / 4u) + 4u;
}

std::array<std::uint32_t, 8> CompressedImage(const void* texels, std::uint32_t width, std::uint32_t format, const std::uint8_t* keys, bool alphaOnMsb) {
    std::array<std::uint32_t, 8> words{};
    const auto image = Image(texels, width, format);
    std::copy(image.begin(), image.end(), words.begin());
    words[3] |= TileRenderTarget64KB << 20u;
    const auto meta = AddressOf(keys) >> 8u;
    words[6] = (1u << 20u) | (1u << 21u) | (alphaOnMsb ? 1u << 22u : 0u) | (static_cast<std::uint32_t>(meta & 0xffu) << 24u);
    words[7] = static_cast<std::uint32_t>(meta >> 8u);
    return words;
}

void SetCompressedRecord(const GuestBlock& table, std::uint32_t record, std::uint32_t width, bool alphaOnMsb) {
    SetRecord(table, record, table.Texels(Compressed), width, Format1555Unorm);
    const auto words = CompressedImage(table.Texels(Compressed), width, Format1555Unorm, table.KeyBytes(), alphaOnMsb);
    std::copy(words.begin(), words.end(), RecordImage(table, record));
}

std::size_t RecordKeys(const GuestBlock& table, std::uint32_t record) {
    std::array<std::uint32_t, 8> words{};
    std::copy_n(RecordImage(table, record), words.size(), words.begin());
    return static_cast<std::size_t>(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(words)).guestBytes / 256u);
}

void Fill(float* texels, float bias) {
    for (std::uint32_t texel = 0; texel < Wide * Height; ++texel) texels[texel] = bias + static_cast<float>(texel);
}

void FillTable(const GuestBlock& block) {
    for (std::uint32_t record = 0; record < Records; ++record) {
        Fill(block.Texels(record), 0.25f + static_cast<float>(record) * 100.0f);
        SetRecord(block, record, block.Texels(record), Wide);
    }
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const GuestBlock& table, std::span<const std::uint32_t> code = TableCode, std::array<std::uint32_t, 3> partialThreads = {}, std::uint32_t pushBytes = 128) {
    const auto srtAddress = AddressOf(Srt.data());
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::span<const std::uint32_t> words(table.Table(), Records * Stride / 4u);
    const std::array<ShaderRecompiler::MemoryRegion, 4> memory{{
        {AddressOf(code.data()), std::as_bytes(code)},
        {srtAddress, std::as_bytes(std::span(Srt))},
        {AddressOf(words.data()), std::as_bytes(words)},
        {AddressOf(Keys.data()), std::as_bytes(std::span(Keys))},
    }};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {true, false, false}, false, 1, partialThreads};
    const ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, AddressOf(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, pushBytes}
    };
    auto result = ShaderRecompiler::Recompile(request);
    const auto binding = std::find_if(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& candidate) { return candidate.role == ShaderRecompiler::DescriptorRole::ImageTable; });
    Require(binding != result.bindings.end() && binding->guestDescriptor.size() == static_cast<std::size_t>(Records) * 8u, "image table variants: the shader does not bind the whole table");
    return result;
}

void Store(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> words) {
    std::vector<std::uint32_t> userData(16, 0u);
    std::copy(words.begin(), words.end(), userData.begin() + 4);
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

void Store(AgcDriver::VulkanDevice& device, const float* texels, std::uint32_t width) {
    Store(device, Image(texels, width));
}

std::vector<std::uint32_t> Dispatch(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& result, std::uint32_t record, const void* code = TableCode.data()) {
    Keys[0] = record;
    std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
    device.Dispatch(result, 1, 1, 1, {}, AddressOf(code));
    device.WaitIdle();
    std::vector<std::uint32_t> red(Lanes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) red[lane] = Output[lane * 4u];
    return red;
}

std::vector<std::uint32_t> Row(const float* texels, std::uint32_t width) {
    std::vector<std::uint32_t> row(Lanes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) row[lane] = std::bit_cast<std::uint32_t>(texels[lane * width / Lanes]);
    return row;
}

struct Variants {
    std::uint64_t surfaces;
    std::uint64_t substituted;
    std::uint64_t rejected;
};

Variants Since(const AgcDriver::Graphics::ImageTableReuseCounts& before) {
    const auto now = AgcDriver::Graphics::ImageTableReuses();
    return {now.variantSurfaces - before.variantSurfaces, now.variantSubstituted - before.variantSubstituted, now.variantRejected - before.variantRejected};
}

void Expect(const AgcDriver::Graphics::ImageTableReuseCounts& before, std::uint64_t surfaces, std::uint64_t substituted, std::uint64_t rejected, const std::string& what) {
    const auto step = Since(before);
    Require(step.surfaces == surfaces && step.substituted == substituted && step.rejected == rejected, "image table variants: " + what + ": " + std::to_string(step.surfaces) + " surfaces, " + std::to_string(step.substituted) + " substituted and " + std::to_string(step.rejected) + " rejected entries, expected " + std::to_string(surfaces) + ", " + std::to_string(substituted) + " and " + std::to_string(rejected));
}

void SetOutput() {
    const auto view = Buffer(Output.data(), static_cast<std::uint32_t>(sizeof(Output)));
    std::copy(view.begin(), view.end(), Srt.begin() + 8);
}

void Prepare(const GuestBlock& table) {
    FillTable(table);
    for (std::uint32_t key = 0; key < Records; ++key) Keys[key] = key;
    const auto srt = Buffer(table.Table(), Records * Stride);
    const auto keys = Buffer(Keys.data(), static_cast<std::uint32_t>(sizeof(Keys)));
    std::copy(srt.begin(), srt.end(), Srt.begin());
    std::copy(keys.begin(), keys.end(), Srt.begin() + 4);
    SetOutput();
}

void RunLargest(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Largest), Wide);
    SetRecord(block, 1, block.Texels(Largest), Narrow);
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1) == Row(block.Texels(Largest), Wide), "image table variants: with no write and no depth surface, the narrow variant did not borrow the largest one");
    Require(Dispatch(device, result, 0) == Row(block.Texels(Largest), Wide), "image table variants: the largest variant sampled the wrong texels");
    Require(Dispatch(device, result, 5) == Row(block.Texels(5), Wide), "image table variants: an entry outside the family sampled the wrong texels");
    Expect(before, 1, 3, 0, "three dispatches of two variants with no write and no depth surface");
}

void RunReused(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Largest), Wide);
    SetRecord(block, 1, block.Texels(Largest), Narrow);
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1) == Row(block.Texels(Largest), Wide), "image table variants: the narrow variant did not borrow the largest one");
    Require(Dispatch(device, result, 1) == Row(block.Texels(Largest), Wide), "image table variants: the narrow variant of a reused table did not borrow the largest one");
    const auto reused = AgcDriver::Graphics::ImageTableReuses().reused - before.reused;
    Require(reused != 0u, "image table variants: a table whose borrowing entry shares a cached representative was resolved again instead of reused");
    const auto step = Since(before);
    Require(step.substituted == 2u && step.rejected == 0u, "image table variants: two dispatches of a reused table with a borrowing entry: " + std::to_string(step.substituted) + " substituted and " + std::to_string(step.rejected) + " rejected entries, expected 2 and 0");
}

void RunReusedWritten(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Written), Wide);
    SetRecord(block, 1, block.Texels(Written), Narrow);
    const auto result = Compile(device, block);
    Store(device, block.Texels(Written), Wide);
    std::vector<float> wide(Wide);
    for (std::uint32_t x = 0; x < Wide; ++x) wide[x] = -4.0f - static_cast<float>(x);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(Dispatch(device, result, 1) == Row(wide.data(), Wide), "image table variants: the narrow variant did not borrow the wide one written last");
    Store(device, block.Texels(Written), Narrow);
    std::vector<float> narrow(Narrow);
    for (std::uint32_t x = 0; x < Narrow; ++x) narrow[x] = -4.0f - static_cast<float>(x);
    Require(Dispatch(device, result, 0) == Row(narrow.data(), Narrow), "image table variants: after a write of the narrow variant, the wide variant of a remembered table did not borrow it");
}

std::uint64_t GuestBytes(const void* texels, std::uint32_t width) {
    std::array<std::uint32_t, 8> words{};
    const auto image = Image(texels, width);
    std::copy(image.begin(), image.end(), words.begin());
    return AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(words)).guestBytes;
}

void RunTied(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Require(GuestBytes(block.Texels(Tied), Narrow) == GuestBytes(block.Texels(Tied), Wide), "image table variants: the narrow and the wide variant no longer have the same guest bytes");
    Prepare(block);
    Fill(block.Texels(Tied), 7000.25f);
    SetRecord(block, 0, block.Texels(Tied), Narrow);
    SetRecord(block, 1, block.Texels(Tied), Wide);
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const auto wide = Row(block.Texels(Tied), Wide);
    Require(Dispatch(device, result, 0) == wide, "image table variants: with the same guest bytes, the narrow variant ahead of the wide one did not borrow the wide one");
    Require(Dispatch(device, result, 1) == wide, "image table variants: with the same guest bytes, the wide variant behind the narrow one sampled the wrong texels");
    Expect(before, 1, 2, 0, "two dispatches of two variants with the same guest bytes");
}

void RunWritten(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Written), Wide);
    SetRecord(block, 1, block.Texels(Written), Narrow);
    const auto result = Compile(device, block);
    Store(device, block.Texels(Written), Narrow);
    std::vector<float> stored(Narrow);
    for (std::uint32_t x = 0; x < Narrow; ++x) stored[x] = -4.0f - static_cast<float>(x);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 0) == Row(stored.data(), Narrow), "image table variants: the wide variant did not borrow the narrow one written last");
    Require(Dispatch(device, result, 1) == Row(stored.data(), Narrow), "image table variants: the narrow variant written last sampled the wrong texels");
    Expect(before, 1, 2, 0, "two dispatches of two variants, the narrow one written last");
}

void RunFetch(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Sized), Wide);
    SetRecord(block, 1, block.Texels(Sized), Narrow);
    const auto result = Compile(device, block, FetchCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::vector<std::uint32_t> texels(Lanes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) texels[lane] = std::bit_cast<std::uint32_t>(block.Texels(Sized)[lane]);
    Require(Dispatch(device, result, 0, FetchCode.data()) == texels, "image table variants: a texel fetch from the largest variant read the wrong texels");
    Require(Dispatch(device, result, 1, FetchCode.data()) == texels, "image table variants: a texel fetch from a borrowing entry did not read the texels of the texture it borrows, past its own width");
}

void RunSize(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Sized), Wide);
    SetRecord(block, 1, block.Texels(Sized), Narrow);
    const auto result = Compile(device, block, SizeCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(Dispatch(device, result, 5, SizeCode.data()) == std::vector<std::uint32_t>(Lanes, Wide), "image table variants: an entry outside the family did not report its own width");
    Require(Dispatch(device, result, 0, SizeCode.data()) == std::vector<std::uint32_t>(Lanes, Wide), "image table variants: the largest variant did not report its own width");
    Require(Dispatch(device, result, 1, SizeCode.data()) == std::vector<std::uint32_t>(Lanes, Wide), "image table variants: a borrowing entry did not report the width of the texture it borrows");
}

void RunDepth(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    SetRecord(block, 0, block.Texels(Depth), Wide);
    SetRecord(block, 1, block.Texels(Depth), Narrow);
    const auto result = Compile(device, block);
    const auto context = device.graphicsContext();
    AgcDriver::Graphics::DepthSurfaceView(context, AgcDriver::Graphics::DepthTarget{AddressOf(block.Texels(Depth)), 0, {Narrow, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    Require(AgcDriver::Graphics::ActiveDepthExtent(device.Device(), AddressOf(block.Texels(Depth))).has_value(), "image table variants: the depth surface is not active");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const auto wide = Dispatch(device, result, 0);
    const auto narrow = Dispatch(device, result, 1);
    Require(wide == narrow, "image table variants: the two variants of a surface under a depth surface sampled different texels");
    Require(wide != Row(block.Texels(Depth), Wide), "image table variants: the wide variant sampled guest memory instead of the depth surface");
    for (std::uint32_t lane = Narrow; lane < Lanes; ++lane) Require(wide[lane] == wide[Narrow - 1u], "image table variants: the wide variant under a narrow depth surface did not clamp to the depth surface extent");
    Expect(before, 1, 2, 0, "two dispatches of two variants under an active depth surface");
    AgcDriver::Graphics::ClearDepthSurfaces(device.Device());
}

void RunRejected(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    const Unreachable surface;
    Prepare(block);
    SetRecord(block, 0, surface.Texels(), Wide);
    SetRecord(block, 1, surface.Texels(), Narrow);
    const auto result = Compile(device, block);
    AgcDriver::Graphics::DepthSurfaceView(device.graphicsContext(), AgcDriver::Graphics::DepthTarget{AddressOf(surface.Texels()), 0, {Wide, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    static_cast<void>(Dispatch(device, result, 1));
    Expect(before, 0, 0, 1, "a narrow variant whose representative is not mapped");
    AgcDriver::Graphics::ClearDepthSurfaces(device.Device());
}

void RunAlphaPlacement(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    std::fill_n(block.KeyBytes(), KeyCount, std::uint8_t{0x40});
    SetCompressedRecord(block, 0, Wide, true);
    SetCompressedRecord(block, 1, Narrow, false);
    const auto result = Compile(device, block, FullDescriptorTableCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == std::vector<std::uint32_t>(Lanes, std::bit_cast<std::uint32_t>(1.0f)), "image table variants: the narrow 1_5_5_5 variant with the DCC alpha in its first channel did not read the 0001 clear of its own placement (0x001f)");
    Require(Dispatch(device, result, 0, FullDescriptorTableCode.data()) == std::vector<std::uint32_t>(Lanes, 0u), "image table variants: the wide 1_5_5_5 variant with the DCC alpha in its last channel did not read the 0001 clear of its own placement (0x8000)");
    Expect(before, 0, 0, 0, "two variants of a surface under 0001 keys with different DCC alpha placements");
}

void SetSpanningRecords(const GuestBlock& block, std::size_t& narrowKeys, std::size_t& wideKeys) {
    SetCompressedRecord(block, 0, Spanning, false);
    SetCompressedRecord(block, 1, Narrow, false);
    narrowKeys = RecordKeys(block, 1);
    wideKeys = RecordKeys(block, 0);
    Require(narrowKeys < wideKeys && KeysOffset + wideKeys <= TexturesOffset && TexturesOffset + Compressed * TextureBytes + wideKeys * 256u <= BlockBytes, "image table variants: the spanning 1_5_5_5 variant does not cover more DCC keys than the narrow one inside the guest block");
    std::memset(block.Texels(Compressed), 0, wideKeys * 256u);
}

void RunKeySpans(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    std::fill_n(block.KeyBytes(), narrowKeys, std::uint8_t{0x40});
    std::fill_n(block.KeyBytes() + narrowKeys, wideKeys - narrowKeys, std::uint8_t{0xff});
    const auto result = Compile(device, block, FullDescriptorTableCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == std::vector<std::uint32_t>(Lanes, std::bit_cast<std::uint32_t>(1.0f)), "image table variants: the narrow 1_5_5_5 variant whose own DCC keys are a uniform 0001 clear read the stored texels of the wide variant whose keys are mixed");
    Require(Dispatch(device, result, 0, FullDescriptorTableCode.data()) == std::vector<std::uint32_t>(Lanes, 0u), "image table variants: the wide 1_5_5_5 variant over mixed DCC keys did not read its stored texels");
    Expect(before, 0, 0, 0, "two variants over different DCC key spans");
}

void RunSharedKeys(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0x40});
    const auto result = Compile(device, block, FullDescriptorTableCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const std::vector<std::uint32_t> clear(Lanes, std::bit_cast<std::uint32_t>(1.0f));
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == clear, "image table variants: the narrow 1_5_5_5 variant under the 0001 clear of the whole DCC range did not read the clear");
    Expect(before, 1, 1, 0, "a narrow variant under the same 0001 clear as the wide one");
    Require(Dispatch(device, result, 0, FullDescriptorTableCode.data()) == clear, "image table variants: the wide 1_5_5_5 variant under a 0001 clear did not read the clear");
}

void ExpectShared(const AgcDriver::Graphics::ImageTableReuseCounts& before, std::uint64_t substituted, const std::string& what) {
    const auto step = Since(before);
    Require(step.substituted == substituted && step.rejected == 0u, "image table variants: " + what + ": " + std::to_string(step.substituted) + " substituted and " + std::to_string(step.rejected) + " rejected entries, expected " + std::to_string(substituted) + " and 0");
}

void RunUncompressedSpans(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    std::memset(block.Texels(Compressed), 0xff, wideKeys * 256u);
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0xff});
    const auto result = Compile(device, block, FullDescriptorTableCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const std::vector<std::uint32_t> stored(Lanes, std::bit_cast<std::uint32_t>(1.0f));
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == stored, "image table variants: the narrow 1_5_5_5 variant under uniform uncompressed DCC keys did not read its stored texels");
    ExpectShared(before, 1, "a narrow variant under the same uncompressed keys as the wide one");
    Require(Dispatch(device, result, 0, FullDescriptorTableCode.data()) == stored, "image table variants: the wide 1_5_5_5 variant under uniform uncompressed DCC keys did not read its stored texels");
}

void RunNarrowRepresentative(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0x40});
    const auto result = Compile(device, block, FullDescriptorTableCode);
    AgcDriver::Graphics::DepthSurfaceView(device.graphicsContext(), AgcDriver::Graphics::DepthTarget{AddressOf(block.Texels(Compressed)), 0, {Narrow, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    const std::vector<std::uint32_t> clear(Lanes, std::bit_cast<std::uint32_t>(1.0f));
    Require(Dispatch(device, result, 0, FullDescriptorTableCode.data()) == clear, "image table variants: the wide 1_5_5_5 variant borrowing the narrow one under a 0001 clear of both key spans did not read the clear");
    ExpectShared(before, 1, "a wide variant under the same 0001 clear as the narrow representative");
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == clear, "image table variants: the narrow 1_5_5_5 representative under a 0001 clear did not read the clear");
    AgcDriver::Graphics::ClearDepthSurfaces(device.Device());
}

void StoreKeys(AgcDriver::VulkanDevice& device, const std::uint8_t* keys, std::uint32_t bytes) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto view = Buffer(keys, bytes);
    std::copy(view.begin(), view.end(), userData.begin());
    const std::span<const std::uint32_t> code(KeyStoreCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{AddressOf(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, AddressOf(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    device.Dispatch(ShaderRecompiler::Recompile(request), 1, 1, 1, {}, AddressOf(code.data()));
}

void RunPendingTail(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    constexpr std::size_t Unaligned = 4;
    if (device.graphicsContext().limits.minStorageBufferOffsetAlignment <= Unaligned) {
        std::printf("pending key tail case skipped: minStorageBufferOffsetAlignment %llu binds the key store in place\n", static_cast<unsigned long long>(device.graphicsContext().limits.minStorageBufferOffsetAlignment));
        return;
    }
    Prepare(block);
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0x40});
    const auto result = Compile(device, block, FullDescriptorTableCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const std::vector<std::uint32_t> clear(Lanes, std::bit_cast<std::uint32_t>(1.0f));
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == clear, "image table variants: the narrow 1_5_5_5 variant under a 0001 clear of both key spans did not read the clear");
    StoreKeys(device, block.KeyBytes() + narrowKeys + Unaligned, static_cast<std::uint32_t>(wideKeys - narrowKeys - 2u * Unaligned));
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == clear, "image table variants: the narrow 1_5_5_5 variant under a 0001 clear borrowed the stored texels of the wide variant, whose key tail a pending copied store made uncompressed");
    Require(Dispatch(device, result, 0, FullDescriptorTableCode.data()) == std::vector<std::uint32_t>(Lanes, 0u), "image table variants: the wide 1_5_5_5 variant whose key tail a copied store made uncompressed did not read its stored texels");
}

void RunPendingStorageTail(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    const auto tail = CompressedImage(block.Texels(Compressed) + narrowKeys * 64u, TailWidth, Format32Float, block.KeyBytes() + narrowKeys, false);
    const auto tailKeys = static_cast<std::size_t>(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(tail)).guestBytes / 256u);
    Require(narrowKeys + tailKeys > wideKeys && KeysOffset + narrowKeys + tailKeys <= TexturesOffset && TexturesOffset + Compressed * TextureBytes + (narrowKeys + tailKeys) * 256u <= BlockBytes, "image table variants: the stored tail image does not reach past the wide 1_5_5_5 variant's DCC keys inside the guest block");
    std::memset(block.Texels(Compressed), 0xff, wideKeys * 256u);
    std::fill_n(block.KeyBytes(), narrowKeys + tailKeys, std::uint8_t{0});
    const auto result = Compile(device, block, FullDescriptorTableCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const std::vector<std::uint32_t> clear(Lanes, 0u);
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == clear, "image table variants: the narrow 1_5_5_5 variant under a 0000 clear of both key spans did not read the clear");
    ExpectShared(before, 1, "a narrow variant under the same 0000 clear as the wide one");
    const auto clean = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == clear, "image table variants: the narrow 1_5_5_5 variant of a reused table under a 0000 clear did not read the clear");
    Require(AgcDriver::Graphics::ImageTableReuses().reused != clean.reused, "image table variants: a table whose narrow variant borrows under a 0000 clear was resolved again instead of reused");
    Store(device, tail);
    const auto stored = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == clear, "image table variants: the narrow 1_5_5_5 variant under a 0000 clear borrowed the stored texels of the wide variant, whose key tail a pending storage write made uncompressed");
    ExpectShared(stored, 0, "a narrow variant whose wide variant's key tail a pending storage write made uncompressed");
}

void RunSwizzled(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    Fill(block.Texels(Swizzled), 8000.25f);
    SetRecord(block, 0, block.Texels(Swizzled), Wide);
    SetRecord(block, 1, block.Texels(Swizzled), Narrow);
    RecordImage(block, 1)[3] &= ~7u;
    const auto result = Compile(device, block);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1) == std::vector<std::uint32_t>(Lanes, 0u), "image table variants: the narrow variant that selects zero for its first channel read the texels through the wide variant's view");
    Require(Dispatch(device, result, 0) == Row(block.Texels(Swizzled), Wide), "image table variants: the wide variant with the identity selects sampled the wrong texels");
    Expect(before, 0, 0, 0, "two variants of a surface with different destination selects");
}

void RunCompressionFlag(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    Fill(block.Texels(Flagged), 8500.25f);
    SetRecord(block, 0, block.Texels(Flagged), Wide);
    SetRecord(block, 1, block.Texels(Flagged), Narrow);
    RecordImage(block, 0)[6] = 1u << 21u;
    const auto result = Compile(device, block, FullDescriptorTableCode);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    Require(Dispatch(device, result, 1, FullDescriptorTableCode.data()) == Row(block.Texels(Flagged), Narrow), "image table variants: the narrow variant without META_COMPRESS read the texels through the wide variant that sets it with no metadata address");
    Require(Dispatch(device, result, 0, FullDescriptorTableCode.data()) == Row(block.Texels(Flagged), Wide), "image table variants: the wide variant with META_COMPRESS and no metadata address sampled the wrong texels");
    Expect(before, 0, 0, 0, "two variants of a surface of which only one sets META_COMPRESS");
}

ShaderRecompiler::RecompileResult TableProgram(std::span<const std::array<std::uint32_t, 8>> entries) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    using ShaderRecompiler::DescriptorKind;
    using ShaderRecompiler::DescriptorRole;
    const auto count = static_cast<std::uint32_t>(entries.size());
    constexpr auto bindingCount = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::Count);
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding map{};
    map.kind = DescriptorKind::StorageBuffer;
    map.role = DescriptorRole::ImageTableMap;
    map.binding = static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Stage::Main) * bindingCount + static_cast<std::uint32_t>(ShaderRecompiler::RuntimeAbi::Binding::ImageTableMap);
    map.count = 1;
    map.guestDescriptor.assign(Abi::HeaderWords + count, 0u);
    map.guestDescriptor[Abi::HeaderVersion] = Abi::Version;
    map.guestDescriptor[Abi::HeaderElementCount] = count;
    map.guestDescriptor[Abi::HeaderElementFaults] = Abi::HeaderWords;
    map.guestDescriptor[Abi::HeaderSamplerFaults] = Abi::HeaderWords + count;
    program.bindings.push_back(map);
    ShaderRecompiler::DescriptorBinding images{};
    images.kind = DescriptorKind::SampledImage;
    images.role = DescriptorRole::ImageTable;
    images.binding = ShaderRecompiler::RuntimeAbi::ImageTableBinding;
    images.count = count;
    for (const auto& entry : entries) images.guestDescriptor.insert(images.guestDescriptor.end(), entry.begin(), entry.end());
    images.imageShapes.assign(count, ShaderRecompiler::DescriptorImageShape::Image2D);
    images.imageDepthCompare.assign(count, false);
    images.imageSamplers.assign(count, 0u);
    program.bindings.push_back(std::move(images));
    return program;
}

ShaderRecompiler::RecompileResult ProofProgram(const float* variants, const float* other) {
    const std::array<std::array<std::uint32_t, 4>, 3> images{Image(variants, Narrow), Image(variants, Wide), Image(other, Wide)};
    std::array<std::array<std::uint32_t, 8>, 3> entries{};
    for (std::size_t entry = 0; entry < images.size(); ++entry) std::copy(images[entry].begin(), images[entry].end(), entries[entry].begin());
    return TableProgram(entries);
}

void RunProof(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Fill(block.Texels(Proved), 9000.25f);
    Fill(block.Texels(Proved + 1u), 9500.25f);
    const auto program = ProofProgram(block.Texels(Proved), block.Texels(Proved + 1u));
    const CompiledShader shader{ShaderStage::Compute, &program, 0};
    const auto context = device.graphicsContext();
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::unique_ptr<ShaderResources> resources;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        resources = std::make_unique<ShaderResources>(context, shader);
        Require(resources->Reusable(), "image table variants: a set over a table with a borrowing entry is not reusable");
        Require(resources->SampledTextures().size() == 2u, "image table variants: a set over two surfaces and a borrowing entry holds " + std::to_string(resources->SampledTextures().size()) + " textures");
        const auto entries = AgcDriver::Graphics::TextureCacheUsage().sampledEntries;
        const bool walked = resources->Revalidate(shader);
        Require(AgcDriver::Graphics::TextureCacheUsage().sampledEntries == entries, "image table variants: the full walk looked up the own extent of a borrowing entry");
        Require(walked, "image table variants: the full walk over a table with a borrowing entry failed");
    }
    AgcDriver::Graphics::DepthSurfaceView(context, AgcDriver::Graphics::DepthTarget{AddressOf(block.Texels(Proved)), 0, {Narrow, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(!resources->Revalidate(shader), "image table variants: a set whose borrowing entry moved to another representative proved current");
        resources.reset();
    }
    device.WaitIdle();
    AgcDriver::Graphics::ClearDepthSurfaces(device.Device());
}

using Entries = std::array<std::array<std::uint32_t, 8>, 2>;

Entries SpanningEntries(const GuestBlock& block, bool narrowFirst) {
    const auto wide = CompressedImage(block.Texels(Compressed), Spanning, Format1555Unorm, block.KeyBytes(), false);
    const auto narrow = CompressedImage(block.Texels(Compressed), Narrow, Format1555Unorm, block.KeyBytes(), false);
    return narrowFirst ? Entries{narrow, wide} : Entries{wide, narrow};
}

bool ProvedByIndex(ShaderResources& resources, std::span<const CompiledShader> shaders) {
    const auto before = AgcDriver::Graphics::WriteIndexOutcomesCounted().accepted;
    const bool proved = resources.Revalidate(shaders);
    return proved && AgcDriver::Graphics::WriteIndexOutcomesCounted().accepted == before + 1u;
}

template <typename Change>
void ProveKeyChange(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& program, Change change, const std::string& what, std::size_t rebuiltTextures = 2, bool indexed = false) {
    const CompiledShader shader{ShaderStage::Compute, &program, 0};
    const auto context = device.graphicsContext();
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::unique_ptr<ShaderResources> resources;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        resources = std::make_unique<ShaderResources>(context, shader);
        Require(resources->SampledTextures().size() == 1u, "image table variants: the 1_5_5_5 variants did not share one texture before " + what);
        if (!indexed) {
            ShaderResources::ProofReport report;
            Require(resources->Revalidate(shader, &report) && report.path == ShaderResources::ProofPath::Fast, "image table variants: the set whose 1_5_5_5 variants share one texture did not take the fast proof before " + what);
        }
    }
    if (indexed) {
        const auto* representative = resources->SampledTextures().front().get();
        bool settled = false;
        for (int round = 0; round < 6 && !settled; ++round) {
            AgcDriver::GuestMemory::BumpCollectEpoch();
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            settled = ProvedByIndex(*resources, std::span(&shader, 1));
        }
        Require(settled, "image table variants: the set whose 1_5_5_5 variants share one unwatched texture never took the write index's proof before " + what);
        Require(resources->SampledTextures().front().get() == representative, "image table variants: the write index's proof rebound the unwatched 1_5_5_5 representative's slot before " + what);
    }
    change();
    AgcDriver::GuestMemory::BumpCollectEpoch();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        Require(!resources->Revalidate(shader), "image table variants: a cached set kept its 1_5_5_5 borrower on the representative's texture after " + what);
        resources = std::make_unique<ShaderResources>(context, shader);
        Require(resources->SampledTextures().size() == rebuiltTextures, "image table variants: after " + what + ", the rebuilt set holds " + std::to_string(resources->SampledTextures().size()) + " textures, expected " + std::to_string(rebuiltTextures));
        resources.reset();
    }
    device.WaitIdle();
}

void RunCachedNarrowClear(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0xff});
    ProveKeyChange(device, TableProgram(SpanningEntries(block, false)), [&] { std::fill_n(block.KeyBytes(), narrowKeys, std::uint8_t{0x40}); }, "the narrow borrower's keys became a 0001 clear inside the wide representative's uncompressed ones");
}

void RunCachedWideTail(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0x40});
    AgcDriver::Graphics::DepthSurfaceView(device.graphicsContext(), AgcDriver::Graphics::DepthTarget{AddressOf(block.Texels(Compressed)), 0, {Narrow, Height}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, 0});
    ProveKeyChange(device, TableProgram(SpanningEntries(block, false)), [&] { std::fill_n(block.KeyBytes() + narrowKeys, wideKeys - narrowKeys, std::uint8_t{0xff}); }, "the wide borrower's key tail past the narrow representative's 0001 clear became uncompressed");
    AgcDriver::Graphics::ClearDepthSurfaces(device.Device());
}

std::array<std::uint32_t, 8> LayeredImage(const GuestBlock& block, std::uint32_t width) {
    auto words = CompressedImage(block.Texels(Compressed), width, Format1555Unorm, block.KeyBytes(), false);
    words[3] = (words[3] & ~((0x1fu << 20u) | (0xfu << 28u))) | (Type2DArray << 28u);
    words[4] = 1u | (1u << 16u);
    return words;
}

ShaderRecompiler::RecompileResult LayeredProgram(const GuestBlock& block, std::size_t& wideKeys) {
    const Entries entries{LayeredImage(block, Spanning), LayeredImage(block, Narrow)};
    const auto keysOf = [](const std::array<std::uint32_t, 8>& words) { return static_cast<std::size_t>(AgcDriver::Graphics::SurfaceGuestBytes(AgcDriver::Graphics::DecodeTextureResource(words)) / 256u); };
    const auto narrowKeys = keysOf(entries[1]);
    wideKeys = keysOf(entries[0]);
    Require(narrowKeys != 0u && narrowKeys < wideKeys && wideKeys <= KeyCount && TexturesOffset + Compressed * TextureBytes + wideKeys * 256u <= BlockBytes, "image table variants: the layered 1_5_5_5 variants do not cover different DCC key counts inside the guest block");
    std::memset(block.Texels(Compressed), 0, wideKeys * 256u);
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0xff});
    auto program = TableProgram(entries);
    program.bindings[1].imageShapes.assign(entries.size(), ShaderRecompiler::DescriptorImageShape::Image2DArray);
    return program;
}

void RunCachedLayeredWindow(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    std::size_t wideKeys = 0;
    const auto program = LayeredProgram(block, wideKeys);
    ProveKeyChange(device, program, [&] { std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0x40}); }, "the uncompressed keys of the layered variants, whose views from slice 1 read no keys of their own, all became a 0001 clear", 1);
}

void RunCachedRegisterClear(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    std::size_t wideKeys = 0;
    const auto program = LayeredProgram(block, wideKeys);
    const CompiledShader shader{ShaderStage::Compute, &program, 0};
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::unique_ptr<ShaderResources> resources;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        resources = std::make_unique<ShaderResources>(device.graphicsContext(), shader);
        Require(resources->SampledTextures().size() == 1u, "image table variants: the layered 1_5_5_5 variants did not share one texture under uniform uncompressed keys");
    }
    const auto path = [&](const std::string& what) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        ShaderResources::ProofReport report;
        Require(resources->Revalidate(shader, &report), "image table variants: the set whose layered 1_5_5_5 variants share one texture failed its proof " + what);
        return report.path;
    };
    Require(path("under uniform uncompressed keys") == ShaderResources::ProofPath::Fast, "image table variants: the set whose layered 1_5_5_5 variants share one texture did not take the fast proof under uniform uncompressed keys");
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0x20});
    static_cast<void>(path("once its uncompressed keys became a register clear, which reads as stored too"));
    Require(path("after its full walk under register-clear keys") == ShaderResources::ProofPath::Fast, "image table variants: once the uncompressed keys of the layered 1_5_5_5 variants became a register clear, the set that proved current again never took the fast proof again");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        resources.reset();
    }
    device.WaitIdle();
}

void RunIndexed(AgcDriver::VulkanDevice& device, const GuestBlock& block, bool narrowFirst) {
    std::size_t narrowKeys = 0;
    std::size_t wideKeys = 0;
    SetSpanningRecords(block, narrowKeys, wideKeys);
    std::fill_n(block.KeyBytes(), wideKeys, std::uint8_t{0xff});
    AgcDriver::GuestMemory::Unwatch(AddressOf(block.Texels(Compressed)), wideKeys * 256u);
    Require(!AgcDriver::GuestMemory::Watched(AddressOf(block.Texels(Compressed)), wideKeys * 256u), "image table variants: the wide 1_5_5_5 representative's surface is still write-watched");
    const std::string order = narrowFirst ? "behind its narrow borrower" : "ahead of its narrow borrower";
    ProveKeyChange(device, TableProgram(SpanningEntries(block, narrowFirst)), [&] { std::fill_n(block.KeyBytes(), narrowKeys, std::uint8_t{0x40}); }, "the narrow borrower's keys became a 0001 clear inside the uncompressed keys of the unwatched wide representative " + order, 2, true);
}

enum class NarrowStore { NewBytes, SameBytes, SameBytesAfterWide };

void RunIndexedDraw(AgcDriver::VulkanDevice& device, const GuestBlock& block, NarrowStore store) {
    auto* texels = block.Texels(store == NarrowStore::NewBytes ? Written : store == NarrowStore::SameBytes ? Largest : Sized);
    Fill(texels, 6000.25f);
    if (store != NarrowStore::NewBytes) {
        for (std::uint32_t x = 0; x < Wide; ++x) texels[x] = -4.0f - static_cast<float>(x);
    }
    Entries entries{};
    const auto wide = Image(texels, Wide);
    const auto narrow = Image(texels, Narrow);
    std::copy(wide.begin(), wide.end(), entries[0].begin());
    std::copy(narrow.begin(), narrow.end(), entries[1].begin());
    const auto program = TableProgram(entries);
    const ShaderRecompiler::RecompileResult vertex{};
    const std::array<CompiledShader, 2> stages{{{ShaderStage::Vertex, &vertex, 0}, {ShaderStage::Fragment, &program, 0}}};
    AgcDriver::GuestMemory::Unwatch(AddressOf(texels), TextureBytes);
    Require(!AgcDriver::GuestMemory::Watched(AddressOf(texels), TextureBytes), "image table variants: the wide representative's surface of the draw set is still write-watched");
    if (store == NarrowStore::SameBytesAfterWide) Store(device, texels, Wide);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    std::unique_ptr<ShaderResources> resources;
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        resources = std::make_unique<ShaderResources>(device.graphicsContext(), stages, AgcDriver::Graphics::ColorTarget{}, 0, 0, 0);
        Require(resources->SampledTextures().size() == 1u, "image table variants: the two variants of a draw set's table did not share one texture");
    }
    bool settled = false;
    for (int round = 0; round < 6 && !settled; ++round) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        settled = ProvedByIndex(*resources, stages);
    }
    Require(settled, "image table variants: the draw set whose variants share one unwatched texture never took the write index's proof");
    Store(device, texels, Narrow);
    {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const std::string write = store == NarrowStore::NewBytes ? "a storage write of the narrow variant" : store == NarrowStore::SameBytes ? "a storage write of the narrow variant that left its bytes as they were" : "a storage write of the narrow variant over the wide variant's stored bytes, which it left as they were,";
        Require(!resources->Revalidate(stages), "image table variants: a draw set kept its narrow variant on the wide variant's texture after " + write + " made it the representative");
        resources.reset();
    }
    device.WaitIdle();
}

void RunZeroKeyRepresentative(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    const auto context = device.graphicsContext();
    VkFormatProperties properties{};
    if (context.formatProperties != nullptr) context.formatProperties(context.physical, VK_FORMAT_R32G32B32_SFLOAT, &properties);
    constexpr VkFormatFeatureFlags Needed = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((properties.optimalTilingFeatures & Needed) != Needed) {
        std::puts("zero-key representative case skipped: the device cannot sample and upload the optimal-tiled R32G32B32_SFLOAT image the 32_32_32 representative's lookup creates");
        return;
    }
    constexpr std::uint32_t Short = 21;
    constexpr std::uint32_t Long = 2 * Short;
    auto* texels = reinterpret_cast<std::uint8_t*>(block.Texels(0)) + (BlockBytes - TexturesOffset - 256u);
    const auto row = [&](std::uint32_t width) {
        auto words = CompressedImage(texels, width, Format32_32_32Float, block.KeyBytes(), false);
        words[2] &= ~((Height - 1u) << 14u);
        words[3] &= ~(0x1fu << 20u);
        return words;
    };
    const Entries entries{row(Short), row(Long)};
    const auto bytesOf = [](const std::array<std::uint32_t, 8>& words) { return AgcDriver::Graphics::SurfaceGuestBytes(AgcDriver::Graphics::DecodeTextureResource(words)); };
    Require(bytesOf(entries[0]) == Short * 12u && bytesOf(entries[1]) == Long * 12u, "image table variants: the one-row linear 32_32_32 variants do not cover 252 and 504 bytes");
    Require(AgcDriver::GuestMemory::Accessible(texels, Short * 12u) && !AgcDriver::GuestMemory::Accessible(texels, Long * 12u), "image table variants: only the short 32_32_32 variant should lie inside the guest block");
    std::memset(texels, 0, Short * 12u);
    block.KeyBytes()[0] = 0u;
    const auto program = TableProgram(entries);
    const CompiledShader shader{ShaderStage::Compute, &program, 0};
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto before = AgcDriver::Graphics::ImageTableReuses();
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        ShaderResources resources(context, shader);
    }
    device.WaitIdle();
    Expect(before, 0, 0, 0, "a 504-byte variant with one DCC key byte requesting a clear and a 252-byte representative with none");
}

void RunThreadDimensions(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    Prepare(block);
    const auto result = Compile(device, block, TableCode, {~0u, ~0u, ~0u}, 0u);
    const auto table = std::find_if(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == ShaderRecompiler::DescriptorRole::ImageTable; });
    const auto data = std::find_if(result.bindings.begin(), result.bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == ShaderRecompiler::DescriptorRole::ShaderData; });
    Require(data != result.bindings.end() && table < data, "image table variants: the thread-dimension shader does not bind its ShaderData after its image table");
    auto* arguments = block.Table() + ArgumentsWord;
    std::fill(arguments, arguments + 4, 0u);
    for (const std::uint32_t threads : {Lanes, Lanes / 2u + 3u}) {
        arguments[0] = threads;
        arguments[1] = 1u;
        arguments[2] = 1u;
        Keys[0] = 5u;
        std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
        device.DispatchIndirect(result, AddressOf(arguments), {}, AddressOf(TableCode.data()), nullptr, nullptr, {Lanes, 1u, 1u});
        device.WaitIdle();
        const auto expected = Row(block.Texels(5), Wide);
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            const auto red = Output[lane * 4u];
            if (lane < threads) Require(red == expected[lane], "image table variants: a thread-dimension dispatch of " + std::to_string(threads) + " threads sampled the wrong texel in lane " + std::to_string(lane));
            else Require(red == 0xdeadbeefu, "image table variants: a thread-dimension dispatch of " + std::to_string(threads) + " threads ran lane " + std::to_string(lane));
        }
    }
}

void RunTests(AgcDriver::VulkanDevice& device, const GuestBlock& block) {
    RunTied(device, block);
    RunProof(device, block);
    RunLargest(device, block);
    RunReused(device, block);
    RunWritten(device, block);
    RunReusedWritten(device, block);
    RunFetch(device, block);
    RunSize(device, block);
    RunDepth(device, block);
    RunRejected(device, block);
    RunThreadDimensions(device, block);
    RunAlphaPlacement(device, block);
    RunKeySpans(device, block);
    RunSharedKeys(device, block);
    RunSwizzled(device, block);
    RunCompressionFlag(device, block);
    RunUncompressedSpans(device, block);
    RunNarrowRepresentative(device, block);
    RunPendingStorageTail(device, block);
    RunZeroKeyRepresentative(device, block);
}

void SetSwitch(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "1");
#else
    setenv(name, "1", 1);
#endif
}

}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode != "--fast-revalidate" && mode != "--table-index") SetSwitch("APS5_NO_FAST_REVALIDATE");
    if (mode == "--pending-copy-tail") SetSwitch("APS5_CPU_COPIES");
    if (mode == "--table-index") SetSwitch("APS5_TABLE_INDEX");
    try {
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (mode.empty()) {
            RunTests(*device, block);
        } else if (mode == "--fast-revalidate") {
            RunCachedNarrowClear(*device, block);
            RunCachedWideTail(*device, block);
            RunCachedLayeredWindow(*device, block);
            RunCachedRegisterClear(*device, block);
        } else if (mode == "--table-index") {
            Require(AgcDriver::GuestMemory::WriteIndexEnabled(), "image table variants: the write index is not enabled");
            if (!AgcDriver::GuestMemory::WriteWatched()) {
                std::puts("table index cases skipped: guest memory has no write watch");
                return VulkanTestSkipped;
            }
            RunIndexed(*device, block, true);
            RunIndexed(*device, block, false);
            RunIndexedDraw(*device, block, NarrowStore::NewBytes);
            RunIndexedDraw(*device, block, NarrowStore::SameBytes);
            RunIndexedDraw(*device, block, NarrowStore::SameBytesAfterWide);
        } else if (mode == "--pending-copy-tail") {
            RunPendingTail(*device, block);
        } else {
            throw std::runtime_error("image table variants: unknown mode " + std::string(mode));
        }
        std::puts("image table variants tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
