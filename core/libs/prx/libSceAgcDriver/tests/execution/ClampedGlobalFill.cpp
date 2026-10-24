#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DwordPatternFill.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
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
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using AgcDriver::DriverDetail::MatchClampedGlobalFill;

alignas(256) constexpr std::array<std::uint32_t, 11> Code{0xd7460000u, 0x04010c06u, 0x7e080205u, 0x7e0a0205u, 0x7e0c0205u, 0x7e0e0205u, 0x34000084u, 0x26000004u, 0xdc788000u, 0x00020400u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> WriteCode{0x7e020280u, 0x7e0402f2u, 0x7e0602f2u, 0x7e0802f2u, 0x7e0a02f2u, 0xf0201f08u, 0x00010200u, 0xbf810000u};
constexpr std::uint32_t Untouched = 0xcafebabeu;
constexpr std::uint32_t WriteThreads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = 65536;
constexpr std::size_t KeyBytes = SurfaceBytes / 256;
constexpr std::size_t KeysOffset = SurfaceBytes;
constexpr std::size_t LabelOffset = SurfaceBytes + 4096;
constexpr std::size_t BlockBytes = 2 * 65536;
constexpr std::size_t OutputDwords = 1028;
alignas(256) std::array<std::uint32_t, OutputDwords> Output{};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
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
#endif
        Require(block != nullptr, "clamped global fill: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
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
};

std::vector<std::uint32_t> UserData(void* output, std::uint32_t maxOffset, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(output);
    return {0u, 0u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), maxOffset, value};
}

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

void MatchTests() {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    const std::array<std::uint32_t, 5> packet{0xc0031500u, 1, 1, 1, 0x41u};
    const auto data = UserData(Output.data(), 1008, 0x5a5a5a5au);
    const auto fill = MatchClampedGlobalFill(packet, Code, data, compute);
    Require(fill && fill->base == reinterpret_cast<std::uintptr_t>(Output.data()) && fill->bytes == 1024 && fill->pattern == std::array<std::uint32_t, 4>{0x5a5a5a5au, 0x5a5a5a5au, 0x5a5a5a5au, 0x5a5a5a5au}, "the clamped global fill kernel was not recognized as a fill of its value");
    for (std::size_t word = 0; word < Code.size(); ++word) {
        auto altered = Code;
        altered[word] ^= 0x10000u;
        Require(!MatchClampedGlobalFill(packet, altered, data, compute), "a different shader matched the clamped global fill kernel");
    }
    Require(!MatchClampedGlobalFill(packet, std::span(Code).first(10), data, compute), "a truncated shader matched");
    auto extra = data;
    extra.push_back(0);
    Require(!MatchClampedGlobalFill(packet, Code, extra, compute), "a different user SGPR count matched (the group id moves)");
    auto threads = packet;
    threads[4] |= 0x20u;
    Require(!MatchClampedGlobalFill(threads, Code, data, compute), "a dispatch in thread dimensions matched");
    auto rows = packet;
    rows[2] = 2;
    Require(!MatchClampedGlobalFill(rows, Code, data, compute), "a two-dimensional dispatch matched");
    auto noGroup = compute;
    noGroup.groupIdEnable[0] = false;
    Require(!MatchClampedGlobalFill(packet, Code, data, noGroup), "a dispatch without the group id matched");
    Require(!MatchClampedGlobalFill(packet, Code, UserData(Output.data() + 1, 1008, 0), compute), "an unaligned destination matched");
    Require(!MatchClampedGlobalFill(packet, Code, UserData(Output.data(), 1000, 0), compute), "a largest offset off the 16-byte grid matched");
    const auto clamped = MatchClampedGlobalFill(packet, Code, UserData(Output.data(), 240, 0), compute);
    Require(clamped && clamped->bytes == 256, "the fill reached past its largest store offset");
    std::array<std::uint32_t, 5> four = packet;
    four[1] = 4;
    const auto launched = MatchClampedGlobalFill(four, Code, UserData(Output.data(), 0x100000u, 0), compute);
    Require(launched && launched->bytes == 4096, "the fill reached past its last thread's store");
}

std::vector<std::uint32_t> Commands(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& data, std::uint32_t groups, std::uint32_t threads) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{threads, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (1u << 7u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, groups, 1, 1, 0x41u});
    return words;
}

void Submit(const std::vector<std::uint32_t>& words) {
    Packet packet{const_cast<std::uint32_t*>(words.data()), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
}

void RangeTests(GuestBlock& block) {
    struct Case {
        std::uint32_t groups, maxOffset, value;
    };
    constexpr std::array<Case, 5> cases{{
        {1, 1008, 0u},
        {4, 1008, 0x5a5a5a5au},
        {4, 240, 0xffffffffu},
        {1, 0x100000u, 0x01020304u},
        {0, 1008, 0x77777777u},
    }};
    const std::span<std::uint32_t> output(reinterpret_cast<std::uint32_t*>(block.Data()), OutputDwords);
    for (const auto& item : cases) {
        std::fill(output.begin(), output.end(), Untouched);
        Submit(Commands(Code, UserData(output.data(), item.maxOffset, item.value), item.groups, 64));
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(output.data()), output.size_bytes());
        const auto filledDwords = std::min(item.groups * 64u * 4u, item.maxOffset / 4u + 4u);
        for (std::uint32_t i = 0; i < output.size(); ++i) {
            const auto expected = i < filledDwords ? item.value : Untouched;
            Require(output[i] == expected, "a flush right after the fill read dword " + std::to_string(i) + " as " + std::to_string(output[i]) + ", expected " + std::to_string(expected) + " (groups " + std::to_string(item.groups) + ", largest offset " + std::to_string(item.maxOffset) + "): the flush did not wait for the fill's range");
        }
    }
}

void KeyFillOrderTests(GuestBlock& block) {
    auto* texels = block.Data();
    auto* keys = texels + KeysOffset;
    auto* label = reinterpret_cast<std::uint32_t*>(texels + LabelOffset);
    const auto surface = AddressOf(texels);
    std::memset(texels, 0x55, SurfaceBytes);
    std::memset(keys, 0xff, KeyBytes);
    *label = 0;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto texture = TextureDescriptor(texels, keys);
    std::vector<std::uint32_t> writeData(16, 0u);
    std::copy(texture.begin(), texture.end(), writeData.begin() + 4);
    const auto write = Commands(WriteCode, writeData, 1, WriteThreads);
    Submit(write);
    auto words = Commands(Code, UserData(keys, static_cast<std::uint32_t>(KeyBytes - 16), 0u), 1, 64);
    words.insert(words.end(), write.begin(), write.end());
    const auto labelAddress = AddressOf(label);
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(labelAddress >> 32u), 1u, 0u, 0u});
    Submit(words);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::atomic_ref(*label).load() != 1 && std::chrono::steady_clock::now() < deadline) AgcDriver::GuestMemory::FlushGpuWrites(labelAddress, sizeof(*label));
    Require(std::atomic_ref(*label).load() == 1, "the label after the key fill and the write did not land");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto image = StorageTexture::FindPending(surface, SurfaceBytes);
        Require(image != nullptr && image->Descriptor().dccAddress == AddressOf(keys), "the surface written after the key fill has no pending image under its keys");
        AgcDriver::GuestMemory::CollectWritesUncached(AddressOf(keys), KeyBytes);
        const auto read = AgcDriver::Graphics::ProvedClearKeys(image->Descriptor(), image->GuestBytes(), image->KeyProof());
        char message[320];
        std::snprintf(message, sizeof(message), "a write recorded after RADV's key fill: once the fill landed, the keys read %s against uploaded %s, so the next key read drops the write", AgcDriver::Graphics::DccKeysName(read), AgcDriver::Graphics::DccKeysName(image->UploadedKeys()));
        Require(read == image->UploadedKeys() && read == DccKeys::Uncompressed, message);
    }
    AgcDriver::GuestMemory::FlushGpuWrites(surface, SurfaceBytes);
    const auto written = static_cast<std::size_t>(std::count(texels, texels + SurfaceBytes, std::uint8_t{0xff}));
    const auto cleared = static_cast<std::size_t>(std::count(texels, texels + SurfaceBytes, std::uint8_t{0x00}));
    char message[320];
    std::snprintf(message, sizeof(message), "the write after RADV's key fill stored %zu written and %zu cleared bytes of %zu, expected %u written over the clear", written, cleared, SurfaceBytes, WriteThreads * 4u);
    Require(written == WriteThreads * 4u && cleared == SurfaceBytes - written, message);
}

}

int main() {
    try {
        if (std::getenv("APS5_NO_FILL_HLE") == nullptr) MatchTests();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        {
            GuestBlock block;
            KeyFillOrderTests(block);
        }
        {
            GuestBlock block;
            RangeTests(block);
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("clamped global fill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
