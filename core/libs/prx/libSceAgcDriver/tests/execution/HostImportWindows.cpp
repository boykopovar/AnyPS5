#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Recompiler.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

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
    explicit GuestBlock(std::uint64_t size, std::uint64_t committedBytes = 0) : bytes(size), committed(committedBytes == 0 ? size : committedBytes) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS));
        Require(block != nullptr && VirtualAlloc(block, committed, MEM_COMMIT, PAGE_READWRITE) != nullptr, "host import windows: cannot commit guest memory");
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(GuestAlignment, bytes));
#endif
        Require(block != nullptr, "host import windows: cannot allocate a guest block of " + Hex(bytes));
#ifndef _WIN32
        if (committed < bytes) Require(mprotect(block + committed, bytes - committed, PROT_NONE) == 0, "host import windows: cannot protect uncommitted guest memory");
#endif
        std::memset(block, 0, committed);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        if (committed < bytes) mprotect(block + committed, bytes - committed, PROT_READ | PROT_WRITE);
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
    std::uint64_t committed = 0;
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

Outcome PartlyCommittedWindow() {
    GuestBlock guest(BlockBytes, GuestAlignment);
    auto device = OpenVulkanTestDevice();
    if (!device) return Outcome::NoDevice;
    Require(!AgcDriver::GuestMemory::DescribeCommitted(guest.Address(), WindowBytes).whole, "host import windows: the partial window has no inaccessible tail");
    Require(Fill(*device, guest.Address()), "host import windows: cannot fill the committed prefix");
    const auto probe = Probe(*device);
    Require(Graphics::HostImportCovers(probe, guest.Address(), ProbeBytes), "host import windows: a committed prefix fell back to copies because of an inaccessible tail");
    std::array<std::uint32_t, 4> actual{};
    std::memcpy(actual.data(), guest.Data(), sizeof(actual));
    Require(actual == std::array<std::uint32_t, 4>{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u}, "host import windows: the imported committed prefix lost its GPU stores");
    Graphics::ClearHostImports(device->Device());
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

void AliasFailurePropagates(AgcDriver::VulkanDevice& device) {
#ifdef _WIN32
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    struct Cleanup {
        VkDevice device;
        HANDLE section = nullptr;
        void* data = nullptr;
        bool registered = false;
        ~Cleanup() {
            Graphics::ClearHostImports(device);
            if (registered) GuestAllocations::Mutation().Remove(data);
            if (data != nullptr) UnmapViewOfFile(data);
            if (section != nullptr) CloseHandle(section);
        }
    } cleanup{device.Device()};
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Graphics::ClearHostImports(device.Device());
    cleanup.section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, GuestAlignment, nullptr);
    Require(cleanup.section != nullptr, "cannot create the alias failure backing");
    cleanup.data = MapViewOfFile(cleanup.section, FILE_MAP_WRITE, 0, 0, GuestAlignment);
    Require(cleanup.data != nullptr, "cannot map the alias failure backing");
    const auto address = reinterpret_cast<std::uintptr_t>(cleanup.data);
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    Require(!GuestArena::GuestArenaResidentSpan_nid_postfix(address, GuestAlignment, &begin, &end), "alias failure fixture overlaps a resident run");
    bool aliasThrew = false;
    try {
        auto* alias = GuestArena::GuestArenaMapAlias_nid_postfix(address, GuestAlignment);
        GuestArena::GuestArenaUnmapAlias_nid_postfix(alias);
    } catch (const std::runtime_error& error) {
        std::printf("non-bounds alias failure: %s\n", error.what());
        aliasThrew = true;
    }
    Require(aliasThrew, "alias failure fixture did not throw");
    GuestAllocations::Mutation().Add(cleanup.data, GuestAlignment, true, true, true);
    cleanup.registered = true;
    for (unsigned pass = 0; pass < 2; ++pass) {
        bool threw = false;
        try {
            Graphics::HostImportFor(context, address, ProbeBytes);
        } catch (const std::runtime_error& error) {
            std::printf("single-range alias failure propagated: %s\n", error.what());
            threw = true;
        }
        Require(threw, "single-range non-bounds alias failure was swallowed as a fallback");
        Require(Graphics::ExistingHostImportFor(context, address, ProbeBytes) == nullptr && Graphics::HostImportLiveBytes() == 0, "alias failure left a live import");
    }
#endif
}

void PartialResidentAliasDispatch(AgcDriver::VulkanDevice& device, bool left) {
#ifdef _WIN32
    constexpr std::size_t page = GuestAlignment;
    constexpr std::size_t bytes = 4 * page;
    auto* data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 0x200000));
    Require(data != nullptr, "cannot reserve the partial resident dispatch fixture");
    struct Cleanup {
        std::uint8_t* data;
        VkDevice device;
        HANDLE section = nullptr;
        void* registered = nullptr;
        ~Cleanup() {
            Graphics::ClearHostImports(device);
            if (registered != nullptr) GuestAllocations::Mutation().Remove(registered);
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
            if (section != nullptr) {
                GuestArena::GuestArenaForgetSection_nid_postfix(section);
                CloseHandle(section);
            }
        }
    } cleanup{data, device.Device()};
    cleanup.section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, bytes, nullptr);
    Require(cleanup.section != nullptr, "cannot reserve the partial resident dispatch backing");
    const auto shared = left ? 3 * page : 0;
    const auto resident = left ? 0 : page;
    GuestArena::GuestArenaMap_nid_postfix(data + shared, page, cleanup.section, shared, PAGE_READWRITE);
    GuestArena::GuestArenaMapDirect_nid_postfix(data + resident, 3 * page, cleanup.section, resident, PAGE_READWRITE);
    const auto requested = left ? page : 0;
    const auto input = shared + 128;
    const auto output = left ? shared - 64 : page - 64;
    std::memset(data, 0x37, bytes);
    std::vector<std::uint8_t> expected(data, data + bytes);
    for (std::uint32_t lane = 0; lane < 32; ++lane) {
        const std::uint32_t value = 0x12340000u + lane;
        std::memcpy(data + input + lane * 4, &value, 4);
        std::memcpy(expected.data() + input + lane * 4, &value, 4);
        std::memcpy(expected.data() + output + lane * 4, &value, 4);
    }
    GuestAllocations::Mutation().Add(data + requested, 3 * page, true, true, true);
    cleanup.registered = data + requested;
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    Require(context.hostImportAlignment != 0, "partial resident dispatch requires host import support");
    alignas(256) constexpr std::array<std::uint32_t, 7> code{0x34020082, 0xe0301000, 0x80000201, 0xbf8c3f70, 0xe0701000, 0x80010201, 0xbf810000};
    std::array<std::uint32_t, 8> userData{};
    for (const auto [index, offset] : {std::pair{0, input}, std::pair{4, output}}) {
        const auto address = reinterpret_cast<std::uintptr_t>(data + offset);
        userData[index] = static_cast<std::uint32_t>(address);
        userData[index + 1] = static_cast<std::uint32_t>(address >> 32u);
        userData[index + 2] = 128;
        userData[index + 3] = 0x31016facu;
    }
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    for (unsigned pass = 0; pass < 2; ++pass) {
        if (pass != 0) {
            const GuestArena::HostWrite write(data + input, 128);
            for (std::uint32_t lane = 0; lane < 32; ++lane) {
                const std::uint32_t value = 0x56780000u + lane;
                std::memcpy(data + input + lane * 4, &value, 4);
                std::memcpy(expected.data() + input + lane * 4, &value, 4);
                std::memcpy(expected.data() + output + lane * 4, &value, 4);
            }
        }
        device.Dispatch(result, 1, 1, 1);
        device.WaitIdle();
        Require(std::memcmp(data, expected.data(), bytes) == 0, "partial resident dispatch lost copied writes or changed neighboring bytes");
        Require(!Graphics::HostImportCovers(context, reinterpret_cast<std::uintptr_t>(data + output), 128), "partial resident dispatch unexpectedly imported the mixed range");
    }
#endif
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--alias-failure") {
            auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).hostImportAlignment == 0) {
                std::puts("skipped, the device does not import guest memory");
                return VulkanTestSkipped;
            }
            AliasFailurePropagates(*device);
            std::puts("single-range alias failure propagation tests passed");
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--partial-resident-alias") {
            auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            if (AgcDriver::VulkanDeviceTestAccess::ContextOf(*device).hostImportAlignment == 0) {
                std::puts("skipped, the device does not import guest memory");
                return VulkanTestSkipped;
            }
            for (const bool left : {false, true}) PartialResidentAliasDispatch(*device, left);
            std::puts("partial resident alias dispatch tests passed");
            return 0;
        }
        const auto counting = ClearingDropsImports();
        if (counting == Outcome::NoDevice) return VulkanTestSkipped;
        if (counting == Outcome::NoImport) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        const auto windows = WindowsFollowTheirRange();
        Require(windows == Outcome::Passed, "host import windows: the device stopped importing guest memory between two scenarios");
        Require(PartlyCommittedWindow() == Outcome::Passed, "host import windows: the partial window lost its device");
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
