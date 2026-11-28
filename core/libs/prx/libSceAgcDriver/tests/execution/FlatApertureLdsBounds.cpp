#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Results = 16;
constexpr std::uint32_t LdsBytes = 4096;
alignas(256) std::array<std::uint32_t, 64 * 4> Input{};
alignas(256) std::array<std::uint32_t, 64 * Results> Output{};
std::vector<std::string> Mismatches;

alignas(256) constexpr std::array<std::uint32_t, 54> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbe9903ffu, 0x9e3779b1u, 0x3654009fu, 0xbe9a03a0u,
    0xd569002bu, 0x0000332au, 0x3a5656ffu, 0x5a5a5a5au, 0x34585482u, 0xd8340000u, 0x00002b2cu, 0x4a5454a0u,
    0x809a811au, 0xbf07801au, 0xbf85fff5u, 0xbf8c0070u,
    0xbe9604ebu, 0x7e5c0304u, 0x7e5e0217u,
    0xdc300000u, 0x0a7d002eu,
    0xdc200000u, 0x0b7d002eu,
    0xdc280000u, 0x0c7d002eu,
    0xdc340000u, 0x0d7d002eu,
    0x7e1e0280u,
    0xd8e80000u, 0x10000004u,
    0xbf8c0070u, 0x7e600305u, 0x7e620217u,
    0xdc700000u, 0x007d0630u,
    0xbf8c0070u,
    0xd8d80000u, 0x11000007u,
    0xd8d80004u, 0x12000007u,
    0xdc300000u, 0x137d0030u,
    0xbf8c0070u,
    0xe0781000u, 0x80010a02u,
    0xe0781010u, 0x80010e02u,
    0xe0741020u, 0x80011202u,
    0xbf810000u,
};

constexpr std::array<std::uint32_t, 32> Loads{
    0u, 4u, 1u, 2u, 3u, 4092u, 4093u, 4094u, 4095u, 4096u, 4097u, 4100u, 8188u, 8192u, 65532u, 65536u,
    0x10004u, 0xfffffffcu, 0x80000000u, 4090u, 4088u, 6u, 5u, 7u, 0x1000u, 0x3ffcu, 0xfffcu, 0x7ffffffcu, 0x100000u, 12u, 16u, 20u,
};

struct Store {
    std::uint32_t address;
    std::uint32_t readback;
};

constexpr std::array<Store, 16> Stores{{
    {2048u, 2048u}, {4096u, 0u}, {4100u, 4u}, {8200u, 8u}, {65548u, 12u}, {0xfffffff0u, 4080u}, {4094u, 4092u}, {2054u, 2056u},
    {2061u, 2060u}, {2071u, 2068u}, {2080u, 2080u}, {0x10020u, 32u}, {5000u, 904u}, {2100u, 2100u}, {0x80000040u, 64u}, {2200u, 2200u},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> Row(std::uint32_t lane) {
    Store store = lane % 32u < Stores.size() ? Stores[lane % 32u] : Store{2300u + 16u * (lane % 32u - 16u), 2300u + 16u * (lane % 32u - 16u)};
    if (lane >= 32u && store.address < LdsBytes) {
        store.address += 1024u;
        store.readback += 1024u;
    }
    return {Loads[lane % 32u], store.address, 0xc0000000u + lane, store.readback};
}

std::uint32_t Load(const std::vector<std::uint8_t>& lds, std::uint64_t address, std::uint32_t bytes) {
    if (address + bytes > LdsBytes) return 0u;
    std::uint32_t value = 0u;
    for (std::uint32_t byte = 0; byte < bytes; ++byte) value |= static_cast<std::uint32_t>(lds[address + byte]) << (8u * byte);
    return value;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const std::string& run) {
    Input.fill(0u);
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const auto row = Row(lane);
        std::copy(row.begin(), row.end(), Input.begin() + lane * 4u);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), waveSize * 16u);
    const auto output = BufferDescriptor(Output.data(), waveSize * Results * 4u);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span{Code})}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, LdsBytes / 4u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    std::vector<std::uint8_t> lds(LdsBytes);
    for (std::uint32_t dword = 0; dword < LdsBytes / 4u; ++dword) {
        const auto value = (dword * 0x9e3779b1u) ^ 0x5a5a5a5au;
        for (std::uint32_t byte = 0; byte < 4u; ++byte) lds[dword * 4u + byte] = static_cast<std::uint8_t>(value >> (8u * byte));
    }
    std::vector<std::array<std::uint32_t, 10>> expected(waveSize);
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const std::uint64_t address = Row(lane)[0];
        expected[lane] = {Load(lds, address, 4u), Load(lds, address, 1u), Load(lds, address, 2u), Load(lds, address, 4u), Load(lds, address + 4u, 4u), 0u, Load(lds, address, 1u), 0u, 0u, 0u};
    }
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const auto row = Row(lane);
        if (row[1] + 4u <= LdsBytes) {
            for (std::uint32_t byte = 0; byte < 4u; ++byte) lds[row[1] + byte] = static_cast<std::uint8_t>(row[2] >> (8u * byte));
        }
    }
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const auto row = Row(lane);
        expected[lane][7] = Load(lds, row[3], 4u);
        expected[lane][8] = Load(lds, static_cast<std::uint64_t>(row[3]) + 4u, 4u);
        expected[lane][9] = Load(lds, row[1], 4u);
    }
    constexpr const char* names[10] = {"flat_load_dword", "flat_load_ubyte", "flat_load_ushort", "flat_load_dwordx2.x", "flat_load_dwordx2.y", "v_mov_b32", "ds_read_u8", "ds_read_b32 after the store", "ds_read_b32 offset:4 after the store", "flat_load_dword after the store"};
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const auto row = Row(lane);
        for (std::uint32_t index = 0; index < 10u; ++index) {
            if (index == 4u && row[0] > 0xffffffffu - 4u) continue;
            const auto actual = Output[lane * Results + index];
            if (actual != expected[lane][index]) {
                Mismatches.push_back(run + " lane " + std::to_string(lane) + " load at " + Hex(row[0]) + ", store at " + Hex(row[1]) + ": " + names[index] + " is " + Hex(actual) + ", expected " + Hex(expected[lane][index]));
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 32, device->Target(), "wave32");
        Run(*device, 64, device->Target(), "wave64");
        Run(*device, 64, device->ComputeTarget(32), "wave64 split");
        for (const auto& mismatch : Mismatches) std::cerr << mismatch << '\n';
        Require(Mismatches.empty(), std::to_string(Mismatches.size()) + " flat LDS aperture bound mismatches");
        std::puts("flat aperture LDS bound tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
