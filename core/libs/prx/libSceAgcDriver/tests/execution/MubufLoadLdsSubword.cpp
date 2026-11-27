#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 128u;
constexpr std::uint32_t LdsDwords = 512u;
constexpr std::uint32_t Guard = 0xa5a5a5a5u;
constexpr std::uint32_t VdataGuard = 0x13572468u;
constexpr std::uint32_t RawFormat = 0x31016facu;
constexpr std::uint32_t OpFormatX = 0u;
constexpr std::uint32_t OpUbyte = 8u;
constexpr std::uint32_t OpSbyte = 9u;
constexpr std::uint32_t OpUshort = 10u;
constexpr std::uint32_t OpSshort = 11u;
constexpr std::uint32_t OpDword = 12u;
constexpr std::uint32_t Full = 0xffffffffu;
alignas(256) std::array<std::uint32_t, 512> Input{};
alignas(256) std::array<std::uint32_t, Threads * 8u + 8u> Output{};

struct Case {
    std::uint32_t opcode;
    std::uint32_t format;
    std::uint32_t vdata;
    std::uint32_t offset;
    std::uint32_t m0;
    std::uint32_t execMask;
    bool perLane;
    std::uint32_t laneShift;
    std::uint32_t numRecords;
    std::uint32_t pinned;
    bool idxen = false;
    std::uint32_t stride = 0u;
    std::uint32_t soffset = 0u;
    std::uint32_t hardwareLane0 = 0u;
};

std::array<std::uint32_t, 4> Descriptor(const void* bytes, std::uint32_t size, std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), size, format};
}

std::uint32_t Bytes(std::uint32_t address, std::uint32_t count) {
    std::uint32_t value = 0u;
    for (std::uint32_t index = 0u; index < count; ++index) value |= static_cast<std::uint32_t>(reinterpret_cast<const std::uint8_t*>(Input.data())[address + index]) << (8u * index);
    return value;
}

std::uint32_t SignExtend(std::uint32_t value, std::uint32_t bits) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << (32u - bits)) >> (32u - bits));
}

struct Access {
    std::uint32_t bytes;
    bool sign;
};

Access AccessOf(const Case& item) {
    switch (item.opcode) {
    case OpUbyte: return {1u, false};
    case OpSbyte: return {1u, true};
    case OpUshort: return {2u, false};
    case OpSshort: return {2u, true};
    case OpFormatX: return item.format == 5u ? Access{1u, false} : item.format == 6u ? Access{1u, true} : item.format == 11u ? Access{2u, false} : item.format == 12u ? Access{2u, true} : Access{4u, false};
    default: return {4u, false};
    }
}

std::uint32_t Loaded(const Case& item, std::uint32_t address) {
    if (item.pinned != 0u) return item.pinned;
    const auto access = AccessOf(item);
    if (static_cast<std::uint64_t>(address) + access.bytes > item.numRecords) return 0u;
    const auto value = Bytes(address, access.bytes);
    return access.sign ? SignExtend(value, access.bytes * 8u) : value;
}

std::array<std::uint32_t, LdsDwords> ExpectedLds(const Case& item, std::uint32_t wave) {
    std::array<std::uint32_t, LdsDwords> lds{};
    lds.fill(Guard);
    for (std::uint32_t lane = 0u; lane < wave; ++lane) {
        const auto target = (item.m0 & 0xffffu) + item.offset + lane * 4u;
        if (((item.execMask >> (lane & 31u)) & 1u) == 0u || target >= LdsDwords * 4u) continue;
        lds[target >> 2u] = Loaded(item, (item.perLane ? lane << item.laneShift : 0u) * (item.idxen ? item.stride : 1u) + item.offset + item.soffset);
    }
    return lds;
}

std::uint32_t FormatWord(const Case& item) {
    return item.opcode == OpFormatX ? (RawFormat & ~(0x7fu << 12u)) | (item.format << 12u) : RawFormat;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t wave, const Case& item) {
    Output.fill(0xfeedfaceu);
    const auto opcode = item.opcode;
    const auto word0 = 0xe0000000u | ((opcode & 0x7fu) << 18u) | (((opcode >> 7u) & 1u) << 25u) | (1u << 16u) | ((item.idxen ? 1u : 0u) << 13u) | ((item.idxen ? 0u : 1u) << 12u) | item.offset;
    alignas(256) const auto code = std::to_array<std::uint32_t>({
        0x34020082u, 0x7e1002ffu, 0x13572468u, 0x7e0e02ffu, 0xa5a5a5a5u, 0xd8340000u, 0x00000701u, 0xd8340200u,
        0x00000701u, 0xd8340400u, 0x00000701u, 0xd8340600u, 0x00000701u, 0xbf8cc07fu, 0xbf8a0000u, 0x3606000au,
        0x3406060bu, 0xbefc0308u, 0xbe8c037eu, 0xbe8d037fu, 0x877e097eu, 0x877f097fu, 0xbf800000u, word0,
        0x00000003u | (item.vdata << 8u) | ((0x80u + item.soffset) << 24u), 0xbf8c3f70u, 0xbefe030cu, 0xbeff030du, 0xbf8a0000u, 0x34040085u, 0xd8d80000u, 0x04000001u,
        0xd8d80200u, 0x05000001u, 0xd8d80400u, 0x06000001u, 0xd8d80600u, 0x07000001u, 0xbf8cc07fu, 0xe0701000u,
        0x80010402u, 0xe0701004u, 0x80010502u, 0xe0701008u, 0x80010602u, 0xe070100cu, 0x80010702u, 0xe0701010u,
        0x80010802u, 0xbf810000u
    });
    std::array<std::uint32_t, 14> userData{};
    auto input = Descriptor(Input.data(), item.numRecords, FormatWord(item));
    input[1] |= item.stride << 16u;
    const auto output = Descriptor(Output.data(), Threads * 32u, RawFormat);
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4u);
    userData[8] = item.m0;
    userData[9] = item.execMask;
    userData[10] = item.perLane ? wave - 1u : 0u;
    userData[11] = item.laneShift;
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, LdsDwords, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0u, {}},
        {wave, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const auto expected = ExpectedLds(item, wave);
    if (item.hardwareLane0 != 0u) Require(expected[((item.m0 & 0xffffu) + item.offset) >> 2u] == item.hardwareLane0, "MUBUF LDS model differs from the measured lane 0 dword");
    for (std::uint32_t tid = 0u; tid < Threads; ++tid) {
        for (std::uint32_t window = 0u; window < 4u; ++window) {
            const auto actual = Output[tid * 8u + window];
            if (actual != expected[window * Threads + tid]) {
                std::fprintf(stderr, "wave=%u op=%u fmt=%u off=%u m0=%x tid=%u window=%u actual=%08x expected=%08x\n", wave, item.opcode, item.format, item.offset, item.m0, tid, window, actual, expected[window * Threads + tid]);
            }
            Require(actual == expected[window * Threads + tid], "MUBUF LDS load wrote the wrong LDS dword");
        }
        Require(Output[tid * 8u + 4u] == VdataGuard, "MUBUF LDS load wrote a VGPR");
    }
}

bool Refused(const std::array<std::uint32_t, 2>& code) {
    try {
        static_cast<void>(ShaderRecompiler::DecodeRdnaEncodedInstruction(0u, code, 0u));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void CheckDecoder() {
    constexpr std::array<std::uint32_t, 6> accepted{OpFormatX, OpUbyte, OpSbyte, OpUshort, OpSshort, OpDword};
    for (std::uint32_t opcode = 0u; opcode < 256u; ++opcode) {
        const auto isAccepted = std::ranges::find(accepted, opcode) != accepted.end();
        for (const auto word1 : {0x80000000u, 0x80000700u}) {
            const std::array<std::uint32_t, 2> code{0xe0010000u | ((opcode & 0x7fu) << 18u) | (((opcode >> 7u) & 1u) << 25u), word1};
            Require(Refused(code) != isAccepted, isAccepted ? "supported MUBUF LDS opcode refused" : "unsupported MUBUF LDS opcode accepted");
        }
    }
    for (const auto opcode : accepted) {
        const auto base = 0xe0010000u | ((opcode & 0x7fu) << 18u);
        Require(Refused({base | (1u << 17u), 0x80000000u}), "MUBUF LDS accepted a reserved bit");
        Require(Refused({base, 0x80800000u}), "MUBUF LDS accepted TFE");
        Require(Refused({base, 0x80200000u}), "MUBUF LDS accepted a reserved bit");
    }
}

std::vector<Case> Cases() {
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 10> loads{{{OpUbyte, 0u}, {OpSbyte, 0u}, {OpUshort, 0u}, {OpSshort, 0u}, {OpDword, 0u},
        {OpFormatX, 5u}, {OpFormatX, 6u}, {OpFormatX, 11u}, {OpFormatX, 12u}, {OpFormatX, 22u}}};
    std::vector<Case> cases;
    for (const auto& [opcode, format] : loads) {
        cases.push_back({opcode, format, 0u, 0u, 0x40u, Full, true, 2u, 2048u, 0u});
        cases.push_back({opcode, format, 0u, 0u, 0x10100u, 0x55555555u, true, 2u, 2048u, 0u});
        cases.push_back({opcode, format, 0u, 8u, 3u, Full, true, 2u, 2048u, 0u});
        cases.push_back({opcode, format, 0u, 0u, 0x40u, Full, true, 2u, 64u, 0u});
        cases.push_back({opcode, format, 0u, 0u, 2048u - 64u, Full, true, 2u, 2048u, 0u});
        cases.push_back({opcode, format, 0u, 0u, 2046u, Full, true, 2u, 2048u, 0u});
        cases.push_back({opcode, format, 0u, 0u, 2048u, Full, true, 2u, 2048u, 0u});
        cases.push_back({opcode, format, 0u, 60u, 2048u - 64u, Full, true, 2u, 2048u, 0u});
        cases.push_back({opcode, format, 0u, 2044u, 0u, Full, true, 2u, 2048u, 0u});
        cases.push_back({opcode, format, 8u, 0u, 0x40u, Full, true, 2u, 2048u, 0u});
    }
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 9> converted{{{1u, 0x3f018182u}, {2u, 0xbf800000u}, {3u, 0x43010000u}, {4u, 0xc2fe0000u},
        {7u, 0x3f088189u}, {8u, 0xbf6effdeu}, {9u, 0x47088100u}, {10u, 0xc6eefe00u}, {13u, 0xb9102000u}}};
    for (const auto opcode : {OpUbyte, OpSbyte, OpUshort, OpSshort, OpDword}) {
        cases.push_back({opcode, 0u, 0u, 0u, 0x40u, 0u, true, 2u, 2048u, 0u});
        cases.push_back({opcode, 0u, 0u, 8u, 0x40u, Full, true, 0u, 2048u, 0u, true, 16u, 16u, opcode == OpDword ? 0x3d34332au : 0u});
        cases.push_back({opcode, 0u, 0u, 8u, 0x40u, Full, true, 2u, 2048u, 0u, false, 0u, 20u});
    }
    for (const auto& [format, bits] : converted) cases.push_back({OpFormatX, format, 0u, 0u, 0x80u, Full, false, 0u, 2048u, bits});
    return cases;
}
}

int main(int argc, char** argv) {
    try {
        const auto wave = argc == 2 ? static_cast<std::uint32_t>(std::stoul(argv[1])) : 32u;
        Require(wave == 32u || wave == 64u, "invalid wave size");
        CheckDecoder();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, the device's subgroups are narrower than a wave (%u lanes)\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        auto* bytes = reinterpret_cast<std::uint8_t*>(Input.data());
        for (std::uint32_t index = 0u; index < Input.size() * 4u; ++index) bytes[index] = static_cast<std::uint8_t>((0x81u + index * 7u) ^ (index >> 3u));
        const auto cases = Cases();
        for (const auto& item : cases) Run(*device, wave, item);
        std::printf("MUBUF LDS subword and format: %zu cases passed\n", cases.size());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
