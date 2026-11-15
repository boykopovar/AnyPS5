#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "BdaAbi.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
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
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
namespace Abi = ShaderRecompiler::BdaAbi;

constexpr std::uint32_t Threads = 32;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t TableBase = 0x0;
constexpr std::uint32_t DataBase = 0x1000;
constexpr std::uint32_t DataBytes = 0x4000;
constexpr std::uint32_t SecondDescriptorOffset = 0x100;
constexpr std::uint32_t OutputBase = 0x8000;
constexpr std::uint32_t OutputStride = 16;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Initial = 0x5a5a5a5au;
constexpr std::uint32_t Format = 0x00016facu;
constexpr std::uint32_t BadSelectorFormat = (Format & ~0x7u) | 2u;
constexpr std::uint32_t Raw = 3u << 28u;
constexpr std::uint32_t ReservedBit = 1u << 30u;
constexpr std::uint32_t FirstResult = 24u;
constexpr std::uint32_t SaveSgpr = 24u;
constexpr std::uint32_t LoopSgpr = 20u;
constexpr std::uint32_t LoopPasses = 3u;
constexpr std::uint32_t DescriptorA = 8u;
constexpr std::uint32_t DescriptorB = 16u;
constexpr std::uint32_t Slots = 4u;
constexpr std::uint32_t HalfLanes = 16u;

enum class Slot { Initial, A, B, AInHalf };

enum class FirstState { Valid, Reserved, BadSelector };

struct Site {
    bool secondDescriptor;
    std::uint32_t op;
    bool formatted = false;
};

struct Expectation {
    std::uint32_t removed;
    std::vector<Site> executed;
    std::array<Slot, Slots> valid;
};

struct Program;
using Body = std::function<void(Program&)>;

struct Program {
    std::vector<std::uint32_t> code;
    std::vector<std::uint32_t> loadPcs;
    bool formatted = false;

    void Mubuf(std::uint32_t op, bool offen, std::uint32_t immediate, std::uint32_t vaddr, std::uint32_t vdata, std::uint32_t resource) {
        code.push_back(0xe0000000u | (op << 18u) | (offen ? 0x1000u : 0u) | immediate);
        code.push_back((0x80u << 24u) | (resource / 4u << 16u) | (vdata << 8u) | vaddr);
    }

    void Begin() {
        Mubuf(14u, false, 4u, 0u, 4u, 4u);
        Mubuf(14u, false, 20u, 0u, 8u, 4u);
        code.push_back(0xbf8c0000u);
        for (const auto word : {0x7e120504u, 0x7e140505u, 0x7e160506u, 0x7e200508u, 0x7e220509u, 0x7e24050au, 0x7e26050bu}) code.push_back(word);
        for (std::uint32_t slot = 0; slot < Slots; ++slot) {
            code.push_back(0x7e0002ffu | ((FirstResult + slot) << 17u));
            code.push_back(Initial);
        }
    }

    void VectorLoad(std::uint32_t descriptor, std::uint32_t slot) {
        loadPcs.push_back(static_cast<std::uint32_t>(code.size() * 4u));
        Mubuf(12u, false, 0u, 0u, FirstResult + slot, descriptor);
        code.push_back(0xbf8c3f70u);
    }

    void FormatLoad(std::uint32_t descriptor, std::uint32_t slot) {
        formatted = true;
        loadPcs.push_back(static_cast<std::uint32_t>(code.size() * 4u));
        Mubuf(0u, false, 0u, 0u, FirstResult + slot, descriptor);
        code.push_back(0xbf8c3f70u);
    }

    void Loop(const Body& body) {
        code.push_back(0xbe800380u | (LoopSgpr << 16u));
        const auto start = code.size();
        body(*this);
        code.push_back(0x80008100u | (LoopSgpr << 16u) | LoopSgpr);
        code.push_back(0xbf0a0000u | ((0x80u + LoopPasses) << 8u) | LoopSgpr);
        const auto back = static_cast<std::int32_t>(start) - static_cast<std::int32_t>(code.size() + 1u);
        code.push_back(0xbf850000u | (static_cast<std::uint32_t>(back) & 0xffffu));
    }

    void DivergentIf(std::uint32_t lanes, bool branch, const Body& body) {
        code.push_back(0x7d880000u | (0x80u + lanes));
        code.push_back(0xbe800000u | (SaveSgpr << 16u) | (0x3cu << 8u) | 0x6au);
        std::size_t jump = 0;
        if (branch) {
            jump = code.size();
            code.push_back(0xbf880000u);
        }
        const auto start = code.size();
        body(*this);
        if (branch) code[jump] |= static_cast<std::uint32_t>(code.size() - start);
        code.push_back(0xbefe0300u | SaveSgpr);
    }

    void UniformIf(const Body& body) {
        code.push_back(0xbf06800au);
        const auto jump = code.size();
        code.push_back(0xbf840000u);
        const auto start = code.size();
        body(*this);
        code[jump] |= static_cast<std::uint32_t>(code.size() - start);
    }

    void End() {
        code.push_back(0x34280084u);
        for (std::uint32_t slot = 0; slot < Slots; ++slot) Mubuf(28u, true, slot * 4u, 20u, FirstResult + slot, 12u);
        code.push_back(0xbf8c3f70u);
        code.push_back(0xbf810000u);
    }
};

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "fault check dedup: cannot open a temporary file for stderr");
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
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

struct Case {
    const char* name;
    std::function<void(Program&)> build;
    Expectation expect;
};

std::vector<Case> Cases() {
    return {
        {"straight line, same descriptor three times", [](Program& p) { p.VectorLoad(DescriptorA, 0); p.VectorLoad(DescriptorA, 1); p.VectorLoad(DescriptorA, 2); },
            {2, {{false, 0}, {false, 1}, {false, 2}}, {Slot::A, Slot::A, Slot::A, Slot::Initial}}},
        {"vector load, divergent if, join", [](Program& p) { p.VectorLoad(DescriptorA, 0); p.DivergentIf(HalfLanes, true, [](Program& q) { q.VectorLoad(DescriptorA, 1); }); p.VectorLoad(DescriptorA, 2); },
            {1, {{false, 0}, {false, 1}, {false, 2}}, {Slot::A, Slot::AInHalf, Slot::A, Slot::Initial}}},
        {"vector load, uniform if, join", [](Program& p) { p.VectorLoad(DescriptorA, 0); p.UniformIf([](Program& q) { q.VectorLoad(DescriptorA, 1); }); p.VectorLoad(DescriptorA, 2); },
            {2, {{false, 0}, {false, 2}}, {Slot::A, Slot::Initial, Slot::A, Slot::Initial}}},
        {"check in a skipped uniform if does not cover the join", [](Program& p) { p.UniformIf([](Program& q) { q.VectorLoad(DescriptorA, 0); }); p.VectorLoad(DescriptorA, 1); },
            {0, {{false, 1}}, {Slot::Initial, Slot::A, Slot::Initial, Slot::Initial}}},
        {"check in a skipped uniform if covers only the join pair", [](Program& p) { p.UniformIf([](Program& q) { q.VectorLoad(DescriptorA, 0); }); p.VectorLoad(DescriptorA, 1); p.VectorLoad(DescriptorA, 2); },
            {1, {{false, 1}, {false, 2}}, {Slot::Initial, Slot::A, Slot::A, Slot::Initial}}},
        {"check under an empty divergent if does not cover the join", [](Program& p) { p.DivergentIf(0, true, [](Program& q) { q.VectorLoad(DescriptorA, 0); }); p.VectorLoad(DescriptorA, 1); },
            {0, {{false, 1}}, {Slot::Initial, Slot::A, Slot::Initial, Slot::Initial}}},
        {"check under an empty exec mask does not cover a full-mask check in the same block", [](Program& p) { p.DivergentIf(0, false, [](Program& q) { q.VectorLoad(DescriptorA, 0); }); p.VectorLoad(DescriptorA, 1); },
            {0, {{false, 1}}, {Slot::Initial, Slot::A, Slot::Initial, Slot::Initial}}},
        {"different descriptors", [](Program& p) { p.VectorLoad(DescriptorA, 0); p.VectorLoad(DescriptorB, 1); },
            {0, {{false, 0}, {true, 1}}, {Slot::A, Slot::B, Slot::Initial, Slot::Initial}}},
        {"different descriptors, second repeated", [](Program& p) { p.VectorLoad(DescriptorB, 0); p.VectorLoad(DescriptorA, 1); p.VectorLoad(DescriptorB, 2); },
            {1, {{true, 0}, {false, 1}, {true, 2}}, {Slot::B, Slot::A, Slot::B, Slot::Initial}}},
        {"formatted load, same descriptor twice", [](Program& p) { p.FormatLoad(DescriptorA, 0); p.FormatLoad(DescriptorA, 1); },
            {2, {{false, 0, true}, {false, 1, true}}, {Slot::A, Slot::A, Slot::Initial, Slot::Initial}}},
        {"formatted load, then a raw load of the same descriptor", [](Program& p) { p.FormatLoad(DescriptorA, 0); p.VectorLoad(DescriptorA, 1); },
            {1, {{false, 0, true}, {false, 1}}, {Slot::A, Slot::A, Slot::Initial, Slot::Initial}}},
        {"raw load does not cover the selector check of a formatted load", [](Program& p) { p.VectorLoad(DescriptorA, 0); p.FormatLoad(DescriptorA, 1); },
            {1, {{false, 0}, {false, 1, true}}, {Slot::A, Slot::A, Slot::Initial, Slot::Initial}}},
        {"formatted load before a loop covers the formatted load in its body", [](Program& p) { p.FormatLoad(DescriptorA, 0); p.Loop([](Program& q) { q.FormatLoad(DescriptorA, 1); }); },
            {2, {{false, 0, true}, {false, 1, true}}, {Slot::A, Slot::A, Slot::Initial, Slot::Initial}}},
        {"raw load before a loop covers the raw load in its body", [](Program& p) { p.VectorLoad(DescriptorA, 0); p.Loop([](Program& q) { q.VectorLoad(DescriptorA, 1); }); },
            {1, {{false, 0}, {false, 1}}, {Slot::A, Slot::A, Slot::Initial, Slot::Initial}}},
        {"check in a loop body covers the loop exit", [](Program& p) { p.Loop([](Program& q) { q.VectorLoad(DescriptorA, 0); }); p.VectorLoad(DescriptorA, 1); },
            {1, {{false, 0}, {false, 1}}, {Slot::A, Slot::A, Slot::Initial, Slot::Initial}}},
        {"check in a skipped loop does not cover the join", [](Program& p) { p.UniformIf([](Program& q) { q.Loop([](Program& r) { r.VectorLoad(DescriptorA, 0); }); }); p.VectorLoad(DescriptorA, 1); },
            {0, {{false, 1}}, {Slot::Initial, Slot::A, Slot::Initial, Slot::Initial}}},
        {"different descriptor in a loop", [](Program& p) { p.Loop([](Program& q) { q.VectorLoad(DescriptorB, 0); }); p.VectorLoad(DescriptorA, 1); p.VectorLoad(DescriptorB, 2); },
            {1, {{true, 0}, {false, 1}, {true, 2}}, {Slot::B, Slot::A, Slot::B, Slot::Initial}}},
    };
}

alignas(256) std::array<std::array<std::uint32_t, 128>, 32> Programs{};
std::uint32_t ProgramCount = 0;
std::uint32_t Runs = 0;

std::span<const std::uint32_t> Store(const Program& program) {
    Require(program.code.size() <= Programs.at(0).size(), "fault check dedup: program too long");
    auto& slot = Programs.at(ProgramCount++);
    std::copy(program.code.begin(), program.code.end(), slot.begin());
    return std::span<const std::uint32_t>(slot.data(), program.code.size());
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "fault check dedup: cannot allocate the guest block");
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
    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(block); }

private:
    std::uint8_t* block = nullptr;
};

std::uint32_t InitialByte(std::uint32_t address) {
    return (address * 7u + 3u + (address >> 8u)) & 0xffu;
}

std::uint32_t DataWord(std::uint32_t byte) {
    std::uint32_t value = 0;
    for (std::uint32_t at = 0; at < 4u; ++at) value |= InitialByte(byte + at) << (at * 8u);
    return value;
}

void Prepare(GuestBlock& guest, std::uint32_t shift, FirstState first, bool invalidB) {
    auto* bytes = guest.Data();
    std::memset(bytes, 0, BlockBytes);
    const auto data = guest.Address() + DataBase + shift;
    const auto second = data + SecondDescriptorOffset;
    const auto firstWord3 = (first == FirstState::BadSelector ? BadSelectorFormat : Format) | Raw | (first == FirstState::Reserved ? ReservedBit : 0u);
    const std::array<std::uint32_t, 9> table{
        static_cast<std::uint32_t>(data), static_cast<std::uint32_t>((data >> 32u) & 0xffffu), 4096u, firstWord3, 0u,
        static_cast<std::uint32_t>(second), static_cast<std::uint32_t>((second >> 32u) & 0xffffu), 4096u, Format | Raw | (invalidB ? ReservedBit : 0u),
    };
    std::memcpy(bytes + TableBase, table.data(), sizeof(table));
    for (std::uint32_t at = 0; at < DataBytes; ++at) bytes[DataBase + at] = static_cast<std::uint8_t>(InitialByte(at));
    std::fill(reinterpret_cast<std::uint32_t*>(bytes + OutputBase), reinterpret_cast<std::uint32_t*>(bytes + OutputBase + Threads * OutputStride), Untouched);
}

ShaderRecompiler::RecompileResult Recompile(std::span<const std::uint32_t> code, GuestBlock& guest, std::uint32_t shift, const ShaderRecompiler::SpirvTarget& target) {
    const auto table = guest.Address() + TableBase;
    const auto data = guest.Address() + DataBase + shift;
    const auto output = guest.Address() + OutputBase;
    const std::vector<std::uint32_t> userData{
        0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>(table), static_cast<std::uint32_t>((table >> 32u) & 0xffffu), 1024u, Format | Raw,
        static_cast<std::uint32_t>(data), static_cast<std::uint32_t>((data >> 32u) & 0xffffu), 0u, 0u,
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), Threads * OutputStride, Format | Raw,
    };
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, regions},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void SetDedup(bool enabled) {
#ifdef _WIN32
    _putenv_s("APS5_NO_FAULT_CHECK_DEDUP", enabled ? "" : "1");
#else
    if (enabled) unsetenv("APS5_NO_FAULT_CHECK_DEDUP");
    else setenv("APS5_NO_FAULT_CHECK_DEDUP", "1", 1);
#endif
}

std::uint32_t CountFaultCalls(std::span<const std::uint32_t> words) {
    std::map<std::uint32_t, std::string> names;
    std::uint32_t faultFunction = 0;
    std::uint32_t calls = 0;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto opcode = words[cursor] & 0xffffu;
        Require(count != 0 && count <= words.size() - cursor, "fault check dedup: truncated SPIR-V instruction");
        if (opcode == spv::OpName && std::string_view(reinterpret_cast<const char*>(&words[cursor + 2])) == "record_bda_fault") faultFunction = words[cursor + 1];
        if (opcode == spv::OpFunctionCall && faultFunction != 0 && words[cursor + 3] == faultFunction) ++calls;
        cursor += count;
    }
    Require(faultFunction != 0, "fault check dedup: the module has no record_bda_fault");
    return calls;
}

struct Outcome {
    std::string fault;
    std::vector<std::uint32_t> output;
    std::uint32_t calls = 0;
};

Outcome Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::span<const std::uint32_t> code, std::uint32_t shift, bool dedup, FirstState first, bool invalidB) {
    SetDedup(dedup);
    const auto shader = Recompile(code, guest, shift, device.Target());
    SetDedup(true);
    Outcome outcome;
    outcome.calls = CountFaultCalls(shader.spirv.Words());
    Prepare(guest, shift, first, invalidB);
    StderrCapture log;
    try {
        device.Dispatch(shader, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
    } catch (...) {
        static_cast<void>(log.Finish());
        throw;
    }
    const auto text = log.Finish();
    if (const auto at = text.find("BDA access failed"); at != std::string::npos) {
        const auto end = text.find_first_of("\r\n", at);
        outcome.fault = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
    }
    const auto* output = reinterpret_cast<const std::uint32_t*>(guest.Data() + OutputBase);
    outcome.output.assign(output, output + Threads * OutputStride / 4u);
    return outcome;
}

std::string FaultText(std::uint64_t address, std::uint32_t pc) {
    char text[160];
    std::snprintf(text, sizeof(text), "BDA access failed: address=0x%llx instruction=0x%x bytes=0 stage=%u reason=%u", static_cast<unsigned long long>(address), pc, static_cast<unsigned>(ShaderRecompiler::IrShaderStage::Compute), static_cast<unsigned>(Abi::FaultReason::InvalidDescriptor));
    return text;
}

std::uint32_t ExpectedWord(Slot slot, std::uint32_t lane, std::uint32_t shift) {
    switch (slot) {
    case Slot::Initial: return Initial;
    case Slot::A: return DataWord(shift);
    case Slot::B: return DataWord(shift + SecondDescriptorOffset);
    case Slot::AInHalf: return lane < HalfLanes ? DataWord(shift) : Initial;
    }
    return 0;
}

void RunCase(AgcDriver::VulkanDevice& device, GuestBlock& guest, const Case& test) {
    Program program;
    program.Begin();
    test.build(program);
    program.End();
    const auto code = Store(program);
    const std::string name = std::string("fault check dedup, ") + test.name;
    std::vector<FirstState> states{FirstState::Valid, FirstState::Reserved};
    if (program.formatted) states.push_back(FirstState::BadSelector);
    for (const auto first : states) {
        for (const bool invalidB : {false, true}) {
            const std::string label = name + (first == FirstState::Valid ? ", A valid" : first == FirstState::Reserved ? ", A invalid" : ", A with a reserved selector") + (invalidB ? ", B invalid" : ", B valid");
            const auto shift = 4u * Runs++;
            const auto off = Run(device, guest, code, shift, false, first, invalidB);
            const auto on = Run(device, guest, code, shift, true, first, invalidB);
            Require(off.calls >= on.calls && off.calls - on.calls == test.expect.removed, label + ": the default removed " + std::to_string(off.calls - on.calls) + " record_bda_fault calls (" + std::to_string(off.calls) + " opt-out, " + std::to_string(on.calls) + " default), expected " + std::to_string(test.expect.removed));
            Require(off.fault == on.fault, label + ": the fault differs, opt-out '" + off.fault + "', default '" + on.fault + "'");
            Require(off.output == on.output, label + ": the output differs between the default and APS5_NO_FAULT_CHECK_DEDUP");
            std::string expectedFault;
            bool faults = false;
            for (const auto& site : test.expect.executed) {
                const bool firstFaults = first == FirstState::Reserved || (first == FirstState::BadSelector && site.formatted);
                if (site.secondDescriptor ? !invalidB : !firstFaults) continue;
                faults = true;
                expectedFault = FaultText(guest.Address() + DataBase + shift + (site.secondDescriptor ? SecondDescriptorOffset : 0u), program.loadPcs.at(site.op));
                break;
            }
            if (faults) {
                Require(off.fault == expectedFault, label + ": the fault is '" + off.fault + "', expected '" + expectedFault + "'");
            } else {
                Require(off.fault.empty(), label + ": unexpected fault '" + off.fault + "'");
                for (std::uint32_t lane = 0; lane < Threads; ++lane) {
                    for (std::uint32_t slot = 0; slot < Slots; ++slot) {
                        const auto actual = off.output[lane * OutputStride / 4u + slot];
                        const auto expected = ExpectedWord(test.expect.valid[slot], lane, shift);
                        Require(actual == expected, label + ": lane " + std::to_string(lane) + " slot " + std::to_string(slot) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
                    }
                }
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        for (const auto& test : Cases()) RunCase(*device, guest, test);
        std::puts("fault check dedup tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
