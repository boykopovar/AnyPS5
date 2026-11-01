#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
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
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::Driver;

constexpr std::array<std::uint32_t, 7> Code{0xf4080100u, 0xfa000000u, 0xbf8cc07fu, 0x7e020280u, 0xe0702000u, 0x80010100u, 0xbf810000u};
constexpr std::size_t SrtOffset = 4096;
constexpr std::size_t OutputOffset = 8192;
constexpr std::size_t BlockBytes = 65536;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* mapped = mmap(nullptr, BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            block = static_cast<std::uint8_t*>(mapped);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "dispatch differing value sets: cannot allocate the guest block");
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

void WriteSrt(const GuestBlock& block, std::uint32_t records) {
    const auto address = block.At(OutputOffset);
    const std::array<std::uint32_t, 4> words{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), records, 0x00014204u};
    std::memcpy(block.Bytes(SrtOffset), words.data(), sizeof(words));
}

void Dispatch(const GuestBlock& block) {
    const auto kernel = block.At(0);
    const auto srt = block.At(SrtOffset);
    std::vector<std::uint32_t> words;
    for (const auto& packet : {Pm4(0x76, {0x20c, static_cast<std::uint32_t>(kernel >> 8u)}), Pm4(0x76, {0x20d, static_cast<std::uint32_t>(kernel >> 40u) & 0xffu}), Pm4(0x76, {0x207, 64}), Pm4(0x76, {0x208, 1}), Pm4(0x76, {0x209, 1}), Pm4(0x76, {0x213, 2u << 1u}), Pm4(0x76, {0x240, static_cast<std::uint32_t>(srt)}), Pm4(0x76, {0x241, static_cast<std::uint32_t>(srt >> 32u)}), Pm4(0x15, {1, 1, 1, 0x41})}) {
        words.insert(words.end(), packet.begin(), packet.end());
    }
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "dispatch differing value sets: the dispatch submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

struct Totals {
    std::uint64_t classified = 0, matched = 0;
    std::uint64_t lastClassified = 0, lastMatched = 0;

    void Add() {
        const auto counters = Driver::Get().DispatchCacheCounters();
        classified += counters.differingClassified >= lastClassified ? counters.differingClassified - lastClassified : counters.differingClassified;
        matched += counters.differingMatchedPrior >= lastMatched ? counters.differingMatchedPrior - lastMatched : counters.differingMatchedPrior;
        lastClassified = counters.differingClassified;
        lastMatched = counters.differingMatchedPrior;
    }
};

void AlternatingValueSets(const GuestBlock& block) {
    Totals totals;
    totals.Add();
    for (std::uint32_t dispatch = 0; dispatch < 5; ++dispatch) {
        WriteSrt(block, dispatch % 2u == 0u ? 64u : 65u);
        Dispatch(block);
        totals.Add();
    }
    std::printf("alternating value sets: %llu differing dispatches classified, %llu matched one of the last 3 value sets\n", static_cast<unsigned long long>(totals.classified), static_cast<unsigned long long>(totals.matched));
    Require(totals.classified == 4, "the four dispatches after the first were not all classified as differing (" + std::to_string(totals.classified) + ")");
    Require(totals.matched == 3, "the three dispatches bringing a value set seen before did not all match it (" + std::to_string(totals.matched) + ")");
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_DRAW", "1");
        _putenv_s("APS5_DISPATCH_VARIANTS", "1");
#else
        setenv("APS5_PROFILE_DRAW", "1", 1);
        setenv("APS5_DISPATCH_VARIANTS", "1", 1);
#endif
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GuestBlock block;
        std::memcpy(block.Bytes(0), Code.data(), sizeof(Code));
        AlternatingValueSets(block);
        AgcDriverShutdown_nid_postfix();
        std::puts("dispatch differing value sets: the differing statistics recognize a value set they saw before");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
