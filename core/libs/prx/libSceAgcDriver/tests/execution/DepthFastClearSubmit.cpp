#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DwordPatternFill.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
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
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <span>
#include <optional>
#include <string>
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

constexpr std::array<std::uint32_t, 12> BufferPatternFillKernel{0xd7460004u, 0x04010c08u, 0xf4280302u, 0xfa000000u, 0xbf8cc07fu, 0x7e00020cu, 0x7e02020du, 0x7e04020eu, 0x7e06020fu, 0xe01c2000u, 0x80000004u, 0xbf810000u};

constexpr std::array<std::uint32_t, 12> BufferPatternFillKernelLowRegisters{0xd7460004u, 0x04010c08u, 0xf4280102u, 0xfa000000u, 0xbf8cc07fu, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0xe01c2000u, 0x80000004u, 0xbf810000u};

void BufferPatternFillMatches() {
    using AgcDriver::DriverDetail::MatchesBufferPatternFillKernel;
    Require(MatchesBufferPatternFillKernel(BufferPatternFillKernel), "the buffer pattern fill loading its pattern into s[12:15] was not recognized");
    Require(MatchesBufferPatternFillKernel(BufferPatternFillKernelLowRegisters), "the buffer pattern fill loading its pattern into s[4:7] was not recognized");
    for (const auto* kernel : {&BufferPatternFillKernel, &BufferPatternFillKernelLowRegisters}) {
        Require(!MatchesBufferPatternFillKernel(std::span(*kernel).first(kernel->size() - 1)), "a truncated buffer pattern fill matched");
        for (std::size_t word = 0; word < kernel->size(); ++word) {
            for (std::uint32_t bit = 0; bit < 32; ++bit) {
                auto altered = *kernel;
                altered[word] ^= 1u << bit;
                Require(!MatchesBufferPatternFillKernel(altered), "a buffer pattern fill with one bit changed matched");
            }
        }
    }
    auto overStore = BufferPatternFillKernelLowRegisters;
    overStore[2] = 0xf4280002u;
    for (std::uint32_t word = 0; word < 4; ++word) overStore[5 + word] = 0x7e000200u | (word << 17u) | word;
    Require(!MatchesBufferPatternFillKernel(overStore), "a buffer pattern fill loading its pattern over the store's V# in s[0:3] matched");
}

void DispatchFill(const GuestBlock& code, const GuestBlock& htile, std::uint32_t bytes, const GuestBlock* patternBuffer = nullptr) {
    const auto kernel = code.Address();
    const auto base = htile.Address();
    const auto source = patternBuffer != nullptr ? patternBuffer->Address() : 0;
    Submit({
        Pm4Packet(0x76, {0x20c, Low(kernel >> 8u)}),
        Pm4Packet(0x76, {0x20d, High(kernel >> 8u) & 0xffu}),
        Pm4Packet(0x76, {0x207, 64}),
        Pm4Packet(0x76, {0x208, 1}),
        Pm4Packet(0x76, {0x209, 1}),
        Pm4Packet(0x76, {0x213, (8u << 1u) | 0x80u}),
        Pm4Packet(0x76, {0x240, Low(base)}),
        Pm4Packet(0x76, {0x241, (High(base) & 0xffffu) | (16u << 16u)}),
        Pm4Packet(0x76, {0x242, bytes / 16u}),
        Pm4Packet(0x76, {0x243, 0x4bfacu}),
        Pm4Packet(0x76, {0x244, Low(source)}),
        Pm4Packet(0x76, {0x245, source != 0 ? (High(source) & 0xffffu) | (16u << 16u) : 0}),
        Pm4Packet(0x76, {0x246, source != 0 ? 1u : 0}),
        Pm4Packet(0x76, {0x247, source != 0 ? 0x4dfacu : 0}),
        Pm4Packet(0x15, {bytes / 1024u, 1, 1, 0x41}),
    });
}

constexpr std::array<std::uint32_t, 10> UnknownFillKernel{0xbf800000u, 0xd7460004u, 0x04010c08u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0xe01c2000u, 0x80000004u, 0xbf810000u};

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
    DispatchFill(code, htile, HtileFillBytes);
    Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a compute fill of 0 over the HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, surface, {}) == 0u, "the depth written after the compute fill clear was lost");
}

void DispatchBufferPatternFillClearsDepth(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    for (const auto* kernel : {&BufferPatternFillKernel, &BufferPatternFillKernelLowRegisters}) {
        const auto& htile = NewBlock();
        auto& code = NewBlock();
        std::memcpy(code.Data(), kernel->data(), sizeof(*kernel));
        auto& pattern = NewBlock();
        const Surface surface{NewBlock().Address(), 0, htile.Address(), false, VK_FORMAT_D32_SFLOAT};
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
        std::memset(pattern.Data(), 0xff, 16);
        DispatchFill(code, htile, HtileFillBytes, &pattern);
        Require(Draw(device, shaders, surface, {}) == 0u, "a compute fill with the buffer pattern 0xffffffff (ZMask 0xf) cleared the depth");
        std::memset(pattern.Data(), 0, 16);
        DispatchFill(code, htile, HtileFillBytes, &pattern);
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a compute fill with the buffer pattern 0 over the HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
    }
}

void DrawAfterFastClearExpandsHtile(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    for (const bool stencil : {false, true}) {
        auto& htile = NewBlock();
        const Surface surface{NewBlock().Address(), stencil ? NewBlock().Address() : 0, htile.Address(), stencil, stencil ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D32_SFLOAT};
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
        DmaFill(htile, HtileFillBytes, stencil ? 0xf0u : 0u);
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a fast clear through the HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        std::vector<std::uint32_t> words(HtileFillBytes / 4u);
        AgcDriver::GuestMemory::Read(htile.Address(), std::as_writable_bytes(std::span(words)), 4);
        const auto expanded = stencil ? 0xfffff3ffu : 0xfffc000fu;
        for (const auto word : words) Require(word == expanded, stencil ? "a draw after a depth and stencil fast clear left HTILE words other than expanded (0xfffff3ff)" : "a draw after a depth fast clear left HTILE words other than expanded (0xfffc000f): a kernel decoding the depth through ZMask 0 reads the clear value, not the drawn depth");
        Require(Draw(device, shaders, surface, {}) == 0u, "the depth drawn after the fast clear was cleared again");
    }
}

void UnknownKernelClearsTwice(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    const auto& htile = NewBlock();
    auto& code = NewBlock();
    std::memcpy(code.Data(), UnknownFillKernel.data(), sizeof(UnknownFillKernel));
    const Surface surface{NewBlock().Address(), 0, htile.Address(), false, VK_FORMAT_D32_SFLOAT};
    Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
    for (const char* frame : {"first", "second"}) {
        DispatchFill(code, htile, HtileFillBytes);
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, std::string("the ") + frame + " clear an unrecognized kernel wrote into the HTILE was not taken");
        Require(Draw(device, shaders, surface, {}) == 0u, std::string("the depth written after the ") + frame + " kernel clear was lost");
    }
}

std::uint32_t* WordsAt(GuestBlock& block, std::size_t offset) { return reinterpret_cast<std::uint32_t*>(block.Data() + offset); }

bool AllWords(const std::uint32_t* words, std::uint32_t value) { return std::all_of(words, words + HtileFillBytes / 4u, [&](std::uint32_t word) { return word == value; }); }

void ExpandedTileKeepsSlice(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    auto& htile = NewBlock();
    const Surface first{NewBlock().Address(), 0, htile.Address(), false, VK_FORMAT_D32_SFLOAT, 0};
    const Surface second{NewBlock().Address(), 0, htile.Address() + HtileFillBytes, false, VK_FORMAT_D32_SFLOAT, 1};
    Require(Draw(device, shaders, first, {.depthWrite = true}) == Covered, "slice 0 was not cleared to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, second, {.depthWrite = true}) == Covered, "slice 1 was not cleared to DB_DEPTH_CLEAR (1.0)");
    auto* sliceOne = WordsAt(htile, HtileFillBytes);
    std::fill(sliceOne, sliceOne + HtileFillBytes / 4u, 0u);
    sliceOne[0] = 0xffffffffu;
    AgcDriver::GuestMemory::MarkWritten(htile.Address() + HtileFillBytes, HtileFillBytes);
    Require(Draw(device, shaders, second, {}) == 0u, "slice 1 took a clear from HTILE words with one tile left expanded");
    Require(sliceOne[0] == 0xffffffffu && std::all_of(sliceOne + 1, sliceOne + HtileFillBytes / 4u, [](std::uint32_t word) { return word == 0u; }), "a bind of slice 1 stored back HTILE words with one tile left expanded");
    Require(AllWords(WordsAt(htile, 0), 0xfffc000fu), "a bind of slice 1 changed slice 0's HTILE");
}

void UniformSliceDecodes(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    auto& htile = NewBlock();
    const Surface first{NewBlock().Address(), 0, htile.Address(), false, VK_FORMAT_D32_SFLOAT, 0};
    const Surface second{NewBlock().Address(), 0, htile.Address() + HtileFillBytes, false, VK_FORMAT_D32_SFLOAT, 1};
    Require(Draw(device, shaders, first, {.depthWrite = true}) == Covered, "slice 0 was not cleared to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, second, {.depthWrite = true}) == Covered, "slice 1 was not cleared to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, second, {}) == 0u, "the depth drawn into slice 1 was lost");
    auto* sliceOne = WordsAt(htile, HtileFillBytes);
    std::fill(sliceOne, sliceOne + HtileFillBytes / 4u, 0u);
    AgcDriver::GuestMemory::MarkWritten(htile.Address() + HtileFillBytes, HtileFillBytes);
    Require(Draw(device, shaders, second, {.depthWrite = true}) == Covered, "slice 1 did not take the clear from HTILE words that are all equal");
    Require(AllWords(sliceOne, 0xfffc000fu), "slice 1's bind did not store its cleared HTILE back as expanded");
    Require(Draw(device, shaders, second, {}) == 0u, "the depth drawn into slice 1 after the clear was lost");
}

void MixedClearedSliceClears(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    auto& htile = NewBlock();
    const Surface first{NewBlock().Address(), 0, htile.Address(), false, VK_FORMAT_D32_SFLOAT, 0};
    const Surface second{NewBlock().Address(), 0, htile.Address() + HtileFillBytes, false, VK_FORMAT_D32_SFLOAT, 1};
    Require(Draw(device, shaders, first, {.depthWrite = true}) == Covered, "slice 0 was not cleared to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, second, {.depthWrite = true}) == Covered, "slice 1 was not cleared to DB_DEPTH_CLEAR (1.0)");
    Require(Draw(device, shaders, second, {}) == 0u, "the depth drawn into slice 1 was lost");
    auto* sliceOne = WordsAt(htile, HtileFillBytes);
    for (std::uint32_t word = 0; word < HtileFillBytes / 4u; ++word) sliceOne[word] = (word / 7u) % 2u == 0u ? 0u : 0xfffffff0u;
    AgcDriver::GuestMemory::MarkWritten(htile.Address() + HtileFillBytes, HtileFillBytes);
    Require(Draw(device, shaders, second, {.depthWrite = true}) == Covered, "slice 1 did not take the clear from HTILE words that all have ZMask 0 but different Min/Max Z");
    Require(AllWords(sliceOne, 0xfffc000fu), "slice 1's bind did not store its cleared HTILE back as expanded");
    Require(AllWords(WordsAt(htile, 0), 0xfffc000fu), "a bind of slice 1 changed slice 0's HTILE");
    Require(Draw(device, shaders, second, {}) == 0u, "the depth drawn into slice 1 after the clear was lost");
}

void RecordedFillClearsWithoutWait(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    const VkStencilOpState writeFive{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 5u};
    const VkStencilOpState equalZero{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, 0u};
    for (const bool stencil : {false, true}) {
        auto& htile = NewBlock();
        const Surface surface{NewBlock().Address(), stencil ? NewBlock().Address() : 0, htile.Address(), stencil, stencil ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D32_SFLOAT};
        Require(Draw(device, shaders, surface, {.depthWrite = true, .stencilTest = stencil, .stencil = writeFive}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
        Require(Draw(device, shaders, surface, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto word = stencil ? 0xf0u : 0u;
            const std::array<std::uint32_t, 4> pattern{word, word, word, word};
            Require(device.FillBuffer(htile.Address(), HtileFillBytes, pattern), stencil ? "the depth and stencil fast clear fill (SMem 0) over the HTILE was not recorded on the GPU" : "the fill of 0 over the HTILE was not recorded on the GPU");
        }
        const auto waits = AgcDriver::Graphics::Recorder::ThreadHookWaits();
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, stencil ? "a depth and stencil fast clear fill recorded over the HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)" : "a fill of 0 recorded over the HTILE did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        Require(AgcDriver::Graphics::Recorder::ThreadHookWaits() == waits, "the bind of the depth target waited for the GPU to run the fill recorded over its HTILE");
        const auto expanded = stencil ? 0xfffff3ffu : 0xfffc000fu;
        Require(AllWords(WordsAt(htile, 0), expanded), stencil ? "the bind after a depth and stencil fast clear fill recorded over the HTILE did not store it back as expanded (0xfffff3ff)" : "the bind after a fill recorded over the HTILE did not store it back as expanded (0xfffc000f)");
        if (stencil) Require(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = equalZero}) == Covered, "a depth and stencil fast clear fill recorded over the HTILE did not clear the stencil to 0");
        Require(Draw(device, shaders, surface, {}) == 0u, "the depth drawn after the clear from the recorded fill was cleared again");
    }
}

void RecordedMaskClearClearsWithoutWait(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    const VkStencilOpState writeFive{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 5u};
    const VkStencilOpState equalZero{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, 0u};
    for (const bool stencil : {false, true}) {
        auto& htile = NewBlock();
        const Surface surface{NewBlock().Address(), stencil ? NewBlock().Address() : 0, htile.Address(), stencil, stencil ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D32_SFLOAT};
        Require(Draw(device, shaders, surface, {.depthWrite = true, .stencilTest = stencil, .stencil = writeFive}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
        Require(Draw(device, shaders, surface, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
        const auto mask = stencil ? 0x0f0u : 0x3f0u;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const std::array<std::uint32_t, 4> words{0x010u & mask, 0x0a0u & mask, 0x150u & mask, 0x3f0u & mask};
            Require(device.FillBuffer(htile.Address(), HtileFillBytes, words), "the words of the HTILE mask clear were not recorded on the GPU");
            device.NoteHtileMask({htile.Address(), HtileFillBytes, mask, 0u});
        }
        const auto waits = AgcDriver::Graphics::Recorder::ThreadHookWaits();
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, stencil ? "a depth and stencil clear by an HTILE mask clear did not clear the depth to DB_DEPTH_CLEAR (1.0)" : "a depth clear by an HTILE mask clear did not clear the depth to DB_DEPTH_CLEAR (1.0)");
        Require(AgcDriver::Graphics::Recorder::ThreadHookWaits() == waits, "the bind of the depth target waited for the GPU to run the HTILE mask clear recorded over its HTILE");
        const auto expanded = stencil ? 0xfffff3ffu : 0xfffc000fu;
        Require(AllWords(WordsAt(htile, 0), expanded), stencil ? "the bind after a depth and stencil HTILE mask clear did not store it back as expanded (0xfffff3ff)" : "the bind after a depth HTILE mask clear did not store it back as expanded (0xfffc000f)");
        if (stencil) Require(Draw(device, shaders, surface, {.depthTest = false, .stencilTest = true, .stencil = equalZero}) == Covered, "a depth and stencil HTILE mask clear did not clear the stencil to 0");
        Require(Draw(device, shaders, surface, {}) == 0u, "the depth drawn after the clear by the HTILE mask clear was cleared again");
    }
}

void RecordedMaskKeepingZMaskReadsBack(AgcDriver::VulkanDevice& device, const Shaders& shaders) {
    for (const bool stencil : {false, true}) {
        auto& htile = NewBlock();
        const Surface surface{NewBlock().Address(), stencil ? NewBlock().Address() : 0, htile.Address(), stencil, stencil ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_D32_SFLOAT};
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == Covered, "a new depth surface was not cleared to DB_DEPTH_CLEAR (1.0)");
        Require(Draw(device, shaders, surface, {}) == 0u, "a draw at depth 0.5 passed LESS against the 0.5 written before it");
        const auto mask = stencil ? 0x0ffu : 0x3ffu;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const std::array<std::uint32_t, 4> words{0x015u & mask, 0x0a0u & mask, 0x15au & mask, 0x3ffu & mask};
            Require(device.FillBuffer(htile.Address(), HtileFillBytes, words), "the words of the HTILE mask clear were not recorded on the GPU");
            device.NoteHtileMask({htile.Address(), HtileFillBytes, mask, 0u});
        }
        const auto waits = AgcDriver::Graphics::Recorder::ThreadHookWaits();
        Require(Draw(device, shaders, surface, {.depthWrite = true}) == 0u, "an HTILE mask clear that keeps non-zero ZMask bits cleared the depth");
        Require(AgcDriver::Graphics::Recorder::ThreadHookWaits() != waits, "the bind after an HTILE mask clear that keeps the ZMask did not read the HTILE back");
    }
}

void RecordHtileWords(const AgcDriver::Graphics::Context& context, AgcDriver::Graphics::Recorder& recorder, const GuestBlock& htile, std::uint32_t word) {
    using namespace AgcDriver::Graphics;
    recorder.Activate();
    const auto* import = HostImportFor(context, htile.Address(), HtileBytes);
    Require(import != nullptr, "the recorder lifetime fixture requires an imported HTILE");
    const auto commands = recorder.Commands();
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, import->buffer, htile.Address() - import->base, HtileBytes, word);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
    recorder.NotePendingWrite(htile.Address(), HtileBytes);
    AgcDriver::GuestMemory::MarkWritten(htile.Address(), HtileBytes);
}

void RecreatedRecorderReadsHtile(AgcDriver::VulkanDevice& device, bool clearSurfaces) {
    using namespace AgcDriver::Graphics;
    device.WaitIdle();
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto context = device.graphicsContext();
    auto* previous = Recorder::Active();
    auto& htile = NewBlock();
    const DepthTarget target{NewBlock().Address(), 0, {64, 32}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, htile.Address(), false};
    std::optional<Recorder> recorder;
    recorder.emplace(context);
    const auto* address = &*recorder;
    RecordHtileWords(context, *recorder, htile, 0xf0u);
    const auto note = recorder->NewestWriteNote(htile.Address(), HtileBytes);
    NoteHtileMaskOnGpu(context, {htile.Address(), HtileBytes, 0x3f0u, 0u});
    recorder.reset();
    if (clearSurfaces) ClearDepthSurfaces(context.device);
    recorder.emplace(context);
    Require(&*recorder == address, "the replacement recorder did not reuse the original storage");
    RecordHtileWords(context, *recorder, htile, 0xfu);
    Require(recorder->NewestWriteNote(htile.Address(), HtileBytes) == note, "the replacement recorder did not reuse the original note");
    const auto waits = Recorder::ThreadHookWaits();
    Require(DepthSurfaceView(context, target) != VK_NULL_HANDLE, "the replacement recorder could not bind the depth surface");
    const auto waited = Recorder::ThreadHookWaits() != waits;
    recorder->Sync();
    const auto words = WordsAt(htile, 0);
    const auto preserved = AllWords(words, 0xfu);
    std::printf("HTILE recorder reuse: cleanup=%d note=%llu waited=%d preserved=%d first=0x%08x\n", clearSurfaces, static_cast<unsigned long long>(note), waited, preserved, words[0]);
    recorder.reset();
    ClearDepthSurfaces(context.device);
    if (previous != nullptr) previous->Activate();
    Require(waited && preserved, "a replacement recorder reused a stale mask proof instead of reading back its ordinary HTILE write");
}

void CheckRecordedHtile(const AgcDriver::Graphics::Context& context, AgcDriver::Graphics::Recorder& recorder, GuestBlock& htile, bool expectedWait, std::uint32_t expectedWord, const std::string& label) {
    using namespace AgcDriver::Graphics;
    RecordHtileWords(context, recorder, NewBlock(), 0u);
    const DepthTarget target{NewBlock().Address(), 0, {64, 32}, VK_FORMAT_D32_SFLOAT, 1.0f, 0, htile.Address(), false};
    const auto waits = Recorder::ThreadHookWaits();
    Require(DepthSurfaceView(context, target) != VK_NULL_HANDLE, label + ": could not bind the depth surface");
    const bool waited = Recorder::ThreadHookWaits() != waits;
    recorder.Sync();
    const auto* words = WordsAt(htile, 0);
    const bool correct = AllWords(words, expectedWord);
    std::printf("HTILE %s: waited=%d correct=%d first=0x%08x\n", label.c_str(), waited, correct, words[0]);
    Require(waited == expectedWait && correct, label + ": wrong HTILE readback or metadata bytes");
}

void LiveRecorderMaskIsolation(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::Graphics;
    device.WaitIdle();
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto context = device.graphicsContext();
    auto* previous = Recorder::Active();
    {
        auto& htile = NewBlock();
        Recorder first(context);
        Recorder second(context);
        RecordHtileWords(context, first, htile, 0xf0u);
        NoteHtileMaskOnGpu(context, {htile.Address(), HtileBytes, 0x3f0u, 0u});
        RecordHtileWords(context, second, htile, 0xfu);
        Require(first.NewestWriteNote(htile.Address(), HtileBytes) == second.NewestWriteNote(htile.Address(), HtileBytes), "live recorders did not reuse the same note");
        CheckRecordedHtile(context, second, htile, true, 0xfu, "live owner isolation");
        CheckRecordedHtile(context, first, htile, false, 0xfffc000fu, "same owner mask");
    }
    ClearDepthSurfaces(context.device);
    if (previous != nullptr) previous->Activate();
}

void MaskCleanupIsolation(AgcDriver::VulkanDevice& device, AgcDriver::VulkanDevice& otherDevice) {
    using namespace AgcDriver::Graphics;
    device.WaitIdle();
    otherDevice.WaitIdle();
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto context = device.graphicsContext();
    const auto otherContext = otherDevice.graphicsContext();
    Require(context.device != otherContext.device, "the device cleanup fixture requires distinct Vulkan devices");
    auto* previous = Recorder::Active();
    {
        auto& firstHtile = NewBlock();
        auto& secondHtile = NewBlock();
        std::optional<Recorder> first;
        first.emplace(context);
        Recorder second(context);
        RecordHtileWords(context, *first, firstHtile, 0xf0u);
        NoteHtileMaskOnGpu(context, {firstHtile.Address(), HtileBytes, 0x3f0u, 0u});
        RecordHtileWords(context, second, secondHtile, 0xf0u);
        NoteHtileMaskOnGpu(context, {secondHtile.Address(), HtileBytes, 0x3f0u, 0u});
        first.reset();
        CheckRecordedHtile(context, second, secondHtile, false, 0xfffc000fu, "other owner survives recorder cleanup");
    }
    {
        auto& firstHtile = NewBlock();
        auto& secondHtile = NewBlock();
        auto& otherHtile = NewBlock();
        Recorder first(context);
        Recorder second(context);
        Recorder other(otherContext);
        RecordHtileWords(context, first, firstHtile, 0xf0u);
        NoteHtileMaskOnGpu(context, {firstHtile.Address(), HtileBytes, 0x3f0u, 0u});
        RecordHtileWords(context, second, secondHtile, 0xf0u);
        NoteHtileMaskOnGpu(context, {secondHtile.Address(), HtileBytes, 0x3f0u, 0u});
        RecordHtileWords(otherContext, other, otherHtile, 0xf0u);
        NoteHtileMaskOnGpu(otherContext, {otherHtile.Address(), HtileBytes, 0x3f0u, 0u});
        ClearDepthSurfaces(context.device);
        CheckRecordedHtile(context, first, firstHtile, true, 0xfffc000fu, "device cleanup first owner");
        CheckRecordedHtile(context, second, secondHtile, true, 0xfffc000fu, "device cleanup second owner");
        CheckRecordedHtile(otherContext, other, otherHtile, false, 0xfffc000fu, "other device survives cleanup");
    }
    ClearDepthSurfaces(context.device);
    ClearDepthSurfaces(otherContext.device);
    if (previous != nullptr) previous->Activate();
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

int main(int argc, char** argv) {
    try {
        BufferPatternFillMatches();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 2 && std::string(argv[1]) == "--mask-lifetime") {
            const auto otherDevice = OpenVulkanTestDevice();
            if (!otherDevice) return VulkanTestSkipped;
            if (!StoresFills(*device, NewBlock()) || !StoresFills(*otherDevice, NewBlock())) {
                std::puts("skipped HTILE mask lifetime: the Vulkan devices cannot fill imported guest memory");
                return VulkanTestSkipped;
            }
            RecreatedRecorderReadsHtile(*device, false);
            RecreatedRecorderReadsHtile(*device, true);
            LiveRecorderMaskIsolation(*device);
            MaskCleanupIsolation(*device, *otherDevice);
            LibcRunShutdown_nid_postfix();
            std::puts("HTILE mask recorder lifetime tests passed");
            return 0;
        }
        const auto shaders = Compile(*device);
        DmaFillClearsDepth(*device, shaders);
        DepthOnlyFillKeepsStencil(*device, shaders);
        DrawAfterFastClearExpandsHtile(*device, shaders);
        const bool computeRan = StoresFills(*device, NewBlock());
        ExpandedTileKeepsSlice(*device, shaders);
        UniformSliceDecodes(*device, shaders);
        MixedClearedSliceClears(*device, shaders);
        if (computeRan) {
            RecordedFillClearsWithoutWait(*device, shaders);
            RecordedMaskClearClearsWithoutWait(*device, shaders);
            RecordedMaskKeepingZMaskReadsBack(*device, shaders);
            DispatchFillClearsDepth(*device, shaders);
            DispatchBufferPatternFillClearsDepth(*device, shaders);
            UnknownKernelClearsTwice(*device, shaders);
        }
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
