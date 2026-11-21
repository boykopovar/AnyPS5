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
using AgcDriver::DriverDetail::MatchesTableCopyKernel;
using AgcDriver::DriverDetail::TableCopyRange;

alignas(256) constexpr std::array<std::uint32_t, 11> Code{0xd7460000u, 0x04010c02u, 0xf40c0100u, 0xfa000000u, 0xbf8cc07fu, 0xe030e000u, 0x80010100u, 0xbf8c3f70u, 0xe0706000u, 0x80020100u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t Format = 0x00005204u;
constexpr std::size_t Words = 4096;
alignas(256) std::array<std::uint32_t, Words> Source{};
alignas(256) std::array<std::uint32_t, Words> Destination{};
alignas(256) std::array<std::uint32_t, 8> Table{};

std::array<std::uint32_t, 8> Descriptors(const std::uint32_t* source, std::uint32_t sourceRecords, const std::uint32_t* destination, std::uint32_t destinationRecords, std::uint32_t stride = 4) {
    const auto from = reinterpret_cast<std::uintptr_t>(source);
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    return {static_cast<std::uint32_t>(from), static_cast<std::uint32_t>((from >> 32u) & 0xffffu) | (stride << 16u), sourceRecords, Format,
            static_cast<std::uint32_t>(to), static_cast<std::uint32_t>((to >> 32u) & 0xffffu) | (stride << 16u), destinationRecords, Format};
}

std::vector<std::uint32_t> UserData() {
    const auto table = reinterpret_cast<std::uintptr_t>(Table.data());
    return {static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u)};
}

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const auto data = UserData();
    Require(MatchesTableCopyKernel(Code, data, compute), "the descriptor-table copy kernel was not recognized");
    for (std::size_t word = 0; word < Code.size(); ++word) {
        auto altered = Code;
        altered[word] ^= 1u;
        Require(!MatchesTableCopyKernel(altered, data, compute), "a different shader matched the descriptor-table copy kernel (word " + std::to_string(word) + ")");
    }
    Require(!MatchesTableCopyKernel(std::span(Code).first(10), data, compute), "a truncated shader matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchesTableCopyKernel(Code, extra, compute), "a different user SGPR count matched (the group id moves)");
    auto wave32 = compute;
    wave32.numThreads[0] = 32;
    Require(!MatchesTableCopyKernel(Code, data, wave32), "a group of 32 threads matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchesTableCopyKernel(Code, data, noGroup), "a dispatch without the group id matched");

    const auto source = reinterpret_cast<std::uintptr_t>(Source.data());
    const auto destination = reinterpret_cast<std::uintptr_t>(Destination.data());
    const auto copy = TableCopyRange(Descriptors(Source.data(), 1024, Destination.data(), 1024), 4);
    Require(copy && copy->source == source && copy->destination == destination && copy->bytes == 1024, "4 groups were not taken as a copy of 256 dwords");
    const auto fewer = TableCopyRange(Descriptors(Source.data(), 1024, Destination.data(), 100), 4);
    Require(fewer && fewer->bytes == 400, "the destination's records did not bound the copy");
    Require(!TableCopyRange(Descriptors(Source.data(), 100, Destination.data(), 1024), 4), "a copy reading past the source's records was taken");
    Require(!TableCopyRange(Descriptors(Source.data(), 1024, Destination.data(), 1024, 16), 4), "a stride other than 4 was taken");
    auto swizzled = Descriptors(Source.data(), 1024, Destination.data(), 1024);
    swizzled[5] |= 0x80000000u;
    Require(!TableCopyRange(swizzled, 4), "a swizzled destination was taken");
    auto formats = Descriptors(Source.data(), 1024, Destination.data(), 1024);
    formats[7] ^= 0x1000u;
    Require(!TableCopyRange(formats, 4), "a destination of another format was taken");
    Require(!TableCopyRange(Descriptors(Source.data(), 1024, Destination.data(), 1024), 0), "a copy of 0 groups was taken");
    Require(!TableCopyRange(Descriptors(Source.data(), 0, Destination.data(), 0), 4), "a copy of 0 dwords was taken");
}

std::vector<std::uint32_t> Commands(std::uint32_t groups) {
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
    const auto data = UserData();
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
    struct Case {
        const char* name;
        std::uint32_t groups, sourceRecords, destinationRecords;
    };
    constexpr std::array<Case, 3> cases{{
        {"4 groups", 4, 1024, 1024},
        {"16 groups", 16, 4096, 4096},
        {"destination records below the launch", 4, 1024, 100},
    }};
    const bool hle = std::getenv("APS5_NO_COPY_HLE") == nullptr;
    for (const auto& item : cases) {
        for (std::size_t i = 0; i < Source.size(); ++i) Source[i] = static_cast<std::uint32_t>(i) * 2654435761u + 1u;
        Destination.fill(Untouched);
        Table = Descriptors(Source.data(), item.sourceRecords, Destination.data(), item.destinationRecords);
        const auto taken = Taken();
        const auto words = Commands(item.groups);
        Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Destination.data()), sizeof(Destination));
        const std::string where = std::string(" (") + item.name + ")";
        Require(Taken() - taken == (hle ? 1u : 0u), (hle ? "the copy did not take the HLE path" : "the copy took the HLE path") + where);
        const auto elements = std::min(item.groups * 64u, item.destinationRecords);
        for (std::size_t i = 0; i < Destination.size(); ++i) {
            const auto expected = i < elements ? Source[i] : Untouched;
            Require(Destination[i] == expected, "descriptor-table copy mismatch at dword " + std::to_string(i) + where + ": got " + std::to_string(Destination[i]) + ", expected " + std::to_string(expected));
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
        std::puts("descriptor-table copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
