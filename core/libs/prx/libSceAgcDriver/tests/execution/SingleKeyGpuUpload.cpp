#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
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
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::Recorder;
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
constexpr std::size_t BlockBytes = 2 * 65536;
constexpr std::uint32_t BlockThreads = SurfaceBytes / 256;
constexpr std::uint32_t UniformTexel = 0x11223344u;
constexpr std::uint32_t FillTexel = 0x5a3c0f81u;

alignas(256) constexpr std::array<std::uint32_t, 8> WriteCode{
    0x7e020280, 0x7e0402f2, 0x7e0602f2, 0x7e0802f2, 0x7e0a02f2, 0xf0201f08, 0x00010200, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> MixedBlocksCode{
    0x34020088, 0x380400ff, 0x80000000, 0xe0701000, 0x80000201, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> UniformBlocksCode{
    0x34020088, 0x7e0402ff, UniformTexel, 0xe0701000, 0x80000201, 0xbf810000,
};

enum class Writer { Mixed, Uniform, Fill };

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        Require(block != nullptr, "single-key GPU upload: cannot reserve shared test memory");
        const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, BlockBytes, nullptr);
        Require(section != nullptr, "single-key GPU upload: cannot create shared test memory");
        GuestArena::GuestArenaMap_nid_postfix(block, BlockBytes, section, 0, PAGE_READWRITE);
        twin = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        Require(twin != nullptr, "single-key GPU upload: cannot reserve the second mapping of shared test memory");
        GuestArena::GuestArenaMap_nid_postfix(twin, BlockBytes, section, 0, PAGE_READWRITE);
        CloseHandle(section);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "single-key GPU upload: cannot map the guest block");
        const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
        const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
        if (aligned != begin) munmap(mapped, aligned - begin);
        if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
#endif
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(twin, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(twin, BlockBytes);
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
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

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, std::uint32_t threads) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

std::uint32_t BlockTexel(Writer writer, std::size_t block) {
    if (writer == Writer::Mixed) return 0x80000000u | static_cast<std::uint32_t>(block);
    return writer == Writer::Uniform ? UniformTexel : FillTexel;
}

const char* WriterName(Writer writer) {
    return writer == Writer::Mixed ? "a kernel storing a different texel per block" : writer == Writer::Uniform ? "a kernel storing one texel in every block" : "a pending word fill";
}

int Run(AgcDriver::VulkanDevice& device, Writer writer, bool off) {
    GuestBlock guest;
    auto* texels = guest.Data();
    auto* keys = texels + KeysOffset;
    const auto surface = AddressOf(texels);
    const std::string what = WriterName(writer);
    std::memset(texels, 0x55, SurfaceBytes);
    std::memset(keys, 0x10, KeyBytes);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    std::vector<std::uint32_t> writeData(16, 0u);
    std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
    std::vector<std::uint32_t> blockData(16, 0u);
    const auto target = BufferDescriptor(texels, SurfaceBytes);
    std::copy(target.begin(), target.end(), blockData.begin());
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    if (writer == Writer::Fill) {
        const std::array<std::uint32_t, 4> pattern{FillTexel, FillTexel, FillTexel, FillTexel};
        if (!device.FillBuffer(surface, SurfaceBytes, pattern)) {
            std::cout << what << ": the surface has no host import: skipped\n";
            device.WaitIdle();
            return VulkanTestSkipped;
        }
    } else {
        Dispatch(device, writer == Writer::Mixed ? std::span<const std::uint32_t>(MixedBlocksCode) : std::span<const std::uint32_t>(UniformBlocksCode), blockData, BlockThreads);
    }
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "single-key GPU upload: no active recorder");
    Require(recorder->OpenWriteOverlaps(surface, SurfaceBytes), what + ": the write of the texels is not pending in the open batch");
    if (!AgcDriver::GuestMemory::Watched(surface, SurfaceBytes)) {
        std::cout << what << ": the imported surface is not write-watched: skipped\n";
        device.WaitIdle();
        return VulkanTestSkipped;
    }
    const auto syncs = Recorder::SyncCounts();
    const auto before = StorageTexture::SingleKeyUploads();
    Dispatch(device, WriteCode, writeData, Threads);
    const auto after = StorageTexture::SingleKeyUploads();
    const auto synced = Recorder::SyncCounts();
    const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
    Require(image != nullptr, what + ": the image write left no pending image over the surface");
    const auto switched = static_cast<std::size_t>(StorageTexture::SingleKeyFallback::Switch);
    char counts[256];
    std::snprintf(counts, sizeof(counts), " (uploads: %llu on the GPU, %llu by a word-fill proof, %llu under the switch; hook syncs %llu -> %llu)", static_cast<unsigned long long>(after.gpu - before.gpu), static_cast<unsigned long long>(after.fill - before.fill), static_cast<unsigned long long>(after.cpu[switched] - before.cpu[switched]), static_cast<unsigned long long>(syncs[1]), static_cast<unsigned long long>(synced[1]));
    if (off) {
        Require(after.cpu[switched] == before.cpu[switched] + 1 && after.gpu == before.gpu && after.fill == before.fill, what + ": APS5_NO_SINGLE_KEY_GPU_UPLOAD did not leave the upload to the CPU" + counts);
        Require(synced[1] > syncs[1], what + ": the CPU upload behind the pending write did not wait through the flush hook" + counts);
    } else {
        const bool decided = writer == Writer::Fill ? after.fill == before.fill + 1 && after.gpu == before.gpu : after.gpu == before.gpu + 1 && after.fill == before.fill;
        Require(decided, what + (writer == Writer::Fill ? ": the upload was not cleared from the word-fill proof" : ": the upload was not decided on the GPU") + counts);
        Require(synced[1] == syncs[1], what + ": the upload read the texels through the flush hook" + counts);
        Require(recorder->OpenWriteOverlaps(surface, SurfaceBytes), what + ": the upload submitted the open batch holding the write of the texels" + counts);
    }
    StorageTexture::FlushPending(surface, SurfaceBytes, nullptr, "test single-key GPU upload");
    device.WaitIdle();
    const AgcDriver::Graphics::ColorTargetLayout layout(Side, Side, AgcDriver::Graphics::ColorTileMode::RenderTarget, 4);
    std::size_t wrong = 0;
    std::uint32_t firstX = 0, firstY = 0, found = 0, expectedFirst = 0;
    for (std::uint32_t y = 0; y < Side; ++y) {
        for (std::uint32_t x = 0; x < Side; ++x) {
            const auto offset = layout.Offset(x, y);
            std::uint32_t texel = 0;
            std::memcpy(&texel, texels + offset, 4);
            const auto expected = y == 0 && x < Threads ? 0xffffffffu : BlockTexel(writer, offset / 256);
            if (texel == expected) continue;
            if (wrong++ == 0) {
                firstX = x;
                firstY = y;
                found = texel;
                expectedFirst = expected;
            }
        }
    }
    char message[384];
    std::snprintf(message, sizeof(message), "%s: %zu of %u written-back texels are neither the image write nor the first texel of their block (texel %u,%u is 0x%08x, expected 0x%08x)", what.c_str(), wrong, Side * Side, firstX, firstY, found, expectedFirst);
    Require(wrong == 0, message);
    std::cout << what << ": " << (off ? "left to the CPU" : writer == Writer::Fill ? "cleared from the fill" : "uploaded on the GPU") << ", texels match" << counts << '\n';
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        const bool off = std::getenv("APS5_NO_SINGLE_KEY_GPU_UPLOAD") != nullptr;
        if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--off")) {
            std::cerr << "usage: " << argv[0] << " [--off]\n";
            return 2;
        }
        Require((argc == 2) == off, "single-key GPU upload: --off runs with APS5_NO_SINGLE_KEY_GPU_UPLOAD set and only then");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!AgcDriver::GuestMemory::WriteWatched()) {
            std::puts("single-key GPU upload: guest memory has no write watch: skipped");
            return VulkanTestSkipped;
        }
        bool ran = false;
        for (const auto writer : {Writer::Mixed, Writer::Uniform, Writer::Fill}) {
            const auto result = Run(*device, writer, off);
            if (result != 0 && result != VulkanTestSkipped) return result;
            ran = ran || result == 0;
        }
        if (!ran) return VulkanTestSkipped;
        std::puts(off ? "single-key uploads left to the CPU under the switch passed" : "single-key GPU uploads passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
