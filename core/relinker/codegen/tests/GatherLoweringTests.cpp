#include "GprVectorExecution.hpp"
#include <codegen/x86/GatherLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using LoweringExecution::Bytes;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Form {
    const char* Name;
    std::uint8_t Opcode;
    bool Wide;
};

constexpr std::array<Form, 8> kForms{{{"VPGATHERDD", 0x90, false}, {"VPGATHERQD", 0x91, false}, {"VGATHERDPS", 0x92, false}, {"VGATHERQPS", 0x93, false},
    {"VPGATHERDQ", 0x90, true}, {"VPGATHERQQ", 0x91, true}, {"VGATHERDPD", 0x92, true}, {"VGATHERQPD", 0x93, true}}};

struct Site {
    std::uint8_t Destination;
    std::uint8_t Mask;
    std::uint8_t Index;
    int Base;
    std::uint8_t ScaleBits;
    std::int32_t Displacement;
};

Bytes encode(const Form& form, bool vector256, const Site& site) {
    const std::uint8_t base = site.Base < 0 ? 5 : static_cast<std::uint8_t>(site.Base);
    Bytes bytes{0xC4, static_cast<std::uint8_t>(((site.Destination & 8) ? 0 : 0x80) | ((site.Index & 8) ? 0 : 0x40) | ((base & 8) ? 0 : 0x20) | 2),
        static_cast<std::uint8_t>((form.Wide ? 0x80 : 0) | ((~site.Mask & 15) << 3) | (vector256 ? 4 : 0) | 1), form.Opcode};
    std::uint8_t mod = 2;
    if (site.Base < 0 || (site.Displacement == 0 && (base & 7) != 5)) mod = 0;
    else if (site.Displacement >= -128 && site.Displacement <= 127) mod = 1;
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | ((site.Destination & 7) << 3) | 4));
    bytes.push_back(static_cast<std::uint8_t>((site.ScaleBits << 6) | ((site.Index & 7) << 3) | (base & 7)));
    if (mod == 1) bytes.push_back(static_cast<std::uint8_t>(site.Displacement));
    if (mod == 2 || site.Base < 0)
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(site.Displacement) >> shift));
    return bytes;
}

void decoding() {
    for (const auto& form : kForms) {
        for (const bool vector256 : {false, true}) {
            const auto bytes = encode(form, vector256, {9, 3, 12, 13, 2, 16});
            const auto operands = Codegen::DecodeGather(bytes.data(), bytes.size());
            require(operands && std::string(operands->Name) == form.Name && operands->Destination == 9 && operands->Mask == 3 && operands->Index == 12 && operands->Base == 13 && operands->Scale == 4 && operands->Vector256 == vector256,
                std::string(form.Name) + " was not decoded");
            require(operands->DataBytes == (form.Wide ? 8 : 4) && operands->IndexBytes == ((form.Opcode & 1) != 0 ? 8 : 4), std::string(form.Name) + " has the wrong element sizes");
        }
    }
    const auto noBase = encode(kForms[5], true, {1, 2, 3, -1, 0, 0x100});
    const auto absolute = Codegen::DecodeGather(noBase.data(), noBase.size());
    require(absolute && !absolute->Base && absolute->Memory.Displacement == 0x100, "a gather without a base register was not decoded");
    for (const Bytes bytes : {encode(kForms[0], true, {1, 1, 2, 0, 0, 0}), encode(kForms[0], true, {1, 2, 1, 0, 0, 0}), encode(kForms[0], true, {1, 2, 2, 0, 0, 0}),
             Bytes{0xC4, 0xE2, 0x75, 0x90, 0x00}, Bytes{0xC4, 0xE2, 0x75, 0x90, 0xC1}, Bytes{0xC4, 0xE2, 0x75, 0x94, 0x04, 0x08}, Bytes{0x90}})
        require(!Codegen::DecodeGather(bytes.data(), bytes.size()), "an undefined or unrelated instruction was decoded as a gather");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0x75, 0x90, 0x04, 0x10, 0x90}, Bytes{0x67, 0xC4, 0xE2, 0x75, 0x90, 0x04, 0x10}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeGather(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "a gather with trailing bytes or 32-bit addressing was accepted");
    }
}

void matching() {
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto avx = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Avx);
    for (const auto& form : kForms) {
        const auto bytes = encode(form, true, {0, 1, 2, 3, 3, 8});
        require(!intel->Match(bytes.data(), bytes.size()), std::string("Intel mode lowered ") + form.Name);
        const auto match = avx->Match(bytes.data(), bytes.size());
        require(match && match->InstructionName == form.Name && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && match->RelocatableWindow, std::string("AVX mode did not lower ") + form.Name + " through a movable stub");
    }
}

#if LOWERING_EXECUTION_AVAILABLE

using GprVectorExecution::Machine;
using GprVectorExecution::Runner;
using Vector = std::array<std::uint64_t, 4>;

Bytes lowered(const Bytes& instruction) {
    Codegen::StubBodyBuilder body;
    Codegen::EmitGather(body, *Codegen::DecodeGather(instruction.data(), instruction.size()));
    const auto finished = body.Finish();
    return Bytes(finished.Bytes.begin(), finished.Bytes.begin() + static_cast<std::ptrdiff_t>(finished.ReturnBranchOffset));
}

std::uint64_t element(const Vector& vector, unsigned bytes, unsigned index) {
    std::uint64_t value = 0;
    std::memcpy(&value, reinterpret_cast<const std::uint8_t*>(vector.data()) + index * bytes, bytes);
    return value;
}

void setElement(Vector& vector, unsigned bytes, unsigned index, std::uint64_t value) {
    std::memcpy(reinterpret_cast<std::uint8_t*>(vector.data()) + index * bytes, &value, bytes);
}

Vector gathered(const Form& form, bool vector256, const Site& site, const Machine& start) {
    const unsigned data = form.Wide ? 8 : 4;
    const unsigned indexBytes = (form.Opcode & 1) != 0 ? 8 : 4;
    const unsigned elements = (vector256 ? 32u : 16u) / (data > indexBytes ? data : indexBytes);
    Vector out{};
    const auto& destination = start.Ymm[site.Destination];
    const auto& mask = start.Ymm[site.Mask];
    const auto& index = start.Ymm[site.Index];
    const std::uint64_t base = site.Base < 0 ? 0 : start.Gpr[static_cast<std::size_t>(site.Base)];
    for (unsigned i = 0; i < elements; ++i) {
        std::uint64_t value = element(destination, data, i);
        if ((element(mask, data, i) >> (data * 8 - 1)) != 0) {
            std::int64_t offset = indexBytes == 4 ? static_cast<std::int32_t>(element(index, 4, i)) : static_cast<std::int64_t>(element(index, 8, i));
            const auto address = base + static_cast<std::uint64_t>(offset * (std::int64_t{1} << site.ScaleBits)) + static_cast<std::uint64_t>(static_cast<std::int64_t>(site.Displacement));
            value = 0;
            std::memcpy(&value, reinterpret_cast<const void*>(address), data);
        }
        setElement(out, data, i, value);
    }
    return out;
}

void verify(const Machine& start, const Machine& end, const Site& site, const Vector& wanted, const std::string& describe) {
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        Vector expected = start.Ymm[reg];
        if (reg == site.Destination) expected = wanted;
        if (reg == site.Mask) expected = {};
        if (end.Ymm[reg] != expected) throw std::runtime_error(describe + ": ymm" + std::to_string(reg) + " is wrong");
        if (reg != 4 && end.Gpr[reg] != start.Gpr[reg]) throw std::runtime_error(describe + ": r" + std::to_string(reg) + " changed");
    }
    require((end.Flags & 0xFFF) == (start.Flags & 0xFFF), describe + " changed RFLAGS");
    for (const auto slot : end.RedZone) require(slot == LoweringExecution::kSentinel, describe + " overwrote the red zone");
}

void prepare(Machine& machine, const Form& form, const Site& site, std::uint64_t table, unsigned round) {
    const unsigned data = form.Wide ? 8 : 4;
    const unsigned indexBytes = (form.Opcode & 1) != 0 ? 8 : 4;
    for (unsigned i = 0; i < 32 / indexBytes; ++i) {
        const std::int64_t value = static_cast<std::int64_t>((i * 7 + round * 5) % 31) - 15;
        setElement(machine.Ymm[site.Index], indexBytes, i, static_cast<std::uint64_t>(value));
    }
    const std::array<std::uint32_t, 4> patterns{0xA5u, 0xFFu, 0x00u, 0x3Cu};
    for (unsigned i = 0; i < 32 / data; ++i) {
        const auto active = ((patterns[round % patterns.size()] >> i) & 1) != 0;
        const auto previous = element(machine.Ymm[site.Mask], data, i);
        const std::uint64_t sign = std::uint64_t{1} << (data * 8 - 1);
        setElement(machine.Ymm[site.Mask], data, i, active ? (previous | sign) : (previous & ~sign));
    }
    if (site.Base >= 0) machine.Gpr[static_cast<std::size_t>(site.Base)] = table;
}

void execution() {
    alignas(64) static std::array<std::uint8_t, 1024> table{};
    for (std::size_t i = 0; i < table.size(); ++i) table[i] = static_cast<std::uint8_t>(i * 37 + 11);
    const auto middle = reinterpret_cast<std::uint64_t>(table.data() + 512);
    const bool native = LoweringExecution::HostExecutes(Bytes{0xC4, 0xE2, 0x75, 0x90, 0x04, 0x20});
    const std::array<Site, 7> sites{{{0, 1, 2, 3, 0, 0}, {15, 14, 13, 0, 3, 8}, {8, 9, 10, 13, 2, -24}, {5, 6, 7, 7, 1, 100}, {3, 12, 15, 5, 3, 0}, {11, 4, 0, 15, 2, 400}, {14, 0, 15, 2, 0, -1}}};
    std::uint64_t seed = 0x243F6A8885A308D3ull;
    for (const auto& form : kForms) {
        for (const bool vector256 : {false, true}) {
            for (const auto& site : sites) {
                const auto instruction = encode(form, vector256, site);
                const Runner runner(lowered(instruction));
                for (unsigned round = 0; round < 4; ++round) {
                    auto machine = GprVectorExecution::Initial(seed += 0x9E3779B97F4A7C15ull, round % 2 == 0 ? LoweringExecution::kBaseFlags : LoweringExecution::kBaseFlags | 0x8D5u);
                    prepare(machine, form, site, middle, round);
                    const auto start = machine;
                    runner.Run(machine);
                    const auto describe = std::string(form.Name) + (vector256 ? " ymm" : " xmm") + " dst " + std::to_string(site.Destination) + " base " + std::to_string(site.Base) + " round " + std::to_string(round);
                    verify(start, machine, site, gathered(form, vector256, site, start), describe);
                    if (native) {
                        auto hardware = start;
                        Runner(instruction).Run(hardware);
                        require(hardware.Ymm == machine.Ymm && hardware.Gpr == machine.Gpr, describe + " differs from the host AVX2 instruction");
                    }
                }
            }
        }
    }
    for (const bool vector256 : {false, true}) {
        const Site site{2, 3, 4, -1, 0, 0};
        const auto instruction = encode(kForms[5], vector256, site);
        auto machine = GprVectorExecution::Initial(77, LoweringExecution::kBaseFlags);
        for (unsigned i = 0; i < 4; ++i) {
            machine.Ymm[4][i] = middle + i * 24;
            machine.Ymm[3][i] = i % 2 == 0 ? ~0ull : 0;
        }
        const auto start = machine;
        Runner(lowered(instruction)).Run(machine);
        verify(start, machine, site, gathered(kForms[5], vector256, site, start), "VPGATHERQQ without a base register");
    }
}

void maskedOffPages() {
    const auto pageSize = LoweringExecution::PageSize();
    auto* area = LoweringExecution::AllocatePages(pageSize * 2);
    require(area != nullptr, "cannot allocate the guarded gather pages");
    for (std::size_t i = 0; i < pageSize; ++i) area[i] = static_cast<std::uint8_t>(i * 13 + 1);
    require(LoweringExecution::ProtectPages(area + pageSize, pageSize, false, false), "cannot protect the page after the gather table");
    for (const auto& form : kForms) {
        for (const bool vector256 : {false, true}) {
            const Site site{1, 2, 3, 0, 0, 0};
            const unsigned data = form.Wide ? 8 : 4;
            const unsigned indexBytes = (form.Opcode & 1) != 0 ? 8 : 4;
            auto machine = GprVectorExecution::Initial(5, LoweringExecution::kBaseFlags);
            machine.Gpr[0] = reinterpret_cast<std::uint64_t>(area + pageSize);
            for (unsigned i = 0; i < 32 / indexBytes; ++i) setElement(machine.Ymm[3], indexBytes, i, i % 2 == 0 ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(data) * (i + 1)) : 0);
            for (unsigned i = 0; i < 32 / data; ++i) setElement(machine.Ymm[2], data, i, i % 2 == 0 ? (std::uint64_t{1} << (data * 8 - 1)) : 0);
            const auto start = machine;
            Runner(lowered(encode(form, vector256, site))).Run(machine);
            verify(start, machine, site, gathered(form, vector256, site, start), std::string(form.Name) + " with masked-off elements on a guard page");
        }
    }
    LoweringExecution::FreePages(area, pageSize * 2);
}

#else

void execution() { std::cout << "gather execution checks require an x86-64 Linux, macOS or Windows build\n"; }
void maskedOffPages() {}

#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        maskedOffPages();
        std::cout << "gather lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
