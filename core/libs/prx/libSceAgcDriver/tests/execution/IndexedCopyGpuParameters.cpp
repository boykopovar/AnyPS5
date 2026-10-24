#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferCopy.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 19> Code{
    0xbfa00001u, 0xd7460000u, 0x04010c0cu, 0xf4201a84u, 0xfa000000u,
    0xbf8cc07fu, 0x7da8006au, 0xbf88000au, 0xf4241a84u, 0xfa000004u,
    0xbf8cc07fu, 0x4a08006bu, 0x4a00006au, 0xe0382000u, 0x80000000u,
    0xbf8c3f70u, 0xe0782000u, 0x80010004u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::size_t Prefix = 16;
constexpr std::size_t Records = 256;
alignas(256) std::array<std::uint32_t, Records * 4 + Prefix * 2> Source{}, Destination{};
alignas(16) std::array<std::uint32_t, 4> Parameters{};
alignas(256) constexpr std::array<std::uint32_t, 7> AddressWriterCode{0x7e000202u, 0x7e020203u, 0x7e040204u, 0x7e060280u, 0xdc7c8000u, 0x00000003u, 0xbf810000u};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        data = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, Bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        data = static_cast<std::uint32_t*>(std::aligned_alloc(Bytes, Bytes));
#endif
        Require(data != nullptr, "cannot allocate parameters");
        std::fill_n(data, Bytes / 4, 0);
        GuestAllocations::Mutation().Add(data, Bytes, true, true, true);
    }
    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint32_t* Data() const { return data; }

private:
    static constexpr std::size_t Bytes = 65536;
    std::uint32_t* data = nullptr;
};

std::vector<std::uint32_t> UserData(std::uint32_t sourceRecords, std::uint32_t destinationRecords, const void* parameters = Parameters.data()) {
    std::vector<std::uint32_t> words;
    const auto descriptor = [&](const void* pointer, std::uint32_t records, std::uint32_t format) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        words.insert(words.end(), {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), records, format});
    };
    descriptor(Source.data() + Prefix, sourceRecords, 0x00005204u);
    descriptor(Destination.data() + Prefix, destinationRecords, 0x00005204u);
    descriptor(parameters, 1, 0x0004dfacu);
    return words;
}

std::uint64_t Taken() {
    using namespace AgcDriver::DriverDetail;
    return copyPathCounts[CopyCpu].load() + copyPathCounts[CopyGpu].load() + copyPathCounts[CopyAliased].load();
}

std::vector<std::uint32_t> Commands(const std::vector<std::uint32_t>& data, std::uint32_t groups, const void* code = Code.data(), std::uint32_t threads = 64, bool groupId = true) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{threads, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(code);
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (static_cast<std::uint32_t>(groupId) << 7u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, groups, 1, 1, 0x41u});
    return words;
}

void GpuParameters() {
    GuestBlock parameters;
    parameters.Data()[0] = 1;
    Destination.fill(Untouched);
    for (std::size_t i = 0; i < Source.size(); ++i) Source[i] = static_cast<std::uint32_t>(i) * 2654435761u + 1u;
    const auto address = reinterpret_cast<std::uintptr_t>(parameters.Data());
    auto words = Commands({static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), 23, 17, 29}, 1, AddressWriterCode.data(), 1, false);
    const auto copy = Commands(UserData(Records, Records, parameters.Data()), 1);
    words.insert(words.end(), copy.begin(), copy.end());
    const auto taken = Taken();
    const auto deferred = AgcDriver::Graphics::LeaseCounters().deferred;
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Destination.data()), sizeof(Destination));
    const auto deferredAfter = AgcDriver::Graphics::LeaseCounters().deferred;
    const auto takenAfter = Taken();
    AgcDriverShutdown_nid_postfix();
    Require(deferredAfter > deferred, "parameter producer was not deferred");
    Require(parameters.Data()[0] == 23 && parameters.Data()[1] == 17 && parameters.Data()[2] == 29, "GPU parameter store differs");
    Require(takenAfter - taken == (std::getenv("APS5_NO_COPY_HLE") == nullptr ? 1u : 0u), "GPU parameter copy did not take the expected path");
    for (std::size_t i = 0; i < Destination.size(); ++i) {
        const bool copied = i >= Prefix + 29 * 4 && i < Prefix + (29 + 23) * 4;
        const auto expected = copied ? Source[i - (29 - 17) * 4] : Untouched;
        Require(Destination[i] == expected, "GPU parameter copy differs at word " + std::to_string(i));
    }
    std::puts("PASS: copy consumes parameters written by a preceding address-based GPU store");
}


}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GpuParameters();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
