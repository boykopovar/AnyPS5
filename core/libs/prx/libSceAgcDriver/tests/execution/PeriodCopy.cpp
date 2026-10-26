#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferCopy.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 38> Code{
    0xbfa00002u, 0xd7460000u, 0x04010c0cu, 0xf4201a84u, 0xfa000000u, 0xbf8cc07fu, 0x7da8006au, 0xbf88001du,
    0xf4200204u, 0xfa000004u, 0xbf8cc07fu, 0x7e020c08u, 0x81ea0880u, 0x7e025701u, 0x100202ffu, 0x4f7ffffeu,
    0x7e020f01u, 0x7ed60501u, 0x936a6b6au, 0x9aea6b6au, 0x4a02026au, 0xd56a0001u, 0x00020101u, 0xd5690001u,
    0x00020208u, 0x4c020300u, 0x7d860208u, 0x4e040208u, 0x02020501u, 0x7d860208u, 0x4e040208u, 0x02020501u,
    0xe0002000u, 0x80000101u, 0xbf8c3f70u, 0xe0102000u, 0x80010100u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t ElementFormat = 0x00014004u;
constexpr std::uint32_t RecordFormat = 0x0004dfacu;
constexpr std::uint32_t Elements = 8192;
alignas(256) std::array<std::uint32_t, Elements> Source{};
alignas(256) std::array<std::uint32_t, Elements> Destination{};
alignas(256) std::array<std::uint32_t, 4> Record{};

void AppendDescriptor(std::vector<std::uint32_t>& data, const void* base, std::uint32_t stride, std::uint32_t records, std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    data.insert(data.end(), {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, format});
}

std::vector<std::uint32_t> Commands(std::uint32_t groups) {
    std::vector<std::uint32_t> data;
    AppendDescriptor(data, Source.data(), 4, Elements, ElementFormat);
    AppendDescriptor(data, Destination.data(), 4, Elements, ElementFormat);
    AppendDescriptor(data, Record.data(), 16, 1, RecordFormat);
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
    struct Case {
        const char* name;
        std::uint32_t count, period, groups;
        bool hle;
    };
    constexpr std::array<Case, 5> cases{{
        {"one full group", 64, 64, 1, true},
        {"a period above the count", 1000, 4096, 16, true},
        {"a partial last group", 100, 100, 2, true},
        {"a period below the count", 300, 7, 5, false},
        {"more groups than the count needs", 64, 64, 2, false},
    }};
    const bool hleEnabled = std::getenv("APS5_NO_COPY_HLE") == nullptr;
    for (const auto& item : cases) {
        const bool hle = item.hle && hleEnabled;
        for (std::size_t i = 0; i < Source.size(); ++i) Source[i] = static_cast<std::uint32_t>(i) * 2654435761u + 1u;
        Destination.fill(Untouched);
        Record = {item.count, item.period, 0, 0};
        const auto taken = Taken();
        const auto words = Commands(item.groups);
        Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
        AgcDriver::Submit(&packet, 0);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Destination.data()), sizeof(Destination));
        const std::string where = std::string(" (") + item.name + ")";
        Require(Taken() - taken == (hle ? 1u : 0u), (hle ? "the copy did not take the HLE path" : "the copy took the HLE path") + where);
        for (std::size_t i = 0; i < Destination.size(); ++i) {
            const auto expected = i < item.count ? Source[i % item.period] : Untouched;
            Require(Destination[i] == expected, "copy mismatch at element " + std::to_string(i) + where + ": got " + std::to_string(Destination[i]) + ", expected " + std::to_string(expected));
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
        ExecuteTests();
        AgcDriverShutdown_nid_postfix();
        std::puts("period copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
