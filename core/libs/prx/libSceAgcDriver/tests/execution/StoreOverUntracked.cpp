#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t TileLinear = 0;
std::uint32_t surfaceTile = TileR64KBX;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t Page = 0x1000;
constexpr std::size_t Block = 0x10000;
constexpr std::size_t SurfaceBytes = 4 * Block;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t Filled = 0x9abcdef0u;
constexpr std::uint32_t StoredAgain = 0x0badf00du;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreAgainCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, StoredAgain, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> FillCode{
    0xd7460004, 0x04010c08, 0x7e000204, 0x7e020205, 0x7e040206, 0x7e060207, 0xe01c2000, 0x80000004, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> ReadCode{
    0x7e020280, 0x7e04020c, 0xf0001108, 0x00010300, 0xbf8c3f70, 0x34080082, 0xe0701000, 0x80000304, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels, std::uint32_t side = Side) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((side - 1u) & 3u) << 30u),
        ((side - 1u) >> 2u) | ((side - 1u) << 14u),
        0xfacu | (surfaceTile << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void Dispatch(std::span<const std::uint32_t> code, std::uint32_t threads, std::uint32_t resource, std::span<const std::uint32_t> userData, std::uint32_t groups) {
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
    words.insert(words.end(), {0xc0031500u, groups, 1, 1, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

void Store(const std::uint32_t* texels, std::uint32_t side = Side, std::span<const std::uint32_t> code = StoreCode) {
    std::array<std::uint32_t, 16> userData{};
    const auto texture = TextureDescriptor(texels, side);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    Dispatch(code, 8, static_cast<std::uint32_t>(userData.size()) << 1u, userData, 1);
}

void Fill(const std::uint32_t* texels) {
    const auto address = AddressOf(texels);
    const std::array<std::uint32_t, 8> userData{
        static_cast<std::uint32_t>(address),
        static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u),
        static_cast<std::uint32_t>(SurfaceBytes / 16u),
        0x4bfacu,
        Filled,
        Filled,
        Filled,
        Filled,
    };
    Dispatch(FillCode, 64, (static_cast<std::uint32_t>(userData.size()) << 1u) | 0x80u, userData, static_cast<std::uint32_t>(SurfaceBytes / 1024u));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::size_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), static_cast<std::uint32_t>(bytes), 0x31016facu};
}

void ReadImage(const std::uint32_t* texels, std::uint32_t* output) {
    std::array<std::uint32_t, 13> userData{};
    const auto buffer = BufferDescriptor(output, SurfaceBytes);
    const auto texture = TextureDescriptor(texels);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    Dispatch(ReadCode, 32, static_cast<std::uint32_t>(userData.size()) << 1u, userData, 1);
}

class Surface {
public:
    explicit Surface(bool watched = false, std::size_t lead = 0) : watched(watched), regionBytes(lead == 0 ? SurfaceBytes : SurfaceBytes + Block) {
        if (watched) {
#ifdef _WIN32
            region = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(regionBytes, Block));
            if (region != nullptr) GuestArena::GuestArenaCommit_nid_postfix(region, regionBytes, PAGE_READWRITE, regionBytes);
#else
            void* mapped = mmap(nullptr, regionBytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapped != MAP_FAILED) {
                const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
                const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
                if (aligned != begin) munmap(mapped, aligned - begin);
                if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + regionBytes), begin + Block - aligned);
                region = reinterpret_cast<std::uint8_t*>(aligned);
                GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(region, regionBytes);
            }
#endif
            if (region != nullptr) texels = reinterpret_cast<std::uint32_t*>(region + lead);
        } else {
            storage.resize((SurfaceBytes + Block) / 4u);
            texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + Block - 1) & ~std::uintptr_t{Block - 1});
        }
        Require(texels != nullptr, "store over untracked: cannot allocate the guest surface");
        std::fill(texels, texels + SurfaceBytes / 4u, Untouched);
        allocation = region != nullptr ? static_cast<void*>(region) : static_cast<void*>(texels);
        GuestAllocations::Mutation().Add(allocation, region != nullptr ? regionBytes : SurfaceBytes, true, true, true);
    }

    ~Surface() {
        GuestAllocations::Mutation().Remove(allocation);
        if (!watched) return;
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(region, regionBytes);
        GuestArena::GuestArenaRelease_nid_postfix(region, regionBytes);
#else
        munmap(region, regionBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(region, regionBytes);
#endif
    }

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    std::uint32_t* Texels() const { return texels; }
    std::uint8_t* Bytes() const { return reinterpret_cast<std::uint8_t*>(texels); }
    std::uint64_t Address() const { return AddressOf(texels); }

private:
    bool watched;
    std::size_t regionBytes;
    std::vector<std::uint32_t> storage;
    std::uint8_t* region = nullptr;
    void* allocation = nullptr;
    std::uint32_t* texels = nullptr;
};

std::uint8_t Pattern(std::size_t offset) {
    return static_cast<std::uint8_t>(0x40u + offset % 0x3bu);
}

std::size_t LostBytes(const std::uint8_t* bytes) {
    std::size_t lost = 0;
    for (std::size_t offset = 0; offset < Page; ++offset) {
        if (static_cast<const volatile std::uint8_t*>(bytes)[offset] != Pattern(offset)) ++lost;
    }
    return lost;
}

bool Holds(const std::uint32_t* texels, std::uint32_t value) {
    const auto* words = static_cast<const volatile std::uint32_t*>(texels);
    for (std::size_t index = 0; index < SurfaceBytes / 4u; ++index) {
        if (words[index] == value) return true;
    }
    return false;
}

template <typename THold>
bool HoldPending(THold& hold, const Surface& surface, std::uint32_t result) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (true) {
        hold.lock();
        const auto live = StorageTexture::FindLive(surface.Address(), SurfaceBytes);
        if (live != nullptr && StorageTexture::FindPending(surface.Address(), SurfaceBytes) == live) return true;
        hold.unlock();
        if (Holds(surface.Texels(), result)) return false;
        Require(std::chrono::steady_clock::now() < deadline, "store over untracked: the result neither became pending nor reached guest memory");
        std::this_thread::yield();
    }
}

template <typename THold>
void Settle(THold& hold, const Surface& surface, std::uint32_t result) {
    if (!HoldPending(hold, surface, result)) return;
    StorageTexture::FlushAllPending("store over untracked fixture");
    hold.unlock();
}

void PendingIdentity() {
    Surface pending;
    Surface completed;
    std::shared_ptr<StorageTexture> image;
    struct WaitForWork {
        ~WaitForWork() { AgcDriverWaitIdle_nid_postfix(); }
    } waitForWork;
    Store(pending.Texels());
    AgcDriverWaitIdle_nid_postfix();
    std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex());
    image = StorageTexture::FindLive(pending.Address(), SurfaceBytes);
    Require(image != nullptr, "store over untracked: the identity test has no live image");
    StorageTexture::FlushAllPending("pending identity fixture");
    image->MarkDirty();
    Require(StorageTexture::FindPending(pending.Address(), SurfaceBytes) == image, "store over untracked: the identity test did not make its image pending");
    Require(StorageTexture::FindLive(completed.Address(), SurfaceBytes) == nullptr && StorageTexture::FindPending(completed.Address(), SurfaceBytes) == nullptr, "store over untracked: the completed surface unexpectedly has an image");
    hold.unlock();
    Require(HoldPending(hold, pending, Stored), "store over untracked: the wait ignored its own pending image");
    Require(hold.owns_lock(), "store over untracked: the pending image was not held under the GPU mutex");
    std::fill(completed.Texels(), completed.Texels() + SurfaceBytes / 4u, Stored);
    hold.unlock();
    const bool held = HoldPending(hold, completed, Stored);
    if (!hold.owns_lock()) hold.lock();
    StorageTexture::FlushAllPending("pending identity fixture cleanup");
    hold.unlock();
    Require(!held, "store over untracked: the wait accepted another surface's pending image");
}

enum class Kind { ReadAfter, ReadBefore, Filled, Copied, LeftWatch, WrittenThenLeftWatch, ImportLeftWatch, NeighbourCopied, NeighbourLeftWatch, UntrackedRepeat };

// One 64 KiB block of the surface: the image of its last block, and the source copied into it.
constexpr std::uint32_t BlockSide = 128;
constexpr std::size_t LastBlock = SurfaceBytes - Block;

struct Case {
    const char* name;
    Kind kind;
    std::size_t access;
};

enum class Outcome { Kept, Lost, Missed };

// The surface is outside the write watch from the start. Its first results are stored, which
// leaves their bytes saved for the next store to compare with; the CPU writes another block, then
// the next results land and are stored: the CPU's block was written after the first store, so it
// keeps the CPU's bytes as the write watch would keep them.
Outcome RunSecondStore(const char* name) {
    Surface surface(true);
    struct WaitForWork {
        ~WaitForWork() { AgcDriverWaitIdle_nid_postfix(); }
    } waitForWork;
    const auto address = surface.Address();
    if (!AgcDriver::GuestMemory::Watched(address, SurfaceBytes)) {
        std::fprintf(stderr, "[store over untracked] %s: not run, the surface is not write-watched\n", name);
        return Outcome::Kept;
    }
    AgcDriver::GuestMemory::Unwatch(address, SurfaceBytes);
    Require(!AgcDriver::GuestMemory::Watched(address, SurfaceBytes), "store over untracked: the surface is still write-watched");
    std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex(), std::defer_lock);
    const auto heldLive = [&](std::uint32_t result) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (true) {
            hold.lock();
            const auto image = StorageTexture::FindLive(address, SurfaceBytes);
            if (image != nullptr && StorageTexture::FindPending(address, SurfaceBytes) == image) return true;
            hold.unlock();
            if (Holds(surface.Texels(), result)) return false;
            Require(std::chrono::steady_clock::now() < deadline, "store over untracked: the result neither became pending nor reached guest memory");
            std::this_thread::yield();
        }
    };
    Store(surface.Texels());
    if (!heldLive(Stored)) return Outcome::Missed;
    StorageTexture::FlushAllPending("store over untracked fixture");
    hold.unlock();
    {
        std::vector<std::byte> first(SurfaceBytes);
        AgcDriver::GuestMemory::Read(address, first);
    }
    Require(static_cast<const volatile std::uint32_t*>(surface.Texels())[0] == Stored, "store over untracked: the first result was not stored");
    hold.lock();
    const auto image = StorageTexture::FindLive(address, SurfaceBytes);
    const bool saved = image != nullptr && image->GuestSnapshotValid();
    hold.unlock();
    Require(image != nullptr, "store over untracked: the surface has no storage image");
    if (!saved) {
        std::fprintf(stderr, "[store over untracked] %s: the store left no saved guest bytes\n", name);
        return Outcome::Lost;
    }
    auto* written = surface.Bytes() + 2 * Block;
    for (std::size_t offset = 0; offset < Page; ++offset) static_cast<volatile std::uint8_t*>(written)[offset] = Pattern(offset);
    Store(surface.Texels(), Side, StoreAgainCode);
    if (!heldLive(StoredAgain)) return Outcome::Missed;
    StorageTexture::FlushAllPending("store over untracked fixture");
    hold.unlock();
    std::vector<std::byte> whole(SurfaceBytes);
    AgcDriver::GuestMemory::Read(address, whole);
    Require(static_cast<const volatile std::uint32_t*>(surface.Texels())[0] == StoredAgain, "store over untracked: the second result did not reach a block the CPU left alone");
    const auto lost = LostBytes(written);
    if (lost == 0) return Outcome::Kept;
    std::fprintf(stderr, "[store over untracked] %s: of 0x%zx bytes the CPU wrote between the two stores, %zu were lost\n", name, Page, lost);
    return Outcome::Lost;
}

Outcome Run(const Case& test) {
    std::optional<Surface> output;
    std::optional<Surface> source;
    if (test.kind == Kind::Copied || test.kind == Kind::NeighbourCopied) source.emplace();
    const bool neighbour = test.kind == Kind::NeighbourLeftWatch;
    const bool leaves = test.kind == Kind::LeftWatch || test.kind == Kind::WrittenThenLeftWatch || test.kind == Kind::ImportLeftWatch || neighbour;
    surfaceTile = neighbour ? TileLinear : TileR64KBX;
    Surface surface(leaves, neighbour ? Page : 0);
    struct WaitForWork {
        ~WaitForWork() { AgcDriverWaitIdle_nid_postfix(); }
    } waitForWork;
    auto* bytes = surface.Bytes();
    const auto address = surface.Address();
    std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex(), std::defer_lock);
    if (test.kind == Kind::Copied) {
        Store(source->Texels());
        Settle(hold, *source, Stored);
        Fill(source->Texels());
        Settle(hold, *source, Filled);
    }
    Store(surface.Texels());
    if (test.kind == Kind::UntrackedRepeat) {
        Settle(hold, surface, Stored);
        output.emplace();
        ReadImage(surface.Texels(), output->Texels());
        hold.lock();
        const auto image = StorageTexture::FindLive(surface.Address(), SurfaceBytes);
        Require(image != nullptr, "store over untracked: the sampled surface has no live storage image");
        image->MarkDirty();
        hold.unlock();
    }
    if (test.kind == Kind::Filled || leaves) {
        Settle(hold, surface, Stored);
        if (leaves) {
            std::vector<std::byte> settled(SurfaceBytes);
            AgcDriver::GuestMemory::Read(address, settled);
        }
        Fill(surface.Texels());
        if (!HoldPending(hold, surface, Filled)) return Outcome::Missed;
    } else if (test.kind == Kind::Copied) {
        Settle(hold, surface, Stored);
        hold.lock();
        const auto from = StorageTexture::FindLive(source->Address(), SurfaceBytes);
        const auto to = StorageTexture::FindLive(address, SurfaceBytes);
        Require(from != nullptr && to != nullptr, "store over untracked: the surfaces have no storage images to copy between");
        const char* refusal = nullptr;
        if (!to->CopyFrom(*from, refusal)) {
            Require(refusal != nullptr && std::string_view(refusal) == "not imported", "store over untracked: the image copy was refused");
            std::fprintf(stderr, "[store over untracked] %s: not run, the surface has no host import\n", test.name);
            return Outcome::Kept;
        }
    } else if (test.kind == Kind::NeighbourCopied) {
        // The surface's image has results pending in its first block when a copy replaces the
        // image of its last block: the copy supersedes the saved bytes there only.
        const auto settledLive = [&](std::uint64_t at, std::size_t size) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (true) {
                hold.lock();
                StorageTexture::FlushAllPending("store over untracked fixture");
                const bool live = StorageTexture::FindLive(at, size) != nullptr;
                hold.unlock();
                if (live) return;
                Require(std::chrono::steady_clock::now() < deadline, "store over untracked: the block's storage image was never created");
                std::this_thread::yield();
            }
        };
        Store(surface.Texels() + LastBlock / 4u, BlockSide);
        settledLive(address + LastBlock, Block);
        Store(source->Texels(), BlockSide);
        settledLive(source->Address(), Block);
        std::fill(surface.Texels(), surface.Texels() + SurfaceBytes / 4u, Untouched);
        Store(surface.Texels());
        // Pending results are the surface image's alone once it exists.
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{address, address + SurfaceBytes}}};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (true) {
            hold.lock();
            if (StorageTexture::FindLive(address, SurfaceBytes) != nullptr && StorageTexture::AnyPendingOverlaps(range)) break;
            hold.unlock();
            if (Holds(surface.Texels(), Stored)) return Outcome::Missed;
            Require(std::chrono::steady_clock::now() < deadline, "store over untracked: the surface's result neither became pending nor reached guest memory");
            std::this_thread::yield();
        }
        const auto from = StorageTexture::FindLive(source->Address(), Block);
        const auto to = StorageTexture::FindLive(address + LastBlock, Block);
        Require(from != nullptr && to != nullptr, "store over untracked: the blocks have no storage images to copy between");
        const char* refusal = nullptr;
        if (!to->CopyFrom(*from, refusal)) {
            Require(refusal != nullptr && std::string_view(refusal) == "not imported", "store over untracked: the image copy was refused");
            StorageTexture::FlushAllPending("store over untracked fixture");
            std::fprintf(stderr, "[store over untracked] %s: not run, the block has no host import\n", test.name);
            return Outcome::Kept;
        }
    } else if (!HoldPending(hold, surface, Stored)) {
        return Outcome::Missed;
    }
    const auto writePattern = [&] {
        for (std::size_t offset = 0; offset < Page; ++offset) static_cast<volatile std::uint8_t*>(bytes)[offset] = Pattern(offset);
    };
    if (test.kind == Kind::WrittenThenLeftWatch) writePattern();
    if (leaves) {
        if (!AgcDriver::GuestMemory::Watched(address, SurfaceBytes)) {
            StorageTexture::FlushAllPending("store over untracked fixture");
            std::fprintf(stderr, "[store over untracked] %s: not run, the surface is not write-watched\n", test.name);
            return Outcome::Kept;
        }
        const auto image = StorageTexture::FindLive(address, SurfaceBytes);
        Require(image != nullptr, "store over untracked: the surface has no storage image");
        Require(!image->GuestSnapshotValid(), "store over untracked: the pending result had saved guest bytes while its memory was write-watched");
        if (test.kind == Kind::ImportLeftWatch) Require(AgcDriver::GuestMemory::ImportWatched(address, SurfaceBytes, [] { return true; }, false), "store over untracked: the import that leaves the write watch failed");
        else if (neighbour) Require(AgcDriver::GuestMemory::ImportWatched(address - Page, Page, [] { return true; }, false), "store over untracked: the import of the page before the surface failed");
        else AgcDriver::GuestMemory::Unwatch(address, SurfaceBytes);
        if (!neighbour) Require(!AgcDriver::GuestMemory::Watched(address, SurfaceBytes), "store over untracked: the surface is still write-watched");
    }
    std::array<std::byte, Page> read{};
    if (test.kind == Kind::ReadBefore) AgcDriver::GuestMemory::Read(address + Block + Page, read);
    if (test.kind != Kind::WrittenThenLeftWatch) writePattern();
    AgcDriver::GuestMemory::Read(address + test.access, read);
    const auto lostAtAccess = LostBytes(bytes);
    StorageTexture::FlushAllPending("store over untracked fixture");
    std::vector<std::byte> whole(SurfaceBytes);
    AgcDriver::GuestMemory::Read(address, whole);
    const auto lostAtEnd = LostBytes(bytes);
    if (test.kind == Kind::Filled || test.kind == Kind::Copied || leaves) Require(static_cast<const volatile std::uint32_t*>(surface.Texels())[2 * Block / 4u] == Filled, "store over untracked: the result did not reach a block the CPU left alone");
    if (lostAtAccess == 0 && lostAtEnd == 0) return Outcome::Kept;
    std::fprintf(stderr, "[store over untracked] %s: of 0x%zx bytes the CPU wrote while the result was pending, %zu were lost after the access at +0x%zx and %zu after every result was stored\n", test.name, Page, lostAtAccess, test.access, lostAtEnd);
    return Outcome::Lost;
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--pending-identity"), "store over untracked: unknown test argument");
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        if (argc == 2) {
            PendingIdentity();
            AgcDriverShutdown_nid_postfix();
            std::puts("pending surface identity tests passed");
            return 0;
        }
        {
            Surface probe;
            Require(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(probe.Texels()))).guestBytes == SurfaceBytes, "store over untracked: unexpected surface size");
        }
        constexpr std::array<Case, 10> cases{{
            {"another block read after the CPU write", Kind::ReadAfter, Block + Page},
            {"another block read before the CPU write", Kind::ReadBefore, 0xe000},
            {"a fill of the surface", Kind::Filled, 0xe000},
            {"an image copy into the surface", Kind::Copied, 0xe000},
            {"a surface that left the write watch", Kind::LeftWatch, 0xe000},
            {"a surface written before it left the write watch", Kind::WrittenThenLeftWatch, 0xe000},
            {"a surface whose import left the write watch", Kind::ImportLeftWatch, 0xe000},
            {"an image copy into another block of the surface", Kind::NeighbourCopied, 0xe000},
            {"a surface whose first block left the write watch with the page before it", Kind::NeighbourLeftWatch, 0xe000},
            {"untracked storage results written again after an image read", Kind::UntrackedRepeat, 0xe000},
        }};
        bool kept = true;
        for (const auto& test : cases) {
            auto outcome = Outcome::Missed;
            for (std::uint32_t attempt = 0; attempt < 256u && outcome == Outcome::Missed; ++attempt) outcome = Run(test);
            Require(outcome != Outcome::Missed, "store over untracked: the result was never held pending");
            kept = outcome == Outcome::Kept && kept;
        }
        {
            constexpr auto name = "a second store over a surface outside the write watch";
            auto outcome = Outcome::Missed;
            for (std::uint32_t attempt = 0; attempt < 256u && outcome == Outcome::Missed; ++attempt) outcome = RunSecondStore(name);
            Require(outcome != Outcome::Missed, "store over untracked: the result was never held pending");
            kept = outcome == Outcome::Kept && kept;
        }
        Require(kept, "a pending storage result was stored over bytes the CPU wrote to untracked memory");
        AgcDriverShutdown_nid_postfix();
        std::puts("store over untracked tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
