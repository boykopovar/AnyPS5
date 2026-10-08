#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
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
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = 65536;
constexpr std::size_t KeyBytes = SurfaceBytes / 256;
constexpr std::size_t KeysOffset = SurfaceBytes;
constexpr std::size_t CopiedKeysOffset = SurfaceBytes + 4096;
constexpr std::size_t BlockBytes = 2 * 65536;

alignas(256) constexpr std::array<std::uint32_t, 8> WriteCode{
    0x7e020280, 0x7e0402f2, 0x7e0602f2, 0x7e0802f2, 0x7e0a02f2, 0xf0201f08, 0x00010200, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> WriteRowCode{
    0x7e020281, 0x7e0402f2, 0x7e0602f2, 0x7e0802f2, 0x7e0a02f2, 0xf0201f08, 0x00010200, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> CopyKeysCode{
    0x34020083, 0xe0341000, 0x80000201, 0xbf8c3f70, 0xe0741000, 0x80010201, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> ClearKeysCode{
    0x34020083, 0x7e040280, 0x7e060280, 0xe0741000, 0x80000201, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    explicit GuestBlock(bool watch) : watched(watch) {
#ifdef _WIN32
        if (watched) {
            block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
            if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
        } else {
            block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            constexpr std::uintptr_t alignment = 65536;
            void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mapped != MAP_FAILED) {
                const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
                const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
                if (aligned != begin) munmap(mapped, aligned - begin);
                if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
                block = reinterpret_cast<std::uint8_t*>(aligned);
                GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
            }
        } else {
            block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
        }
#endif
        Require(block != nullptr, "DCC keys first write: cannot allocate the guest block");
        Require(!watched || AgcDriver::GuestMemory::Watched(AddressOf(block), BlockBytes), "DCC keys first write: the watched guest block is not write-watched");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
            GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
        } else {
            VirtualFree(block, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(block, BlockBytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
        } else {
            std::free(block);
        }
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    bool watched;
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels, const std::uint8_t* keys) {
    const auto address = AddressOf(texels);
    const auto meta = AddressOf(keys) >> 8u;
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888Unorm << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileRenderTarget64KB << 20u) | (Type2D << 28u),
        0u,
        0u,
        (1u << 20u) | (1u << 21u) | (static_cast<std::uint32_t>(meta & 0xffu) << 24u),
        static_cast<std::uint32_t>(meta >> 8u),
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, bool wait = true) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    if (wait) device.WaitIdle();
}

std::size_t Count(const std::uint8_t* bytes, std::size_t size, std::uint8_t value) {
    return static_cast<std::size_t>(std::count(bytes, bytes + size, value));
}

void RequireKeys(const std::uint8_t* keys, std::uint8_t expected, const std::string& what) {
    const auto found = Count(keys, KeyBytes, expected);
    char message[256];
    std::snprintf(message, sizeof(message), "%s: %zu of %zu key bytes are 0x%02x (key 0 is 0x%02x)", what.c_str(), found, KeyBytes, expected, keys[0]);
    Require(found == KeyBytes, message);
}

void Run(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    bool watched = AgcDriver::GuestMemory::Watched(AddressOf(block), BlockBytes);
    auto* texels = block;
    auto* keys = block + KeysOffset;
    auto* copied = block + CopiedKeysOffset;
    const auto surface = AddressOf(texels);
    std::memset(texels, 0x55, SurfaceBytes);
    std::memset(keys, 0x00, KeyBytes);
    std::memset(copied, 0xaa, KeyBytes);
    const auto texture = TextureDescriptor(texels, keys);
    std::vector<std::uint32_t> writeData(16, 0u);
    std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
    std::vector<std::uint32_t> copyData(16, 0u);
    const auto source = BufferDescriptor(keys, KeyBytes);
    const auto destination = BufferDescriptor(copied, KeyBytes);
    std::copy(source.begin(), source.end(), copyData.begin());
    std::copy(destination.begin(), destination.end(), copyData.begin() + 4);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Dispatch(device, WriteCode, writeData);
    Dispatch(device, CopyKeysCode, copyData);
    RequireKeys(copied, 0xff, "a kernel reading the keys after the first write of a 0000-cleared surface");
    Require(AgcDriver::Graphics::CurrentDccKeys(AddressOf(copied), SurfaceBytes) == DccKeys::Uncompressed, "the copied keys do not read uncompressed");
    const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
    Require(image != nullptr && image->Descriptor().dccAddress == AddressOf(keys), "the written surface has no pending image under its keys");
    Require(AgcDriver::Graphics::StorageImageServesKeys(*image, AddressOf(copied)), "the written image does not serve the copied keys");
    Require(image->UploadedKeys() == DccKeys::Uncompressed && image->FilledKeys() == DccKeys::Uncompressed, "the written image still holds the clear it was uploaded under");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    RequireKeys(keys, 0xff, "the keys after the write-back");
    const auto requireTexels = [&](const char* what) {
        const auto written = Count(texels, SurfaceBytes, 0xff);
        const auto cleared = Count(texels, SurfaceBytes, 0x00);
        char text[320];
        std::snprintf(text, sizeof(text), "%s: the write-back stored %zu written and %zu cleared bytes of %zu, expected %u written", what, written, cleared, SurfaceBytes, Threads * 4u);
        Require(written == Threads * 4u && cleared == SurfaceBytes - written, text);
    };
    requireTexels("the first write");
    const auto fillKeys = [&](std::uint8_t key, DccKeys code) {
        const std::uint32_t word = key * 0x01010101u;
        const std::array<std::uint32_t, 4> pattern{word, word, word, word};
        Require(StorageTexture::NoteKeysFill(AddressOf(keys), KeyBytes, key) == 1, "the key fill did not cover the surface");
        if (!device.FillBuffer(AddressOf(keys), KeyBytes, pattern)) {
            device.WaitIdle();
            const std::vector<std::byte> filled(KeyBytes, std::byte{key});
            AgcDriver::GuestMemory::Write(AddressOf(keys), filled);
        }
        AgcDriver::Graphics::NoteKeysFillOnGpu(AddressOf(keys), KeyBytes, code);
        StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
        device.WaitIdle();
    };
    Dispatch(device, WriteCode, writeData);
    fillKeys(0x00, DccKeys::Clear0000);
    RequireKeys(keys, 0x00, "the keys of a fill made after a write, after the write-back of that write");
    Dispatch(device, WriteCode, writeData, false);
    Require(image->UploadedKeys() == DccKeys::Uncompressed, "the first write after a key fill did not store the keys uncompressed");
    fillKeys(0x00, DccKeys::Clear0000);
    RequireKeys(keys, 0x00, "the keys of a fill recorded right after a first write, after its write-back");
    Dispatch(device, WriteCode, writeData);
    std::vector<std::uint32_t> clearData(16, 0u);
    std::copy(source.begin(), source.end(), clearData.begin());
    Dispatch(device, ClearKeysCode, clearData, false);
    Dispatch(device, WriteRowCode, writeData);
    AgcDriver::GuestMemory::CollectWritesUncached(AddressOf(keys), KeyBytes);
    const auto proved = AgcDriver::Graphics::ProvedClearKeys(image->Descriptor(), image->GuestBytes(), image->KeyProof());
    char message[256];
    std::snprintf(message, sizeof(message), "a write after a kernel stored 0000 over written keys: the keys read %s against uploaded %s, so the next key read drops the write", AgcDriver::Graphics::DccKeysName(proved), AgcDriver::Graphics::DccKeysName(image->UploadedKeys()));
    Require(proved == image->UploadedKeys() && proved == DccKeys::Uncompressed, message);
    Require(StorageTexture::FindPending(surface, SurfaceBytes) == image, "the write after a kernel key clear is not pending");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    RequireKeys(keys, 0xff, "the keys of a write after a kernel key clear, after its write-back");
    requireTexels("a write after a kernel key clear over an earlier write");
    auto* recorder = AgcDriver::Graphics::Recorder::Active();
    Require(recorder != nullptr, "DCC keys first write: no active recorder");
    const auto readPending = [&](DccKeys expected, bool wait, const char* what) {
        const auto read = image->ProvedKeys();
        const bool waited = !recorder->PendingWriteOverlaps(AddressOf(keys), KeyBytes);
        char text[320];
        std::snprintf(text, sizeof(text), "%s: the keys read %s (expected %s) and the read %s", what, AgcDriver::Graphics::DccKeysName(read), AgcDriver::Graphics::DccKeysName(expected), waited ? "waited for the GPU" : "did not wait");
        Require(read == expected && waited == wait, text);
        device.WaitIdle();
    };
    std::vector<std::uint32_t> clearTailData(16, 0u);
    const auto tail = BufferDescriptor(keys + KeyBytes / 2, KeyBytes / 2);
    std::copy(tail.begin(), tail.end(), clearTailData.begin());
    fillKeys(0x00, DccKeys::Clear0000);
    Dispatch(device, WriteCode, writeData, false);
    Dispatch(device, ClearKeysCode, clearTailData, false);
    readPending(DccKeys::Uncompressed, false, "a kernel's 0000 store over half the keys after the first write's uncompressed store");
    Require(AgcDriver::Graphics::CurrentDccKeys(AddressOf(keys), SurfaceBytes) == DccKeys::Mixed, "the keys after a kernel's 0000 store over half of them are not mixed");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    requireTexels("a first write followed by a kernel's 0000 store over half the keys");
    fillKeys(0x00, DccKeys::Clear0000);
    image->Refresh();
    Require(image->UploadedKeys() == DccKeys::Clear0000 && image->FilledKeys() == DccKeys::Uncompressed, "the refresh after a 0000 key fill did not clear the image");
    fillKeys(0x40, DccKeys::Clear0001);
    Require(image->FilledKeys() == DccKeys::Clear0001, "the 0001 key fill over the cleared image was not noted on it");
    Dispatch(device, ClearKeysCode, clearData);
    const auto overFill = image->ProvedKeys();
    std::snprintf(message, sizeof(message), "a key read after a kernel's 0000 store over a 0001 key fill: the keys read %s and the image keeps a %s fill (expected 0000 and none)", AgcDriver::Graphics::DccKeysName(overFill), AgcDriver::Graphics::DccKeysName(image->FilledKeys()));
    Require(overFill == DccKeys::Clear0000 && image->FilledKeys() == DccKeys::Uncompressed, message);
    std::memset(copied, 0xaa, KeyBytes);
    fillKeys(0x40, DccKeys::Clear0001);
    Dispatch(device, ClearKeysCode, clearData);
    Dispatch(device, WriteCode, writeData);
    Dispatch(device, CopyKeysCode, copyData);
    RequireKeys(copied, 0xff, "a kernel reading the keys after a 0001 key fill, a kernel's 0000 store over them and a write");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
    requireTexels("a write after a 0001 key fill and a kernel's 0000 store over the keys");
    const auto fill = [&](std::size_t offset, std::size_t bytes, std::uint8_t key) {
        const std::uint32_t word = key * 0x01010101u;
        const std::array<std::uint32_t, 4> pattern{word, word, word, word};
        return device.FillBuffer(AddressOf(keys + offset), bytes, pattern);
    };
    if (!fill(0, KeyBytes, 0x10)) {
        std::puts("the key range has no host import: pending key fills are not tested");
        return;
    }
    if (watched && !AgcDriver::GuestMemory::Watched(AddressOf(block), BlockBytes)) {
        std::puts("host imports are compared, not watched: the write-watched block runs as unwatched");
        watched = false;
    }
    readPending(DccKeys::Uncompressed, false, "a pending fill of the keys with 0x10, which is no clear code");
    Require(fill(0, KeyBytes / 2, 0x00) && fill(KeyBytes / 2, KeyBytes / 2, 0x00), "the 0000 fills of the key halves were not recorded");
    readPending(DccKeys::Clear0000, true, "pending 0000 fills over both halves of the keys");
    Require(fill(0, KeyBytes / 2, 0x40), "the 0001 fill of the first key half was not recorded");
    Dispatch(device, ClearKeysCode, clearTailData, false);
    readPending(DccKeys::Uncompressed, true, "a pending 0001 fill over half the keys and a kernel's 0000 store over the other half");
    Require(fill(0, KeyBytes, 0x10) && fill(0, KeyBytes, 0x00), "the 0x10 and 0000 fills of the keys were not recorded");
    readPending(DccKeys::Clear0000, true, "a pending 0x10 fill of the keys overwritten by a pending 0000 fill");
    Require(fill(KeyBytes / 2, KeyBytes / 2, 0x10), "the 0x10 fill of the second key half was not recorded");
    device.WaitIdle();
    Require(fill(0, KeyBytes / 2, 0x00), "the 0000 fill of the first key half was not recorded");
    readPending(DccKeys::Uncompressed, false, "a pending 0000 fill over half the keys whose other half holds 0x10");
    Require(fill(0, KeyBytes, 0x10), "the 0x10 fill of the keys was not recorded");
    device.WaitIdle();
    Require(fill(0, KeyBytes / 2, 0x00) && fill(KeyBytes / 2, KeyBytes / 2, 0x00), "the 0000 fills of the key halves were not recorded");
    image->Refresh();
    std::snprintf(message, sizeof(message), "a refresh while 0000 fills over both halves of the keys are pending uploaded the image under %s keys (key 0 is 0x%02x)", AgcDriver::Graphics::DccKeysName(image->UploadedKeys()), keys[0]);
    Require(image->UploadedKeys() == DccKeys::Clear0000, message);
    if (watched) {
        const auto version = image->Version();
        image->Refresh();
        Require(image->Version() == version, "the refresh after the one that cleared the image uploaded it again");
    }
    device.WaitIdle();
    fillKeys(0x40, DccKeys::Clear0001);
    image->MarkDirty();
    device.WaitIdle();
    RequireKeys(keys, 0x40, "a write marked after a 0001 key fill over a cleared image, before a refresh saw the fill");
}

}

int main() {
    try {
        std::optional<GuestBlock> block;
        std::optional<GuestBlock> watched;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        block.emplace(false);
        Run(*device, block->Data());
        if (AgcDriver::GuestMemory::WriteWatched()) {
            watched.emplace(true);
            Run(*device, watched->Data());
        } else {
            std::puts("guest memory has no write watch: a write-watched surface is not tested");
        }
        std::puts("DCC keys first write tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
