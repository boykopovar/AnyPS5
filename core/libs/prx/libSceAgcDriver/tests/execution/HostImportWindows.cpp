#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

namespace Graphics = AgcDriver::Graphics;
using Graphics::Require;

constexpr std::uint64_t MiB = 1ull << 20u;
constexpr std::uint64_t WindowBytes = 64 * MiB;
constexpr std::uint64_t BlockBytes = 2 * WindowBytes;
constexpr std::uint64_t OtherBytes = 64 * 1024;
constexpr std::uint64_t GuestAlignment = 64 * 1024;
constexpr std::uint64_t ProbeBytes = 4096;

std::string Hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

class GuestBlock {
public:
    explicit GuestBlock(std::uint64_t size) : bytes(size) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(GuestAlignment, bytes));
#endif
        Require(block != nullptr, "host import windows: cannot allocate a guest block of " + Hex(bytes));
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
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
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
    std::uint64_t bytes = 0;
};

enum class Outcome : std::uint8_t { NoDevice, NoImport, Passed };

Graphics::Context Probe(const AgcDriver::VulkanDevice& device) {
    Graphics::Context probe{};
    probe.device = device.Device();
    probe.hostImportAlignment = 1;
    return probe;
}

bool Fill(AgcDriver::VulkanDevice& device, std::uint64_t address) {
    const std::array<std::uint32_t, 4> pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const bool filled = device.FillBuffer(address, ProbeBytes, pattern);
    device.WaitIdle();
    return filled;
}

Outcome ClearingDropsImports() {
    GuestBlock guest(BlockBytes);
    auto device = OpenVulkanTestDevice();
    if (!device) return Outcome::NoDevice;
    if (!Fill(*device, guest.Address() + WindowBytes)) return Outcome::NoImport;
    const auto probe = Probe(*device);
    Require(Graphics::HostImportCovers(probe, guest.Address() + WindowBytes, ProbeBytes), "host import windows: the second window of " + Hex(guest.Address()) + " is not imported");
    Graphics::ClearHostImports(device->Device());
    Require(!Graphics::HostImportCovers(probe, guest.Address() + WindowBytes, ProbeBytes), "host import windows: ClearHostImports kept the window of " + Hex(guest.Address()));
    return Outcome::Passed;
}

Outcome WindowsFollowTheirRange() {
    GuestBlock guest(BlockBytes);
    auto device = OpenVulkanTestDevice();
    if (!device) return Outcome::NoDevice;
    const auto probe = Probe(*device);
    const auto address = guest.Address() + WindowBytes + WindowBytes / 2;
    if (!Fill(*device, address)) return Outcome::NoImport;
    Require(Graphics::HostImportCovers(probe, guest.Address() + WindowBytes, ProbeBytes), "host import windows: the second window of " + Hex(guest.Address()) + " is not imported");
    Require(!Graphics::HostImportCovers(probe, guest.Address(), ProbeBytes), "host import windows: the first window of " + Hex(guest.Address()) + " is imported along with the second");
    const auto first = Graphics::HostImportSerial(probe, address, ProbeBytes, false);
    Require(first != 0, "host import windows: the window at " + Hex(address) + " has no serial");

    GuestBlock other(OtherBytes);
    Require(Fill(*device, address), "host import windows: the window at " + Hex(address) + " cannot be filled after an unrelated registration");
    Require(Graphics::HostImportSerial(probe, address, ProbeBytes, false) == first, "host import windows: the window at " + Hex(address) + " was dropped by an unrelated registration, although its range still owns it");

    GuestAllocations::Mutation().Remove(guest.Data());
    GuestAllocations::Mutation().Add(guest.Data(), BlockBytes, true, true, true);
    Require(Fill(*device, address), "host import windows: the window at " + Hex(address) + " cannot be filled after its range was mapped again");
    const auto second = Graphics::HostImportSerial(probe, address, ProbeBytes, false);
    Require(second != 0 && second != first, "host import windows: the window of the range mapped again kept the serial " + Hex(first));
    return Outcome::Passed;
}

Outcome ResidentMappingsImportAfterCollection() {
#ifdef _WIN32
    constexpr std::size_t bytes = MiB;
    auto* data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 0x200000));
    Require(data != nullptr, "cannot allocate resident import address");
    const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_RESERVE, 0, bytes, nullptr);
    Require(section != nullptr, "cannot reserve resident import backing");
    GuestArena::GuestArenaMapDirect_nid_postfix(data, bytes, section, 0, PAGE_READWRITE);
    std::memset(data, 0x37, bytes);
    GuestAllocations::Mutation().Add(data, bytes, true, true, true);
    const auto address = reinterpret_cast<std::uint64_t>(data);
    Require(AgcDriver::GuestMemory::CollectWritesUncached(address, bytes) != 0, "resident import memory is not watched");
    {
        auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "resident import test lost its Vulkan device");
        const auto context = Probe(*device);
        for (unsigned int pass = 0; pass < 2; ++pass) {
            Require(AgcDriver::GuestMemory::CollectWritesUncached(address, bytes) != 0, "resident import collection failed");
            Require(Fill(*device, address), "resident memory could not be filled after collecting its writes");
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Require(Graphics::HostImportCovers(context, address, ProbeBytes), "resident import did not cover its data");
            Graphics::ClearHostImports(device->Device());
        }
        Require(Fill(*device, address), "resident memory could not be filled after collecting its writes");
        std::uint32_t first = 0;
        std::memcpy(&first, data, sizeof(first));
        Require(first == 0x11111111u && data[ProbeBytes] == 0x37 && data[bytes - 1] == 0x37, "resident import lost the GPU fill or neighboring bytes");
        Graphics::ClearHostImports(device->Device());
    }
    GuestAllocations::Mutation().Remove(data);
    GuestArena::GuestArenaReset_nid_postfix(data, bytes);
    GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
    GuestArena::GuestArenaForgetSection_nid_postfix(section);
    CloseHandle(section);
#endif
    return Outcome::Passed;
}

Outcome MixedBlocksImportInSmallerRuns() {
#ifdef _WIN32
    constexpr std::size_t piece = 64 * 1024;
    constexpr std::size_t pieces = 32;
    auto* data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(pieces * piece, 8 * MiB));
    Require(data != nullptr, "cannot allocate the mixed import block");
    std::array<HANDLE, 6> sections{};
    for (auto& section : sections) {
        section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, piece, nullptr);
        Require(section != nullptr, "cannot reserve the mixed import backing");
    }
    const auto at = [&](std::size_t index) { return data + index * piece; };
    const auto map = [&](std::size_t index, std::size_t section) { GuestArena::GuestArenaMapDirect_nid_postfix(at(index), piece, sections[section], 0, PAGE_READWRITE); };
    map(0, 0);
    map(1, 1);
    map(3, 1);
    map(8, 2);
    map(9, 3);
    map(11, 2);
    map(12, 3);
    map(16, 4);
    map(17, 5);
    map(19, 5);
    const auto type = [](const std::uint8_t* address) {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery(address, &info, sizeof(info)) == sizeof(info), "cannot query the mixed import block");
        return info.Type;
    };
    Require(type(at(0)) == MEM_PRIVATE && type(at(1)) == MEM_MAPPED && type(at(8)) == MEM_MAPPED && type(at(9)) == MEM_MAPPED && type(at(16)) == MEM_PRIVATE && type(at(17)) == MEM_MAPPED, "the mixed import block does not hold the resident memory and shared views it maps");
    const std::array<std::pair<std::size_t, std::size_t>, 5> ranges{{{0, 1}, {1, 1}, {8, 1}, {9, 1}, {16, 2}}};
    for (const auto& [index, count] : ranges) GuestAllocations::Mutation().Add(at(index), count * piece, true, true, true);
    const auto address = [&](std::size_t index) { return reinterpret_cast<std::uint64_t>(at(index)); };
    {
        auto device = OpenVulkanTestDevice();
        Require(device != nullptr, "mixed import test lost its Vulkan device");
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto imported = [&](std::size_t index) { return Graphics::HostImportFor(context, address(index), ProbeBytes, Graphics::ImportRequester::Build); };
            const auto* resident = imported(0);
            Require(resident != nullptr && resident->base == address(0) && resident->bytes == piece, "the resident range of a block that mixes it with a shared view was not imported on its own");
            const auto* shared = imported(1);
            Require(shared != nullptr && shared->base == address(1) && shared->bytes == piece, "the shared range of a block that mixes it with resident memory was not imported on its own");
            const auto* joined = imported(8);
            Require(joined != nullptr && joined->base == address(8) && joined->bytes == 2 * piece, "neighbouring shared ranges were not imported as one block");
            const auto* single = imported(16);
            Require(single != nullptr && single->base == address(16) && single->bytes == 2 * piece, "a range that mixes resident memory with a shared view was not imported whole");
        }
        Require(type(at(0)) == MEM_PRIVATE, "importing a block that mixes resident memory with a shared view moved the resident memory into a shared view");
        Graphics::ClearHostImports(device->Device());
    }
    for (const auto& [index, count] : ranges) GuestAllocations::Mutation().Remove(at(index));
    GuestArena::GuestArenaReset_nid_postfix(data, pieces * piece);
    GuestArena::GuestArenaRelease_nid_postfix(data, pieces * piece);
    for (const auto section : sections) {
        GuestArena::GuestArenaForgetSection_nid_postfix(section);
        CloseHandle(section);
    }
#endif
    return Outcome::Passed;
}

Outcome SmallMappingsImportOnFirstUse() {
    GuestBlock small(GuestAlignment);
    GuestBlock large(2 * GuestAlignment);
    auto device = OpenVulkanTestDevice();
    if (!device) return Outcome::NoDevice;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
    if (context.hostImportAlignment == 0) return Outcome::NoImport;
    for (auto* guest : {&small, &large}) {
        Graphics::ClearHostImports(device->Device());
        GuestAllocations::Mapped ranges;
        for (const auto& range : GuestAllocations::GuestAllocationsAcquire_nid_postfix()) {
            if (range->address == guest->Address()) ranges.push_back(range);
        }
        Require(ranges.size() == 1, "mapped import test lost its registered range");
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Require(Graphics::ImportMappedRanges(context, ranges, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), true), "mapped import test could not adopt the device");
        }
        Require(Graphics::HostImportCovers(context, guest->Address(), ProbeBytes) == (guest == &large), "mapping did not defer exactly the small import");
        Require(Fill(*device, guest->Address()), "first GPU use could not import the mapped buffer");
        Require(Graphics::HostImportCovers(context, guest->Address(), ProbeBytes), "first GPU use left the mapped buffer unimported");
        constexpr std::array<std::uint32_t, 4> pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
        for (std::size_t offset = 0; offset < ProbeBytes; offset += sizeof(std::uint32_t)) {
            std::uint32_t actual = 0;
            std::memcpy(&actual, guest->Data() + offset, sizeof(actual));
            Require(actual == pattern[(offset / sizeof(actual)) % pattern.size()], "first GPU use changed the fill result");
        }
        for (std::size_t offset = ProbeBytes; offset < GuestAlignment; ++offset) {
            Require(guest->Data()[offset] == 0, "first GPU use overwrote bytes outside the fill");
        }
    }
    Graphics::ClearHostImports(device->Device());
    return Outcome::Passed;
}

}

int main() {
    try {
        const auto counting = ClearingDropsImports();
        if (counting == Outcome::NoDevice) return VulkanTestSkipped;
        if (counting == Outcome::NoImport) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        const auto windows = WindowsFollowTheirRange();
        Require(windows == Outcome::Passed, "host import windows: the device stopped importing guest memory between two scenarios");
        Require(ResidentMappingsImportAfterCollection() == Outcome::Passed, "resident import test failed");
        Require(MixedBlocksImportInSmallerRuns() == Outcome::Passed, "mixed import test failed");
        Require(SmallMappingsImportOnFirstUse() == Outcome::Passed, "mapped import test lost its Vulkan device or host import support");
        std::puts("host import windows tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
