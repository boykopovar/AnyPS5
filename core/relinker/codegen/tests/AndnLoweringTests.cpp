#include "LoweringExecution.hpp"
#include <codegen/x86/AndnLowering.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using LoweringExecution::Bytes;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes instruction(bool wide, std::uint8_t dst, std::uint8_t first, std::uint8_t second) {
    return {0xC4, static_cast<std::uint8_t>(0xE2 ^ ((dst & 8) << 4) ^ ((second & 8) << 2)),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~first & 15) << 3)), 0xF2,
        static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (second & 7))};
}

void decoding() {
    for (bool wide : {false, true}) {
        for (std::uint8_t dst = 0; dst < 16; ++dst) {
            for (std::uint8_t first = 0; first < 16; ++first) {
                for (std::uint8_t second = 0; second < 16; ++second) {
                    const auto bytes = instruction(wide, dst, first, second);
                    const auto operands = Codegen::DecodeAndn(bytes.data(), bytes.size());
                    require(operands && operands->Wide == wide && operands->Destination == dst && operands->InvertedSource == first && operands->Source == second && !operands->Memory, "ANDN register decoding changed an operand");
                }
            }
        }
    }
    const Bytes memory{0x64, 0x67, 0xC4, 0x02, 0xA8, 0xF2, 0x8C, 0xAC, 0x10, 0, 0, 0};
    const auto operands = Codegen::DecodeAndn(memory.data(), memory.size());
    require(operands && operands->Destination == 9 && operands->InvertedSource == 10 && operands->Memory && operands->Memory->RexIndexBase == 3 && operands->Memory->Prefixes == Bytes{0x64, 0x67}, "ANDN memory extensions or prefixes were lost");
    for (const Bytes bytes : {Bytes{0x90}, Bytes{0xC4, 0xE2, 0xF0}, Bytes{0xC4, 0xE1, 0xF0, 0xF2, 0xC0}})
        require(!Codegen::DecodeAndn(bytes.data(), bytes.size()), "Unrelated or truncated opcode matched ANDN");
    for (const Bytes bytes : {Bytes{0xC4, 0xE2, 0xF4, 0xF2, 0xC0}, Bytes{0xC4, 0xE2, 0xF1, 0xF2, 0xC0}, Bytes{0xC4, 0xE2, 0xF0, 0xF2, 0x84}}) {
        bool rejected = false;
        try { (void)Codegen::DecodeAndn(bytes.data(), bytes.size()); }
        catch (const Codegen::CodegenException&) { rejected = true; }
        require(rejected, "Malformed ANDN was accepted");
    }
    const Bytes overflow{0xC4, 0xE2, 0xF0, 0xF2, 0x84, 0x24, 0xFF, 0xFF, 0xFF, 0x7F};
    bool rejected = false;
    try {
        Codegen::StubBodyBuilder body;
        Codegen::EmitAndn(body, *Codegen::DecodeAndn(overflow.data(), overflow.size()));
    } catch (const Codegen::CodegenException&) { rejected = true; }
    require(rejected, "An overflowing stack displacement was silently wrapped");
}

void matching() {
    const auto bytes = instruction(true, 0, 1, 6);
    const auto intel = Codegen::MakeAmd64OnlyInstructionMatcher();
    const auto rosetta = Codegen::MakeAmd64OnlyInstructionMatcher(Codegen::Amd64OnlyTarget::Rosetta);
    require(!intel->Match(bytes.data(), bytes.size()), "Intel mode rewrote ANDN");
    const auto match = rosetta->Match(bytes.data(), bytes.size());
    require(match && match->InstructionName == "ANDN" && match->Lowering == Codegen::Amd64OnlyLowering::Trampoline && !match->Optional, "Rosetta mode did not require an ANDN trampoline");
    const std::array<std::span<const std::uint8_t>, 2> sequence{bytes, bytes};
    require(rosetta->MatchSequence(sequence, {}).has_value(), "An ANDN sequence could not move out of line");
}

#if LOWERING_EXECUTION_AVAILABLE

using LoweringExecution::Execution;
using LoweringExecution::State;

Execution executable(const Bytes& site, std::uint64_t memoryValue) {
    const auto operands = Codegen::DecodeAndn(site.data(), site.size());
    require(operands.has_value(), "Execution site was not ANDN");
    return Execution(site, memoryValue, operands->Memory && operands->Memory->RipRelative, [](Codegen::StubBodyBuilder& body, const Bytes& relocated) {
        Codegen::EmitAndn(body, *Codegen::DecodeAndn(relocated.data(), relocated.size()));
    });
}

void check(const State& state, const Codegen::AndnOperands& operands, std::uint64_t second) {
    const auto mask = operands.Wide ? ~std::uint64_t{0} : 0xFFFFFFFFull;
    const auto expected = (~state.Input[operands.InvertedSource] & second) & mask;
    for (std::size_t reg = 0; reg < 16; ++reg)
        require(state.Output[reg] == (reg == operands.Destination ? expected : state.Input[reg]), "ANDN changed a source/scratch register or produced an incorrect result");
    const auto flags = (expected == 0 ? LoweringExecution::kZero : 0u) | ((expected >> (operands.Wide ? 63 : 31)) != 0 ? LoweringExecution::kSign : 0u);
    require((state.Flags & LoweringExecution::kFlagMask) == flags, "ANDN did not preserve its defined flag semantics");
    for (const auto value : state.RedZone) require(value == LoweringExecution::kSentinel, "ANDN overwrote the caller's red zone");
}

void execution() {
    for (bool wide : {false, true}) {
        for (std::uint8_t dst = 0; dst < 16; ++dst) {
            for (const auto pair : {std::pair<std::uint8_t, std::uint8_t>{1, 6}, {dst, 10}, {11, dst}, {dst, dst}, {4, 9}, {9, 4}}) {
                const auto site = instruction(wide, dst, pair.first, pair.second);
                const auto run = executable(site, 0);
                for (int variant = 0; variant < 3; ++variant) {
                    auto state = LoweringExecution::InitialState();
                    if (variant == 1) state.Input.fill(0);
                    if (variant == 2) state.Input.fill(~std::uint64_t{0});
                    run.Run(state);
                    check(state, *Codegen::DecodeAndn(site.data(), site.size()), state.Input[pair.second]);
                }
            }
        }
        for (const Bytes tail : {Bytes{0x06}, Bytes{0x46, 0x10}, Bytes{0x84, 0x24, 0x40, 0, 0, 0}, Bytes{0x04, 0xCA}, Bytes{0x85, 0x10, 0, 0, 0}, Bytes{0x05, 0, 0, 0, 0}}) {
            Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>(wide ? 0xF0 : 0x70), 0xF2};
            site.insert(site.end(), tail.begin(), tail.end());
            constexpr std::uint64_t value = 0xFEDCBA9876543210ull;
            const auto run = executable(site, value);
            auto state = LoweringExecution::InitialState();
            state.Input[6] = reinterpret_cast<std::uint64_t>(run.Memory()) - (tail[0] == 0x46 ? 16 : 0);
            state.Input[2] = reinterpret_cast<std::uint64_t>(run.Memory()) - 3 * 8;
            state.Input[1] = 3;
            state.Input[5] = reinterpret_cast<std::uint64_t>(run.Memory()) - 16;
            state.Output[0] = value;
            run.Run(state);
            check(state, *Codegen::DecodeAndn(site.data(), site.size()), value);
        }
        for (const std::uint8_t dst : std::array<std::uint8_t, 2>{0, 4}) {
            for (const std::int32_t displacement : {-176, -160, -152, -151, -145, -144, -140, -136}) {
                Bytes site{0xC4, 0xE2, static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~1 & 15) << 3)), 0xF2,
                    static_cast<std::uint8_t>(0x84 | (dst << 3)), 0x24};
                for (unsigned shift = 0; shift < 32; shift += 8)
                    site.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(displacement) >> shift));
                constexpr std::uint64_t value = 0xFEDCBA9876543210ull;
                const auto operands = *Codegen::DecodeAndn(site.data(), site.size());
                const Execution run(site, value, false, [&](Codegen::StubBodyBuilder& body, const Bytes&) {
                    for (int part = 0; part < 2; ++part) {
                        Bytes store{0xC7, 0x84, 0x24};
                        const auto slot = static_cast<std::uint32_t>(displacement + part * 4);
                        const auto word = static_cast<std::uint32_t>(value >> (part * 32));
                        for (unsigned shift = 0; shift < 32; shift += 8) store.push_back(static_cast<std::uint8_t>(slot >> shift));
                        for (unsigned shift = 0; shift < 32; shift += 8) store.push_back(static_cast<std::uint8_t>(word >> shift));
                        body.Raw(store);
                    }
                    Codegen::EmitAndn(body, operands);
                });
                auto state = LoweringExecution::InitialState();
                run.Run(state);
                check(state, operands, value);
            }
        }
        for (const auto pair : {std::pair<std::uint8_t, std::uint8_t>{1, 6}, {4, 9}, {9, 4}}) {
            const auto site = instruction(wide, 4, pair.first, pair.second);
            const auto operands = *Codegen::DecodeAndn(site.data(), site.size());
            Codegen::StubBodyBuilder probe;
            Codegen::EmitAndn(probe, operands);
            const auto built = probe.Finish();
            const Bytes raw(built.Bytes.begin(), built.Bytes.begin() + static_cast<std::ptrdiff_t>(built.ReturnBranchOffset));
            Codegen::X64InstructionDecoder decoder;
            for (std::size_t cut = 0; cut < raw.size(); cut += decoder.Decode(raw.data() + cut, raw.size() - cut)) {
                const Execution run(site, 0, false, [&](Codegen::StubBodyBuilder& body, const Bytes&) {
                    body.Raw(std::span<const std::uint8_t>(raw.data(), cut));
                    for (std::int32_t disp = -256; disp < -128; disp += 4) {
                        Bytes clobber{0xC7, 0x84, 0x24};
                        for (unsigned shift = 0; shift < 32; shift += 8)
                            clobber.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(disp) >> shift));
                        clobber.insert(clobber.end(), {0xEF, 0xBE, 0xAD, 0xDE});
                        body.Raw(clobber);
                    }
                    body.Raw(std::span<const std::uint8_t>(raw.data() + cut, raw.size() - cut));
                });
                auto state = LoweringExecution::InitialState();
                run.Run(state);
                check(state, operands, state.Input[pair.second]);
            }
        }
    }
}
#else
void execution() { std::cout << "ANDN execution checks require an x86-64 Linux, macOS or Windows build\n"; }
#endif

}

int main() {
    try {
        decoding();
        matching();
        execution();
        std::cout << "ANDN lowering checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
