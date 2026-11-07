#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t Entries = 16;
constexpr std::uint32_t Dispatches = 40;
constexpr std::size_t SrtBytes = 64;
constexpr std::size_t OutputBytes = 256;
constexpr std::size_t SrtOffset = 4096;
constexpr std::size_t OutputOffset = SrtOffset + static_cast<std::size_t>(Dispatches) * SrtBytes;
constexpr std::size_t BlockBytes = (OutputOffset + static_cast<std::size_t>(Dispatches) * OutputBytes + 65535) & ~static_cast<std::size_t>(65535);

constexpr std::array<std::uint32_t, 7> StoreCode{0xf4080100u, 0xfa000000u, 0xbf8c0000u, 0x7e020281u, 0xe0700000u, 0x80010100u, 0xbf810000u};

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
        Require(block != nullptr, "recipe template aging: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }

private:
    std::uint8_t* block = nullptr;
};

std::vector<std::uint32_t> Pm4(std::uint32_t opcode, std::initializer_list<std::uint32_t> body) {
    std::vector<std::uint32_t> words{0xc0000000u | (static_cast<std::uint32_t>(body.size() - 1) << 16u) | (opcode << 8u)};
    words.insert(words.end(), body);
    return words;
}

void Prepare(const GuestBlock& block) {
    std::memcpy(block.Bytes(0), StoreCode.data(), sizeof(StoreCode));
    for (std::uint32_t dispatch = 0; dispatch < Dispatches; ++dispatch) {
        const auto output = block.At(OutputOffset + static_cast<std::size_t>(dispatch) * OutputBytes);
        const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), static_cast<std::uint32_t>(OutputBytes), 0x31016facu};
        std::memcpy(block.Bytes(SrtOffset + static_cast<std::size_t>(dispatch) * SrtBytes), descriptor.data(), sizeof(descriptor));
    }
    std::memset(block.Bytes(OutputOffset), 0, static_cast<std::size_t>(Dispatches) * OutputBytes);
}

void Run(const GuestBlock& block, std::span<const std::uint32_t> order) {
    const auto kernel = block.At(0);
    std::vector<std::uint32_t> words;
    for (const auto dispatch : order) {
        const auto srt = block.At(SrtOffset + static_cast<std::size_t>(dispatch) * SrtBytes);
        for (const auto& packet : {Pm4(0x76, {0x20c, static_cast<std::uint32_t>(kernel >> 8u)}), Pm4(0x76, {0x20d, static_cast<std::uint32_t>(kernel >> 40u) & 0xffu}), Pm4(0x76, {0x207, 64}), Pm4(0x76, {0x208, 1}), Pm4(0x76, {0x209, 1}), Pm4(0x76, {0x213, 2u << 1u}), Pm4(0x76, {0x240, static_cast<std::uint32_t>(srt)}), Pm4(0x76, {0x241, static_cast<std::uint32_t>(srt >> 32u)}), Pm4(0x15, {1, 1, 1, 0x41})}) {
            words.insert(words.end(), packet.begin(), packet.end());
        }
    }
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "recipe template aging: the dispatch submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void HotTemplateOutlivesChurn(const GuestBlock& block) {
    std::vector<std::uint32_t> warm{0, 0, 0};
    Run(block, warm);
    std::vector<std::uint32_t> order;
    for (std::uint32_t cold = 1; cold < Dispatches; ++cold) {
        order.push_back(0);
        order.push_back(cold);
    }
    const auto hot = static_cast<std::uint64_t>(std::count(order.begin(), order.end(), 0u));
    const auto restarts = AgcDriver::VulkanDevice::DispatchRecipeTemplatesGone();
    const auto touches = AgcDriver::Graphics::ResourceCache::Touches();
    for (std::size_t at = 0; at < order.size(); at += 2) Run(block, std::span(order).subspan(at, 2));
    const auto touched = AgcDriver::Graphics::ResourceCache::Touches() - touches;
    const auto restarted = AgcDriver::VulkanDevice::DispatchRecipeTemplatesGone() - restarts;
    if (!AgcDriver::VulkanDevice::VerifyRecipes()) Require(touched > 0 && touched < hot, "recipe template aging: " + std::to_string(hot) + " recipe hits on the hot dispatch made " + std::to_string(touched) + " cache touches; expected some, and fewer than one per hit");
    Require(restarted == 0, "recipe template aging: the hot dispatch's template aged out of a " + std::to_string(Entries) + "-entry cache under cold dispatches, so " + std::to_string(restarted) + " of its " + std::to_string(hot) + " recipe hits rebuilt");
    for (std::uint32_t dispatch = 0; dispatch < Dispatches; ++dispatch) {
        std::uint32_t stored = 0;
        AgcDriver::GuestMemory::Read(block.At(OutputOffset + static_cast<std::size_t>(dispatch) * OutputBytes), std::as_writable_bytes(std::span(&stored, 1)), 4);
        Require(stored == 1u, "recipe template aging: dispatch " + std::to_string(dispatch) + " stored " + std::to_string(stored) + ", expected 1");
    }
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_RESOURCE_CACHE_ENTRIES", "16");
#else
    setenv("APS5_RESOURCE_CACHE_ENTRIES", "16", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        if (!AgcDriver::VulkanDevice::DispatchRecipes()) {
            std::puts("skipped, dispatch recipes are off");
            return VulkanTestSkipped;
        }
        auto& cache = AgcDriver::Graphics::SharedResourceCache();
        cache.Clear();
        GuestBlock block;
        Prepare(block);
        HotTemplateOutlivesChurn(block);
        AgcDriverShutdown_nid_postfix();
        std::puts("a hot dispatch served by recipes kept its template in a churning resource cache with fewer touches than hits");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
