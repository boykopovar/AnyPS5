#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::int32_t Zero = -1;
constexpr std::int32_t OneFloat = -2;
constexpr std::int32_t OneInt = -3;
constexpr std::int32_t Any = -4;

constexpr std::uint32_t Sel(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t w) {
    return x | (y << 3u) | (z << 6u) | (w << 9u);
}

constexpr std::uint32_t Identity = Sel(4, 5, 6, 7);

struct Row {
    const char* name;
    bool typed;
    bool idxen;
    std::uint32_t format;
    std::uint32_t offset;
    std::uint32_t records;
    std::uint32_t stride;
    std::uint32_t dstSel;
    bool unorm;
    std::array<std::int32_t, 4> expect;
    bool dword = false;
};

const std::vector<Row> Rows{
    {"8_UNORM off 2 nr 3", false, false, 1, 2, 3, 0, Identity, true, {OneFloat, Any, Any, Any}},
    {"8_UNORM off 0 nr 1", false, false, 1, 0, 1, 0, Identity, true, {OneFloat, Any, Any, Any}},
    {"16_UNORM off 4 nr 6", false, false, 7, 4, 6, 0, Identity, true, {OneFloat, Any, Any, Any}},
    {"32_32_UINT off 0 nr 4", false, false, 62, 0, 4, 0, Identity, false, {Zero, Zero, Zero, Zero}},
    {"32_32_UINT off 0 nr 7", false, false, 62, 0, 7, 0, Identity, false, {Zero, Zero, Zero, Zero}},
    {"32_32_UINT off 0 nr 8", false, false, 62, 0, 8, 0, Identity, false, {0, 1, Any, Any}},
    {"32_32_32_32_UINT off 0 nr 8", false, false, 75, 0, 8, 0, Identity, false, {Zero, Zero, Zero, Zero}},
    {"32_32_32_32_UINT off 0 nr 12", false, false, 75, 0, 12, 0, Identity, false, {Zero, Zero, Zero, Zero}},
    {"32_32_32_32_UINT off 0 nr 15", false, false, 75, 0, 15, 0, Identity, false, {Zero, Zero, Zero, Zero}},
    {"32_32_32_32_UINT off 0 nr 16", false, false, 75, 0, 16, 0, Identity, false, {0, 1, 2, 3}},
    {"32_32_UINT off 4 nr 11", false, false, 62, 4, 11, 0, Identity, false, {Zero, Zero, Zero, Zero}},
    {"32_32_UINT off 4 nr 12", false, false, 62, 4, 12, 0, Identity, false, {1, 2, Any, Any}},
    {"structured 32_32_32_32_UINT stride 8", false, true, 75, 0, 64, 8, Identity, false, {Zero, Zero, Zero, Zero}},
    {"structured 32_32_UINT stride 4", false, true, 62, 0, 64, 4, Identity, false, {Zero, Zero, Zero, Zero}},
    {"structured 32_32_UINT stride 8 off 4", false, true, 62, 4, 64, 8, Identity, false, {Zero, Zero, Zero, Zero}},
    {"structured 32_32_32_32_UINT stride 16", false, true, 75, 0, 64, 16, Identity, false, {0, 1, 2, 3}},
    {"structured 32_32_UINT stride 8", false, true, 62, 0, 64, 8, Identity, false, {0, 1, Any, Any}},
    {"tbuffer 32_32_UINT off 0 nr 4", true, false, 62, 0, 4, 0, Identity, false, {Zero, Zero, Any, Any}},
    {"tbuffer 32_32_UINT off 0 nr 8", true, false, 62, 0, 8, 0, Identity, false, {0, 1, Any, Any}},
    {"tbuffer 8_UNORM off 2 nr 3", true, false, 1, 2, 3, 0, Identity, true, {OneFloat, Any, Any, Any}},
    {"tbuffer structured 32_32_UINT stride 4", true, true, 62, 0, 64, 4, Identity, false, {Zero, Zero, Any, Any}},
    {"tbuffer structured 32_32_UINT stride 8", true, true, 62, 0, 64, 8, Identity, false, {0, 1, Any, Any}},
    {"structured dword stride 6 off 4", false, true, 20, 4, 64, 6, Identity, false, {Zero, Any, Any, Any}, true},
    {"structured dword stride 8 off 4", false, true, 20, 4, 64, 8, Identity, false, {1, Any, Any, Any}, true},
    {"dst_sel 2222 32_32_UINT", false, false, 62, 0, 64, 0, Sel(2, 2, 2, 2), false, {0, 1, 0, 1}},
    {"dst_sel 2222 32_32_32_UINT", false, false, 72, 0, 64, 0, Sel(2, 2, 2, 2), false, {0, 1, 2, 0}},
    {"dst_sel 2222 32_UINT", false, false, 20, 0, 64, 0, Sel(2, 2, 2, 2), false, {0, 0, 0, 0}},
    {"dst_sel 3333 32_UINT", false, false, 20, 0, 64, 0, Sel(3, 3, 3, 3), false, {0, 0, 0, 0}},
    {"dst_sel 3333 32_32_UINT", false, false, 62, 0, 64, 0, Sel(3, 3, 3, 3), false, {Zero, Zero, Zero, Zero}},
    {"dst_sel 3333 32_32_32_UINT", false, false, 72, 0, 64, 0, Sel(3, 3, 3, 3), false, {Zero, Zero, Zero, Zero}},
    {"dst_sel 3333 8_UNORM", false, false, 1, 0, 4, 0, Sel(3, 3, 3, 3), true, {OneFloat, OneFloat, OneFloat, OneFloat}},
    {"dst_sel 0000 32_32_UINT", false, false, 62, 0, 64, 0, Sel(0, 0, 0, 0), false, {Zero, Zero, Zero, Zero}},
    {"dst_sel 1111 32_32_UINT", false, false, 62, 0, 64, 0, Sel(1, 1, 1, 1), false, {OneInt, OneInt, OneInt, OneInt}},
    {"dst_sel 0101 32_32_UINT", false, false, 62, 0, 64, 0, Sel(0, 1, 0, 1), false, {Zero, OneInt, Zero, OneInt}},
    {"dst_sel 0000 32_UINT", false, false, 20, 0, 64, 0, Sel(0, 0, 0, 0), false, {Zero, Zero, Zero, Zero}},
    {"dst_sel 1111 32_UINT", false, false, 20, 0, 64, 0, Sel(1, 1, 1, 1), false, {OneInt, OneInt, OneInt, OneInt}},
    {"dst_sel 4444 32_UINT", false, false, 20, 0, 64, 0, Sel(4, 4, 4, 4), false, {0, 0, 0, 0}},
    {"dst_sel 4567 32_UINT (inferred past count)", false, false, 20, 0, 64, 0, Sel(4, 5, 6, 7), false, {0, 0, 0, 0}},
    {"dst_sel 1111 8_UNORM", false, false, 1, 0, 64, 0, Sel(1, 1, 1, 1), true, {OneFloat, OneFloat, OneFloat, OneFloat}},
    {"dst_sel 7654 32_32_32_32_UINT", false, false, 75, 0, 64, 0, Sel(7, 6, 5, 4), false, {3, 2, 1, 0}},
    {"dst_sel 4455 32_32_UINT", false, false, 62, 0, 64, 0, Sel(4, 4, 5, 5), false, {0, 0, 1, 1}},
    {"dst_sel 2222 32_32_UINT out of bounds", false, false, 62, 0, 4, 0, Sel(2, 2, 2, 2), false, {Zero, Zero, Zero, Zero}},
    {"unknown FORMAT 0", false, false, 0, 0, 64, 0, Identity, false, {Zero, Zero, Zero, Zero}},
    {"unknown FORMAT 78", false, false, 78, 0, 64, 0, Identity, false, {Zero, Zero, Zero, Zero}},
    {"unknown FORMAT 127", false, false, 127, 0, 64, 0, Identity, false, {Zero, Zero, Zero, Zero}},
};

alignas(256) std::array<std::uint32_t, 256> Words{};
alignas(256) std::array<std::uint32_t, 256> UnormWords{};
alignas(4096) std::array<std::array<std::uint32_t, 1024>, 128> AtomicPool{};
alignas(256) std::array<std::array<std::uint32_t, 256>, 128> TablePool{};
alignas(256) std::array<std::array<std::uint32_t, 256>, 128> OutputPool{};
std::size_t Slot = 0;

void FillData() {
    for (std::size_t index = 0; index < Words.size(); ++index) Words[index] = 0xa0000000u | static_cast<std::uint32_t>((index + 1u) * 0x1111u);
    UnormWords.fill(0u);
    UnormWords[0] = 0x00ff00ffu;
    UnormWords[1] = 0x0000ffffu;
}

std::array<std::uint32_t, 4> Descriptor(const void* base, std::uint32_t bytes, std::uint32_t stride, std::uint32_t word3) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), bytes, word3};
}

std::uint32_t VMov(std::uint32_t vdst, std::uint32_t value) {
    return 0x7e000000u | (vdst << 17u) | (1u << 9u) | (128u + value);
}

std::uint32_t VShl(std::uint32_t vdst, std::uint32_t shift, std::uint32_t vsrc) {
    return 0x34000000u | (vdst << 17u) | (vsrc << 9u) | (128u + shift);
}

std::uint32_t Mubuf0(std::uint32_t op, bool offen, bool idxen, bool glc) {
    return 0xe0000000u | (op << 18u) | (glc ? 0x4000u : 0u) | (idxen ? 0x2000u : 0u) | (offen ? 0x1000u : 0u);
}

std::uint32_t Mtbuf0(std::uint32_t op, std::uint32_t format, bool idxen) {
    return 0xe8000000u | (op << 16u) | ((format & 0xfu) << 19u) | ((format >> 4u) << 23u) | (idxen ? 0x2000u : 0u) | 0x1000u;
}

std::uint32_t Word1(std::uint32_t vdata, std::uint32_t vaddr, std::uint32_t srsrc) {
    return 0x80000000u | ((srsrc / 4u) << 16u) | (vdata << 8u) | vaddr;
}

std::vector<std::uint32_t> Prologue() {
    return {
        0x7e080500u,
        0x8f048404u,
        0xf4280200u,
        0x08000000u,
        0xbf8cc07fu,
    };
}

void Append(std::vector<std::uint32_t>& code, std::initializer_list<std::uint32_t> words) {
    code.insert(code.end(), words.begin(), words.end());
}

std::vector<std::uint32_t> RowKernel(const Row& row) {
    auto code = Prologue();
    if (row.idxen) {
        Append(code, {VMov(2, 0), VMov(3, row.offset)});
    } else {
        Append(code, {VMov(1, row.offset)});
    }
    const auto vaddr = row.idxen ? 2u : 1u;
    const auto load = row.typed ? Mtbuf0(3, row.format, row.idxen) : row.dword ? Mubuf0(12, true, row.idxen, false) : Mubuf0(3, true, row.idxen, false);
    Append(code, {load, Word1(4, vaddr, 8), 0xbf8c3f70u, VShl(2, 4, 0), Mubuf0(30, true, false, false), Word1(4, 2, 12), 0xbf8c3f70u, 0xbf810000u});
    return code;
}

std::vector<std::uint32_t> AtomicKernel(bool structured) {
    auto code = Prologue();
    if (structured) {
        Append(code, {VShl(1, 0, 0), VMov(2, 4)});
    } else {
        Append(code, {VShl(1, 2, 0)});
    }
    Append(code, {VMov(4, 5), Mubuf0(50, true, structured, true), Word1(4, 1, 8), 0xbf8c3f70u, VShl(2, 2, 0), Mubuf0(28, true, false, false), Word1(4, 2, 12), 0xbf8c3f70u, 0xbf810000u});
    return code;
}

void Dispatch(AgcDriver::VulkanDevice& device, const std::vector<std::uint32_t>& code, std::uint32_t waveSize, std::array<std::uint32_t, 256>& tableWords, std::array<std::uint32_t, 256>& outputWords) {
    std::vector<std::uint32_t> userData(16u, 0u);
    const auto table = Descriptor(tableWords.data(), 64u, 0u, 0x31016facu);
    const auto output = Descriptor(outputWords.data(), static_cast<std::uint32_t>(outputWords.size() * 4u), 0u, 0x31016facu);
    std::copy(table.begin(), table.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 12);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(words.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(words.data()), words, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(words.data()));
    device.WaitIdle();
}

std::uint32_t Expected(std::int32_t source, const std::array<std::uint32_t, 256>& data) {
    if (source == Zero) return 0u;
    if (source == OneFloat) return 0x3f800000u;
    if (source == OneInt) return 1u;
    return data[static_cast<std::size_t>(source)];
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool RunFormatRow(AgcDriver::VulkanDevice& device, const Row& row, std::uint32_t waveSize) {
    const auto& data = row.unorm ? UnormWords : Words;
    auto& table = TablePool[Slot];
    auto& output = OutputPool[Slot];
    ++Slot;
    output.fill(0xdeadbeefu);
    const auto element = Descriptor(data.data(), row.records, row.stride, row.dstSel | (row.format << 12u) | ((row.idxen ? 0u : 3u) << 28u));
    std::copy(element.begin(), element.end(), table.begin());
    try {
        Dispatch(device, RowKernel(row), waveSize, table, output);
    } catch (const std::exception& error) {
        std::printf("%s (wave%u): %s\n", row.name, waveSize, error.what());
        return false;
    }
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto expect = row.expect[component];
            if (expect == Any) continue;
            const auto actual = output[lane * 4u + component];
            const auto wanted = Expected(expect, data);
            if (actual != wanted) {
                std::printf("%s (wave%u): lane %u output %u is %s, expected %s\n", row.name, waveSize, lane, component, Hex(actual).c_str(), Hex(wanted).c_str());
                return false;
            }
        }
    }
    return true;
}

enum class AtomicCase { Raw, RawOutOfBounds, Structured, StructuredOutOfBounds };

bool RunAtomicRow(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, AtomicCase atomicCase) {
    const bool structured = atomicCase == AtomicCase::Structured || atomicCase == AtomicCase::StructuredOutOfBounds;
    const std::uint32_t stride = atomicCase == AtomicCase::Structured ? 8u : atomicCase == AtomicCase::StructuredOutOfBounds ? 6u : 0u;
    const std::uint32_t records = structured ? waveSize : atomicCase == AtomicCase::RawOutOfBounds ? waveSize * 2u : waveSize * 4u;
    const char* name = atomicCase == AtomicCase::Raw ? "buffer_atomic_add offen glc in bounds"
                       : atomicCase == AtomicCase::RawOutOfBounds ? "buffer_atomic_add offen glc out of bounds"
                       : atomicCase == AtomicCase::Structured ? "buffer_atomic_add idxen offen glc structured in bounds"
                                                              : "buffer_atomic_add idxen offen glc structured out of bounds";
    auto& atomicWords = AtomicPool[Slot];
    auto& table = TablePool[Slot];
    auto& output = OutputPool[Slot];
    ++Slot;
    for (std::size_t index = 0; index < atomicWords.size(); ++index) atomicWords[index] = 100u + static_cast<std::uint32_t>(index);
    output.fill(0xdeadbeefu);
    const auto element = Descriptor(atomicWords.data(), records, stride, 20u << 12u | ((structured ? 0u : 3u) << 28u));
    std::copy(element.begin(), element.end(), table.begin());
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(atomicWords.data(), atomicWords.size() * 4u, true, true);
    }
    try {
        Dispatch(device, AtomicKernel(structured), waveSize, table, output);
    } catch (const std::exception& error) {
        std::printf("%s (wave%u): %s\n", name, waveSize, error.what());
        return false;
    }
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const bool inBounds = structured ? 4u + 4u <= stride : lane * 4u + 4u <= records;
        const auto dword = structured ? (lane * stride + 4u) / 4u : lane;
        const auto wantedReturn = inBounds ? 100u + dword : 0u;
        const auto wantedMemory = inBounds ? 105u + dword : 100u + dword;
        if (output[lane] != wantedReturn) {
            std::printf("%s (wave%u): lane %u returned %s, expected %s\n", name, waveSize, lane, Hex(output[lane]).c_str(), Hex(wantedReturn).c_str());
            return false;
        }
        if (atomicWords[dword] != wantedMemory) {
            std::printf("%s (wave%u): lane %u memory is %s, expected %s\n", name, waveSize, lane, Hex(atomicWords[dword]).c_str(), Hex(wantedMemory).c_str());
            return false;
        }
    }
    return true;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillData();
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(Words.data(), Words.size() * 4u, true, false);
            mutation.Add(UnormWords.data(), UnormWords.size() * 4u, true, false);
        }
        int failures = 0;
        for (const std::uint32_t waveSize : {32u, 64u}) {
            for (const auto& row : Rows) {
                if (!RunFormatRow(*device, row, waveSize)) ++failures;
            }
            if (!RunAtomicRow(*device, waveSize, AtomicCase::Raw)) ++failures;
            if (!RunAtomicRow(*device, waveSize, AtomicCase::RawOutOfBounds)) ++failures;
            if (!RunAtomicRow(*device, waveSize, AtomicCase::Structured)) ++failures;
            if (!RunAtomicRow(*device, waveSize, AtomicCase::StructuredOutOfBounds)) ++failures;
        }
        if (failures != 0) {
            std::printf("gpu-selected buffer format: %d checks failed\n", failures);
            return 1;
        }
        std::puts("gpu-selected buffer format tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
