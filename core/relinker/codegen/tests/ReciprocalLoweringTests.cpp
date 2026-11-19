#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/SharedStubData.hpp>
#include "VectorExecution.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Golden {
    std::uint32_t Input;
    std::uint32_t Reciprocal;
    std::uint32_t SquareRoot;
};

constexpr std::array<Golden, 42> kGolden{{
    {0x3F800000, 0x3F7FF000, 0x3F7FF800},
    {0x40000000, 0x3EFFF000, 0x3F350000},
    {0x40800000, 0x3E7FF000, 0x3EFFF800},
    {0x3E800000, 0x407FF000, 0x3FFFF800},
    {0x40400000, 0x3EAAA800, 0x3F13C800},
    {0x3FC00000, 0x3F2AA800, 0x3F510800},
    {0x3F800001, 0x3F7FF000, 0x3F7FF800},
    {0x3F7FFFFF, 0x3F800000, 0x3F800000},
    {0x7F7FFFFF, 0x00000000, 0x1F800000},
    {0x00800000, 0x7E7FF000, 0x5EFFF800},
    {0x00800001, 0x7E7FF000, 0x5EFFF800},
    {0x00000001, 0x7F800000, 0x7F800000},
    {0x007FFFFF, 0x7F800000, 0x7F800000},
    {0x00000000, 0x7F800000, 0x7F800000},
    {0x80000000, 0xFF800000, 0xFF800000},
    {0x7F800000, 0x00000000, 0x00000000},
    {0xFF800000, 0x80000000, 0xFFC00000},
    {0x7FC00000, 0x7FC00000, 0x7FC00000},
    {0x7F800001, 0x7FC00001, 0x7FC00001},
    {0xFFC00001, 0xFFC00001, 0xFFC00001},
    {0xBF800000, 0xBF7FF000, 0xFFC00000},
    {0xC0600000, 0xBE924000, 0xFFC00000},
    {0x7E7FFFFF, 0x00800000, 0x20000000},
    {0x7E800000, 0x00000000, 0x1FFFF800},
    {0x7F000000, 0x00000000, 0x1FB50000},
    {0x006CE3EE, 0x7F800000, 0x7F800000},
    {0x7149F2CA, 0x0DA24000, 0x26902000},
    {0x80000001, 0xFF800000, 0xFF800000},
    {0x807FFFFF, 0xFF800000, 0xFF800000},
    {0x80800000, 0xFE7FF000, 0xFFC00000},
    {0xFF7FFFFF, 0x80000000, 0xFFC00000},
    {0x3F801000, 0x3F7FD800, 0x3F7FE800},
    {0x3FFFF800, 0x3F000000, 0x3F350800},
    {0x407FF800, 0x3E800000, 0x3F000000},
    {0x1E3CE508, 0x60AD7800, 0x50150000},
    {0x5F800000, 0x1F7FF000, 0x2F7FF800},
    {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
    {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF},
    {0x3F8007FF, 0x3F7FF000, 0x3F7FF800},
    {0x40C90FDB, 0x3E22F800, 0x3ECC4800},
    {0xE0DDF00D, 0x9E13A800, 0xFFC00000},
    {0xBADC0FFE, 0xC414E800, 0xFFC00000},
}};

constexpr std::array<std::uint64_t, 2> kHashes{0xAD1BACB877CC420Dull, 0x8A964BDD8766320Dull};

std::uint32_t expected(const bool squareRoot, const std::uint32_t input) {
    const auto golden = std::find_if(kGolden.begin(), kGolden.end(), [&](const Golden& entry) { return entry.Input == input; });
    if (golden == kGolden.end()) throw std::runtime_error("Input has no result measured on a desktop Zen 2");
    return squareRoot ? golden->SquareRoot : golden->Reciprocal;
}

std::vector<std::uint32_t> hashInputs() {
    std::vector<std::uint32_t> inputs;
    for (const auto& golden : kGolden) inputs.push_back(golden.Input);
    for (std::uint32_t index = 0; index < 4096; ++index) {
        inputs.push_back(0x3F800000u | (index << 11) | ((index * 0x2Fu) & 0x7FFu));
        inputs.push_back(0x40000000u | (index << 11) | ((index * 0x13u) & 0x7FFu));
        inputs.push_back(0xC1000000u | (index << 11));
    }
    std::uint32_t state = 0x12345678u;
    for (std::uint32_t index = 0; index < 65536; ++index) {
        state = state * 1664525u + 1013904223u;
        inputs.push_back(state);
    }
    return inputs;
}

void ripRelocations() {
    const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
    const Bytes site{0xC5, 0xFC, 0x53, 0x0D, 0x10, 0x00, 0x00, 0x00};
    const auto match = matcher->Match(site.data(), site.size());
    require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "RIP-relative 256-bit VRCPPS was not lowered");
    std::vector<std::int64_t> targets;
    for (const auto& relocation : match->Relocations) {
        if (relocation.SharedData)
            require(relocation.SiteTarget >= 0 && static_cast<std::size_t>(relocation.SiteTarget) + 4 <= Codegen::SharedStubData().size(), "A shared relocation points outside the shared stub data");
        else
            targets.push_back(relocation.SiteTarget);
    }
    require(targets == std::vector<std::int64_t>{0x18, 0x28}, "RIP-relative 256-bit VRCPPS did not load both halves of its operand");
}

#if LOWERING_EXECUTION_AVAILABLE

struct alignas(64) Frame {
    VectorExecution::Machine Machine;
    std::uint64_t FlagsIn;
    std::uint64_t FlagsOut;
    std::uint32_t HostMxcsr;
    std::uint32_t TestMxcsr;
    std::uint32_t MxcsrOut;
    std::uint32_t Padding;
    std::uint64_t RaxOut;
    std::uint64_t RdxIn;
    std::uint64_t RdxOut;
};

constexpr std::uint64_t kStatusFlags = 0x8D5;

void frameOperand(Bytes& bytes, const std::initializer_list<std::uint8_t> opcode, const std::size_t offset) {
    bytes.insert(bytes.end(), opcode);
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(offset >> shift));
}

void belowRedZone(Bytes& bytes) {
    bytes.insert(bytes.end(), {0x48, 0x8D, 0x64, 0x24, 0x80});
}

void aboveRedZone(Bytes& bytes) {
    bytes.insert(bytes.end(), {0x48, 0x8D, 0xA4, 0x24, 0x80, 0x00, 0x00, 0x00});
}

std::array<std::uint32_t, 8> lanes(const std::array<std::uint64_t, 4>& ymm) {
    std::array<std::uint32_t, 8> result{};
    std::memcpy(result.data(), ymm.data(), sizeof(result));
    return result;
}

void setLanes(std::array<std::uint64_t, 4>& ymm, const std::array<std::uint32_t, 8>& values) {
    std::memcpy(ymm.data(), values.data(), sizeof(values));
}

Frame initial(const std::uint64_t flags = 0x8D7) {
    Frame frame{};
    frame.Machine = VectorExecution::Initial();
    frame.FlagsIn = flags;
    frame.TestMxcsr = 0;
    return frame;
}

class Stub {
public:
    Stub(const Bytes& site, const std::uint8_t destination) : destination(destination) {
        const auto matcher = Codegen::MakeAmd64OnlyInstructionMatcher();
        const auto match = matcher->Match(site.data(), site.size());
        require(match && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline, "Reciprocal stub was not produced");
        Bytes body;
        frameOperand(body, {0x0F, 0xAE, 0x9F}, offsetof(Frame, HostMxcsr));
        frameOperand(body, {0x0F, 0xAE, 0x97}, offsetof(Frame, TestMxcsr));
        frameOperand(body, {0x48, 0x89, 0x97}, offsetof(Frame, RdxIn));
        belowRedZone(body);
        frameOperand(body, {0xFF, 0xB7}, offsetof(Frame, FlagsIn));
        body.push_back(0x9D);
        aboveRedZone(body);
        const auto start = body.size();
        body.insert(body.end(), match->StubBody.begin(), match->StubBody.begin() + static_cast<std::ptrdiff_t>(match->ReturnBranchOffset));
        belowRedZone(body);
        body.push_back(0x9C);
        frameOperand(body, {0x8F, 0x87}, offsetof(Frame, FlagsOut));
        aboveRedZone(body);
        frameOperand(body, {0x0F, 0xAE, 0x9F}, offsetof(Frame, MxcsrOut));
        frameOperand(body, {0x0F, 0xAE, 0x97}, offsetof(Frame, HostMxcsr));
        frameOperand(body, {0x48, 0x89, 0x87}, offsetof(Frame, RaxOut));
        frameOperand(body, {0x48, 0x89, 0x97}, offsetof(Frame, RdxOut));
        auto relocations = match->Relocations;
        for (auto& relocation : relocations) {
            relocation.DisplacementOffset += start;
            relocation.InstructionEnd += start;
        }
        runner = std::make_unique<VectorExecution::Runner>(body, relocations, Codegen::SharedStubData());
    }

    Frame Run(const Frame& before) const {
        Frame after = before;
        runner->Run(after.Machine);
        for (std::size_t reg = 0; reg < 16; ++reg)
            if (reg != destination) require(after.Machine.Ymm[reg] == before.Machine.Ymm[reg], "Reciprocal stub changed a register other than its destination");
        require(after.Machine.Memory == before.Machine.Memory, "Reciprocal stub wrote its memory operand");
        for (const auto slot : after.Machine.RedZone) require(slot == LoweringExecution::kSentinel, "Reciprocal stub wrote the red zone");
        require((after.FlagsOut & kStatusFlags) == (before.FlagsIn & kStatusFlags), "Reciprocal stub changed RFLAGS");
        require(after.MxcsrOut == before.TestMxcsr, "Reciprocal stub changed MXCSR");
        require(after.RaxOut == reinterpret_cast<std::uint64_t>(after.Machine.Memory.data()) && after.RdxOut == after.RdxIn, "Reciprocal stub did not restore RAX or RDX");
        return after;
    }

private:
    std::uint8_t destination;
    std::unique_ptr<VectorExecution::Runner> runner;
};

std::array<std::uint32_t, 8> goldenLanes(const std::size_t first) {
    std::array<std::uint32_t, 8> values{};
    for (std::size_t lane = 0; lane < values.size(); ++lane) values[lane] = kGolden[(first + lane) % kGolden.size()].Input;
    return values;
}

struct Packed {
    Bytes Site;
    std::uint8_t Destination;
    std::uint8_t Source;
    bool Vex;
    bool Wide;
};

void packedForms() {
    for (const bool squareRoot : {false, true}) {
        const std::uint8_t op = squareRoot ? 0x52 : 0x53;
        for (const auto& form : {Packed{{0x0F, op, 0xD5}, 2, 5, false, false}, Packed{{0x0F, op, 0xD2}, 2, 2, false, false}, Packed{{0x45, 0x0F, op, 0xCC}, 9, 12, false, false},
                                 Packed{{0xC5, 0xF8, op, 0xD5}, 2, 5, true, false}, Packed{{0xC4, 0x41, 0x78, op, 0xCC}, 9, 12, true, false},
                                 Packed{{0xC5, 0xFC, op, 0xD5}, 2, 5, true, true}, Packed{{0xC5, 0xFC, op, 0xD2}, 2, 2, true, true}, Packed{{0xC4, 0x41, 0x7C, op, 0xCC}, 9, 12, true, true}}) {
            const Stub stub(form.Site, form.Destination);
            for (const std::uint64_t flags : {0x8D7ull, 0x002ull}) {
                for (std::size_t first = 0; first < kGolden.size(); first += 4) {
                    auto before = initial(flags);
                    const auto inputs = goldenLanes(first);
                    setLanes(before.Machine.Ymm[form.Source], inputs);
                    const auto out = lanes(stub.Run(before).Machine.Ymm[form.Destination]);
                    const auto old = lanes(before.Machine.Ymm[form.Destination]);
                    for (std::size_t lane = 0; lane < 8; ++lane) {
                        if (lane < 4 || form.Wide)
                            require(out[lane] == expected(squareRoot, inputs[lane]), "Packed reciprocal stub does not match the desktop Zen 2 result");
                        else
                            require(out[lane] == (form.Vex ? 0 : old[lane]), "Packed reciprocal stub changed the wrong upper bits");
                    }
                }
            }
        }
    }
}

struct Scalar {
    Bytes Site;
    std::uint8_t Destination;
    std::uint8_t Upper;
    std::uint8_t Source;
    bool Vex;
};

void scalarForms() {
    for (const bool squareRoot : {false, true}) {
        const std::uint8_t op = squareRoot ? 0x52 : 0x53;
        for (const auto& form : {Scalar{{0xF3, 0x0F, op, 0xD5}, 2, 2, 5, false}, Scalar{{0xF3, 0x0F, op, 0xD2}, 2, 2, 2, false}, Scalar{{0xF3, 0x45, 0x0F, op, 0xCC}, 9, 9, 12, false},
                                 Scalar{{0xC5, 0xE2, op, 0xD5}, 2, 3, 5, true}, Scalar{{0xC5, 0xE6, op, 0xD5}, 2, 3, 5, true}, Scalar{{0xC4, 0xE1, 0xE6, op, 0xD5}, 2, 3, 5, true}, Scalar{{0xC5, 0xEA, op, 0xD5}, 2, 2, 5, true}, Scalar{{0xC4, 0x41, 0x1A, op, 0xCC}, 9, 12, 12, true}}) {
            const Stub stub(form.Site, form.Destination);
            for (const auto& golden : kGolden) {
                auto before = initial();
                auto source = lanes(before.Machine.Ymm[form.Source]);
                source[0] = golden.Input;
                setLanes(before.Machine.Ymm[form.Source], source);
                const auto out = lanes(stub.Run(before).Machine.Ymm[form.Destination]);
                const auto upper = lanes(before.Machine.Ymm[form.Upper]);
                const auto old = lanes(before.Machine.Ymm[form.Destination]);
                require(out[0] == expected(squareRoot, golden.Input), "Scalar reciprocal stub does not match the desktop Zen 2 result");
                for (std::size_t lane = 1; lane < 8; ++lane)
                    require(out[lane] == (lane < 4 ? (form.Vex ? upper[lane] : old[lane]) : (form.Vex ? 0 : old[lane])), "Scalar reciprocal stub changed the wrong lanes");
            }
        }
    }
}

struct Memory {
    Bytes Site;
    std::size_t Lanes;
    std::size_t First;
    bool Vex;
    bool Stack;
};

void memoryForms() {
    for (const bool squareRoot : {false, true}) {
        const std::uint8_t op = squareRoot ? 0x52 : 0x53;
        for (const auto& form : {Memory{{0x0F, op, 0x10}, 4, 0, false, false}, Memory{{0xC5, 0xF8, op, 0x10}, 4, 0, true, false}, Memory{{0xC5, 0xFC, op, 0x10}, 8, 0, true, false},
                                 Memory{{0xF3, 0x0F, op, 0x50, 0x04}, 1, 1, false, false}, Memory{{0xC5, 0xEA, op, 0x50, 0x08}, 1, 2, true, false},
                                 Memory{{0x0F, op, 0x54, 0x24, 0xE8}, 4, 2, false, true}, Memory{{0xC5, 0xFC, op, 0x54, 0x24, 0xE0}, 8, 0, true, true}}) {
            const Stub stub(form.Site, 2);
            for (std::size_t first = 0; first < kGolden.size(); first += 8) {
                auto before = initial();
                const auto inputs = goldenLanes(first);
                std::memcpy(before.Machine.Memory.data(), inputs.data(), sizeof(inputs));
                std::array<std::uint32_t, 8> stack{};
                std::memcpy(stack.data(), before.Machine.RedZone.data() + 12, sizeof(stack));
                const auto& source = form.Stack ? stack : inputs;
                const auto out = lanes(stub.Run(before).Machine.Ymm[2]);
                const auto old = lanes(before.Machine.Ymm[2]);
                for (std::size_t lane = 0; lane < 8; ++lane) {
                    if (lane < form.Lanes)
                        require(out[lane] == expected(squareRoot, source[form.First + lane]), "Memory reciprocal stub does not match the desktop Zen 2 result");
                    else
                        require(out[lane] == (lane >= 4 && form.Vex ? 0 : old[lane]), "Memory reciprocal stub changed the wrong lanes");
                }
            }
        }
    }
}

std::uint64_t hash(const Bytes& site, const std::size_t width, const bool memory) {
    const Stub stub(site, 2);
    const auto inputs = hashInputs();
    std::uint64_t value = 0xCBF29CE484222325ull;
    for (std::size_t first = 0; first < inputs.size(); first += width) {
        auto before = initial();
        std::array<std::uint32_t, 8> chunk{};
        const auto count = std::min(width, inputs.size() - first);
        std::copy_n(inputs.begin() + static_cast<std::ptrdiff_t>(first), count, chunk.begin());
        if (memory)
            std::memcpy(before.Machine.Memory.data(), chunk.data(), sizeof(chunk));
        else
            setLanes(before.Machine.Ymm[5], chunk);
        const auto out = lanes(stub.Run(before).Machine.Ymm[2]);
        for (std::size_t lane = 0; lane < count; ++lane) value = (value ^ out[lane]) * 0x100000001B3ull;
    }
    return value;
}

void measuredHashes() {
    for (const bool squareRoot : {false, true}) {
        const std::uint8_t op = squareRoot ? 0x52 : 0x53;
        const auto expectedHash = kHashes[squareRoot ? 1 : 0];
        require(hash({0x0F, op, 0xD5}, 4, false) == expectedHash, "Legacy packed reciprocal stub differs from the desktop Zen 2 on the sampled inputs");
        require(hash({0xC5, 0xFC, op, 0xD5}, 8, false) == expectedHash, "256-bit reciprocal stub differs from the desktop Zen 2 on the sampled inputs");
        require(hash({0xC5, 0xFC, op, 0x10}, 8, true) == expectedHash, "256-bit memory reciprocal stub differs from the desktop Zen 2 on the sampled inputs");
    }
}

#else
void packedForms() {}
void scalarForms() {}
void memoryForms() {}
void measuredHashes() {}
#endif

}

int main() {
    try {
        ripRelocations();
        packedForms();
        scalarForms();
        memoryForms();
        measuredHashes();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
