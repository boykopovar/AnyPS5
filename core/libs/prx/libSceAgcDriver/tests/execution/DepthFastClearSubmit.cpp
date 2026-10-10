#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <vector>

namespace {

using DepthFastClearHarness::Compile;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::Draw;
using DepthFastClearHarness::HtileBytes;
using DepthFastClearHarness::Require;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Surface;

constexpr auto HtileFillBytes = static_cast<std::uint32_t>(HtileBytes);

class GuestBlock {
public:
    GuestBlock() {
        constexpr std::size_t bytes = 65536;
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(bytes, bytes));
#endif
        Require(block != nullptr, "depth fast clear submit: cannot allocate a guest block");
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

GuestBlock& NewBlock() {
    static auto* blocks = new std::deque<GuestBlock>();
    return blocks->emplace_back();
}

std::uint32_t Low(std::uint64_t value) { return static_cast<std::uint32_t>(value); }

std::uint32_t High(std::uint64_t value) { return static_cast<std::uint32_t>(value >> 32u); }

std::vector<std::uint32_t> Pm4Packet(std::uint32_t opcode, std::initializer_list<std::uint32_t> body) {
    std::vector<std::uint32_t> words{0xc0000000u | (static_cast<std::uint32_t>(body.size() - 1) << 16u) | (opcode << 8u)};
    words.insert(words.end(), body);
    return words;
}

void Submit(std::initializer_list<std::vector<std::uint32_t>> packets) {
    std::vector<std::uint32_t> words;
    for (const auto& packet : packets) words.insert(words.end(), packet.begin(), packet.end());
    Packet dcb{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&dcb) == 0, "depth fast clear submit: DCB submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void DmaFill(const GuestBlock& htile, std::uint32_t bytes, std::uint32_t pattern) {
    Submit({Pm4Packet(0x50, {0x40000000u, pattern, 0, Low(htile.Address()), High(htile.Address()), bytes})});
}

constexpr std::array<std::uint32_t, 9> FillKernel{0xd7460004u, 0x04010c08u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0xe01c2000u, 0x80000004u, 0xbf810000u};

void DispatchFill(const GuestBlock& code, const GuestBlock& htile, std::uint32_t records) {
    const auto kernel = code.Address();
    const auto base = htile.Address();
    Submit({
        Pm4Packet(0x76, {0x20c, Low(kernel >> 8u)}),
        Pm4Packet(0x76, {0x20d, High(kernel >> 8u) & 0xffu}),
        Pm4Packet(0x76, {0x207, 64}),
        Pm4Packet(0x76, {0x208, 1}),
        Pm4Packet(0x76, {0x209, 1}),
        Pm4Packet(0x76, {0x213, 8u << 1u}),
        Pm4Packet(0x76, {0x240, Low(base)}),
        Pm4Packet(0x76, {0x241, (High(base) & 0xffffu) | (16u << 16u)}),
        Pm4Packet(0x76, {0x242, records}),
        Pm4Packet(0x76, {0x243, 0x4bfacu}),
        Pm4Packet(0x76, {0x244, 0}),
        Pm4Packet(0x76, {0x245, 0}),
        Pm4Packet(0x76, {0x246, 0}),
        Pm4Packet(0x76, {0x247, 0}),
        Pm4Packet(0x15, {1, 1, 1, 0x41}),
    });
}

bool StoresFills(AgcDriver::VulkanDevice& device, const GuestBlock& probe) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    std::array<std::uint32_t, 4> pattern{};
    const bool stored = device.FillBuffer(probe.Address(), 4096, pattern);
    device.WaitIdle();
    return stored;
}

void DmaFillClearsDepth(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    const auto& htile = NewBlock();
    const Surface surface{NewBlock().Address(), 0, htile.Address(), false, VK_FORMAT_D32_SFLOAT};
    Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, surface, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
    DmaFill(htile, HtileFillBytes - 4u, 0u);
    Require(Draw(device, shaders, surface, {}) == 0u, "a DMA_DATA fill shorter than the HTILE cleared the depth");
    DmaFill(htile, HtileFillBytes, 0xffffffffu);
    Require(Draw(device, shaders, surface, {}) == 0u, "a DMA_DATA fill expanded to ZMask 0xf cleared the depth");
    DmaFill(htile, HtileFillBytes, 0u);
    Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a DMA_DATA fill of 0 over the HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, surface, {}) == 0u, "the depth written after the DMA_DATA clear was lost");
}

void DispatchFillClearsDepth(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    const auto& htile = NewBlock();
    auto& code = NewBlock();
    std::memcpy(code.Data(), FillKernel.data(), sizeof(FillKernel));
    const Surface surface{NewBlock().Address(), 0, htile.Address(), false, VK_FORMAT_D32_SFLOAT};
    Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, surface, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
    DispatchFill(code, htile, HtileFillBytes / 16u);
    Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a compute fill of 0 over the HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, surface, {}) == 0u, "the depth written after the compute fill clear was lost");
}

void DepthOnlyFillKeepsStencil(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    const auto& htile = NewBlock();
    const Surface surface{NewBlock().Address(), NewBlock().Address(), htile.Address(), true, VK_FORMAT_D32_SFLOAT_S8_UINT};
    const VkStencilOpState writeFive{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 5u};
    const VkStencilOpState equalFive{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, 5u};
    const VkStencilOpState equalZero{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, 0u};
    Require(Draw(device, shaders, surface, {.depthWrite = true, .stencilTest = true, .stencil = writeFive}) == Covered, "the stencil write over a new depth-stencil surface did not cover the target");
    DmaFill(htile, HtileFillBytes, 0x3f0u);
    Require(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = equalFive}) == Covered, "a depth-only fill (SMem 3) cleared the stencil");
    Require(Draw(device, shaders, surface, {}) == Covered, "a depth-only fill did not clear the depth to DB_DEPTH_CLEAR (1.0)");
    DmaFill(htile, HtileFillBytes, 0xf0u);
    Require(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = equalZero}) == Covered, "a depth and stencil fast clear (SMem 0) did not clear the stencil to 0");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto shaders = Compile(*device);
        DmaFillClearsDepth(*device, shaders);
        DepthOnlyFillKeepsStencil(*device, shaders);
        const bool computeRan = StoresFills(*device, NewBlock());
        if (computeRan) DispatchFillClearsDepth(*device, shaders);
        else std::printf("compute fill not run: the device does not store fills into imported guest memory\n");
        LibcRunShutdown_nid_postfix();
        std::puts(computeRan ? "depth fast clear submit tests passed" : "depth fast clear submit tests passed without the compute fill");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
