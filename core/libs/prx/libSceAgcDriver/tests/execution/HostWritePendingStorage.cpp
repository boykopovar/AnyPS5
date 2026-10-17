#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 120;
constexpr std::uint32_t FormatRgba16Float = 71;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Standard64KB = 0x09;
constexpr std::size_t SurfaceBytes = 0x20000;
constexpr std::size_t BlockBytes = 0x10000;

alignas(256) constexpr std::array<std::uint32_t, 11> WriteCode{0xd7460004u, 0x0401060eu, 0xd7460005u, 0x0405060fu, 0x7e100d04u, 0x7e120d05u, 0x7e140c0du, 0x7e1602f2u, 0xf0200f08u, 0x00000804u, 0xbf810000u};

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels) {
    const auto address = reinterpret_cast<std::uintptr_t>(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (FormatRgba16Float << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (Standard64KB << 20u) | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void Dispatch(std::uint8_t* texels) {
    std::vector<std::uint32_t> data;
    const auto texture = TextureDescriptor(texels);
    data.insert(data.end(), texture.begin(), texture.end());
    data.resize(14, 0u);
    data[13] = 7u;
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{8, 8, 1};
    registers(0x207, shape);
    const auto code = reinterpret_cast<std::uintptr_t>(WriteCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (7u << 7u) | (1u << 11u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, Width / 8u, Height / 8u, 1u, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
}

class GuestSurface {
public:
    GuestSurface() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, Alignment));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + Alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + Alignment - 1) & ~(static_cast<std::uintptr_t>(Alignment) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + SurfaceBytes != begin + SurfaceBytes + Alignment) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
        }
#endif
        Require(block != nullptr, "host write over pending storage: cannot allocate the guest surface");
        std::memset(block, 0, SurfaceBytes);
        GuestAllocations::Mutation mutation;
        mutation.Add(block, SurfaceBytes, true, true, true);
    }

    ~GuestSurface() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block);
        }
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestSurface(const GuestSurface&) = delete;
    GuestSurface& operator=(const GuestSurface&) = delete;

    std::uint8_t* Data() { return block; }

private:
    static constexpr std::size_t Alignment = 65536;
    std::uint8_t* block = nullptr;
};

std::string Hex(std::size_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%zx", value);
    return text;
}

std::string WatchMode() {
    if (!AgcDriver::GuestMemory::WriteWatched()) return "not watched, compared";
#ifdef __linux__
    const auto probe = static_cast<int>(syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK));
    if (probe >= 0) {
        close(probe);
        return "userfaultfd";
    }
    return errno == EPERM ? "userfaultfd, user mode only" : "userfaultfd, probe failed";
#else
    return "watched";
#endif
}

class FileBytes {
public:
    explicit FileBytes(std::size_t bytes) : bytes(bytes) {
        std::vector<char> pattern(bytes);
        for (std::size_t at = 0; at < bytes; ++at) pattern[at] = static_cast<char>(0x40u + (at * 37u + at / 251u) % 0xbfu);
        contents.assign(pattern.begin(), pattern.end());
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("anyps5_host_write_pending_" + std::to_string(stamp) + "_" + std::to_string(bytes) + ".bin");
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(pattern.data(), static_cast<std::streamsize>(pattern.size()));
        Require(static_cast<bool>(stream), "host write over pending storage: cannot write the file to read");
    }

    ~FileBytes() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    FileBytes(const FileBytes&) = delete;
    FileBytes& operator=(const FileBytes&) = delete;

    void ReadInto(std::uint8_t* destination) const {
        std::ifstream stream(path, std::ios::binary);
        Require(static_cast<bool>(stream), "host write over pending storage: cannot open the file to read");
        const GuestArena::HostWrite scope(destination, bytes);
        Require(scope.Open(), "host write over pending storage: the read destination is not writable guest memory");
        stream.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(bytes));
        Require(static_cast<std::size_t>(stream.gcount()) == bytes, "host write over pending storage: the file read returned short");
    }

    std::span<const char> Contents() const { return contents; }

private:
    std::size_t bytes;
    std::vector<char> contents;
    std::filesystem::path path;
};

std::uint64_t Address(const std::uint8_t* guest) {
    return reinterpret_cast<std::uint64_t>(guest);
}

void Land(const std::uint8_t* guest) {
    AgcDriver::GuestMemory::FlushGpuWrites(Address(guest), SurfaceBytes);
}

bool HostWriteAfterResults(GuestSurface& surface, const char* name, std::size_t offset, std::size_t bytes, bool storeRecorded, std::span<const std::uint8_t> reference) {
    auto* guest = surface.Data();
    Dispatch(guest);
    const auto untouched = [&] { return std::all_of(guest, guest + SurfaceBytes, [](std::uint8_t byte) { return byte == 0; }); };
    Require(untouched(), std::string("host write over pending storage, ") + name + ": the image's results reached guest memory before the host write, so none was pending");
    if (storeRecorded) {
        StorageTexture::FlushPending(Address(guest), SurfaceBytes, nullptr, "sampled texture");
        Require(untouched(), std::string("host write over pending storage, ") + name + ": the image's store landed when it was made, so none was still recorded at the host write");
    }
    const FileBytes file(bytes);
    file.ReadInto(guest + offset);
    Land(guest);
    const auto contents = file.Contents();
    for (std::size_t at = 0; at < bytes; ++at) {
        if (static_cast<char>(guest[offset + at]) == contents[at]) continue;
        std::cout << "host write over pending storage, " << name << ": FAILED, byte " << Hex(offset + at) << " of the file read was overwritten by the image's store after it (" << Hex(guest[offset + at]) << ", the file has " << Hex(static_cast<std::uint8_t>(contents[at])) << ")\n";
        return false;
    }
    for (std::size_t at = 0; at < SurfaceBytes; ++at) {
        if ((at >= offset && at < offset + bytes) || guest[at] == reference[at]) continue;
        std::cout << "host write over pending storage, " << name << ": FAILED, byte " << Hex(at) << " outside the file read holds " << Hex(guest[at]) << ", not the image's " << Hex(reference[at]) << ", so its results there never landed\n";
        return false;
    }
    std::cout << "host write over pending storage, " << name << ": the file's bytes survived the image's store and its texels landed around them\n";
    return true;
}

double HostWriteNanoseconds(std::uint8_t* guest, std::size_t rounds) {
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t round = 0; round < rounds; ++round) {
        const GuestArena::HostWrite scope(guest, BlockBytes);
        Require(scope.Open(), "host write cost: the destination is not writable guest memory");
    }
    return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / static_cast<double>(rounds);
}

void HostWriteCost(std::size_t rounds) {
    GuestSurface pending;
    GuestSurface idle;
    Dispatch(pending.Data());
    const auto measure = [&](const char* state) {
        double hooked = 1e300;
        double bare = 1e300;
        for (int pass = 0; pass < 5; ++pass) {
            hooked = std::min(hooked, HostWriteNanoseconds(idle.Data(), rounds));
            GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
            bare = std::min(bare, HostWriteNanoseconds(idle.Data(), rounds));
            GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(&AgcDriver::GuestMemory::PrepareHostWrite);
        }
        std::printf("host write cost, %s: %.1f ns per 64 KiB host write with the flush before it, %.1f ns without (best of 5 x %zu)\n", state, hooked, bare, rounds);
    };
    measure("an image's results pending elsewhere");
    Land(pending.Data());
    measure("nothing pending");
}

}

int main(int argc, char** argv) {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        std::cout << "write watch: " << WatchMode() << '\n';
        if (argc == 3 && std::string(argv[1]) == "--cost") {
            HostWriteCost(static_cast<std::size_t>(std::stoull(argv[2])));
            AgcDriverShutdown_nid_postfix();
            return 0;
        }
        std::vector<std::uint8_t> reference(SurfaceBytes);
        GuestSurface referenceSurface;
        Dispatch(referenceSurface.Data());
        Land(referenceSurface.Data());
        std::copy(referenceSurface.Data(), referenceSurface.Data() + SurfaceBytes, reference.begin());
        Require(std::any_of(reference.begin(), reference.begin() + BlockBytes, [](std::uint8_t byte) { return byte != 0; }), "host write over pending storage: the image wrote no texel into its first block");
        std::array<GuestSurface, 4> surfaces;
        bool passed = HostWriteAfterResults(surfaces[0], "a store recorded before a 64 KiB host write", 0, BlockBytes, true, reference);
        passed = HostWriteAfterResults(surfaces[1], "a store recorded before a 4 KiB host write", 0x3000, 0x1000, true, reference) && passed;
        passed = HostWriteAfterResults(surfaces[2], "a 64 KiB host write over pending results", 0, BlockBytes, false, reference) && passed;
        passed = HostWriteAfterResults(surfaces[3], "a 4 KiB host write over pending results", 0x3000, 0x1000, false, reference) && passed;
        Require(passed, "host write over pending storage: a host write did not end up over the image's results");
        AgcDriverShutdown_nid_postfix();
        std::puts("host write pending storage tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
