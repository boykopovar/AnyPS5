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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::size_t Block = 0x10000;
constexpr std::size_t SurfaceBytes = 4 * Block;
constexpr std::size_t Page = 0x1000;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t Unread = 0xa5a5a5a5u;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> LoadCode{
    0x7e020280, 0xf0001108, 0x00020300, 0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 9> LoadArrayCode{
    0x7e020280, 0x7e040280, 0xf0001128, 0x00020300, 0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0xbf810000,
};

alignas(256) std::array<std::uint32_t, Threads> Output{};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels, std::uint32_t type = Type2D) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (type << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Dispatch(const std::uint32_t* texels, std::span<const std::uint32_t> code, std::uint32_t type = Type2D) {
    std::array<std::uint32_t, 16> userData{};
    const auto buffer = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * sizeof(std::uint32_t)));
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    const auto texture = TextureDescriptor(texels, type);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::vector<std::uint32_t> words;
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{Threads, 1, 1};
    registers(0x207, shape);
    const auto address = AddressOf(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> layout{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
}

void Protect(std::uint32_t* texels, std::size_t offset, std::size_t bytes, bool committed) {
    auto* begin = reinterpret_cast<std::byte*>(texels) + offset;
    GuestAllocations::Mutation().Protect(begin, bytes, committed, committed, committed, committed, [&] {
#ifdef _WIN32
        DWORD prior = 0;
        Require(VirtualProtect(begin, bytes, committed ? PAGE_READWRITE : PAGE_NOACCESS, &prior) != 0, "non-resident storage store: cannot change the protection of the surface's pages");
#else
        Require(mprotect(begin, bytes, committed ? PROT_READ | PROT_WRITE : PROT_NONE) == 0, "non-resident storage store: cannot change the protection of the surface's pages");
#endif
    });
}

std::uint32_t* MapSurface() {
#ifdef _WIN32
    auto* texels = static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, Block));
    if (texels != nullptr) GuestArena::GuestArenaCommit_nid_postfix(texels, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
    return texels;
#else
    void* mapped = mmap(nullptr, SurfaceBytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) return nullptr;
    const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
    const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
    if (aligned != begin) munmap(mapped, aligned - begin);
    if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Block - aligned);
    auto* texels = reinterpret_cast<std::uint32_t*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(texels, SurfaceBytes);
    return texels;
#endif
}

void ReleaseSurface(std::uint32_t* texels) {
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(texels, SurfaceBytes);
    GuestArena::GuestArenaRelease_nid_postfix(texels, SurfaceBytes);
#else
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(texels, SurfaceBytes);
    munmap(texels, SurfaceBytes);
#endif
}

class Surface {
public:
    explicit Surface(std::uint32_t fill) : texels(MapSurface()) {
        Require(texels != nullptr, "non-resident storage store: cannot allocate a guest surface");
        std::fill(texels, texels + SurfaceBytes / 4u, fill);
        GuestAllocations::Mutation().Add(texels, SurfaceBytes, true, true, true);
        Require(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels))).guestBytes == SurfaceBytes, "non-resident storage store: unexpected surface size");
    }
    ~Surface() {
        try {
            StorageTexture::FlushAllPending("non-resident storage store");
            AgcDriverWaitIdle_nid_postfix();
        } catch (...) {
        }
        GuestAllocations::Mutation().Remove(texels);
        ReleaseSurface(texels);
    }
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    std::uint32_t* texels;
};

void Load(const std::uint32_t* texels, std::uint32_t view = Type2D) {
    Output.fill(Unread);
    if (view == Type2DArray) Dispatch(texels, LoadArrayCode, view);
    else Dispatch(texels, LoadCode);
}

std::string ViewName(std::uint32_t view) {
    return view == Type2DArray ? " through a one-slice array view" : "";
}

void Flush() {
    StorageTexture::FlushAllPending("non-resident storage store");
    AgcDriverWaitIdle_nid_postfix();
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void RequireTexels(const std::array<std::uint32_t, Threads>& expected, const std::string& what) {
    for (std::uint32_t x = 0; x < Threads; ++x) Require(Output[x] == expected[x], "non-resident storage store: " + what + ": texel " + std::to_string(x) + " reads " + Hex(Output[x]) + ", expected " + Hex(expected[x]));
}

std::array<std::uint32_t, Threads> Uniform(std::uint32_t value) {
    std::array<std::uint32_t, Threads> values{};
    values.fill(value);
    return values;
}

std::array<std::uint32_t, Threads> TexelOffsets() {
    Surface probe(0u);
    for (std::size_t word = 0; word < SurfaceBytes / 4u; ++word) probe.texels[word] = static_cast<std::uint32_t>(word * 4u);
    Load(probe.texels);
    std::array<std::uint32_t, Threads> offsets{};
    std::copy(Output.begin(), Output.end(), offsets.begin());
    for (const auto offset : offsets) Require(offset < SurfaceBytes && offset % 4u == 0, "non-resident storage store: the offset probe read " + Hex(offset));
    return offsets;
}

void TestWholeUnit(std::uint32_t view) {
    const auto through = ViewName(view);
    Surface surface(Untouched);
    const auto address = AddressOf(surface.texels);
    Protect(surface.texels, 0, Block, false);
    Require(AgcDriver::GuestMemory::CommittedRanges(address, Block).empty(), "non-resident storage store: the decommitted block still has a committed page");
    Require(!AgcDriver::GuestMemory::CommittedRanges(address + Block, SurfaceBytes - Block, true).empty(), "non-resident storage store: the rest of the surface is not committed");
    Load(surface.texels, view);
    RequireTexels(Uniform(0u), "a load from a decommitted block" + through);
    Dispatch(surface.texels, StoreCode);
    Load(surface.texels, view);
    RequireTexels(Uniform(0u), "the next dispatch after a store to a decommitted block" + through);
    Load(surface.texels, view);
    RequireTexels(Uniform(0u), "a second dispatch after a store to a decommitted block" + through);
    Flush();
    Load(surface.texels, view);
    RequireTexels(Uniform(0u), "a load after the store was flushed" + through);
    Dispatch(surface.texels, StoreCode);
    Flush();
    Load(surface.texels, view);
    RequireTexels(Uniform(0u), "a load after a second store was flushed" + through);
    Flush();
    Require(AgcDriver::GuestMemory::CommittedRanges(address, Block).empty(), "non-resident storage store: a store committed a page of the decommitted block");
    Protect(surface.texels, 0, Block, true);
    Require(std::all_of(surface.texels, surface.texels + SurfaceBytes / 4u, [](std::uint32_t value) { return value == Untouched; }), "non-resident storage store: a store to a decommitted block reached guest memory");
}

void TestPartialUnit(const std::array<std::uint32_t, Threads>& offsets, std::uint32_t view) {
    const auto through = ViewName(view);
    const auto page = static_cast<std::size_t>(offsets[0]) & ~(Page - 1u);
    const auto decommitted = [&](std::uint32_t x) { return offsets[x] >= page && offsets[x] < page + Page; };
    std::array<std::uint32_t, Threads> beforeStore{};
    std::array<std::uint32_t, Threads> afterStore{};
    for (std::uint32_t x = 0; x < Threads; ++x) {
        beforeStore[x] = decommitted(x) ? 0u : Untouched;
        afterStore[x] = decommitted(x) ? 0u : Stored;
        Require(offsets[x] < Block, "non-resident storage store: texel " + std::to_string(x) + " is outside the first block");
    }
    Require(std::any_of(offsets.begin(), offsets.end(), [&](std::uint32_t offset) { return offset < page || offset >= page + Page; }), "non-resident storage store: every loaded texel is in one page");
    Surface surface(Untouched);
    const auto address = AddressOf(surface.texels);
    Protect(surface.texels, page, Page, false);
    Require(AgcDriver::GuestMemory::CommittedRanges(address + page, Page).empty(), "non-resident storage store: the decommitted page is still committed");
    Load(surface.texels, view);
    RequireTexels(beforeStore, "a load from a partly decommitted block" + through);
    Dispatch(surface.texels, StoreCode);
    Load(surface.texels, view);
    RequireTexels(afterStore, "the next dispatch after a store to a partly decommitted block" + through);
    Load(surface.texels);
    RequireTexels(afterStore, "a second dispatch after a store to a partly decommitted block" + through);
    Flush();
    Load(surface.texels, view);
    RequireTexels(afterStore, "a load after the store to a partly decommitted block was flushed" + through);
    Flush();
    Protect(surface.texels, page, Page, true);
    for (std::uint32_t x = 0; x < Threads; ++x) {
        const auto value = surface.texels[offsets[x] / 4u];
        Require(value == (decommitted(x) ? Untouched : Stored), "non-resident storage store: guest memory of texel " + std::to_string(x) + " holds " + Hex(value) + " after the flush");
    }
    std::vector<bool> loaded(SurfaceBytes / 4u, false);
    for (const auto offset : offsets) loaded[offset / 4u] = true;
    for (std::size_t word = 0; word < SurfaceBytes / 4u; ++word) Require(loaded[word] || surface.texels[word] == Untouched, "non-resident storage store: word " + std::to_string(word) + " outside the stored texels changed");
}

void TestStoreAfterCommit(const std::array<std::uint32_t, Threads>& offsets, std::uint32_t view, bool wholeUnit) {
    const auto through = ViewName(view) + (wholeUnit ? " (whole block)" : " (one page)");
    const auto begin = wholeUnit ? std::size_t{0} : static_cast<std::size_t>(offsets[0]) & ~(Page - 1u);
    const auto bytes = wholeUnit ? Block : Page;
    Surface surface(Untouched);
    Protect(surface.texels, begin, bytes, false);
    Dispatch(surface.texels, StoreCode);
    Protect(surface.texels, begin, bytes, true);
    Dispatch(surface.texels, StoreCode);
    Load(surface.texels, view);
    RequireTexels(Uniform(Stored), "a load after a store made once the decommitted pages were committed again" + through);
    Flush();
    for (std::uint32_t x = 0; x < Threads; ++x) {
        const auto value = surface.texels[offsets[x] / 4u];
        Require(value == Stored, "non-resident storage store: guest memory of texel " + std::to_string(x) + " holds " + Hex(value) + " after the store made once the pages were committed again was flushed" + through);
    }
}

void Run() {
    const auto offsets = TexelOffsets();
    for (const auto view : {Type2D, Type2DArray}) {
        TestWholeUnit(view);
        TestPartialUnit(offsets, view);
        TestStoreAfterCommit(offsets, view, true);
        TestStoreAfterCommit(offsets, view, false);
    }
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        Run();
        std::puts("non-resident storage store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
