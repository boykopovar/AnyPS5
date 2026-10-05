#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 128;
constexpr std::uint32_t Results = 8;
constexpr std::uint32_t Written = 5;
constexpr std::uint32_t TableDwords = 256;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Sentinel = 0xabcdabcdu;
constexpr std::uint32_t PositiveOffsetDword = 16;
constexpr std::uint32_t NegativeOffsetDword = 121;
constexpr std::uint32_t MaskedExecLo = 0x55555555u;
alignas(256) std::array<std::uint32_t, TableDwords> Table{};
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 39> AddtidCode{
    0x34020085, 0x34040082, 0xbe880300, 0x8709ff01, 0x0000ffff, 0x800aff08, 0x00000200, 0x820b8009,
    0x7e080280, 0x460808c1, 0x480808c1, 0x7e0a02ff, 0xabcdabcd, 0x7e0c02ff, 0xabcdabcd, 0x7e0e02ff,
    0xabcdabcd, 0xe0301000, 0x80000802, 0xdc588040, 0x05080000, 0xdc588fe4, 0x060a0000, 0xbefe03ff,
    0x55555555, 0xdc588000, 0x07080000, 0xbefe03c1, 0xe0701000, 0x80010401, 0xe0701004, 0x80010501,
    0xe0701008, 0x80010601, 0xe070100c, 0x80010701, 0xe0701010, 0x80010801, 0xbf810000,
};

struct Workgroup {
    std::uint32_t threads;
    std::uint32_t waveSize;

    [[nodiscard]] std::string Name() const {
        return "wave" + std::to_string(waveSize) + " " + std::to_string(threads) + " threads";
    }
};

constexpr std::array<Workgroup, 4> Workgroups{{{32, 32}, {128, 32}, {64, 64}, {128, 64}}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

ShaderRecompiler::RecompileResult Recompile(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const Workgroup& workgroup, std::span<const std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{workgroup.threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {workgroup.waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::vector<std::uint32_t> UserData() {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto table = BufferDescriptor(Table.data(), static_cast<std::uint32_t>(Table.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(table.begin(), table.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    return userData;
}

void Run(AgcDriver::VulkanDevice& device, const Workgroup& workgroup) {
    for (std::uint32_t i = 0; i < Table.size(); ++i) Table[i] = 0x51000000u + i * 0x00010203u;
    Output.fill(Untouched);
    const auto userData = UserData();
    const std::span<const std::uint32_t> code(AddtidCode);
    const auto result = Recompile(device, code, workgroup, userData);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const Workgroup& workgroup) {
    constexpr std::array<const char*, 4> names{"positive offset", "negative offset", "masked by exec", "buffer load"};
    for (std::uint32_t tid = 0; tid < MaxThreads; ++tid) {
        const auto* out = &Output[tid * Results];
        const auto where = "global load addtid: " + workgroup.Name() + " thread " + std::to_string(tid);
        if (tid >= workgroup.threads) {
            for (std::uint32_t j = 0; j < Results; ++j) {
                Require(out[j] == Untouched, where + " is outside the workgroup but result " + std::to_string(j) + " was written");
            }
            continue;
        }
        const auto lane = out[0];
        Require(lane < workgroup.waveSize, where + " reports lane " + std::to_string(lane));
        const bool active = lane >= 32u || ((MaskedExecLo >> lane) & 1u) != 0u;
        const std::array<std::uint32_t, 4> expected{Table[PositiveOffsetDword + lane], Table[NegativeOffsetDword + lane], active ? Table[lane] : Sentinel, Table[tid]};
        for (std::uint32_t j = 0; j < expected.size(); ++j) {
            Require(out[j + 1u] == expected[j], where + " lane " + std::to_string(lane) + " " + names[j] + " is " + Hex(out[j + 1u]) + ", expected " + Hex(expected[j]));
        }
        for (std::uint32_t j = Written; j < Results; ++j) {
            Require(out[j] == Untouched, where + " result " + std::to_string(j) + " was written");
        }
    }
}

void CheckRejected(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& reason) {
    const auto userData = UserData();
    std::string failure;
    try {
        static_cast<void>(Recompile(device, code, Workgroups[0], userData));
    } catch (const std::exception& error) {
        failure = error.what();
    }
    Require(failure.find(reason) != std::string::npos, "global load addtid: expected '" + reason + "', got '" + failure + "'");
}

void CheckRejections(const AgcDriver::VulkanDevice& device) {
    alignas(256) static constexpr std::array<std::uint32_t, 3> flatSegment{0xdc580040, 0x05080000, 0xbf810000};
    alignas(256) static constexpr std::array<std::uint32_t, 3> scratchSegment{0xdc584040, 0x05080000, 0xbf810000};
    alignas(256) static constexpr std::array<std::uint32_t, 3> noScalarBase{0xdc588040, 0x057d0000, 0xbf810000};
    CheckRejected(device, flatSegment, "global_load_dword_addtid is available only in the global segment");
    CheckRejected(device, scratchSegment, "global_load_dword_addtid is available only in the global segment");
    CheckRejected(device, noScalarBase, "global_load_dword_addtid supports only an SGPR pair as base address");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const auto& workgroup : Workgroups) {
            Run(*device, workgroup);
            Check(workgroup);
        }
        CheckRejections(*device);
        std::puts("global load addtid tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
