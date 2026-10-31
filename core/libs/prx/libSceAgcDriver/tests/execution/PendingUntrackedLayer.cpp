#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t Page = 0x1000;
constexpr std::size_t Block = 0x10000;
constexpr std::size_t SurfaceBytes = 4 * Block;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t StoredAgain = 0x0badf00du;
constexpr std::uint32_t CpuWritten = 0x5a5a5a5au;
constexpr std::uint32_t StoreThreads = 8;
constexpr std::uint32_t ReadThreads = 32;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreAgainCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, StoredAgain, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> ReadCode{
    0x7e020280, 0x7e04020c, 0xf0001108, 0x00010300, 0xbf8c3f70, 0x34080082, 0xe0701000, 0x80000304, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Dispatch(std::span<const std::uint32_t> code, std::uint32_t threads, std::uint32_t resource, std::span<const std::uint32_t> userData) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{threads, 1, 1};
    registers(0x207, shape);
    const auto address = AddressOf(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> layout{resource};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1u, 1u, 1u, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

void Store(const std::uint32_t* texels, std::span<const std::uint32_t> code) {
    std::array<std::uint32_t, 16> userData{};
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    Dispatch(code, StoreThreads, static_cast<std::uint32_t>(userData.size()) << 1u, userData);
}

void ReadImage(const std::uint32_t* texels, std::uint32_t* output) {
    std::array<std::uint32_t, 13> userData{};
    const auto address = AddressOf(output);
    const std::array<std::uint32_t, 4> buffer{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), ReadThreads * 4u, 0x31016facu};
    const auto texture = TextureDescriptor(texels);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    Dispatch(ReadCode, ReadThreads, static_cast<std::uint32_t>(userData.size()) << 1u, userData);
}

class Surface {
public:
    Surface() {
#ifndef _WIN32
        void* mapped = mmap(nullptr, SurfaceBytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "pending untracked layer: cannot map the guest surface");
        const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
        const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
        if (aligned != begin) munmap(mapped, aligned - begin);
        if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Block - aligned);
        texels = reinterpret_cast<std::uint32_t*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(texels, SurfaceBytes);
        std::fill(texels, texels + SurfaceBytes / 4u, Untouched);
        GuestAllocations::Mutation().Add(texels, SurfaceBytes, true, true, true);
#endif
    }

    ~Surface() {
#ifndef _WIN32
        GuestAllocations::Mutation().Remove(texels);
        munmap(texels, SurfaceBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(texels, SurfaceBytes);
#endif
    }

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    std::uint32_t* Texels() const { return texels; }
    std::uint64_t Address() const { return AddressOf(texels); }

    bool LoseWalk() const {
#ifdef _WIN32
        return false;
#else
        auto* page = reinterpret_cast<std::uint8_t*>(texels) + 3 * Block;
        std::array<std::uint8_t, Page> saved{};
        std::memcpy(saved.data(), page, Page);
        if (mmap(page, Page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED) return false;
        std::memcpy(page, saved.data(), Page);
        return true;
#endif
    }

private:
    std::uint32_t* texels = nullptr;
};

class Output {
public:
    Output() : words(ReadThreads + 1024u, Untouched) {
        data = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(words.data()) + 255u) & ~std::uintptr_t{255u});
        GuestAllocations::Mutation().Add(words.data(), words.size() * 4u, true, true, true);
    }

    ~Output() { GuestAllocations::Mutation().Remove(words.data()); }

    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;

    std::uint32_t* Data() const { return data; }

private:
    std::vector<std::uint32_t> words;
    std::uint32_t* data = nullptr;
};

std::size_t Count(const std::uint32_t* words, std::size_t size, std::uint32_t value) {
    const auto* volatileWords = static_cast<const volatile std::uint32_t*>(words);
    std::size_t count = 0;
    for (std::size_t index = 0; index < size; ++index) count += volatileWords[index] == value ? 1u : 0u;
    return count;
}

class CapturedErrors {
public:
    CapturedErrors() {
#ifndef _WIN32
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "pending untracked layer: cannot open the stderr capture");
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }

    std::string Finish() {
        std::string text;
#ifndef _WIN32
        std::fflush(stderr);
        dup2(saved, fileno(stderr));
        close(saved);
        std::rewind(file);
        std::array<char, 4096> chunk{};
        while (const auto read = std::fread(chunk.data(), 1, chunk.size(), file)) text.append(chunk.data(), read);
        std::fclose(file);
        file = nullptr;
        std::fputs(text.c_str(), stderr);
#endif
        return text;
    }

    ~CapturedErrors() {
        if (file != nullptr) Finish();
    }

    CapturedErrors(const CapturedErrors&) = delete;
    CapturedErrors& operator=(const CapturedErrors&) = delete;

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

enum class Outcome { Kept, Lost, Missed, NotRun };

enum class Kind { Results, CpuWhilePending, CpuBeforeMark };

struct Case {
    const char* name;
    Kind kind;
    bool loseWalk;
};

Outcome Run(const Case& test) {
    Surface surface;
    struct WaitForWork {
        ~WaitForWork() { AgcDriverWaitIdle_nid_postfix(); }
    } waitForWork;
    const auto address = surface.Address();
    if (!AgcDriver::GuestMemory::Watched(address, SurfaceBytes)) {
        std::fprintf(stderr, "[pending untracked layer] %s: not run, the surface is not write-watched\n", test.name);
        return Outcome::NotRun;
    }
    std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex(), std::defer_lock);
    const auto heldPending = [&](std::uint32_t result) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (true) {
            hold.lock();
            const auto image = StorageTexture::FindLive(address, SurfaceBytes);
            if (image != nullptr && StorageTexture::FindPending(address, SurfaceBytes) == image) return true;
            hold.unlock();
            if (Count(surface.Texels(), SurfaceBytes / 4u, result) != 0) return false;
            Require(std::chrono::steady_clock::now() < deadline, "pending untracked layer: the result neither became pending nor reached guest memory");
            std::this_thread::yield();
        }
    };
    const auto readGuest = [&] {
        std::vector<std::byte> whole(SurfaceBytes);
        AgcDriver::GuestMemory::Read(address, whole);
    };
    Store(surface.Texels(), StoreCode);
    if (heldPending(Stored)) {
        StorageTexture::FlushAllPending("pending untracked layer fixture");
        hold.unlock();
    }
    readGuest();
    Require(Count(surface.Texels(), SurfaceBytes / 4u, Stored) == StoreThreads, "pending untracked layer: the first results were not stored");
    if (test.loseWalk) {
        if (!surface.LoseWalk()) {
            std::fprintf(stderr, "[pending untracked layer] %s: not run, the surface's page cannot be remapped\n", test.name);
            return Outcome::NotRun;
        }
        if (!AgcDriver::GuestMemory::Watched(address, SurfaceBytes) || AgcDriver::GuestMemory::CollectWritesUncached(address, SurfaceBytes) != 0) {
            std::fprintf(stderr, "[pending untracked layer] %s: not run, the remapped page left the walk complete\n", test.name);
            return Outcome::NotRun;
        }
    }
    const auto readOutput = [&](Output& output) {
        ReadImage(surface.Texels(), output.Data());
        AgcDriverWaitIdle_nid_postfix();
        std::vector<std::byte> read(ReadThreads * 4u);
        AgcDriver::GuestMemory::Read(AddressOf(output.Data()), read);
    };
    if (test.kind == Kind::CpuBeforeMark) {
        const auto* first = std::find(surface.Texels(), surface.Texels() + SurfaceBytes / 4u, Stored);
        auto* result = static_cast<volatile std::uint32_t*>(surface.Texels()) + (first - surface.Texels());
        hold.lock();
        const auto image = StorageTexture::FindLive(address, SurfaceBytes);
        Require(image != nullptr, "pending untracked layer: the surface has no storage image");
        image->Refresh();
        *result = CpuWritten;
        image->MarkDirty();
        image->Refresh();
        hold.unlock();
        Output output;
        readOutput(output);
        const auto imageCpu = Count(output.Data(), ReadThreads, CpuWritten);
        hold.lock();
        StorageTexture::FlushAllPending("pending untracked layer fixture");
        hold.unlock();
        readGuest();
        const auto results = Count(surface.Texels(), SurfaceBytes / 4u, Stored);
        const auto cpuKept = Count(surface.Texels(), SurfaceBytes / 4u, CpuWritten);
        if (imageCpu == 1 && results == StoreThreads - 1 && cpuKept == 1) return Outcome::Kept;
        std::fprintf(stderr, "[pending untracked layer] %s: the image holds %zu CPU words, guest memory %zu of %u results and %zu CPU words\n", test.name, imageCpu, results, StoreThreads - 1, cpuKept);
        return Outcome::Lost;
    }
    auto* cpuWord = static_cast<volatile std::uint32_t*>(surface.Texels()) + 2 * Block / 4u;
    Store(surface.Texels(), StoreAgainCode);
    if (!heldPending(StoredAgain)) return Outcome::Missed;
    const auto image = StorageTexture::FindLive(address, SurfaceBytes);
    Require(image != nullptr, "pending untracked layer: the surface has no storage image");
    if (test.kind == Kind::CpuWhilePending) *cpuWord = CpuWritten;
    std::string errors;
    {
        CapturedErrors captured;
        image->Refresh();
        errors = captured.Finish();
    }
    hold.unlock();
    Output output;
    readOutput(output);
    const auto imageHolds = Count(output.Data(), ReadThreads, StoredAgain);
    hold.lock();
    StorageTexture::FlushAllPending("pending untracked layer fixture");
    hold.unlock();
    readGuest();
    const auto guestHolds = Count(surface.Texels(), SurfaceBytes / 4u, StoredAgain);
    const auto cpuKept = Count(surface.Texels(), SurfaceBytes / 4u, CpuWritten);
    const bool line = errors.find("guest memory changed while GPU results were pending") != std::string::npos;
    const bool kept = imageHolds == StoreThreads && guestHolds == StoreThreads && (test.kind == Kind::CpuWhilePending ? cpuKept == 1 : !line);
    if (kept) return Outcome::Kept;
    std::fprintf(stderr, "[pending untracked layer] %s: the image holds %zu of %u results, guest memory %zu, CPU words %zu, CPU write reported %d\n", test.name, imageHolds, StoreThreads, guestHolds, cpuKept, line ? 1 : 0);
    return Outcome::Lost;
}

}

int main() {
    try {
#ifdef _WIN32
        std::fprintf(stderr, "[pending untracked layer] not run, the walk is made to fail by remapping a watched page, which needs Linux\n");
        return VulkanTestSkipped;
#else
        Require(setenv("APS5_NO_HOST_IMPORT", "1", 1) == 0, "pending untracked layer: cannot keep the surface out of host imports");
#endif
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        {
            Surface probe;
            Require(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(probe.Texels()))).guestBytes == SurfaceBytes, "pending untracked layer: unexpected surface size");
        }
        constexpr std::array<Case, 4> cases{{
            {"results over a surface whose walk fails", Kind::Results, true},
            {"results and a CPU store over a surface whose walk fails", Kind::CpuWhilePending, true},
            {"a CPU store between a refresh and the pending mark", Kind::CpuBeforeMark, false},
            {"a CPU store between a refresh and the pending mark over a surface whose walk fails", Kind::CpuBeforeMark, true},
        }};
        bool kept = true;
        for (const auto& test : cases) {
            auto outcome = Outcome::Missed;
            for (std::uint32_t attempt = 0; attempt < 64u && outcome == Outcome::Missed; ++attempt) outcome = Run(test);
            Require(outcome != Outcome::Missed, "pending untracked layer: the result was never held pending");
            if (outcome == Outcome::NotRun) return VulkanTestSkipped;
            kept = outcome == Outcome::Kept && kept;
        }
        Require(kept, "pending storage results or a CPU store over a write-watched surface were lost");
        AgcDriverShutdown_nid_postfix();
        std::puts("pending untracked layer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
