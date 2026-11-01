#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/SrtWalker.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Records = 65536;
constexpr std::uint32_t Stride = 32;
constexpr std::uint32_t Elements = 4096;
constexpr std::uint32_t Spare = 64;
constexpr std::uint32_t TextureBytes = 2048;
constexpr std::uint32_t Aliases = 256;
constexpr std::uint32_t Surfaces = (Elements + Spare + Aliases - 1) / Aliases;
constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SrtBytes = 256;
constexpr std::size_t SrtSlots = 16;
constexpr std::size_t SrtOffset = 4096;
constexpr std::size_t OutputOffset = SrtOffset + SrtSlots * SrtBytes;
constexpr std::size_t HeapOffset = OutputOffset + 4096;
constexpr std::size_t TexturesOffset = HeapOffset + static_cast<std::size_t>(Records) * Stride;
constexpr std::size_t BlockBytes = (TexturesOffset + static_cast<std::size_t>(Surfaces) * TextureBytes + 65535) & ~static_cast<std::size_t>(65535);

constexpr std::uint32_t WideStride = 48;
constexpr std::uint32_t WideRecords = 32768;
constexpr std::size_t WideKernelOffset = 2048;
constexpr std::size_t WideHeapOffset = 65536;
constexpr std::array<std::size_t, 3> WideSlots{SrtSlots - 1u, SrtSlots - 2u, SrtSlots - 3u};
constexpr std::array<std::uint32_t, 17> WideTableCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x9310b010u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
constexpr std::array<std::uint32_t, 17> TableCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};

constexpr std::size_t VertexOffset = 256;
constexpr std::size_t PixelOffset = 512;
constexpr std::size_t DrawSrtBytes = 64;
constexpr std::size_t DrawKeys = SrtSlots * SrtBytes / DrawSrtBytes;
constexpr std::size_t ColorBytes = 1024;
constexpr std::array<std::uint32_t, 13> TriangleVertexCode{0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u, 0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u};
constexpr std::array<std::uint32_t, 20> TablePixelCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xd7650002u, 0x000100c1u, 0x7e200502u, 0x8f108510u, 0xbf8cc07fu, 0xf42c0502u, 0x20000000u, 0x7e000280u, 0x7e020280u, 0xbf8cc07fu, 0xf09c0f08u, 0x00450000u, 0xbf8c3f70u, 0xf800180fu, 0x03020100u, 0xbf810000u};

class GuestBlock {
public:
    GuestBlock() {
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
        Require(block != nullptr, "image table cache bounds: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> V(std::uint64_t address, std::uint32_t stride, std::uint32_t records) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, stride == 0u ? 0x31016facu : 0x01016facu};
}

void SetRecord(const GuestBlock& block, std::uint32_t record, std::uint32_t element) {
    const auto address = block.At(TexturesOffset + static_cast<std::size_t>(element / Aliases) * TextureBytes);
    const std::array<std::uint32_t, 8> words{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 7u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, element % Aliases, 0u};
    std::memcpy(block.Bytes(HeapOffset + static_cast<std::size_t>(record) * Stride), words.data(), sizeof(words));
}

std::vector<std::uint32_t> Pm4(std::uint32_t opcode, std::initializer_list<std::uint32_t> body) {
    std::vector<std::uint32_t> words{0xc0000000u | (static_cast<std::uint32_t>(body.size() - 1) << 16u) | (opcode << 8u)};
    words.insert(words.end(), body);
    return words;
}

void Dispatch(const GuestBlock& block, std::size_t slot, std::size_t kernelOffset = 0, std::vector<std::uint32_t> words = {}) {
    const auto kernel = block.At(kernelOffset);
    const auto srt = block.At(SrtOffset + slot * SrtBytes);
    for (const auto& packet : {Pm4(0x76, {0x20c, static_cast<std::uint32_t>(kernel >> 8u)}), Pm4(0x76, {0x20d, static_cast<std::uint32_t>(kernel >> 40u) & 0xffu}), Pm4(0x76, {0x207, 64}), Pm4(0x76, {0x208, 1}), Pm4(0x76, {0x209, 1}), Pm4(0x76, {0x213, 2u << 1u}), Pm4(0x76, {0x240, static_cast<std::uint32_t>(srt)}), Pm4(0x76, {0x241, static_cast<std::uint32_t>(srt >> 32u)}), Pm4(0x15, {1, 1, 1, 0x41})}) {
        words.insert(words.end(), packet.begin(), packet.end());
    }
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "image table cache bounds: the dispatch submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void Prepare(const GuestBlock& block) {
    std::memcpy(block.Bytes(0), TableCode.data(), sizeof(TableCode));
    for (std::uint32_t surface = 0; surface < Surfaces; ++surface) std::memset(block.Bytes(TexturesOffset + static_cast<std::size_t>(surface) * TextureBytes), static_cast<int>(surface), TextureBytes);
    for (std::uint32_t record = 0; record < Records; ++record) SetRecord(block, record, record % Elements);
    for (std::size_t slot = 0; slot < SrtSlots; ++slot) {
        auto* srt = reinterpret_cast<std::uint32_t*>(block.Bytes(SrtOffset + slot * SrtBytes));
        std::fill(srt, srt + SrtBytes / 4u, 0u);
        const auto heap = V(block.At(HeapOffset), Stride, Records);
        const auto output = V(block.At(OutputOffset), 0u, 4096u);
        std::copy(heap.begin(), heap.end(), srt);
        std::copy(output.begin(), output.end(), srt + 12);
    }
}

void TablesChangingPerDispatch(const GuestBlock& block) {
    auto& cache = AgcDriver::Graphics::SharedResourceCache();
    std::uint64_t most = 0;
    for (std::uint32_t dispatch = 0; dispatch < 6; ++dispatch) {
        SetRecord(block, dispatch * 977u % Records, Elements + dispatch % Spare);
        Dispatch(block, 0);
        most = std::max(most, cache.TableElements());
    }
    Require(most >= Elements, "image table cache bounds: no dispatch kept its table in the resource cache");
    Require(most <= AgcDriver::Graphics::ResourceCache::TableElementBudget(), "image table cache bounds: tables that changed every dispatch kept " + std::to_string(most) + " table elements in the resource cache, over its budget of " + std::to_string(AgcDriver::Graphics::ResourceCache::TableElementBudget()));
}

void SrtMovingPerDispatch(const GuestBlock& block) {
    using AgcDriver::DriverDetail::Driver;
    for (std::uint32_t record = 0; record < Records; ++record) SetRecord(block, record, record % 4u);
    std::uint64_t most = 0;
    std::size_t entries = 0;
    for (std::size_t slot = 0; slot < SrtSlots; ++slot) {
        Dispatch(block, slot);
        const auto size = Driver::Get().DispatchCacheSize();
        most = std::max(most, size.variantBytes);
        entries = std::max(entries, size.entries);
    }
    Require(entries >= 2, "image table cache bounds: the dispatches were not kept in the dispatch cache");
    Require(most <= Driver::DispatchCacheBytes(), "image table cache bounds: dispatches reading a 2 MiB table through " + std::to_string(SrtSlots) + " SRTs kept " + std::to_string(most >> 20u) + " MiB of captured words in the dispatch cache, over its budget of " + std::to_string(Driver::DispatchCacheBytes() >> 20u) + " MiB");
}

void Draw(const GuestBlock& block, std::size_t key) {
    const auto color = block.At(OutputOffset);
    const auto vertex = block.At(VertexOffset);
    const auto pixel = block.At(PixelOffset);
    const auto srt = block.At(SrtOffset + key * DrawSrtBytes);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000}, {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800}, {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa}, {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240}, {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028}, {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u}, {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040}, {0x81, 0x80000000}, {0x82, 0x40040}, {0x90, 0x80000000}, {0x91, 0x40040}, {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)}, {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0}, {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)}, {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 0}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 2u << 1u},
        {0x00c, static_cast<std::uint32_t>(srt)}, {0x00d, static_cast<std::uint32_t>(srt >> 32u)},
    };
    std::vector<std::uint32_t> words;
    const auto append = [&](const std::vector<std::uint32_t>& packet) { words.insert(words.end(), packet.begin(), packet.end()); };
    append(Pm4(0x79, {0x242, 4}));
    for (const auto& [offset, value] : context) append(Pm4(0x69, {offset, value}));
    for (const auto& [offset, value] : shader) append(Pm4(0x76, {offset, value}));
    append(Pm4(0x2d, {3, 2}));
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "image table draw cache bounds: the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

unsigned BrightestColor(const GuestBlock& block) {
    AgcDriver::GuestMemory::FlushGpuWrites(block.At(OutputOffset), ColorBytes);
    const auto* bytes = block.Bytes(OutputOffset);
    return *std::max_element(bytes, bytes + ColorBytes);
}

void SrtMovingPerDraw(const GuestBlock& block) {
    using AgcDriver::DriverDetail::Driver;
    std::memcpy(block.Bytes(VertexOffset), TriangleVertexCode.data(), sizeof(TriangleVertexCode));
    std::memcpy(block.Bytes(PixelOffset), TablePixelCode.data(), sizeof(TablePixelCode));
    const auto fill = [&](std::uint32_t surface) {
        for (std::uint32_t record = 0; record < Records; ++record) SetRecord(block, record, surface * Aliases);
    };
    fill(3u);
    for (std::size_t key = 0; key < DrawKeys; ++key) {
        auto* srt = reinterpret_cast<std::uint32_t*>(block.Bytes(SrtOffset + key * DrawSrtBytes));
        std::fill(srt, srt + DrawSrtBytes / 4u, 0u);
        const auto heap = V(block.At(HeapOffset + key * Stride), Stride, Records - static_cast<std::uint32_t>(key));
        std::copy(heap.begin(), heap.end(), srt);
    }
    auto& driver = Driver::Get();
    const auto budget = Driver::DrawCacheBytes();
    const auto before = driver.DrawCacheCounters();
    std::uint64_t most = 0;
    std::size_t entries = 0;
    for (std::size_t key = 0; key < DrawKeys; ++key) {
        for (std::size_t sight = 0; sight < 2; ++sight) {
            Draw(block, key);
            const auto size = driver.DrawCacheSize();
            Require(size.variantBytes <= budget, "image table draw cache bounds: after draw " + std::to_string(2 * key + sight) + " the draw cache kept " + std::to_string(size.entries) + " entries holding " + std::to_string(size.variantBytes >> 10u) + " KiB, over its budget of " + std::to_string(budget >> 10u) + " KiB");
            most = std::max(most, size.variantBytes);
            entries = std::max(entries, size.entries);
        }
    }
    const auto filled = driver.DrawCacheCounters();
    const auto inserts = filled.inserts - before.inserts;
    const auto evictions = filled.evictions - before.evictions;
    const auto byteEvictions = filled.byteEvictions - before.byteEvictions;
    const auto counts = " (" + std::to_string(inserts) + " inserts, " + std::to_string(evictions) + " evictions, " + std::to_string(byteEvictions) + " over the byte budget, at most " + std::to_string(entries) + " entries holding " + std::to_string(most >> 10u) + " KiB)";
    Require(BrightestColor(block) == 3u, "image table draw cache bounds: the draws through the table wrote " + std::to_string(BrightestColor(block)) + ", expected 3");
    Require(entries >= 2 && inserts >= DrawKeys / 2, "image table draw cache bounds: the draws were not kept in the draw cache" + counts);
    Require(inserts * (most / entries) > 2 * budget, "image table draw cache bounds: the draws did not insert more than twice the budget" + counts);
    Require(byteEvictions >= 1 && byteEvictions == evictions, "image table draw cache bounds: the draw cache was not bounded by its byte budget" + counts);

    fill(9u);
    std::memset(block.Bytes(OutputOffset), 0, ColorBytes);
    Draw(block, 0);
    const auto rebuilt = driver.DrawCacheCounters();
    Require(rebuilt.absent - filled.absent == 1 && rebuilt.inserts - filled.inserts == 1, "image table draw cache bounds: the first key, evicted, was not looked up without an entry and inserted again (" + std::to_string(rebuilt.absent - filled.absent) + " absent, " + std::to_string(rebuilt.inserts - filled.inserts) + " inserts)");
    Require(BrightestColor(block) == 9u, "image table draw cache bounds: the evicted key drawn again wrote " + std::to_string(BrightestColor(block)) + ", expected 9");
    std::memset(block.Bytes(OutputOffset), 0, ColorBytes);
    Draw(block, 0);
    const auto hit = driver.DrawCacheCounters();
    Require(hit.hits - rebuilt.hits == 1, "image table draw cache bounds: the evicted key, inserted again, was not hit by its next draw");
    Require(BrightestColor(block) == 9u, "image table draw cache bounds: the hit on the rebuilt key wrote " + std::to_string(BrightestColor(block)) + ", expected 9");
    std::puts(("image table draw cache bounds passed" + counts).c_str());
}

std::array<std::uint32_t, 8> TextureWords(const GuestBlock& block, std::uint32_t element) {
    const auto address = block.At(TexturesOffset + static_cast<std::size_t>(element / Aliases) * TextureBytes);
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 7u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, element % Aliases, 0u};
}

std::uint64_t WideRecord(const GuestBlock& block, std::uint32_t record) {
    return block.At(WideHeapOffset + static_cast<std::size_t>(record) * WideStride);
}

void SetWideRecord(const GuestBlock& block, std::uint32_t record, std::uint32_t element) {
    const auto words = TextureWords(block, element);
    std::memcpy(block.Bytes(WideHeapOffset + static_cast<std::size_t>(record) * WideStride), words.data(), sizeof(words));
}

void VouchedLookups(const GuestBlock& block) {
    using AgcDriver::ShaderMemory;
    using ShaderRecompiler::ResourceMaterializer;
    std::memcpy(block.Bytes(WideKernelOffset), WideTableCode.data(), sizeof(WideTableCode));
    for (std::uint32_t record = 0; record < WideRecords; ++record) {
        SetWideRecord(block, record, (1u + record % 15u) * Aliases);
        std::memset(block.Bytes(WideHeapOffset + static_cast<std::size_t>(record) * WideStride + 32u), 0x5a, WideStride - 32u);
    }
    for (const auto slot : WideSlots) {
        auto* srt = reinterpret_cast<std::uint32_t*>(block.Bytes(SrtOffset + slot * SrtBytes));
        std::fill(srt, srt + SrtBytes / 4u, 0u);
        const auto heap = V(block.At(WideHeapOffset), WideStride, WideRecords);
        const auto output = V(block.At(OutputOffset), 0u, 4096u);
        std::copy(heap.begin(), heap.end(), srt);
        std::copy(output.begin(), output.end(), srt + 12);
    }
    struct Step {
        std::uint64_t vouched = 0;
        std::uint64_t refused = 0;
        std::uint64_t columns = 0;
        std::uint64_t reused = 0;
    };
    const auto dispatch = [&](std::size_t slot, std::vector<std::uint32_t> before = {}) {
        const auto vouches = ShaderMemory::Vouches();
        const auto reuse = ResourceMaterializer::TableReuse();
        Dispatch(block, slot, WideKernelOffset, std::move(before));
        const auto after = ShaderMemory::Vouches();
        const auto reused = ResourceMaterializer::TableReuse();
        return Step{after.blocks - vouches.blocks, after.refused - vouches.refused, reused.columns - reuse.columns, (reused.unwritten - reuse.unwritten) + (reused.compared - reuse.compared)};
    };
    const auto text = [](const Step& step) { return " (" + std::to_string(step.vouched) + " vouched, " + std::to_string(step.refused) + " refused, " + std::to_string(step.columns) + " columns read, " + std::to_string(step.reused) + " reused)"; };
    const auto read = dispatch(WideSlots[0]);
    Require(read.columns == 1u, "image table vouched lookups: the first dispatch did not read the strided table" + text(read));
    const auto shared = dispatch(WideSlots[1]);
    Require(shared.columns == 0u && shared.reused == 1u, "image table vouched lookups: a second dispatch over the unwritten table did not reuse its column" + text(shared));
    const auto hit = dispatch(WideSlots[1]);
    Require(hit.vouched >= 1u && hit.refused == 0u && hit.columns == 0u && hit.reused == 0u, "image table vouched lookups: a cached dispatch over an unwritten table was not validated by its table's vouch" + text(hit));

    std::size_t vouchedSlot = WideSlots[1];
    std::size_t otherSlot = WideSlots[0];
    const auto recapture = [&](const std::string& what, std::vector<std::uint32_t> before = {}) {
        const auto changed = dispatch(vouchedSlot, std::move(before));
        Require(changed.refused >= 1u && changed.columns == 1u, "image table vouched lookups: " + what + " was hidden by the cached dispatch's vouch" + text(changed));
        const auto other = dispatch(otherSlot);
        Require(other.columns == 0u && other.reused == 1u, "image table vouched lookups: the other dispatch after " + what + " did not reuse the column read again" + text(other));
        const auto again = dispatch(otherSlot);
        Require(again.vouched >= 1u && again.refused == 0u && again.columns == 0u && again.reused == 0u, "image table vouched lookups: the dispatch after " + what + " was not vouched for again" + text(again));
        std::swap(vouchedSlot, otherSlot);
    };
    SetWideRecord(block, 0u, 9u * Aliases);
    recapture("a CPU store into a record");

    const auto words = TextureWords(block, 10u * Aliases);
    const auto target = WideRecord(block, 0u);
    std::vector<std::uint32_t> cpWrite{0xc0000000u | ((3u + static_cast<std::uint32_t>(words.size()) - 1u) << 16u) | (0x37u << 8u), 0x00100200u, static_cast<std::uint32_t>(target), static_cast<std::uint32_t>(target >> 32u)};
    cpWrite.insert(cpWrite.end(), words.begin(), words.end());
    recapture("a GPU write into a record pending ahead of the dispatch", cpWrite);
    Require(std::memcmp(block.Bytes(WideHeapOffset), words.data(), sizeof(words)) == 0, "image table vouched lookups: the GPU write did not land in the record");

    const auto driverWords = TextureWords(block, 11u * Aliases);
    AgcDriver::GuestMemory::Write(WideRecord(block, 0u), std::as_bytes(std::span(driverWords)), 4);
    recapture("a driver store into a record");

    *reinterpret_cast<std::uint32_t*>(block.Bytes(WideHeapOffset + 36u)) ^= 0x10u;
    const auto gap = dispatch(vouchedSlot);
    Require(gap.refused >= 1u && gap.columns == 0u && gap.reused == 0u, "image table vouched lookups: a store between two records did not refuse the vouch, or the table was captured again" + text(gap));
    const auto compared = dispatch(WideSlots[2]);
    Require(compared.columns == 0u && compared.reused == 1u, "image table vouched lookups: a new dispatch after a store between two records did not compare and keep the column" + text(compared));
    const auto restamped = dispatch(vouchedSlot);
    Require(restamped.vouched >= 1u && restamped.refused == 0u && restamped.columns == 0u, "image table vouched lookups: a dispatch sharing a column compared after a store between two records was not vouched for again" + text(restamped));
    std::puts("image table vouched lookups passed");
}

struct QueuedDraw {
    std::atomic<bool> release{false};

    void Enqueue(std::uint64_t begin, std::uint64_t end) {
        AgcDriver::DriverDetail::DrawPipeline::Queue0().Enqueue([this] {
            while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }, {{begin, end}});
    }
};

void VouchUnderQueuedDraws(const GuestBlock& block) {
    using AgcDriver::DriverDetail::DrawPipeline;
    using AgcDriver::DriverDetail::Driver;
    Require(DrawPipeline::Depth() >= 3u, "image table vouch under queued draws: the draw pipeline is off");
    for (std::uint32_t record = 0; record < WideRecords; ++record) SetWideRecord(block, record, (1u + record % 15u) * Aliases);
    const auto low = WideRecord(block, 0u);
    const auto bytes = static_cast<std::size_t>(WideRecord(block, WideRecords - 1u) + 32u - low);
    static_cast<void>(Driver::Get());
    const ShaderRecompiler::SrtWriteStamp stamp{AgcDriver::GuestMemory::CollectWrites(low, bytes), GuestAllocations::GuestAllocationsGeneration_nid_postfix()};
    Require(stamp.generation != 0u, "image table vouch under queued draws: the table is not write-watched");
    auto& pipeline = DrawPipeline::Queue0();
    QueuedDraw elsewhere;
    QueuedDraw between;
    QueuedDraw into;
    const auto releaseAll = [&] {
        elsewhere.release = true;
        between.release = true;
        into.release = true;
    };
    DrawPipeline::Active() = true;
    const auto vouch = [&](const std::string& what, bool expected) {
        const auto outstanding = pipeline.Outstanding();
        std::atomic<bool> finished{false};
        std::atomic<bool> waited{false};
        std::thread watchdog([&] {
            const auto start = std::chrono::steady_clock::now();
            while (!finished.load()) {
                if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
                    waited = true;
                    releaseAll();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        const bool vouched = AgcDriver::ShaderMemory::Vouched(&Driver::QueryPendingSpan, low, bytes, stamp);
        finished = true;
        watchdog.join();
        const bool queued = !waited.load() && pipeline.Outstanding() == outstanding;
        if (!queued || vouched != expected) releaseAll();
        Require(queued, "image table vouch under queued draws: " + what + ": the vouch waited for the queued draws");
        Require(vouched == expected, "image table vouch under queued draws: " + what + (expected ? ": the table was not vouched for" : ": the table was still vouched for"));
    };
    const auto settle = [&](std::size_t outstanding) {
        const auto start = std::chrono::steady_clock::now();
        while (pipeline.Outstanding() != outstanding) {
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(20)) {
                releaseAll();
                Require(false, "image table vouch under queued draws: the released draws were never committed");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    vouch("an idle pipeline", true);
    elsewhere.Enqueue(block.At(OutputOffset), block.At(OutputOffset) + 4096u);
    vouch("a queued draw writing outside the table", true);
    between.Enqueue(low + 32u, low + WideStride);
    vouch("a queued draw writing between two records", false);
    into.Enqueue(WideRecord(block, 7u), WideRecord(block, 7u) + 32u);
    elsewhere.release = true;
    between.release = true;
    settle(1u);
    vouch("a queued draw writing into a record", false);
    into.release = true;
    pipeline.Drain(DrawPipeline::DrainReason::Capture);
    vouch("committed draws that wrote nothing", true);
    DrawPipeline::Active() = false;
    std::puts("image table vouch under queued draws passed");
}

void SetBudgets() {
#ifdef _WIN32
    _putenv_s("APS5_RESOURCE_CACHE_TABLE_ELEMENTS", "8192");
    _putenv_s("APS5_DISPATCH_CACHE_BYTES", "16777216");
    _putenv_s("APS5_DRAW_CACHE_BYTES", "16777216");
#else
    setenv("APS5_RESOURCE_CACHE_TABLE_ELEMENTS", "8192", 1);
    setenv("APS5_DISPATCH_CACHE_BYTES", "16777216", 1);
    setenv("APS5_DRAW_CACHE_BYTES", "16777216", 1);
#endif
}

#ifdef _WIN32
bool WindowsImportsStayWatchedThroughReads(const AgcDriver::Graphics::Context& context) {
    if (context.hostImportAlignment == 0) return true;
    const auto probe = AgcDriver::Graphics::ProbeImportWriteProtection(context);
    if (probe.failure == nullptr && probe.writtenAfterSubmit == 0) return true;
    if (probe.failure != nullptr) {
        std::printf("skipped, the host import probe failed at %s (%d)\n", probe.failure, static_cast<int>(probe.result));
    } else {
        std::printf(
            "skipped, host imports on this device report %u of %u pages written at the import and "
            "%u after a GPU read, so a watched imported table cannot be vouched for\n",
            probe.writtenAtImport, probe.pages, probe.writtenAfterSubmit);
    }
    return false;
}
#else
bool ImportsStayWatchedThroughReads(const AgcDriver::Graphics::Context& context) {
    if (context.hostImportAlignment == 0) return true;
    const auto probe = AgcDriver::Graphics::ProbeImportWriteProtection(context);
    if (probe.failure == nullptr && probe.writtenAfterSubmit == 0) return true;
    if (probe.failure != nullptr) std::printf("skipped, the device reports no host import probe result: it failed at %s (%d)\n", probe.failure, static_cast<int>(probe.result));
    else std::printf("skipped, the device reports %u of %u host-imported pages written after a GPU read, so a watched imported table cannot be vouched for\n", probe.writtenAfterSubmit, probe.pages);
    return false;
}
#endif

}

int main(int argc, char** argv) {
    SetBudgets();
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
#ifdef _WIN32
            if (argc == 2 && std::string(argv[1]) == "--vouched-lookups" &&
                !WindowsImportsStayWatchedThroughReads(device->graphicsContext())) return VulkanTestSkipped;
#else
            if (argc == 2 && std::string(argv[1]) == "--vouched-lookups" && !ImportsStayWatchedThroughReads(device->graphicsContext())) return VulkanTestSkipped;
#endif
        }
        GuestBlock block;
        Prepare(block);
        if (argc == 2 && std::string(argv[1]) == "--vouched-lookups") {
            VouchedLookups(block);
            AgcDriverShutdown_nid_postfix();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--queued-draws") {
            VouchUnderQueuedDraws(block);
            AgcDriverShutdown_nid_postfix();
            return 0;
        }
        TablesChangingPerDispatch(block);
        SrtMovingPerDispatch(block);
        SrtMovingPerDraw(block);
        AgcDriverShutdown_nid_postfix();
        std::puts("image table cache bounds tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
