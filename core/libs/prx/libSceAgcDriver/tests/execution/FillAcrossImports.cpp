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
constexpr std::size_t FirstBytes = 0x90000;
constexpr std::size_t SecondBytes = 0x50000;
constexpr std::size_t FillOffset = FirstBytes - 0x40000;
constexpr std::size_t FillBytes = 0x60000;
constexpr std::uint8_t Untouched = 0x37;
constexpr std::array<std::uint32_t, 4> Uniform{0x5a5a5a5au, 0x5a5a5a5au, 0x5a5a5a5au, 0x5a5a5a5au};
constexpr std::array<std::uint32_t, 4> Pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};

std::string Hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

class TwoRanges {
public:
    TwoRanges() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes));
        Require(block != nullptr, "fill across imports: cannot allocate a block");
        GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* raw = mmap(nullptr, 2 * BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(raw != MAP_FAILED, "fill across imports: cannot map a block");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (begin + BlockBytes - 1) & ~static_cast<std::uintptr_t>(BlockBytes - 1);
        if (aligned != begin) munmap(raw, aligned - begin);
        if (aligned + BlockBytes != begin + 2 * BlockBytes) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + BlockBytes - aligned);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
#endif
        std::memset(block, Untouched, BlockBytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(block, FirstBytes, true, true, true);
        mutation.Add(block + FirstBytes, SecondBytes, true, true, true);
    }

    ~TwoRanges() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block + FirstBytes);
            mutation.Remove(block);
        }
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    TwoRanges(const TwoRanges&) = delete;
    TwoRanges& operator=(const TwoRanges&) = delete;

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

void RequireBytes(const TwoRanges& guest, const std::array<std::uint32_t, 4>* pattern, const std::string& what) {
    for (std::size_t at = 0; at < BlockBytes; at += sizeof(std::uint32_t)) {
        std::uint32_t word = 0;
        std::memcpy(&word, guest.Data() + at, sizeof(word));
        std::uint32_t expected = 0;
        if (pattern != nullptr && at >= FillOffset && at < FillOffset + FillBytes) expected = (*pattern)[((at - FillOffset) / sizeof(word)) % pattern->size()];
        else std::memset(&expected, Untouched, sizeof(expected));
        Require(word == expected, "fill across imports: " + what + " left " + Hex(word) + " at +" + Hex(at) + ", expected " + Hex(expected));
    }
}

bool Fill(AgcDriver::VulkanDevice& device, const TwoRanges& guest, const std::array<std::uint32_t, 4>& pattern, const std::string& what) {
    std::lock_guard lock(GuestMemory::GpuMutex());
    const auto* recorder = Graphics::Recorder::Active();
    Require(recorder != nullptr && recorder->Idle(), "fill across imports: the recorder was busy before " + what);
    const bool stored = device.FillBuffer(guest.Address() + FillOffset, FillBytes, pattern);
    Require(stored != recorder->Idle(), "fill across imports: " + what + (stored ? " reported a GPU store but recorded nothing" : " recorded work although it was refused"));
    device.WaitIdle();
    return stored;
}

bool FillsAcrossImports(AgcDriver::VulkanDevice& device) {
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    TwoRanges guest;
    const auto first = guest.Address();
    const auto second = first + FirstBytes;
    const auto fill = first + FillOffset;
    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        if (Graphics::HostImportFor(context, first, FirstBytes) == nullptr) return false;
        Require(Graphics::ExistingHostImportFor(context, second, 16) == nullptr, "fill across imports: importing the first range imported the second");
        Require(Graphics::HostImportFor(context, fill, FillBytes) == nullptr, "fill across imports: one import covers both registered ranges");
    }
    Require(!Fill(device, guest, Uniform, "a fill whose second part has no import"), "fill across imports: a fill whose second part has no import was stored on the GPU");
    RequireBytes(guest, nullptr, "the refused fill");

    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        const auto* secondImport = Graphics::HostImportFor(context, second, SecondBytes);
        const auto* firstImport = Graphics::ExistingHostImportFor(context, first, 16);
        Require(firstImport != nullptr && secondImport != nullptr && firstImport != secondImport, "fill across imports: the two registered ranges are not imported on their own");
        Require(Graphics::HostImportFor(context, fill, FillBytes) == nullptr, "fill across imports: one import covers both registered ranges");
    }
    std::memset(guest.Data(), Untouched, BlockBytes);
    Require(Fill(device, guest, Uniform, "a uniform fill across two imports"), "fill across imports: a uniform fill across two imports was not stored on the GPU");
    RequireBytes(guest, &Uniform, "the uniform fill across two imports");
    std::memset(guest.Data(), Untouched, BlockBytes);
    Require(Fill(device, guest, Pattern, "a pattern fill across two imports"), "fill across imports: a pattern fill across two imports was not stored on the GPU");
    RequireBytes(guest, &Pattern, "the pattern fill across two imports");

    {
        std::lock_guard lock(GuestMemory::GpuMutex());
        Graphics::ClearHostImports(device.Device());
    }
    return true;
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
        if (!FillsAcrossImports(*device)) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        std::puts("fill across imports tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
