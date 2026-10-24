#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
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

#ifndef _WIN32
namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}
#endif

namespace {

using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::LookupOutcomes;
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

alignas(256) constexpr std::array<std::uint32_t, 11> SampleSecondTexelCode{
    0x34060084, 0x7e2802ff, 0x3c400000, 0x7e2a02ff, 0x3b800000, 0xf09c0f08, 0x00610a14, 0xbf8c3f70,
    0xe0781000, 0x80000a03, 0xbf810000,
};

constexpr std::array<std::uint32_t, 4> PointSampler{0x00000092u, 0x00fff000u, 0u, 0u};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    explicit GuestBlock(bool watch, bool shared = false) : watched(watch) {
#ifdef _WIN32
        if (watched) {
            block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
            if (shared) {
                Require(block != nullptr, "cannot reserve shared test memory");
                const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, BlockBytes, nullptr);
                Require(section != nullptr, "cannot create shared test memory");
                GuestArena::GuestArenaMap_nid_postfix(block, BlockBytes, section, 0, PAGE_READWRITE);
                twin = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
                Require(twin != nullptr, "cannot reserve the second mapping of shared test memory");
                GuestArena::GuestArenaMap_nid_postfix(twin, BlockBytes, section, 0, PAGE_READWRITE);
                CloseHandle(section);
            } else if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
        } else {
            block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        Require(!shared, "shared test memory requires Windows");
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
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        if (twin != nullptr) {
            GuestArena::GuestArenaReset_nid_postfix(twin, BlockBytes);
            GuestArena::GuestArenaRelease_nid_postfix(twin, BlockBytes);
        }
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
    std::uint8_t* twin = nullptr;
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
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
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
    AgcDriver::GuestMemory::BumpCollectEpoch();
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
    const bool stillWatched = AgcDriver::GuestMemory::Watched(surface, SurfaceBytes);
    Require(image->GuestSnapshotValid() == !stillWatched, stillWatched ? "a fast-cleared upload of write-watched memory read its guest bytes" : "a fast-cleared upload of untracked memory kept no guest bytes to compare");
    const auto pendingVersion = image->Version();
    for (int repeat = 0; repeat < 3; ++repeat) {
        Require(image->Refresh(), "an unchanged pending image was uploaded again");
        Require(image->Version() == pendingVersion, "an unchanged pending image changed version");
        Require(StorageTexture::FindPending(surface, SurfaceBytes) == image, "an unchanged refresh wrote the pending image back");
    }
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
    if (!fill(0, KeyBytes, 0x5a)) {
        std::puts("the key range has no host import: pending key fills are not tested");
        return;
    }
    if (watched && !AgcDriver::GuestMemory::Watched(AddressOf(block), BlockBytes)) {
        std::puts("host imports are compared, not watched: the write-watched block runs as unwatched");
        watched = false;
    }
    readPending(DccKeys::Uncompressed, false, "a pending fill of the keys with 0x5a, which is no clear code");
    Require(fill(0, KeyBytes, 0x10), "the comp-to-single fill of the keys was not recorded");
    readPending(DccKeys::ClearSingle, false, "a pending comp-to-single fill of the keys");
    RequireKeys(keys, 0x10, "the keys after a pending comp-to-single fill landed");
    Require(fill(0, KeyBytes / 2, 0x00) && fill(KeyBytes / 2, KeyBytes / 2, 0x00), "the 0000 fills of the key halves were not recorded");
    readPending(DccKeys::Clear0000, false, "pending 0000 fills over both halves of the keys");
    RequireKeys(keys, 0x00, "the keys after pending 0000 fills over both halves landed");
    Require(fill(0, KeyBytes / 2, 0x00) && fill(KeyBytes / 2, KeyBytes / 2, 0x40), "the 0000 and 0001 fills of the key halves were not recorded");
    readPending(DccKeys::Uncompressed, false, "a pending 0000 fill and a pending 0001 fill over the halves of the keys");
    Require(fill(0, KeyBytes / 2, 0x40), "the 0001 fill of the first key half was not recorded");
    Dispatch(device, ClearKeysCode, clearTailData, false);
    readPending(DccKeys::Uncompressed, true, "a pending 0001 fill over half the keys and a kernel's 0000 store over the other half");
    Require(fill(0, KeyBytes, 0x40), "the 0001 fill of the keys was not recorded");
    Dispatch(device, ClearKeysCode, clearTailData, false);
    Require(fill(0, KeyBytes / 2, 0x40), "the 0001 fill of the first key half over the kernel's store was not recorded");
    readPending(DccKeys::Uncompressed, true, "a pending 0001 fill over half the keys, whose other half a kernel stored over an earlier 0001 fill");
    Require(fill(0, KeyBytes, 0x10) && fill(0, KeyBytes, 0x00), "the 0x10 and 0000 fills of the keys were not recorded");
    readPending(DccKeys::Clear0000, false, "a pending 0x10 fill of the keys overwritten by a pending 0000 fill");
    RequireKeys(keys, 0x00, "the keys after a pending 0x10 fill and a pending 0000 fill landed");
    Require(fill(0, KeyBytes, 0x10), "the 0x10 fill of the keys under a kernel's store was not recorded");
    Dispatch(device, ClearKeysCode, clearData, false);
    Require(fill(0, KeyBytes, 0x40), "the 0001 fill over the kernel's store was not recorded");
    readPending(DccKeys::Clear0001, false, "a pending 0001 fill of the keys over a kernel's 0000 store over a pending 0x10 fill");
    RequireKeys(keys, 0x40, "the keys after a 0x10 fill, a kernel's 0000 store and a 0001 fill landed");
    Require(fill(KeyBytes / 2, KeyBytes / 2, 0x10), "the 0x10 fill of the second key half was not recorded");
    device.WaitIdle();
    Require(fill(0, KeyBytes / 2, 0x00), "the 0000 fill of the first key half was not recorded");
    readPending(DccKeys::Uncompressed, false, "a pending 0000 fill over half the keys whose other half holds 0x10");
    Require(fill(KeyBytes / 2, KeyBytes / 2, 0xc0), "the 1111 fill of the second key half was not recorded");
    device.WaitIdle();
    Require(fill(0, KeyBytes / 2, 0xc0), "the 1111 fill of the first key half was not recorded");
    readPending(DccKeys::Clear1111, false, "a pending 1111 fill over half the keys whose other half holds 1111");
    RequireKeys(keys, 0xc0, "the keys after a pending 1111 fill over a half that holds 1111 landed");
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

void RunReusedKeys(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto* texels = block;
    auto* keys = block + KeysOffset;
    const auto surface = AddressOf(texels);
    std::memset(texels, 0x55, SurfaceBytes);
    std::memset(keys, 0x00, KeyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    std::vector<std::uint32_t> writeData(16, 0u);
    std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Dispatch(device, WriteCode, writeData);
    const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
    Require(image != nullptr && image->UploadedKeys() == DccKeys::Uncompressed, "reused keys: the first write did not store the keys uncompressed");
    RequireKeys(keys, 0xff, "reused keys: the keys after the first write");
    std::memset(keys, 0x5a, KeyBytes);
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test reused keys");
    device.WaitIdle();
    RequireKeys(keys, 0x5a, "the key bytes the title reused after the first write, after the image's write-back");
    Require(Count(texels, SurfaceBytes, 0xff) == Threads * 4u, "reused keys: the write-back lost the image's results");
}

void RunSingleKeys(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto* texels = block;
    auto* keys = block + KeysOffset;
    const auto surface = AddressOf(texels);
    std::vector<std::uint32_t> original(SurfaceBytes / 4);
    for (std::size_t index = 0; index < original.size(); ++index) original[index] = 0x01000000u + static_cast<std::uint32_t>(index);
    std::memcpy(texels, original.data(), SurfaceBytes);
    std::memset(keys, 0x10, KeyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    std::vector<std::uint32_t> writeData(16, 0u);
    std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Dispatch(device, WriteCode, writeData);
    const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
    Require(image != nullptr && image->UploadedKeys() == DccKeys::Uncompressed, "comp-to-single keys: the first write did not store the keys uncompressed");
    RequireKeys(keys, 0xff, "comp-to-single keys: the keys after the first write");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test comp-to-single keys");
    device.WaitIdle();
    const AgcDriver::Graphics::ColorTargetLayout layout(Side, Side, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4);
    std::size_t wrong = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            const auto offset = layout.Offset(x, y);
            std::uint32_t texel = 0;
            std::memcpy(&texel, texels + offset, 4);
            const auto expected = y == 0 && x < Threads ? 0xffffffffu : original[offset / 256 * 64];
            if (texel != expected) ++wrong;
        }
    }
    char message[256];
    std::snprintf(message, sizeof(message), "comp-to-single keys: %zu of %u written-back texels are neither the kernel's write nor the first texel of their 256-byte block", wrong, Side * Side);
    Require(wrong == 0, message);
}

void RunUniformSingleClear(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto* texels = block;
    auto* keys = block + KeysOffset;
    const auto surface = AddressOf(texels);
    const std::uint32_t color = 0x11223344u;
    std::memset(texels, 0x55, SurfaceBytes);
    for (std::size_t offset = 0; offset < SurfaceBytes; offset += 256) std::memcpy(texels + offset, &color, 4);
    std::memset(keys, 0x10, KeyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    std::vector<std::uint32_t> writeData(16, 0u);
    std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
    const auto uploads = AgcDriver::Graphics::ThreadLookupOutcomes().counts;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Dispatch(device, WriteCode, writeData);
    const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
    Require(image != nullptr && image->UploadedKeys() == DccKeys::Uncompressed, "uniform single-key clear: the first write did not store the keys uncompressed");
    const auto& uploaded = AgcDriver::Graphics::ThreadLookupOutcomes().counts;
    Require(uploaded[LookupOutcomes::UploadClear] == uploads[LookupOutcomes::UploadClear] + 1 && uploaded[LookupOutcomes::UploadCpu] == uploads[LookupOutcomes::UploadCpu], "uniform single-key clear: the first write read the surface and uploaded it instead of clearing it on the GPU");
    const bool stillWatched = AgcDriver::GuestMemory::Watched(surface, SurfaceBytes);
    Require(image->GuestSnapshotValid() == !stillWatched, stillWatched ? "uniform single-key clear: the upload of write-watched memory kept a guest snapshot, so the surface was read rather than cleared" : "uniform single-key clear: the upload of untracked memory kept no guest bytes to compare");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test uniform single-key clear");
    device.WaitIdle();
    RequireKeys(keys, 0xff, "uniform single-key clear: the keys after the write-back");
    const AgcDriver::Graphics::ColorTargetLayout layout(Side, Side, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4);
    std::size_t wrong = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            const auto offset = layout.Offset(x, y);
            std::uint32_t texel = 0;
            std::memcpy(&texel, texels + offset, 4);
            const auto expected = y == 0 && x < Threads ? 0xffffffffu : color;
            if (texel != expected) ++wrong;
        }
    }
    char message[256];
    std::snprintf(message, sizeof(message), "uniform single-key clear: %zu of %u written-back texels are neither the kernel's write nor the proved clear color 0x%08x", wrong, Side * Side, color);
    Require(wrong == 0, message);
}

void RunSingleKeysSampled(AgcDriver::VulkanDevice& device, std::uint8_t* block, std::uint8_t* outputBlock, const char* memory) {
    auto* texels = block;
    auto* keys = block + KeysOffset;
    auto* output = reinterpret_cast<std::uint32_t*>(outputBlock);
    const bool watched = AgcDriver::GuestMemory::Watched(AddressOf(texels), SurfaceBytes);
    const AgcDriver::Graphics::ColorTargetLayout layout(Side, Side, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4);
    const auto first = layout.Offset(1, 0) / 256 * 256;
    std::memset(texels, 0x00, SurfaceBytes);
    std::memset(texels + first, 0xff, 4);
    std::memset(keys, 0x10, KeyBytes);
    const auto texture = TextureDescriptor(texels, keys);
    const auto results = BufferDescriptor(output, Threads * 16u);
    std::vector<std::uint32_t> userData(24, 0u);
    std::copy(results.begin(), results.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::copy(PointSampler.begin(), PointSampler.end(), userData.begin() + 12);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const auto sample = [&](std::uint32_t expected, const char* when) {
        std::fill(output, output + Threads * 4u, 0xdeadbeefu);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Dispatch(device, SampleSecondTexelCode, userData);
        for (std::uint32_t index = 0; index < Threads * 4u; ++index) {
            char message[256];
            std::snprintf(message, sizeof(message), "comp-to-single keys sampled %s from %s memory: component %u of thread %u is 0x%08x, expected 0x%08x", when, memory, index % 4u, index / 4u, output[index], expected);
            Require(output[index] == expected, message);
        }
    };
    sample(0x3f800000u, "before the first texel of the block changed");
    Require(!watched || AgcDriver::GuestMemory::Watched(AddressOf(texels), SurfaceBytes), "comp-to-single keys sampled from write-watched memory: the first sampling left the surface unwatched");
    std::memset(texels + first, 0x00, 4);
    sample(0u, "after the first texel of the block changed");
}

void RunSingleKeysOverPendingImage(AgcDriver::VulkanDevice& device, std::uint8_t* block, std::uint8_t* outputBlock) {
#ifndef _WIN32
    const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    const auto decided = AgcDriver::Graphics::PrepareImportWatch(context);
    AgcDriver::Graphics::SetImportWatch(context, AgcDriver::Graphics::ImportWatch::Watch);
    struct Restore {
        const AgcDriver::Graphics::Context& context;
        AgcDriver::Graphics::ImportWatch decided;
        ~Restore() { AgcDriver::Graphics::SetImportWatch(context, decided); }
    } restore{context, decided};
#endif
    auto* texels = block;
    auto* keys = block + KeysOffset;
    auto* output = reinterpret_cast<std::uint32_t*>(outputBlock);
    const auto surface = AddressOf(texels);
    const AgcDriver::Graphics::ColorTargetLayout layout(Side, Side, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4);
    std::memset(texels, 0x00, SurfaceBytes);
    std::memset(texels + layout.Offset(1, 0) / 256 * 256, 0xff, 4);
    std::memset(keys, 0xff, KeyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    std::vector<std::uint32_t> writeData(16, 0u);
    std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
    const auto results = BufferDescriptor(output, Threads * 16u);
    std::vector<std::uint32_t> sampleData(24, 0u);
    std::copy(results.begin(), results.end(), sampleData.begin());
    std::copy(texture.begin(), texture.end(), sampleData.begin() + 4);
    std::copy(PointSampler.begin(), PointSampler.end(), sampleData.begin() + 12);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Dispatch(device, WriteRowCode, writeData);
    const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
    Require(image != nullptr && image->UploadedKeys() == DccKeys::Uncompressed, "comp-to-single keys over a pending storage image: the row write left no image pending under uncompressed keys");
    Require(AgcDriver::GuestMemory::Watched(surface, SurfaceBytes), "comp-to-single keys over a pending storage image: the image's host import left the surface unwatched");
    std::memset(keys, 0x10, KeyBytes);
    std::fill(output, output + Threads * 4u, 0xdeadbeefu);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Dispatch(device, SampleSecondTexelCode, sampleData);
    for (std::uint32_t index = 0; index < Threads * 4u; ++index) {
        char message[256];
        std::snprintf(message, sizeof(message), "comp-to-single keys stored over a pending storage image: component %u of thread %u is 0x%08x, expected 0x3f800000", index % 4u, index / 4u, output[index]);
        Require(output[index] == 0x3f800000u, message);
    }
}

void RunSampledKeysBehindKernel(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto* texels = block;
    auto* keys = block + KeysOffset;
    std::memset(texels, 0x55, SurfaceBytes);
    for (std::size_t i = 0; i < KeyBytes; i += 4) {
        keys[i] = 0x00;
        keys[i + 1] = 0x00;
        keys[i + 2] = 0x80;
        keys[i + 3] = 0x3f;
    }
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(texture);
    std::vector<std::uint32_t> clearData(16, 0u);
    const auto head = BufferDescriptor(keys, KeyBytes / 2);
    std::copy(head.begin(), head.end(), clearData.begin());
    std::vector<std::uint32_t> clearTailData(16, 0u);
    const auto tail = BufferDescriptor(keys + KeyBytes / 2, KeyBytes / 2);
    std::copy(tail.begin(), tail.end(), clearTailData.begin());
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Require(AgcDriver::Graphics::TextureClearKeys(resource, SurfaceBytes) == DccKeys::Uncompressed, "sampled keys behind a kernel: key bytes that are no keys were not read as stored");
    Dispatch(device, ClearKeysCode, clearData, false);
    Dispatch(device, ClearKeysCode, clearTailData, false);
    const auto read = AgcDriver::Graphics::TextureClearKeys(resource, SurfaceBytes);
    char message[256];
    std::snprintf(message, sizeof(message), "a sampled texture whose keys a recorded kernel stores 0000 over read %s before the kernel ran", AgcDriver::Graphics::DccKeysName(read));
    Require(read == DccKeys::Clear0000, message);
    device.WaitIdle();
    RequireKeys(keys, 0x00, "sampled keys behind a kernel: the keys after the kernel");
}

void RunUniformKeysBehindKernel(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto* texels = block;
    auto* keys = block + KeysOffset;
    std::memset(texels, 0x55, SurfaceBytes);
    std::memset(keys, 0xff, KeyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(texture);
    std::vector<std::uint32_t> clearData(16, 0u);
    const auto head = BufferDescriptor(keys, KeyBytes / 2);
    std::copy(head.begin(), head.end(), clearData.begin());
    std::vector<std::uint32_t> clearTailData(16, 0u);
    const auto tail = BufferDescriptor(keys + KeyBytes / 2, KeyBytes / 2);
    std::copy(tail.begin(), tail.end(), clearTailData.begin());
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Require(AgcDriver::Graphics::TextureClearKeys(resource, SurfaceBytes) == DccKeys::Uncompressed, "uniform sampled keys behind a kernel: the keys before the kernel were not read uncompressed");
    Dispatch(device, ClearKeysCode, clearData, false);
    Dispatch(device, ClearKeysCode, clearTailData, false);
    const auto read = AgcDriver::Graphics::TextureClearKeys(resource, SurfaceBytes);
    char message[256];
    std::snprintf(message, sizeof(message), "a sampled texture whose uniform keys a recorded kernel stores 0000 over read %s before the kernel ran", AgcDriver::Graphics::DccKeysName(read));
    Require(read == DccKeys::Clear0000, message);
    device.WaitIdle();
    RequireKeys(keys, 0x00, "uniform sampled keys behind a kernel: the keys after the kernel");
}

void RunUntrackedReuse(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    const auto address = AddressOf(block);
    std::memset(block, 0x55, BlockBytes);
    auto descriptor = TextureDescriptor(block, nullptr);
    descriptor[2] = ((Side - 1u) >> 2u) | ((2 * Side - 1u) << 14u);
    descriptor[6] = 0;
    descriptor[7] = 0;
    std::vector<std::uint32_t> userData(16, 0u);
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Dispatch(device, WriteCode, userData);
    const auto image = StorageTexture::FindPending(address, BlockBytes);
    Require(image != nullptr && image->GuestBytes() == BlockBytes, "the two-block image has no pending results");
    Require(!AgcDriver::GuestMemory::Watched(address, BlockBytes), "the imported test image is still write-watched");
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto version = image->Version();
    for (int repeat = 0; repeat < 3; ++repeat) {
        Require(image->Refresh() && image->Version() == version, "an unchanged untracked image was uploaded again");
        Require(Count(block, BlockBytes, 0x55) == BlockBytes, "refresh wrote GPU results into unchanged guest memory");
    }
    std::memset(block + SurfaceBytes, 0x33, SurfaceBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(!image->Refresh(), "an untracked CPU edit was missed");
    const auto editedVersion = image->Version();
    Require(image->Refresh() && image->Version() == editedVersion, "the CPU-edited image was uploaded twice");
    StorageTexture::FlushPending(address, BlockBytes, nullptr, "test untracked reuse");
    device.WaitIdle();
    Require(Count(block, SurfaceBytes, 0xff) == Threads * 4u, "refresh lost the unchanged block's GPU results");
    Require(Count(block, SurfaceBytes, 0x55) == SurfaceBytes - Threads * 4u, "refresh changed unrelated texels");
    Require(Count(block + SurfaceBytes, SurfaceBytes, 0x33) == SurfaceBytes, "write-back overwrote the CPU-edited block");

    Dispatch(device, WriteCode, userData);
    Require(image->Refresh(), "a freshly dispatched image did not retain its snapshot");
    std::memset(block, 0x77, SurfaceBytes);
    image->WriteBack();
    device.WaitIdle();
    Require(Count(block, SurfaceBytes, 0x77) == SurfaceBytes, "write-back without refresh overwrote a CPU edit");
    image->Refresh();
    Require(image->Refresh(), "the image did not stabilize after write-back");
    AgcDriver::GuestMemory::StoreOwnBytes(address, SurfaceBytes, [&] { std::memset(block, 0x22, SurfaceBytes); });
    Require(!image->Refresh(), "a driver store within the comparison epoch was missed");
    Require(image->Refresh(), "the driver-edited image was uploaded twice");
    const std::array<std::uint32_t, 4> pattern{0x44444444u, 0x44444444u, 0x44444444u, 0x44444444u};
    Require(device.FillBuffer(address, SurfaceBytes, pattern), "the imported GPU fill was not recorded");
    Require(!image->Refresh(), "a pending GPU store within the comparison epoch was missed");
    device.WaitIdle();
    Require(Count(block, SurfaceBytes, 0x44) == SurfaceBytes, "the GPU fill did not reach guest memory");
    Require(image->Refresh(), "the GPU-edited image was uploaded twice");
}

#ifndef _WIN32
// An import window stamps the pages whose write protection it saw resolved as a possible CPU
// store. A driver that pins imported pages for writing (NVIDIA on Linux) resolves it without
// storing anything, which a store of the byte already there reproduces on any device.
void RunImportWindow(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    auto* texels = block;
    auto* keys = block + KeysOffset;
    const auto surface = AddressOf(texels);
    std::memset(texels, 0x55, SurfaceBytes);
    std::memset(keys, 0x00, KeyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    std::vector<std::uint32_t> writeData(16, 0u);
    std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Dispatch(device, WriteCode, writeData);
    const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
    Require(image != nullptr, "import window: the written surface has no pending image");
    if (!AgcDriver::GuestMemory::Watched(surface, SurfaceBytes)) {
        std::puts("host imports are compared, not watched: the import window is not tested");
        StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
        device.WaitIdle();
        return;
    }
    const auto storeInWindow = [&](std::uint8_t value) {
        auto* page = reinterpret_cast<volatile std::uint8_t*>(texels);
        Require(AgcDriver::GuestMemory::ImportWatched(surface, SurfaceBytes, [&] {
            page[0] = value;
            return true;
        }), "import window: the import failed");
    };
    const auto pendingVersion = image->Version();
    storeInWindow(0x55);
    Require(image->Refresh(), "import window: a pending image was uploaded again after an import that stored nothing new");
    Require(image->Version() == pendingVersion && StorageTexture::FindPending(surface, SurfaceBytes) == image, "import window: an import that stored nothing new wrote the pending image back");
    storeInWindow(0x66);
    Require(!image->Refresh(), "import window: a CPU store within an import window was missed");
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test");
    device.WaitIdle();
}
#endif

#ifdef _WIN32
void RunHintedImport(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    using namespace AgcDriver::GuestMemory;
    AgcDriver::Graphics::SetHostImportDeferredSettles(false);
    struct Restore {
        ~Restore() { AgcDriver::Graphics::SetHostImportDeferredSettles(true); }
    } restore;
    const auto address = AddressOf(block);
    std::memset(block, 0x55, BlockBytes);
    auto descriptor = TextureDescriptor(block, nullptr);
    descriptor[2] = ((Side - 1u) >> 2u) | ((2 * Side - 1u) << 14u);
    descriptor[6] = 0;
    descriptor[7] = 0;
    std::vector<std::uint32_t> userData(16, 0u);
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4);
    std::lock_guard gpu(GpuMutex());
    BumpCollectEpoch();
    Dispatch(device, WriteCode, userData);
    const auto image = StorageTexture::FindPending(address, BlockBytes);
    Require(image != nullptr && image->GuestBytes() == BlockBytes, "hinted import: the two-block image has no pending results");
    if (Watched(address, BlockBytes)) {
        std::puts("the write-watched image stayed watched: its untracked hint is not tested");
        StorageTexture::FlushPending(address, BlockBytes, nullptr, "test hinted import");
        device.WaitIdle();
        return;
    }
    StorageTexture::FlushPending(address, BlockBytes, nullptr, "test hinted import");
    device.WaitIdle();
    for (int settle = 0; settle < 2; ++settle) {
        BumpCollectEpoch();
        image->Refresh();
    }
    const auto version = image->Version();
    const auto compares = StorageTexture::UntrackedCompares();
    for (int epoch = 0; epoch < 50; ++epoch) {
        BumpCollectEpoch();
        Require(image->Refresh() && image->Version() == version, "hinted import: an unchanged image was uploaded again");
    }
    char message[192];
    std::snprintf(message, sizeof(message), "hinted import: 50 collect epochs without a store compared the image %llu times", static_cast<unsigned long long>(StorageTexture::UntrackedCompares() - compares));
    Require(StorageTexture::UntrackedCompares() == compares, message);
    block[SurfaceBytes + 0x100] = 0x33;
    BumpCollectEpoch();
    Require(!image->Refresh() && image->Version() != version, "hinted import: a CPU store into the imported image was missed");
    const auto stored = StorageTexture::UntrackedCompares();
    Require(stored > compares, "hinted import: the CPU store was found without a compare");
    BumpCollectEpoch();
    Require(image->Refresh() && StorageTexture::UntrackedCompares() - stored <= 1, "hinted import: the stored image did not settle");
    const auto settled = image->Version();
    block[SurfaceBytes + 0x200] = 0x44;
    std::array<void*, 1> marked{};
    std::size_t count = marked.size();
    Require(GuestArena::GuestArenaCollectWrites_nid_postfix(static_cast<std::uintptr_t>(address + SurfaceBytes), 4096, marked.data(), &count, true) && count == 1, "hinted import: the store to hide was not marked");
    MarkWritten(address + SurfaceBytes + 0x200, 1);
    BumpCollectEpoch();
    Require(!image->Refresh() && image->Version() != settled, "hinted import: a recorded store without a write-watch mark was missed");
    Dispatch(device, WriteCode, userData);
    Require(StorageTexture::FindPending(address, BlockBytes) == image, "hinted import: the second write left no pending results");
    BumpCollectEpoch();
    image->Refresh();
    const auto pendingVersion = image->Version();
    const auto beforeRewrite = StorageTexture::UntrackedCompares();
    auto* rewritten = reinterpret_cast<volatile std::uint8_t*>(block + SurfaceBytes + 0x2000);
    *rewritten = *rewritten;
    BumpCollectEpoch();
    Require(image->Refresh() && image->Version() == pendingVersion, "hinted import: a store of the bytes already there uploaded the image");
    Require(StorageTexture::UntrackedCompares() - beforeRewrite == 1, "hinted import: a store of the bytes already there did not cost exactly one compare");
    Require(StorageTexture::FindPending(address, BlockBytes) == image, "hinted import: a store of the bytes already there dropped the pending results");
    BumpCollectEpoch();
    Require(image->Refresh() && StorageTexture::UntrackedCompares() - beforeRewrite == 1, "hinted import: the image was compared again without a store");
    StorageTexture::FlushPending(address, BlockBytes, nullptr, "test hinted import");
    device.WaitIdle();
    Require(Count(block, SurfaceBytes, 0xff) == Threads * 4u, "hinted import: the pending results were lost");
    Require(block[SurfaceBytes + 0x100] == 0x33 && block[SurfaceBytes + 0x200] == 0x44, "hinted import: the write-back lost the CPU store");
}

void RunSharedImport(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    using namespace AgcDriver::GuestMemory;
    const auto address = AddressOf(block);
    std::lock_guard gpu(GpuMutex());
    std::memset(block, 0x11, BlockBytes);
    const std::array<std::uint32_t, 4> pattern{0x22222222u, 0x22222222u, 0x22222222u, 0x22222222u};
    Require(device.FillBuffer(address, SurfaceBytes, pattern), "shared memory was not imported");
    device.WaitIdle();
    Require(Watched(address, BlockBytes), "importing a separate alias disabled guest write tracking");
    auto* alias = static_cast<std::uint8_t*>(GuestArena::GuestArenaMapAlias_nid_postfix(address, BlockBytes));
    const auto beforeAliasWrite = CollectWritesUncached(address, BlockBytes);
    alias[SurfaceBytes] = 0x11;
    CollectWritesUncached(address, BlockBytes);
    Require(UnchangedSince(address + SurfaceBytes, SurfaceBytes, beforeAliasWrite), "a driver alias write dirtied the protected guest view");
    GuestArena::GuestArenaUnmapAlias_nid_postfix(alias);
    for (int repeat = 0; repeat < 8; ++repeat) {
        const auto before = CollectWritesUncached(address, BlockBytes);
        Require(device.FillBuffer(address, SurfaceBytes, pattern), "shared memory import was lost");
        device.WaitIdle();
        CollectWritesUncached(address, BlockBytes);
        Require(UnchangedSince(address + SurfaceBytes, SurfaceBytes, before), "GPU access dirtied untouched guest pages");
        const auto current = CollectWritesUncached(address, BlockBytes);
        block[SurfaceBytes] = static_cast<std::uint8_t>(repeat);
        CollectWritesUncached(address, BlockBytes);
        Require(!UnchangedSince(address + SurfaceBytes, SurfaceBytes, current), "CPU write after GPU completion was missed");
        block[SurfaceBytes] = 0x11;
    }
}
#endif

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_DRAW", "1");
#else
        setenv("APS5_PROFILE_DRAW", "1", 1);
#endif
        std::optional<GuestBlock> block;
        std::optional<GuestBlock> watched;
        std::optional<GuestBlock> untracked;
        std::optional<GuestBlock> shared;
        std::optional<GuestBlock> window;
        std::optional<GuestBlock> pendingSingle;
        std::optional<GuestBlock> hinted;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        block.emplace(false);
        Run(*device, block->Data());
        untracked.emplace(false);
        RunUntrackedReuse(*device, untracked->Data());
        std::optional<GuestBlock> reused;
        reused.emplace(false);
        RunReusedKeys(*device, reused->Data());
        std::optional<GuestBlock> single;
        single.emplace(false);
        RunSingleKeys(*device, single->Data());
        std::optional<GuestBlock> uniformClear;
        uniformClear.emplace(false);
        RunUniformSingleClear(*device, uniformClear->Data());
        std::optional<GuestBlock> sampled;
        sampled.emplace(false);
        std::optional<GuestBlock> sampledOutput;
        sampledOutput.emplace(false);
        RunSingleKeysSampled(*device, sampled->Data(), sampledOutput->Data(), "unwatched");
        std::optional<GuestBlock> behind;
        behind.emplace(false);
        RunSampledKeysBehindKernel(*device, behind->Data());
        std::optional<GuestBlock> uniform;
        uniform.emplace(false);
        RunUniformKeysBehindKernel(*device, uniform->Data());
#ifdef _WIN32
        shared.emplace(true, true);
        RunSharedImport(*device, shared->Data());
        Run(*device, shared->Data());
#endif
        if (AgcDriver::GuestMemory::WriteWatched()) {
            watched.emplace(true);
            Run(*device, watched->Data());
            std::optional<GuestBlock> sampledWatched;
            sampledWatched.emplace(true);
            RunSingleKeysSampled(*device, sampledWatched->Data(), sampledOutput->Data(), "write-watched");
            std::optional<GuestBlock> uniformClearWatched;
            uniformClearWatched.emplace(true);
            RunUniformSingleClear(*device, uniformClearWatched->Data());
#ifdef _WIN32
            pendingSingle.emplace(true, true);
#else
            pendingSingle.emplace(true);
#endif
            RunSingleKeysOverPendingImage(*device, pendingSingle->Data(), sampledOutput->Data());
#ifndef _WIN32
            window.emplace(true);
            RunImportWindow(*device, window->Data());
#else
            hinted.emplace(true);
            RunHintedImport(*device, hinted->Data());
#endif
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
