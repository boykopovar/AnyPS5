#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

namespace Graphics = AgcDriver::Graphics;
namespace GuestMemory = AgcDriver::GuestMemory;
using Graphics::Require;

constexpr std::size_t BlockBytes = std::size_t{1} << 20u;
constexpr std::size_t FillOffset = 64 * 1024;
constexpr std::size_t FillBytes = 256 * 1024;
constexpr std::array<std::uint32_t, 4> Pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
constexpr std::array<std::uint32_t, 4> Zero{};

std::string Hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

class WatchedBlock {
public:
    WatchedBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes));
        Require(block != nullptr, "repeated fill elision: cannot allocate a watched block");
        GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* raw = mmap(nullptr, 2 * BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "repeated fill elision: cannot map a watched block");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (begin + BlockBytes - 1) & ~static_cast<std::uintptr_t>(BlockBytes - 1);
        if (aligned != begin) munmap(raw, aligned - begin);
        if (aligned + BlockBytes != begin + 2 * BlockBytes) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + BlockBytes - aligned);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
#endif
        std::memset(block, 0x37, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~WatchedBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    WatchedBlock(const WatchedBlock&) = delete;
    WatchedBlock& operator=(const WatchedBlock&) = delete;

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

enum class Outcome : std::uint8_t { NoImport, Passed };
enum class Store : std::uint8_t { Recorded, Elided };

Store Fill(AgcDriver::VulkanDevice& device, std::uint64_t address, std::size_t bytes, const std::array<std::uint32_t, 4>& pattern, const std::string& what) {
    std::lock_guard lock(GuestMemory::GpuMutex());
    const auto* recorder = Graphics::Recorder::Active();
    Require(recorder != nullptr && recorder->Idle(), "repeated fill elision: the recorder was busy before " + what);
    const auto before = GuestMemory::TrackerGeneration();
    Require(device.FillBuffer(address, bytes, pattern), "repeated fill elision: " + what + " was not stored on the GPU");
    Require(!GuestMemory::UnchangedSince(address, bytes, before), "repeated fill elision: " + what + " did not stamp its range");
    const auto store = recorder->Idle() ? Store::Elided : Store::Recorded;
    device.WaitIdle();
    return store;
}

std::string Unexpected(Store expected) {
    return expected == Store::Elided ? " was recorded although the write watch proves the range unchanged" : " was elided although nothing proves the range unchanged";
}

void RequireFilled(const WatchedBlock& guest, std::size_t offset, std::size_t bytes, const std::array<std::uint32_t, 4>& pattern, const std::string& what) {
    for (std::size_t at = 0; at < bytes; at += sizeof(std::uint32_t)) {
        std::uint32_t word = 0;
        std::memcpy(&word, guest.Data() + offset + at, sizeof(word));
        Require(word == pattern[(at / sizeof(word)) % pattern.size()], "repeated fill elision: " + what + " left " + Hex(word) + " at +" + Hex(offset + at));
    }
}

Outcome RepeatsAreElidedUntilTheRangeChanges(AgcDriver::VulkanDevice& device, const char* mode) {
    WatchedBlock guest;
    const auto address = guest.Address() + FillOffset;
    std::uint64_t filled = 0;
    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        if (!device.FillBuffer(address, FillBytes, Pattern)) return Outcome::NoImport;
        filled = GuestMemory::TrackerGeneration();
        device.WaitIdle();
    }
    RequireFilled(guest, FillOffset, FillBytes, Pattern, "the first fill");
    Require(guest.Data()[FillOffset - 1] == 0x37 && guest.Data()[FillOffset + FillBytes] == 0x37, "repeated fill elision: the first fill stored outside its range");
    GuestMemory::CollectWritesUncached(address, FillBytes);
    const bool proven = GuestMemory::Watched(address, FillBytes) && GuestMemory::UnchangedSince(address, FillBytes, filled);
    const auto repeat = proven ? Store::Elided : Store::Recorded;
    std::printf("repeated fill elision, %s: %s\n", mode, proven ? "the write watch proves the range unchanged, so repeats are elided" : "nothing proves the range unchanged, so every fill is recorded");

    Require(Fill(device, address, FillBytes, Pattern, "a repeat over unchanged bytes") == repeat, std::string("repeated fill elision: a repeat over unchanged bytes") + Unexpected(repeat));
    Require(Fill(device, address, FillBytes, Pattern, "a second repeat") == repeat, std::string("repeated fill elision: a second repeat") + Unexpected(repeat));
    RequireFilled(guest, FillOffset, FillBytes, Pattern, "the repeats");

    guest.Data()[FillOffset + 4096] = 0x5a;
    Require(Fill(device, address, FillBytes, Pattern, "the fill after a CPU write") == Store::Recorded, "repeated fill elision: the fill after a CPU write was elided");
    RequireFilled(guest, FillOffset, FillBytes, Pattern, "the fill after a CPU write");

    Require(Fill(device, address, FillBytes, Zero, "a fill with another pattern") == Store::Recorded, "repeated fill elision: a fill with another pattern was elided");
    RequireFilled(guest, FillOffset, FillBytes, Zero, "the fill with another pattern");
    Require(Fill(device, address, FillBytes, Zero, "a repeat of the other pattern") == repeat, std::string("repeated fill elision: a repeat of the other pattern") + Unexpected(repeat));

    Require(Fill(device, address + 4096, 4096, Pattern, "a GPU store inside the range") == Store::Recorded, "repeated fill elision: a fill of another range was elided");
    Require(Fill(device, address, FillBytes, Zero, "the fill after a GPU store") == Store::Recorded, "repeated fill elision: the fill after a GPU store was elided");
    RequireFilled(guest, FillOffset, FillBytes, Zero, "the fill after a GPU store");

    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        GuestMemory::MarkWritten(address + FillBytes / 2, 16);
    }
    Require(Fill(device, address, FillBytes, Zero, "the fill after a driver stamp") == Store::Recorded, "repeated fill elision: the fill after a write-back's stamp was elided");

    Require(Fill(device, address, FillBytes / 2, Zero, "a shorter fill at the same address") == Store::Recorded, "repeated fill elision: a shorter fill at the same address was elided");
    Require(Fill(device, address, FillBytes, Zero, "the fill after a shorter one") == Store::Recorded, "repeated fill elision: the fill after a shorter one was elided");

    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        Graphics::ClearHostImports(device.Device());
    }
    Require(Fill(device, address, FillBytes, Zero, "the fill after its import was dropped") == Store::Recorded, "repeated fill elision: the fill through a new import was elided");
    RequireFilled(guest, FillOffset, FillBytes, Zero, "the fill through a new import");
    Require(guest.Data()[FillOffset - 1] == 0x37 && guest.Data()[FillOffset + FillBytes] == 0x37, "repeated fill elision: a fill stored outside its range");
    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        Graphics::ClearHostImports(device.Device());
    }
    return Outcome::Passed;
}

}

int main() {
    try {
        if (!GuestMemory::WriteWatched()) {
            std::printf("skipped, guest memory is not write watched\n");
            return VulkanTestSkipped;
        }
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
#ifndef _WIN32
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        const auto decided = Graphics::PrepareImportWatch(context);
        struct Restore {
            const Graphics::Context& context;
            Graphics::ImportWatch decided;
            ~Restore() { Graphics::SetImportWatch(context, decided); }
        } restore{context, decided};
        std::vector<Graphics::ImportWatch> modes{decided};
        if (decided != Graphics::ImportWatch::Unwatch) modes.push_back(Graphics::ImportWatch::Unwatch);
        for (const auto mode : modes) {
            Graphics::SetImportWatch(context, mode);
            if (RepeatsAreElidedUntilTheRangeChanges(*device, mode == Graphics::ImportWatch::Watch ? "imports under the write watch" : "imports left unwatched") == Outcome::NoImport) {
                std::printf("skipped, the device does not import guest memory\n");
                return VulkanTestSkipped;
            }
        }
#else
        if (RepeatsAreElidedUntilTheRangeChanges(*device, "imports as decided") == Outcome::NoImport) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
#endif
        std::puts("repeated fill elision tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
