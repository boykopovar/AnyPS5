#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t ActiveLanes = 40;
constexpr std::uint32_t Counters = 4;
constexpr std::uint32_t CounterOffset = 0x10;
constexpr std::uint32_t CopyOffset = 0x100;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t RoundTripOffset = 256;
constexpr std::array<std::uint32_t, Counters> Starts{1000, 2000, 3000, 4000};
constexpr std::array<std::uint32_t, Counters> Expected{1000 + Threads, 2000 - Threads, 3000 + ActiveLanes, 4000 - ActiveLanes};
alignas(256) std::array<std::uint32_t, Threads * 4> Output{};

alignas(256) auto Code = std::to_array<std::uint32_t>({
    0x34060082,
    0x7e0c02ff, 0x5eed0001u, 0x7e0e02ff, 0x5eed0002u,
    0xbefc03ff, 0x0000ffff,
    0xd8fa0010, 0x04000000,
    0xd8f60014, 0x05000000,
    0x7da800ff, ActiveLanes,
    0xd8fa0018, 0x06000000,
    0xd8f6001c, 0x07000000,
    0xbefe04c1,
    0xbf8cc07f,
    0xe0702000, 0x80000403, 0xe0702004, 0x80000503, 0xe0702008, 0x80000603, 0xe070200c, 0x80000703,
    0xbf810000,
});

alignas(256) auto EmptyCode = std::to_array<std::uint32_t>({0xbf810000});

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "GDS DMA copy: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::span<std::uint32_t, Counters> FromGds() const { return std::span<std::uint32_t, Counters>(block, Counters); }
    std::span<std::uint32_t, Counters> RoundTrip() const { return std::span<std::uint32_t, Counters>(block + RoundTripOffset / 4, Counters); }

private:
    std::uint32_t* block = nullptr;
};

void Append(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    words.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    words.insert(words.end(), payload);
}

void Registers(std::vector<std::uint32_t>& words, std::uint32_t first, std::span<const std::uint32_t> values) {
    words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
    words.push_back(first);
    words.insert(words.end(), values.begin(), values.end());
}

void Dma(std::vector<std::uint32_t>& words, std::uint32_t selectors, std::uint64_t source, std::uint64_t destination, std::uint32_t bytes) {
    Append(words, 0x50, {selectors, static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u), bytes});
}

void Dispatch(std::vector<std::uint32_t>& words, std::span<const std::uint32_t> code, std::uint32_t userSgprs) {
    const std::array<std::uint32_t, 3> shape{Threads, 1, 1};
    Registers(words, 0x207, shape);
    const auto program = reinterpret_cast<std::uintptr_t>(code.data());
    const std::array<std::uint32_t, 2> address{static_cast<std::uint32_t>(program >> 8u), static_cast<std::uint32_t>(program >> 40u)};
    Registers(words, 0x20c, address);
    const std::array<std::uint32_t, 1> resource{userSgprs << 1u};
    Registers(words, 0x213, resource);
}

std::vector<std::uint32_t> Commands(const GuestBlock& guest) {
    std::vector<std::uint32_t> words;
    for (std::uint32_t counter = 0; counter < Counters; ++counter) Dma(words, 0x40100000u, Starts[counter], CounterOffset + counter * 4u, 4);
    Dispatch(words, Code, 4);
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> descriptor{static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu) | (4u << 16u), static_cast<std::uint32_t>(Output.size()), 0x11016facu};
    Registers(words, 0x240, descriptor);
    Append(words, 0x15, {1, 1, 1, 0x41u});
    const auto fromGds = reinterpret_cast<std::uintptr_t>(guest.FromGds().data());
    const auto roundTrip = reinterpret_cast<std::uintptr_t>(guest.RoundTrip().data());
    Dma(words, 0x20000000u, CounterOffset, fromGds, Counters * 4u);
    Dispatch(words, EmptyCode, 0);
    Append(words, 0x15, {1, 1, 1, 0x41u});
    Dma(words, 0x00100000u, fromGds, CopyOffset, Counters * 4u);
    Dma(words, 0x20000000u, CopyOffset, roundTrip, Counters * 4u);
    return words;
}

void ExecuteTests(const GuestBlock& guest) {
    const auto fromGds = guest.FromGds();
    const auto roundTrip = guest.RoundTrip();
    for (int pass = 0; pass < 2; ++pass) {
        Output.fill(0xdeadbeefu);
        std::fill(fromGds.begin(), fromGds.end(), 0xdeadbeefu);
        std::fill(roundTrip.begin(), roundTrip.end(), 0xdeadbeefu);
        const auto words = Commands(guest);
        Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(fromGds.data()), fromGds.size_bytes());
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(roundTrip.data()), roundTrip.size_bytes());
        for (std::uint32_t counter = 0; counter < Counters; ++counter) {
            Require(fromGds[counter] == Expected[counter], "GDS counter " + std::to_string(counter) + " copied to memory after the dispatch reads " + std::to_string(fromGds[counter]) + ", expected " + std::to_string(Expected[counter]) + " (pass " + std::to_string(pass) + ")");
            Require(roundTrip[counter] == Expected[counter], "GDS counter " + std::to_string(counter) + " copied to memory, back to GDS and out again reads " + std::to_string(roundTrip[counter]) + ", expected " + std::to_string(Expected[counter]) + " (pass " + std::to_string(pass) + ")");
        }
    }
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        {
            const GuestBlock guest;
            ExecuteTests(guest);
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("GDS DMA copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
