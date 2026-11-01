#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferCopy.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::MatchesStreamCopyKernel;
using AgcDriver::DriverDetail::StreamCopyRange;

alignas(256) constexpr std::array<std::uint32_t, 16> Code{0xd7460008u, 0x04010e08u, 0x8f6a8708u, 0xd76d0009u, 0x0401806au, 0xe00c2000u, 0x80000008u, 0xe00c2000u, 0x80000409u, 0xbf8c3f71u, 0xe01c2000u, 0x80010008u, 0xbf8c3f70u, 0xe01c2000u, 0x80010409u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Format = 0x0004bfacu;
constexpr std::uint32_t LargeGroups = 8193;
constexpr std::size_t Words = (std::size_t{LargeGroups} * 128u + 64u) * 4u;
alignas(256) std::array<std::uint32_t, Words> Source{};
alignas(256) std::array<std::uint32_t, Words> Destination{};

std::vector<std::uint32_t> UserData(const std::uint32_t* source, std::uint32_t sourceRecords, const std::uint32_t* destination, std::uint32_t destinationRecords, std::uint32_t sourceStride = 16, std::uint32_t destinationStride = 16) {
    const auto from = reinterpret_cast<std::uintptr_t>(source);
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    return {static_cast<std::uint32_t>(from), static_cast<std::uint32_t>((from >> 32u) & 0xffffu) | (sourceStride << 16u), sourceRecords, Format,
            static_cast<std::uint32_t>(to), static_cast<std::uint32_t>((to >> 32u) & 0xffffu) | (destinationStride << 16u), destinationRecords, Format};
}

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const auto data = UserData(Source.data(), 512, Destination.data(), 512);
    Require(MatchesStreamCopyKernel(Code, data, compute), "the stream copy kernel was not recognized");
    for (std::size_t word = 0; word < Code.size(); ++word) {
        auto altered = Code;
        altered[word] ^= 1u;
        Require(!MatchesStreamCopyKernel(altered, data, compute), "a different shader matched the stream copy kernel (word " + std::to_string(word) + ")");
    }
    Require(!MatchesStreamCopyKernel(std::span(Code).first(15), data, compute), "a truncated shader matched");
    Require(!MatchesStreamCopyKernel(Code, UserData(Source.data(), 512, Destination.data(), 512, 4), compute), "a source stride other than 16 matched");
    Require(!MatchesStreamCopyKernel(Code, UserData(Source.data(), 512, Destination.data(), 512, 16, 4), compute), "a destination stride other than 16 matched");
    auto swizzled = data;
    swizzled[5] |= 0x80000000u;
    Require(!MatchesStreamCopyKernel(Code, swizzled, compute), "a swizzled destination matched");
    auto formats = data;
    formats[7] ^= 0x1000u;
    Require(!MatchesStreamCopyKernel(Code, formats, compute), "descriptors of different formats matched");
    const auto both = [&](std::uint32_t word3) {
        auto changed = data;
        changed[3] = changed[7] = word3;
        return MatchesStreamCopyKernel(Code, changed, compute);
    };
    Require(!both((Format & ~0x7f000u) | (0x14u << 12u)), "a 32-bit format matched a copy of 16-byte elements");
    Require(!both((Format & ~0xfffu) | 0xc6cu), "a swapped destination select matched");
    Require(!both(Format | 0x00800000u), "an add-tid descriptor matched");
    Require(!both(Format | 0x30000000u), "a raw out-of-bounds mode matched");
    Require(!both(Format | 0x40000000u), "a descriptor of another type matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchesStreamCopyKernel(Code, extra, compute), "a different user SGPR count matched (the group id moves)");
    auto wave32 = compute;
    wave32.numThreads[0] = 32;
    Require(!MatchesStreamCopyKernel(Code, data, wave32), "a group of 32 threads matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchesStreamCopyKernel(Code, data, noGroup), "a dispatch without the group id matched");

    const auto source = reinterpret_cast<std::uintptr_t>(Source.data());
    const auto destination = reinterpret_cast<std::uintptr_t>(Destination.data());
    const auto copy = StreamCopyRange(data, 4);
    Require(copy && copy->source == source && copy->destination == destination && copy->bytes == 8192, "4 groups were not taken as a copy of 512 elements");
    const auto clamped = StreamCopyRange(UserData(Source.data(), 512, Destination.data(), 300), 4);
    Require(clamped && clamped->bytes == 300u * 16u, "the copy was not clamped to the destination's records");
    const auto launched = StreamCopyRange(UserData(Source.data(), 600, Destination.data(), 600), 4);
    Require(launched && launched->bytes == 8192, "the copy exceeded the launched threads");
    Require(!StreamCopyRange(UserData(Source.data(), 400, Destination.data(), 512), 4), "a source with fewer records than the copy was taken");
    Require(!StreamCopyRange(UserData(Source.data(), 512, Destination.data(), 0), 4), "a copy of 0 bytes was taken");
    Require(!StreamCopyRange(data, 0), "a copy of 0 groups was taken");
    Require(StreamCopyRange(UserData(Source.data(), 8192u * 128u, Destination.data(), 8192u * 128u), 8192).has_value(), "a copy of 16 MiB was refused");
    Require(!StreamCopyRange(UserData(Source.data(), LargeGroups * 128u, Destination.data(), LargeGroups * 128u), LargeGroups), "a copy above 16 MiB was taken");
    Require(!StreamCopyRange(UserData(Source.data() + 1, 512, Destination.data(), 512), 4), "a source not aligned to 16 bytes was taken");
    Require(!StreamCopyRange(UserData(Source.data(), 512, Destination.data() + 1, 512), 4), "a destination not aligned to 16 bytes was taken");
}

std::vector<std::uint32_t> Commands(const std::vector<std::uint32_t>& data, std::uint32_t groups) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{64, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (1u << 7u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, groups, 1, 1, 0x41u});
    return words;
}

std::uint64_t Taken() {
    using namespace AgcDriver::DriverDetail;
    return copyPathCounts[CopyCpu].load() + copyPathCounts[CopyGpu].load() + copyPathCounts[CopyAliased].load();
}

void ExecuteTests() {
    using AgcDriver::DriverDetail::copyPathCounts;
    using AgcDriver::DriverDetail::CopyShape;
    struct Case {
        const char* name;
        std::uint32_t groups, sourceRecords, destinationRecords, sourceOffset, destinationOffset;
        bool hle;
    };
    constexpr std::array<Case, 9> cases{{
        {"4 groups", 4, 512, 512, 0, 0, true},
        {"16 groups", 16, 2048, 2048, 0, 0, true},
        {"destination records below the launch", 4, 512, 300, 0, 0, true},
        {"records above the launch", 4, 600, 600, 0, 0, true},
        {"source records below the copy", 4, 400, 512, 0, 0, false},
        {"0 destination records", 4, 512, 0, 0, 0, false},
        {"above 16 MiB", LargeGroups, LargeGroups * 128u, LargeGroups * 128u, 0, 0, false},
        {"source not aligned to 16 bytes", 4, 512, 512, 1, 0, false},
        {"destination not aligned to 16 bytes", 4, 512, 512, 0, 1, false},
    }};
    const bool hleEnabled = std::getenv("APS5_NO_COPY_HLE") == nullptr;
    for (const auto& item : cases) {
        const bool hle = item.hle && hleEnabled;
        for (std::size_t i = 0; i < Source.size(); ++i) Source[i] = static_cast<std::uint32_t>(i) * 2654435761u + 1u;
        Destination.fill(Untouched);
        const auto taken = Taken();
        const auto refused = copyPathCounts[CopyShape].load();
        const auto words = Commands(UserData(Source.data() + item.sourceOffset, item.sourceRecords, Destination.data() + item.destinationOffset, item.destinationRecords), item.groups);
        Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Destination.data()), sizeof(Destination));
        const std::string where = std::string(" (") + item.name + ")";
        Require(Taken() - taken == (hle ? 1u : 0u), (hle ? "the copy did not take the HLE path" : "the copy took the HLE path") + where);
        Require(copyPathCounts[CopyShape].load() - refused == (hle || !hleEnabled ? 0u : 1u), (hle ? "the copy was refused for its shape" : "the copy was not refused for its shape") + where);
        const auto elements = std::min(item.groups * 128u, item.destinationRecords);
        for (std::size_t i = 0; i < Destination.size(); ++i) {
            const bool inside = i >= item.destinationOffset && (i - item.destinationOffset) / 4u < elements;
            const auto element = (i - item.destinationOffset) / 4u;
            const auto expected = !inside ? Untouched : element < item.sourceRecords ? Source[item.sourceOffset + i - item.destinationOffset] : 0u;
            Require(Destination[i] == expected, "stream copy mismatch at word " + std::to_string(i) + where + ": got " + std::to_string(Destination[i]) + ", expected " + std::to_string(expected));
        }
    }
}

}

int main() {
    try {
        if (std::getenv("APS5_NO_COPY_HLE") == nullptr) MatchTests();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        ExecuteTests();
        AgcDriverShutdown_nid_postfix();
        std::puts("stream copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
