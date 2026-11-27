#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
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
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t WaveSize = 64;
constexpr std::uint32_t Threads = 512;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 2;
constexpr std::size_t PageBytes = 4096;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t InputOffset = PageBytes;
constexpr std::size_t OutputOffset = InputOffset + 3 * PageBytes;

alignas(256) constexpr std::array<std::uint32_t, 14> AlignedCode{
    0xf4080100, 0xfa000000, 0x34020084, 0x34040083, 0xbf8cc07f, 0xdc388000, 0x04040001, 0xbf8c3f70,
    0x7d820d07, 0x50140906, 0x26160b04, 0xdc748000, 0x00060a02, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 17> MixedCode{
    0xf4080100, 0xfa000000, 0x34020084, 0x34040083, 0x36060081, 0x34060681, 0x4a020701, 0xbf8cc07f,
    0xdc388000, 0x04040001, 0xbf8c3f70, 0x7d820d07, 0x50140906, 0x26160b04, 0xdc748000, 0x00060a02,
    0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
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

    std::uint8_t* Data() const { return block; }
    std::uint32_t* Input() const { return reinterpret_cast<std::uint32_t*>(block + InputOffset); }
    std::array<std::uint32_t, Inputs> Row(std::uint32_t thread, bool mixed) const {
        std::array<std::uint32_t, Inputs> row{};
        std::memcpy(row.data(), block + InputOffset + thread * Inputs * 4u + (mixed ? (thread & 1u) * 2u : 0u), sizeof(row));
        return row;
    }
    std::uint32_t* Output() const { return reinterpret_cast<std::uint32_t*>(block + OutputOffset); }

private:
    std::uint8_t* block = nullptr;
};

void Fill(const GuestBlock& guest) {
    const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::array<std::uint64_t, 2> arguments{base + InputOffset, base + OutputOffset};
    std::memcpy(guest.Data(), arguments.data(), sizeof(arguments));
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (std::uint32_t word = 0; word < Threads * Inputs + 1u; ++word) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        guest.Input()[word] = static_cast<std::uint32_t>(state >> 32u);
    }
    std::fill_n(guest.Output(), Threads * Results, 0xdeadbeefu);
}

ShaderRecompiler::RecompileResult Compile(const ShaderRecompiler::SpirvTarget& target, const GuestBlock& guest, std::span<const std::uint32_t> code) {
    const auto base = reinterpret_cast<std::uintptr_t>(guest.Data());
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{
        {reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)},
        {base, std::span<const std::byte>(reinterpret_cast<const std::byte*>(guest.Data()), BlockBytes)}
    }};
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(base), static_cast<std::uint32_t>(static_cast<std::uint64_t>(base) >> 32u)};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {WaveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

struct SpanFunctions {
    std::uint32_t reads = 0;
    std::uint32_t readsInline = 0;
    std::uint32_t readsDontInline = 0;
    std::uint32_t loads = 0;
    std::uint32_t loadsDontInline = 0;
};

SpanFunctions Controls(std::span<const std::uint32_t> words) {
    std::vector<std::uint32_t> reads;
    std::vector<std::uint32_t> loads;
    SpanFunctions functions;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpName) {
            const std::string_view name(reinterpret_cast<const char*>(&words[at + 2]));
            if (name.starts_with("read_bda_span")) reads.push_back(words[at + 1]);
            if (name.starts_with("load_bda_span")) loads.push_back(words[at + 1]);
        }
        if (op != spv::OpFunction) continue;
        const std::uint32_t control = words[at + 3];
        if (std::ranges::find(reads, words[at + 2]) != reads.end()) {
            ++functions.reads;
            if ((control & spv::FunctionControlInlineMask) != 0u) ++functions.readsInline;
            if ((control & spv::FunctionControlDontInlineMask) != 0u) ++functions.readsDontInline;
        }
        if (std::ranges::find(loads, words[at + 2]) != loads.end()) {
            ++functions.loads;
            if ((control & spv::FunctionControlDontInlineMask) != 0u) ++functions.loadsDontInline;
        }
    }
    return functions;
}

void CheckControls(const AgcDriver::VulkanDevice& device, const GuestBlock& guest) {
    const bool expected = device.DriverId() == VK_DRIVER_ID_NVIDIA_PROPRIETARY && std::getenv("APS5_NO_TWO_LANE_SPAN_READ_INLINE") == nullptr;
    Require(device.Target().twoLaneSpanReadInline == expected, std::string("the device target ") + (expected ? "does not ask" : "asks") + " for inlined span reads in two-lane modules on Vulkan driver " + std::to_string(static_cast<int>(device.DriverId())) +
        "; NVIDIA's proprietary driver needs them unless APS5_NO_TWO_LANE_SPAN_READ_INLINE is set, and other drivers keep the calls");
    for (const std::uint32_t subgroupSize : {32u, 64u}) {
        for (const bool requested : {false, true}) {
            auto target = device.Target();
            target.subgroupSize = subgroupSize;
            target.twoLaneSpanReadInline = requested;
            const std::string name = std::string("a 512-thread wave64 module for ") + std::to_string(subgroupSize) + "-lane subgroups " + (requested ? "with" : "without") + " the span read inlining requested";
            const auto functions = Controls(Compile(target, guest, AlignedCode).spirv.Words());
            Require(functions.reads != 0u && functions.loads != 0u, name + " defines " + std::to_string(functions.reads) + " read_bda_span and " + std::to_string(functions.loads) + " load_bda_span functions, so this test does not cover the span read");
            Require(functions.loadsDontInline == functions.loads, name + " keeps " + std::to_string(functions.loadsDontInline) + " of " + std::to_string(functions.loads) + " load_bda_span functions DontInline; the loads in the caller's arm hang NVIDIA drivers");
            if (requested && subgroupSize == 32u) {
                Require(functions.readsInline == functions.reads && functions.readsDontInline == 0u, name + " gives " + std::to_string(functions.readsInline) + " of " + std::to_string(functions.reads) + " read_bda_span functions the Inline control, and it holds two lanes per invocation");
            } else {
                Require(functions.readsDontInline == functions.reads && functions.readsInline == 0u, name + " keeps " + std::to_string(functions.readsDontInline) + " of " + std::to_string(functions.reads) + " read_bda_span functions DontInline");
            }
        }
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void CheckResults(const GuestBlock& guest, bool mixed) {
    std::uint32_t wrong = 0;
    std::uint32_t wrongUpper = 0;
    std::uint32_t fromLowerHalf = 0;
    std::string first;
    for (std::uint32_t thread = 0; thread < Threads; ++thread) {
        const auto row = guest.Row(thread, mixed);
        const std::uint32_t lane = thread % WaveSize;
        const std::array<std::uint32_t, Results> expected{row[2] + row[0] + (row[3] < row[2] ? 1u : 0u), std::min(row[0], row[1])};
        const std::array<std::uint32_t, Results> got{guest.Output()[thread * Results], guest.Output()[thread * Results + 1u]};
        if (got == expected) continue;
        ++wrong;
        if (lane >= 32u) {
            ++wrongUpper;
            const auto lower = guest.Row(thread - 32u, mixed);
            if (got[0] == lower[2] + lower[0] + 1u) ++fromLowerHalf;
        }
        if (first.empty()) first = "wave " + std::to_string(thread / WaveSize) + " lane " + std::to_string(lane) + ": v_add_co_ci_u32 gives " + Hex(got[0]) + ", expected " + Hex(expected[0]) + ", and v_min_u32 gives " + Hex(got[1]) + ", expected " + Hex(expected[1]);
    }
    Require(wrong == 0u, std::string(mixed ? "rows at aligned and unaligned addresses" : "rows at aligned addresses") + ": " + std::to_string(wrong) + " of " + std::to_string(Threads) + " lanes are wrong, " + std::to_string(wrongUpper) + " of them in lanes 32 to 63, of which " + std::to_string(fromLowerHalf) + " hold the sum of lane L-32 plus one; first " + first);
}

void Run(AgcDriver::VulkanDevice& device, const GuestBlock& guest, bool mixed) {
    const std::span<const std::uint32_t> code = mixed ? std::span<const std::uint32_t>(MixedCode) : std::span<const std::uint32_t>(AlignedCode);
    Fill(guest);
    const auto result = Compile(device.Target(), guest, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    CheckResults(guest, mixed);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const GuestBlock guest;
        Fill(guest);
        CheckControls(*device, guest);
        for (const bool mixed : {false, true}) Run(*device, guest, mixed);
        std::puts("wave64 span read lane tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
