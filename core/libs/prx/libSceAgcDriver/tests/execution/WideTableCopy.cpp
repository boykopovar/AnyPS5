#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferCopy.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::RuntimeSwitch;
using AgcDriver::DriverDetail::MatchesWideTableCopyKernel;
using AgcDriver::DriverDetail::WideTableCopyFormat;
using AgcDriver::DriverDetail::WideTableCopyKernel;
using AgcDriver::DriverDetail::WideTableCopyRange;

alignas(256) constexpr std::array<std::uint32_t, 11> Code = WideTableCopyKernel;
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::size_t Elements = 262144;
constexpr std::size_t Words = Elements * 4u;
alignas(256) std::array<std::uint32_t, Words> Source{};
alignas(256) std::array<std::uint32_t, Words> Destination{};
alignas(256) std::array<std::uint32_t, 8> Table{};

struct Switched {
    bool before;
    explicit Switched(bool hle) : before(AgcDriver::Graphics::RuntimeSwitches::Set(RuntimeSwitch::NoWideTableCopyHle)) { AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::NoWideTableCopyHle, !hle); }
    ~Switched() { AgcDriver::Graphics::RuntimeSwitches::Assign(RuntimeSwitch::NoWideTableCopyHle, before); }
    Switched(const Switched&) = delete;
    Switched& operator=(const Switched&) = delete;
};

std::array<std::uint32_t, 8> Descriptors(std::uint64_t from, std::uint32_t sourceRecords, std::uint64_t to, std::uint32_t destinationRecords, std::uint32_t stride = 16, std::uint32_t format = WideTableCopyFormat) {
    return {static_cast<std::uint32_t>(from), static_cast<std::uint32_t>((from >> 32u) & 0xffffu) | (stride << 16u), sourceRecords, format,
            static_cast<std::uint32_t>(to), static_cast<std::uint32_t>((to >> 32u) & 0xffffu) | (stride << 16u), destinationRecords, format};
}

std::uint64_t At(const std::array<std::uint32_t, Words>& words, std::size_t element = 0) {
    return reinterpret_cast<std::uintptr_t>(words.data() + element * 4u);
}

std::vector<std::uint32_t> UserData() {
    const auto table = reinterpret_cast<std::uintptr_t>(Table.data());
    return {static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u)};
}

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const auto data = UserData();
    const bool switchedOff = std::getenv("APS5_NO_WIDE_TABLE_COPY_HLE") != nullptr;
    Require(MatchesWideTableCopyKernel(Code, data, compute) == !switchedOff, switchedOff ? "the kernel matched with APS5_NO_WIDE_TABLE_COPY_HLE set" : "the kernel did not match by default");
    const Switched on(true);
    Require(MatchesWideTableCopyKernel(Code, data, compute), "the wide descriptor-table copy kernel was not recognized");
    for (std::size_t word = 0; word < Code.size(); ++word) {
        auto altered = Code;
        altered[word] ^= 1u;
        Require(!MatchesWideTableCopyKernel(altered, data, compute), "a different shader matched the wide descriptor-table copy kernel (word " + std::to_string(word) + ")");
    }
    Require(!MatchesWideTableCopyKernel(std::span(Code).first(10), data, compute), "a truncated shader matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchesWideTableCopyKernel(Code, extra, compute), "a different user SGPR count matched");
    auto wave32 = compute;
    wave32.numThreads[0] = 32;
    Require(!MatchesWideTableCopyKernel(Code, data, wave32), "a group of 32 threads matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchesWideTableCopyKernel(Code, data, noGroup), "a dispatch without the group id matched");
    auto groupY = compute;
    groupY.groupIdEnable[1] = true;
    Require(!MatchesWideTableCopyKernel(Code, data, groupY), "a dispatch with a group id y matched");
    {
        const Switched off(false);
        Require(!MatchesWideTableCopyKernel(Code, data, compute), "the kernel matched with the replacement switched off");
    }
    const auto from = At(Source);
    const auto to = At(Destination);
    const auto copy = WideTableCopyRange(Descriptors(from, 1024, to, 1024), 4);
    Require(copy && copy->source == from && copy->destination == to && copy->bytes == 4096, "4 groups were not taken as a copy of 256 elements of 16 bytes");
    const auto fewer = WideTableCopyRange(Descriptors(from, 1024, to, 100), 4);
    Require(fewer && fewer->bytes == 1600, "the destination's records did not bound the copy");
    Require(!WideTableCopyRange(Descriptors(from, 100, to, 1024), 4), "a copy reading past the source's records was taken");
    Require(!WideTableCopyRange(Descriptors(from, 1024, to, 1024, 4), 4), "a stride of 4 was taken");
    Require(!WideTableCopyRange(Descriptors(from, 1024, to, 1024, 32), 4), "a stride of 32 was taken");
    auto swizzled = Descriptors(from, 1024, to, 1024);
    swizzled[1] |= 0x80000000u;
    Require(!WideTableCopyRange(swizzled, 4), "a swizzled source was taken");
    Require(!WideTableCopyRange(Descriptors(from, 1024, to, 1024, 16, WideTableCopyFormat | (1u << 23u)), 4), "a descriptor with add_tid was taken");
    Require(!WideTableCopyRange(Descriptors(from, 1024, to, 1024, 16, WideTableCopyFormat ^ 0x1000u), 4), "a descriptor of another format was taken");
    Require(!WideTableCopyRange(Descriptors(0, 1024, to, 1024), 4), "a source with base 0 was taken");
    Require(!WideTableCopyRange(Descriptors(from, 1024, 0, 1024), 4), "a destination with base 0 was taken");
    Require(!WideTableCopyRange(Descriptors(from, 1024, to, 1024), 0), "a copy of 0 groups was taken");
    Require(!WideTableCopyRange(Descriptors(from, 0, to, 0), 4), "a copy of 0 records was taken");
}

std::vector<std::uint32_t> Commands(std::uint32_t launch, bool threads) {
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
    words.insert(words.end(), {0xc0031500u, launch, 1, 1, threads ? 0x61u : 0x41u});
    return words;
}

std::uint64_t Taken() {
    using namespace AgcDriver::DriverDetail;
    return copyPathCounts[CopyCpu].load() + copyPathCounts[CopyGpu].load() + copyPathCounts[CopyAliased].load();
}

struct Case {
    const char* name;
    std::uint32_t launch;
    bool threads;
    std::uint32_t sourceRecords, destinationRecords, stride;
    std::size_t destinationElement;
    bool taken;
    bool exact;
};

std::vector<std::uint32_t> Run(const Case& item, bool hle, double& milliseconds) {
    const Switched set(hle);
    for (std::size_t i = 0; i < Source.size(); ++i) Source[i] = static_cast<std::uint32_t>(i) * 2654435761u + 1u;
    Destination.fill(Untouched);
    const auto to = item.destinationElement == Elements ? At(Source, 1) : At(Destination, item.destinationElement);
    Table = Descriptors(At(Source), item.sourceRecords, to, item.destinationRecords, item.stride);
    const auto taken = Taken();
    const auto words = Commands(item.launch, item.threads);
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    const auto start = std::chrono::steady_clock::now();
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    AgcDriver::GuestMemory::FlushGpuWrites(At(Source), sizeof(Source));
    AgcDriver::GuestMemory::FlushGpuWrites(At(Destination), sizeof(Destination));
    const std::string where = std::string(" (") + item.name + (hle ? ", replacement on)" : ", replacement off)");
    const bool expected = hle && item.taken;
    Require(Taken() - taken == (expected ? 1u : 0u), (expected ? "the copy did not take the replacement" : "the copy took the replacement") + where);
    std::vector<std::uint32_t> result(Destination.begin(), Destination.end());
    if (item.destinationElement == Elements) result.assign(Source.begin(), Source.end());
    return result;
}

void Expect(const Case& item, const std::vector<std::uint32_t>& result) {
    const auto launched = item.threads ? item.launch : item.launch * 64u;
    const auto elements = std::min<std::uint64_t>(launched, item.destinationRecords);
    for (std::size_t i = 0; i < Words; ++i) {
        const auto element = i / 4u;
        std::uint32_t expected = Untouched;
        if (element >= item.destinationElement && element - item.destinationElement < elements) {
            const auto index = element - item.destinationElement;
            expected = index < item.sourceRecords ? static_cast<std::uint32_t>(index * 4u + i % 4u) * 2654435761u + 1u : 0u;
        }
        Require(result[i] == expected, std::string("wide table copy mismatch at dword ") + std::to_string(i) + " (" + item.name + "): got " + std::to_string(result[i]) + ", expected " + std::to_string(expected));
    }
}

void ExecuteTests() {
    constexpr std::array<Case, 7> cases{{
        {"4 groups", 4, false, 1024, 1024, 16, 0, true, true},
        {"destination records below the launch", 4, false, 1024, 100, 16, 0, true, true},
        {"source shorter than the destination", 4, false, 100, 256, 16, 0, false, true},
        {"partial last group", 200, true, 1024, 1024, 16, 0, false, true},
        {"source records 0", 4, false, 0, 256, 16, 0, false, true},
        {"stride 32", 4, false, 1024, 1024, 32, 0, false, false},
        {"overlapping source and destination", 4, false, 1024, 1024, 16, Elements, false, false},
    }};
    for (const auto& item : cases) {
        double shaderMs = 0;
        double copyMs = 0;
        const auto shader = Run(item, false, shaderMs);
        const auto replaced = Run(item, true, copyMs);
        if (item.exact) Expect(item, shader);
        if (item.destinationElement != Elements) {
            for (std::size_t i = 0; i < Words; ++i) Require(shader[i] == replaced[i], std::string("the replacement and the kernel differ at dword ") + std::to_string(i) + " (" + item.name + ")");
        }
    }
    const Case large{"4250 groups over 832 records", 4250, false, 832, 832, 16, 0, true, true};
    double shaderMs = 0;
    double copyMs = 0;
    const auto shader = Run(large, false, shaderMs);
    const auto replaced = Run(large, true, copyMs);
    Expect(large, shader);
    Require(shader == replaced, "the replacement and the kernel differ over 4250 groups");
    std::printf("wide table copy: 4250 groups over 832 records (13 KiB) took %.2f ms through the kernel and %.2f ms through the replacement, submit to idle\n", shaderMs, copyMs);
}

}

int main() {
    try {
        if (std::getenv("APS5_NO_COPY_HLE") != nullptr) {
            std::puts("skipped, APS5_NO_COPY_HLE turns every copy replacement off");
            return VulkanTestSkipped;
        }
        MatchTests();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        ExecuteTests();
        AgcDriverShutdown_nid_postfix();
        std::puts("wide descriptor-table copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
