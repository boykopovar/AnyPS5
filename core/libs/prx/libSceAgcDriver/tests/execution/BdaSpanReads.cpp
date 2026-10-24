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
#include <io.h>
#else
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t InputDwords = 768;
constexpr std::uint32_t Fill = 0xcdcdcdcdu;
constexpr std::uint32_t LoadBase = 0x100;
constexpr std::uint32_t OutputBase = 0x8000;

alignas(256) std::array<std::array<std::uint32_t, 64>, 4> Programs{};

std::span<const std::uint32_t> Code(bool coherent, bool barrier, std::uint32_t loadBase = LoadBase) {
    auto& program = Programs[(coherent ? 2u : 0u) + (barrier ? 1u : 0u)];
    std::vector<std::uint32_t> code{0x34040084, 0x4a0404ff, loadBase, 0x34080081, 0x36080882, 0x4a040504, 0x34060083, 0x4a0606ff, OutputBase, 0xdc388ff8 | (coherent ? 0x10000u : 0u), 0x04000002, 0xbf8c3f70};
    if (barrier) code.push_back(0xbf8a0000);
    code.insert(code.end(), {0xdc708000, 0x00000503, 0xdc708004, 0x00000703, 0xbf810000});
    std::copy(code.begin(), code.end(), program.begin());
    return std::span<const std::uint32_t>(program.data(), code.size());
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "BDA span reads: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::vector<std::uint32_t> Initial() {
    std::vector<std::uint32_t> memory(BlockBytes / 4u, Fill);
    for (std::uint32_t dword = 0; dword < InputDwords; ++dword) memory[dword] = 0x52000000u + dword * 0x00010203u;
    return memory;
}

std::uint32_t Bytes(const std::vector<std::uint32_t>& memory, std::uint32_t address) {
    std::uint32_t value = 0;
    for (std::uint32_t byte = 0; byte < 4u; ++byte) {
        const std::uint32_t at = address + byte;
        value |= ((memory[at / 4u] >> (at % 4u * 8u)) & 0xffu) << (byte * 8u);
    }
    return value;
}

std::vector<std::uint32_t> Expected() {
    const auto initial = Initial();
    auto memory = initial;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t load = LoadBase + tid * 16u + (tid & 1u) * 2u - 8u;
        memory[OutputBase / 4u + tid * 2u] = Bytes(initial, load + 4u);
        memory[OutputBase / 4u + tid * 2u + 1u] = Bytes(initial, load + 12u);
    }
    return memory;
}

ShaderRecompiler::RecompileResult Recompile(std::span<const std::uint32_t> code, std::uint32_t waveSize, std::span<const std::uint32_t> userData, const ShaderRecompiler::SpirvTarget& target) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, regions},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "BDA span reads: cannot capture the fault report");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }
    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        std::array<char, 4096> buffer{};
        for (std::size_t read = 0; (read = std::fread(buffer.data(), 1, buffer.size(), file)) != 0;) text.append(buffer.data(), read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }
private:
    std::FILE* file = nullptr;
    int saved = -1;
};

bool RunFault(AgcDriver::VulkanDevice& device, GuestBlock& guest, bool coherent, bool barrier, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const std::string& name, bool mixed = false) {
    const auto loadBase = static_cast<std::uint32_t>(mixed ? BlockBytes - 512u : BlockBytes + LoadBase);
    const auto code = Code(coherent, barrier, loadBase);
    auto memory = Initial();
    if (mixed) {
        for (std::size_t dword = memory.size() - 128u; dword < memory.size(); ++dword) memory[dword] = 0x51000000u + static_cast<std::uint32_t>(dword) * 0x00010203u;
    }
    std::memcpy(guest.Data(), memory.data(), BlockBytes);
    auto expected = memory;
    if (mixed) {
        for (std::uint32_t tid = 0u; tid < Threads; ++tid) {
            const auto load = loadBase + tid * 16u + (tid & 1u) * 2u - 8u;
            if (load + 16u > BlockBytes) continue;
            expected[OutputBase / 4u + tid * 2u] = Bytes(memory, load + 4u);
            expected[OutputBase / 4u + tid * 2u + 1u] = Bytes(memory, load + 12u);
        }
        Require(expected != memory, "BDA span reads: the mixed control has no distinguishable valid lane writes");
    }
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    Require(!GuestAllocations::Mutation().Overlaps(reinterpret_cast<const void*>(address + BlockBytes), Threads * 16u + LoadBase + 16u), "BDA span reads: the fault input overlaps a registered allocation");
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    const auto result = Recompile(code, waveSize, userData, target);
    Require(std::ranges::any_of(result.bindings, [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; }), "BDA span reads: the fault control has no BDA table");
    std::string failure;
    StderrCapture log;
    try {
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
    } catch (const std::exception& error) {
        failure = error.what();
    }
    const auto deferred = log.Finish();
    if (failure.empty()) failure = deferred;
    Require(failure.find("BDA access failed:") != std::string::npos && failure.find("reason=1") != std::string::npos, "BDA span reads: no unmapped fault report: " + failure);
    std::uint32_t changed = 0u;
    for (std::size_t dword = 0; dword < memory.size(); ++dword) {
        std::uint32_t actual = 0u;
        std::memcpy(&actual, guest.Data() + dword * 4u, sizeof(actual));
        if (actual != expected[dword]) ++changed;
    }
    std::printf("BDA fault span %s%s coherent=%u barrier=%u changed=%u refused=1\n", name.c_str(), mixed ? "-mixed" : "", coherent ? 1u : 0u, barrier ? 1u : 0u, changed);
    return changed != 0u;
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, bool coherent, bool barrier, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const std::string& name) {
    const auto code = Code(coherent, barrier);
    auto memory = Initial();
    std::memcpy(guest.Data(), memory.data(), BlockBytes);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    const auto result = Recompile(code, waveSize, userData, target);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::memcpy(memory.data(), guest.Data(), BlockBytes);
    const auto expected = Expected();
    for (std::size_t dword = 0; dword < memory.size(); ++dword) {
        Require(memory[dword] == expected[dword], "BDA span reads: " + name + (coherent ? " glc" : "") + (barrier ? " barrier" : "") + " byte " + std::to_string(dword * 4u) + " is " + Hex(memory[dword]) + ", expected " + Hex(expected[dword]));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        const auto native = device->Target();
        const auto split = device->ComputeTarget(32);
        std::uint32_t faultingWrites = 0u;
        for (const bool coherent : {false, true}) {
            for (const bool barrier : {false, true}) {
                Run(*device, guest, coherent, barrier, 32, native, "wave32");
                Run(*device, guest, coherent, barrier, 64, native, "wave64");
                if (split.subgroupSize != native.subgroupSize) Run(*device, guest, coherent, barrier, 64, split, "wave64 split");
                faultingWrites += RunFault(*device, guest, coherent, barrier, 32, native, "wave32") ? 1u : 0u;
                faultingWrites += RunFault(*device, guest, coherent, barrier, 64, native, "wave64") ? 1u : 0u;
                if (split.subgroupSize != native.subgroupSize) faultingWrites += RunFault(*device, guest, coherent, barrier, 64, split, "wave64-split") ? 1u : 0u;
                if (barrier && native.subgroupSize == 32u) {
                    faultingWrites += RunFault(*device, guest, coherent, barrier, 32, native, "wave32", true) ? 1u : 0u;
                    faultingWrites += RunFault(*device, guest, coherent, barrier, 64, native, "wave64", true) ? 1u : 0u;
                    if (split.subgroupSize != native.subgroupSize) faultingWrites += RunFault(*device, guest, coherent, barrier, 64, split, "wave64-split", true) ? 1u : 0u;
                }
            }
        }
        if (native.subgroupSize != 32u) std::printf("skipped, mixed-lane barrier BDA fault controls need subgroup size 32, device reports %u\n", native.subgroupSize);
        Require(faultingWrites == 0u, "BDA span reads: " + std::to_string(faultingWrites) + " faulting programs still wrote guest memory");
        std::puts("BDA span read tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
