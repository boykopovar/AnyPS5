#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uintptr_t GcnAddressLimit = std::uintptr_t{1} << 44u;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t OutputOffset = 0;
constexpr std::uint32_t TexelOffset = 4096;
constexpr std::uint32_t Fill = 0xdeadbeefu;

alignas(256) constexpr std::array<std::uint32_t, 9> Code{
    0x7e020280u, 0x7e040280u, 0x340e0084u, 0xf09c0f00u, 0x00400301u, 0xbf8c0f70u, 0xe0781000u, 0x80030307u, 0xbf810000u,
};

class LowBlock {
public:
    LowBlock() : block(static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes))) {
        Require(block != nullptr, "GCN invalid format image: cannot allocate the block in the guest arena");
        GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, 0x04u, BlockBytes);
    }

    ~LowBlock() {
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
    }

    LowBlock(const LowBlock&) = delete;
    LowBlock& operator=(const LowBlock&) = delete;

    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }
    bool Addressable() const { return Address() + BlockBytes <= GcnAddressLimit; }
    std::uint32_t* Words(std::uint32_t offset) const { return block + offset / 4u; }

private:
    std::uint32_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(std::uintptr_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x00027facu};
}

std::array<std::uint32_t, 8> ImageWords(std::uintptr_t address, std::uint32_t dataFormat, std::uint32_t dstSel, std::uint32_t type) {
    const auto base = static_cast<std::uint32_t>(address >> 8u);
    const auto high = static_cast<std::uint32_t>((address >> 40u) & 0xffu);
    const auto word1 = high | 0x000c0000u | (dataFormat << 20u);
    return {base, word1, 0x00000008u, dstSel | 0x00050000u | (type << 28u), base + 1u, word1, 0x00000008u, 0x2005f22cu};
}

void Dispatch(AgcDriver::VulkanDevice& device, const LowBlock& block, const std::array<std::uint32_t, 8>& image) {
    std::fill(block.Words(OutputOffset), block.Words(OutputOffset) + Threads * 4u, Fill);
    std::fill(block.Words(TexelOffset), block.Words(TexelOffset) + 1024u, 0x3f800000u);
    std::vector<std::uint32_t> userData(16, 0u);
    std::copy(image.begin(), image.end(), userData.begin());
    const auto output = BufferDescriptor(reinterpret_cast<std::uintptr_t>(block.Words(OutputOffset)), Threads * 16u);
    std::copy(output.begin(), output.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void ExpectZeros(AgcDriver::VulkanDevice& device, const LowBlock& block, const std::array<std::uint32_t, 8>& image, const std::string& what) {
    Dispatch(device, block, image);
    std::array<std::uint32_t, Threads * 4u> stored{};
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(block.Words(OutputOffset)), std::as_writable_bytes(std::span(stored)), 4);
    for (std::uint32_t word = 0; word < stored.size(); ++word) {
        char text[192];
        std::snprintf(text, sizeof(text), "GCN invalid format image: %s, thread %u component %u read 0x%08x, an unbound resource reads 0", what.c_str(), word / 4u, word % 4u, stored[word]);
        Require(stored[word] == 0u, text);
    }
}

void ExpectRefused(AgcDriver::VulkanDevice& device, const LowBlock& block, const std::array<std::uint32_t, 8>& image, const std::string& what) {
    try {
        Dispatch(device, block, image);
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find("runtime image descriptor is invalid") != std::string::npos, "GCN invalid format image: " + what + " failed with another error: " + error.what());
        return;
    }
    throw std::runtime_error("GCN invalid format image: " + what + " was accepted");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        ShaderRecompiler::SetGcnGuestShaders(true);
        const LowBlock block;
        if (!block.Addressable()) {
            std::printf("skipped, the guest arena block at 0x%llx lies above the 44-bit GCN address range\n", static_cast<unsigned long long>(block.Address()));
            return VulkanTestSkipped;
        }
        const auto texels = reinterpret_cast<std::uintptr_t>(block.Words(TexelOffset));
        constexpr std::uint32_t Xyzw = 0xfacu;
        constexpr std::uint32_t XyzOne = 0x3acu;
        ExpectZeros(*device, block, ImageWords(texels, 0u, Xyzw, 2u), "a reserved TYPE 2 T# whose words are two V#s");
        ExpectZeros(*device, block, ImageWords(texels, 0u, Xyzw, 7u), "a reserved TYPE 7 T#");
        ExpectZeros(*device, block, ImageWords(texels, 0u, Xyzw, 15u), "a 2D MSAA array T#");
        ExpectRefused(*device, block, ImageWords(texels, 0u, XyzOne, 2u), "a reserved TYPE 2 T# with DST_SEL_W 1");
        ExpectRefused(*device, block, ImageWords(texels, 10u, Xyzw, 2u), "a reserved TYPE 2 T# with DATA_FORMAT 8_8_8_8");
        std::puts("GCN invalid format image tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
