#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Int64AtomicsCapability = 12;
constexpr std::uint32_t RowsPerOp = 16;
constexpr std::uint32_t OpCount = 0;
constexpr std::uint32_t NarrowOps = 0;
constexpr std::uint32_t OpStride = 0x200;
constexpr std::uint32_t Special = OpCount * OpStride;
constexpr std::uint32_t Sentinel = 0x05e471e1;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint8_t Fill = 0xcd;
alignas(256) std::array<std::uint32_t, OpCount * Threads * 4> Input{};
alignas(256) std::array<std::uint32_t, (OpCount + 2) * Threads * 2> Output{};

alignas(256) constexpr std::array<std::uint32_t, 57> GlobalAtomicsCode{
    0x34020083, 0x34040084, 0x340a0083, 0x7e2802ff, 0x05e471e1, 0x7e2a02ff, 0x05e471e1, 0x4a060280,
    0x7e2802ff, 0x05e471e1, 0x4a140081, 0xdcc88000, 0x00080a03, 0xbf8c3f70, 0xd70f6a1e, 0x02020608,
    0x7e3e0209, 0x503e3e80, 0xd70f6a1e, 0x02023cff, 0x00000200, 0x503e3e80, 0x34140088, 0xdced0000,
    0x147d0a1e, 0xbf8c0070, 0x4a0c0a80, 0xe0701000, 0x80011406, 0x7e0602ff, 0x00000400, 0x4a1414ff,
    0x00000100, 0xdcc98004, 0x14080a03, 0xbf8c3f70, 0x4a0c0aff, 0x00000200, 0xe0701000, 0x80011406,
    0x4a0602ff, 0x00000800, 0x4a1400ff, 0x00000100, 0x7e160280, 0xdd488000, 0x00080a03, 0xbf8c3f70,
    0x7da80090, 0x4a0602ff, 0x00000600, 0x7e1402ff, 0x000000f0, 0xdce88000, 0x00080a03, 0xbf8c3f70,
    0xbf810000
};

struct Row {
    std::uint32_t op;
    std::uint64_t memory;
    std::uint64_t data;
    std::uint64_t comparator;
    std::uint64_t returned;
    std::uint64_t final;
};

constexpr std::array<Row, 0> Rows{};

class GuestBlock {
public:
    explicit GuestBlock(bool writable) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "global atomics: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, writable);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }
    const std::uint8_t* Data() const { return block; }

private:
    std::uint8_t* block = nullptr;
};

void Put(std::vector<std::uint8_t>& image, std::uint32_t offset, std::uint64_t value, std::uint32_t bytes) {
    for (std::uint32_t byte = 0; byte < bytes; ++byte) image.at(offset + byte) = static_cast<std::uint8_t>(value >> (byte * 8u));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

const Row& RowOf(std::uint32_t op, std::uint32_t tid) {
    return Rows.at(op * RowsPerOp + tid % RowsPerOp);
}

std::uint32_t OpBytes(std::uint32_t op) {
    return op < NarrowOps ? 4u : 8u;
}

std::vector<std::uint8_t> Initial() {
    std::vector<std::uint8_t> image(BlockBytes, Fill);
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            Put(image, op * OpStride + tid * 8u, RowOf(op, tid).memory, OpBytes(op));
        }
    }
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Put(image, Special + tid * 8u, 0x1000u * tid, 4);
        Put(image, Special + Threads * 8u + tid * 8u, 0xa5a5a5a5u ^ tid, 4);
        Put(image, Special + Threads * 24u + tid * 8u, 0x0f0f0f00u + tid, 4);
        Put(image, Special + Threads * 32u + tid * 8u, 0x7fffffffffffff00ull + tid, 8);
    }
    Put(image, Special + Threads * 16u + 4u, 0x11u, 4);
    return image;
}

std::vector<std::uint8_t> Expected() {
    auto image = Initial();
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            Put(image, op * OpStride + tid * 8u, RowOf(op, tid).final, OpBytes(op));
        }
    }
    std::uint32_t total = 0x11u;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Put(image, Special + tid * 8u, 0x1000u * tid + tid + 1u, 4);
        Put(image, Special + Threads * 8u + tid * 8u, (0xa5a5a5a5u ^ tid) ^ (tid << 8u), 4);
        if (tid < 16u) Put(image, Special + Threads * 24u + tid * 8u, (0x0f0f0f00u + tid) | 0xf0u, 4);
        Put(image, Special + Threads * 32u + tid * 8u, 0x7fffffffffffff00ull + tid + 0x100u + tid, 8);
        total += (tid + 1u) << 8u;
    }
    Put(image, Special + Threads * 16u + 4u, total, 4);
    return image;
}

void FillInput() {
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto& row = RowOf(op, tid);
            auto* words = &Input[(op * Threads + tid) * 4u];
            if (OpBytes(op) == 4u) {
                words[0] = static_cast<std::uint32_t>(row.data);
                words[1] = static_cast<std::uint32_t>(row.comparator);
                words[2] = 0u;
                words[3] = 0u;
            } else {
                words[0] = static_cast<std::uint32_t>(row.data);
                words[1] = static_cast<std::uint32_t>(row.data >> 32u);
                words[2] = static_cast<std::uint32_t>(row.comparator);
                words[3] = static_cast<std::uint32_t>(row.comparator >> 32u);
            }
        }
    }
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const std::uint8_t* base) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    std::vector<std::uint32_t> userData(10, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[8] = static_cast<std::uint32_t>(address);
    userData[9] = static_cast<std::uint32_t>(address >> 32u);
    const std::span<const std::uint32_t> code(GlobalAtomicsCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void RunAtomics(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize) {
    const auto initial = Initial();
    std::memcpy(guest.Data(), initial.data(), BlockBytes);
    FillInput();
    Output.fill(Sentinel);
    Dispatch(device, waveSize, guest.Data());
    const auto wave = "global atomics: wave" + std::to_string(waveSize) + " ";
    for (std::uint32_t op = 0; op < OpCount; ++op) {
        for (std::uint32_t tid = 0; tid < Threads; ++tid) {
            const auto& row = RowOf(op, tid);
            const auto* words = &Output[(op * Threads + tid) * 2u];
            const auto returned = OpBytes(op) == 4u ? static_cast<std::uint64_t>(words[0]) : (static_cast<std::uint64_t>(words[1]) << 32u) | words[0];
            Require(returned == row.returned, wave + "op " + std::to_string(op) + " lane " + std::to_string(tid) + " returned " + Hex(returned) + ", expected " + Hex(row.returned));
        }
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> order;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto flat = Output[(OpCount * Threads + tid) * 2u];
        Require(flat == (0xa5a5a5a5u ^ tid), wave + "flat xor returned " + Hex(flat) + " to lane " + std::to_string(tid));
        order.emplace_back(Output[((OpCount + 1u) * Threads + tid) * 2u], tid);
    }
    std::sort(order.begin(), order.end());
    std::uint32_t running = 0x11u;
    for (const auto& [returned, tid] : order) {
        Require(returned == running, wave + "contended add returned " + Hex(returned) + " to lane " + std::to_string(tid) + ", expected " + Hex(running));
        running += (tid + 1u) << 8u;
    }
    const auto expected = Expected();
    for (std::uint32_t offset = 0; offset < BlockBytes; ++offset) {
        const auto actual = guest.Data()[offset];
        Require(actual == expected[offset], wave + "byte " + std::to_string(offset) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[offset]));
    }
}

void RunReadOnly(AgcDriver::VulkanDevice& device, GuestBlock& guest) {
    const auto initial = Initial();
    std::memcpy(guest.Data(), initial.data(), BlockBytes);
    FillInput();
    Dispatch(device, 32, guest.Data());
    for (std::uint32_t offset = 0; offset < BlockBytes; ++offset) {
        Require(guest.Data()[offset] == initial[offset], "global atomics: an atomic into a read-only range changed byte " + std::to_string(offset));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto capabilities = device->Target().supportedCapabilities;
        if (std::find(capabilities.begin(), capabilities.end(), Int64AtomicsCapability) == capabilities.end()) {
            std::puts("skipped, the device has no shaderBufferInt64Atomics");
            return VulkanTestSkipped;
        }
        GuestBlock writable(true);
        GuestBlock readOnly(false);
        RunAtomics(*device, writable, 32);
        RunAtomics(*device, writable, 64);
        RunReadOnly(*device, readOnly);
        std::puts("global atomics lanes tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
