#include "VulkanTestDevice.hpp"
#include "prx/libSceAgc/include/Agc.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::size_t BlockBytes = 65536;

struct GuestBlock {
    explicit GuestBlock(std::size_t bytes) : bytes(bytes), watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map a guest block");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "cannot allocate a guest block");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~GuestBlock() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        } else {
            std::free(data);
        }
#endif
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* data = nullptr;
    std::size_t bytes = 0;
    bool watched = false;
};

const char* FillPath() {
    return std::getenv("APS5_NO_FILL_HLE") != nullptr ? "translated" : "hle";
}

void CheckFilled(const char* label, GuestBlock& destination, std::size_t bytes, const std::array<std::uint32_t, 4>& pattern) {
    std::vector<std::uint32_t> words(bytes / sizeof(std::uint32_t));
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(destination.data), std::as_writable_bytes(std::span(words)), 4);
    for (std::size_t index = 0; index < words.size(); ++index) {
        Require(words[index] == pattern[index % pattern.size()],
            std::string(label) + " left dword " + std::to_string(index) + " as " + std::to_string(words[index]) + " instead of the pattern");
    }
}

void RunFill(GuestBlock& destination, std::uint32_t bytes, const std::array<std::uint32_t, 4>& pattern, const char* label) {
    std::memset(destination.data, 0xcd, bytes);
    alignas(256) std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    sceAgcCbMemsetExclusive(&buffer, destination.data, pattern.data(), bytes);
    const auto dwords = static_cast<std::uint32_t>(buffer.cursor_up - words.data());
    std::cerr << "[" << label << "] packet built, dwords=" << dwords << std::endl;
    Require(dwords == 29u, std::string(label) + ": expected a 29-dword packet, got " + std::to_string(dwords));
    Packet packet{words.data(), dwords, 0, {}};
    std::cerr << "[" << label << "] submitting" << std::endl;
    Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, std::string(label) + ": submit failed");
    std::cerr << "[" << label << "] waiting" << std::endl;
    AgcDriverWaitIdle_nid_postfix();
    std::cerr << "[" << label << "] reading back" << std::endl;
    CheckFilled(label, destination, bytes, pattern);
    std::cerr << "[" << label << "] verified" << std::endl;
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        const std::array<std::uint32_t, 4> pattern{0x11223344u, 0x55667788u, 0x99aabccdu, 0xddeeff00u};
        GuestBlock destination(BlockBytes);
        RunFill(destination, 256, pattern, "256-byte fill");
        RunFill(destination, 4096, pattern, "4096-byte fill");
        std::cout << "memset exclusive fill test passed (" << FillPath() << " path)\n";
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}

