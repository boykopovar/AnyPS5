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
using AgcDriver::DriverDetail::IndexedCopyRange;
using AgcDriver::DriverDetail::MatchesIndexedCopyKernel;

alignas(256) constexpr std::array<std::uint32_t, 19> Code{0xbfa00001u, 0xd7460000u, 0x04010c0cu, 0xf4201a84u, 0xfa000000u, 0xbf8cc07fu, 0x7da8006au, 0xbf88000au, 0xf4241a84u, 0xfa000004u, 0xbf8cc07fu, 0x4a08006bu, 0x4a00006au, 0xe0382000u, 0x80000000u, 0xbf8c3f70u, 0xe0782000u, 0x80010004u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Format = 0x00005204u;
constexpr std::uint32_t RecordFormat = 0x0004dfacu;
constexpr std::uint32_t LargeGroups = 16385;
constexpr std::size_t Words = (std::size_t{LargeGroups} * 64u + 64u) * 4u;
alignas(256) std::array<std::uint32_t, Words> Source{};
alignas(256) std::array<std::uint32_t, Words> Destination{};
alignas(256) std::array<std::uint32_t, 4> Record{};

std::vector<std::uint32_t> UserData(const std::uint32_t* source, std::uint32_t sourceRecords, const std::uint32_t* destination, std::uint32_t destinationRecords, const std::uint32_t* record = Record.data(), std::uint32_t sourceStride = 16, std::uint32_t destinationStride = 16, std::uint32_t recordStride = 16) {
    const auto from = reinterpret_cast<std::uintptr_t>(source);
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    const auto at = reinterpret_cast<std::uintptr_t>(record);
    return {static_cast<std::uint32_t>(from), static_cast<std::uint32_t>((from >> 32u) & 0xffffu) | (sourceStride << 16u), sourceRecords, Format,
            static_cast<std::uint32_t>(to), static_cast<std::uint32_t>((to >> 32u) & 0xffffu) | (destinationStride << 16u), destinationRecords, Format,
            static_cast<std::uint32_t>(at), static_cast<std::uint32_t>((at >> 32u) & 0xffffu) | (recordStride << 16u), 1u, RecordFormat};
}

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const auto data = UserData(Source.data(), 512, Destination.data(), 512);
    Require(MatchesIndexedCopyKernel(Code, data, compute), "the indexed copy kernel was not recognized");
    for (std::size_t word = 0; word < Code.size(); ++word) {
        auto altered = Code;
        altered[word] ^= 1u;
        Require(!MatchesIndexedCopyKernel(altered, data, compute), "a different shader matched the indexed copy kernel (word " + std::to_string(word) + ")");
    }
    Require(!MatchesIndexedCopyKernel(std::span(Code).first(18), data, compute), "a truncated shader matched");
    Require(!MatchesIndexedCopyKernel(Code, UserData(Source.data(), 512, Destination.data(), 512, Record.data(), 4), compute), "a source stride other than 16 matched");
    Require(!MatchesIndexedCopyKernel(Code, UserData(Source.data(), 512, Destination.data(), 512, Record.data(), 16, 4), compute), "a destination stride other than 16 matched");
    Require(!MatchesIndexedCopyKernel(Code, UserData(Source.data(), 512, Destination.data(), 512, Record.data(), 16, 16, 4), compute), "a record stride other than 16 matched");
    auto swizzled = data;
    swizzled[5] |= 0x80000000u;
    Require(!MatchesIndexedCopyKernel(Code, swizzled, compute), "a swizzled destination matched");
    auto formats = data;
    formats[7] ^= 0x1000u;
    Require(!MatchesIndexedCopyKernel(Code, formats, compute), "descriptors of different formats matched");
    const auto both = [&](std::uint32_t word3) {
        auto changed = data;
        changed[3] = changed[7] = word3;
        return MatchesIndexedCopyKernel(Code, changed, compute);
    };
    Require(!both(Format | 0x00800000u), "an add-tid descriptor matched");
    Require(!both(Format | 0x30000000u), "a raw out-of-bounds mode matched");
    Require(!both(Format | 0x40000000u), "a descriptor of another type matched");
    auto noRecords = data;
    noRecords[10] = 0;
    Require(!MatchesIndexedCopyKernel(Code, noRecords, compute), "a record descriptor without records matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchesIndexedCopyKernel(Code, extra, compute), "a different user SGPR count matched (the group id moves)");
    auto wave32 = compute;
    wave32.numThreads[0] = 32;
    Require(!MatchesIndexedCopyKernel(Code, data, wave32), "a group of 32 threads matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchesIndexedCopyKernel(Code, data, noGroup), "a dispatch without the group id matched");

    const auto source = reinterpret_cast<std::uintptr_t>(Source.data());
    const auto destination = reinterpret_cast<std::uintptr_t>(Destination.data());
    const auto copy = IndexedCopyRange(data, 4, 256, 0, 0);
    Require(copy && copy->source == source && copy->destination == destination && copy->bytes == 4096, "4 groups were not taken as a copy of 256 elements");
    const auto offset = IndexedCopyRange(data, 4, 200, 16, 300);
    Require(offset && offset->source == source + 256 && offset->destination == destination + 4800 && offset->bytes == 3200, "the offsets were not applied in elements");
    const auto launched = IndexedCopyRange(data, 4, 300, 0, 0);
    Require(launched && launched->bytes == 4096, "the copy exceeded the launched threads");
    const auto fewer = IndexedCopyRange(data, 4, 100, 0, 0);
    Require(fewer && fewer->bytes == 1600, "a count below the launch was not taken as the copy");
    Require(!IndexedCopyRange(data, 4, 256, 300, 0), "a source window past the records was taken");
    Require(!IndexedCopyRange(data, 4, 256, 0, 300), "a destination window past the records was taken");
    Require(!IndexedCopyRange(data, 4, 0, 0, 0), "a copy of 0 elements was taken");
    Require(!IndexedCopyRange(data, 0, 256, 0, 0), "a copy of 0 groups was taken");
    Require(IndexedCopyRange(UserData(Source.data(), 16384u * 64u, Destination.data(), 16384u * 64u), 16384, 16384u * 64u, 0, 0).has_value(), "a copy of 16 MiB was refused");
    Require(!IndexedCopyRange(UserData(Source.data(), LargeGroups * 64u, Destination.data(), LargeGroups * 64u), LargeGroups, LargeGroups * 64u, 0, 0), "a copy above 16 MiB was taken");
    Require(!IndexedCopyRange(UserData(Source.data() + 1, 512, Destination.data(), 512), 4, 256, 0, 0), "a source not aligned to 16 bytes was taken");
    Require(!IndexedCopyRange(UserData(Source.data(), 512, Destination.data() + 1, 512), 4, 256, 0, 0), "a destination not aligned to 16 bytes was taken");
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
        std::uint32_t groups, count, sourceOffset, destinationOffset, sourceRecords, destinationRecords;
        bool hle;
    };
    constexpr std::array<Case, 8> cases{{
        {"4 groups", 4, 256, 0, 0, 512, 512, true},
        {"16 groups", 16, 1024, 0, 0, 2048, 2048, true},
        {"offsets", 4, 200, 16, 300, 512, 512, true},
        {"count below the launch", 4, 100, 0, 0, 512, 512, true},
        {"count above the launch", 4, 300, 0, 0, 512, 512, true},
        {"source window past the records", 4, 256, 300, 0, 512, 512, false},
        {"destination window past the records", 4, 256, 0, 300, 512, 512, false},
        {"above 16 MiB", LargeGroups, LargeGroups * 64u, 0, 0, LargeGroups * 64u, LargeGroups * 64u, false},
    }};
    const bool hleEnabled = std::getenv("APS5_NO_COPY_HLE") == nullptr;
    for (const auto& item : cases) {
        const bool hle = item.hle && hleEnabled;
        for (std::size_t i = 0; i < Source.size(); ++i) Source[i] = static_cast<std::uint32_t>(i) * 2654435761u + 1u;
        Destination.fill(Untouched);
        Record = {item.count, item.sourceOffset, item.destinationOffset, 0u};
        const auto taken = Taken();
        const auto refused = copyPathCounts[CopyShape].load();
        const auto words = Commands(UserData(Source.data(), item.sourceRecords, Destination.data(), item.destinationRecords), item.groups);
        Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Destination.data()), sizeof(Destination));
        const std::string where = std::string(" (") + item.name + ")";
        Require(Taken() - taken == (hle ? 1u : 0u), (hle ? "the copy did not take the HLE path" : "the copy took the HLE path") + where);
        Require(copyPathCounts[CopyShape].load() - refused == (hle || !hleEnabled ? 0u : 1u), (hle ? "the copy was refused for its shape" : "the copy was not refused for its shape") + where);
        const auto elements = std::min(item.groups * 64u, item.count);
        for (std::size_t i = 0; i < Destination.size(); ++i) {
            const auto element = i / 4u;
            const bool written = element >= item.destinationOffset && element < std::uint64_t{item.destinationOffset} + elements && element < item.destinationRecords;
            const auto sourceElement = element - item.destinationOffset + item.sourceOffset;
            const auto expected = !written ? Untouched : sourceElement < item.sourceRecords ? Source[sourceElement * 4u + i % 4u] : 0u;
            Require(Destination[i] == expected, "indexed copy mismatch at word " + std::to_string(i) + where + ": got " + std::to_string(Destination[i]) + ", expected " + std::to_string(expected));
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
        std::puts("indexed copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
