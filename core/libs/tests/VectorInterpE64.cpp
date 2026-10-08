#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
using namespace ShaderRecompiler;
static void Require(bool condition) {
    if (!condition) throw std::runtime_error("VOP3 interpolation regression");
}
static std::string Translate(const RdnaInstruction& instruction, bool barycentric) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    context.SetPixelInput(nullptr, barycentric);
    context.TranslateInstruction(instruction);
    return ProgramToString(program);
}
static void Reject(std::array<std::uint32_t, 2> code) {
    bool rejected = false;
    try { (void)DecodeRdnaVop3(0, code, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected);
}
int main() try {
    std::uint32_t checks = 0;
    for (std::uint32_t op = 0; op < 3; ++op) {
        for (std::uint32_t attr : {0u, 1u, 31u, 32u, 63u}) {
            for (std::uint32_t chan = 0; chan < 4; ++chan) {
                for (std::uint32_t dest : {0u, 5u, 255u}) {
                    for (std::uint32_t source : {0u, 1u, 2u, 255u}) {
                        if (op == 2 && source > 2) continue;
                        const auto encodedSource = op == 2 ? source : source + 256u;
                        const std::array<std::uint32_t, 2> wide{
                            0xd6000000u | (op << 16u) | dest,
                            0x02000000u | (encodedSource << 9u) | (chan << 6u) | attr};
                        const std::array<std::uint32_t, 1> compact{
                            0xc8000000u | (dest << 18u) | (op << 16u) | (attr << 10u) | (chan << 8u) | source};
                        const auto a = DecodeRdnaVop3(0, wide, 0);
                        const auto b = DecodeRdnaVintrp(0, compact, 0);
                        Require(a.family == RdnaInstructionFamily::VOP3 && a.wordCount == 2 && a.opcodeId == 0x200u + op);
                        Require(a.op == b.op && a.destination.reg == b.destination.reg);
                        Require(a.source0.kind == b.source0.kind && a.source0.reg == b.source0.reg && a.source0.value == b.source0.value);
                        Require(a.source1.value == attr && a.source2.value == chan && a.sourceCount == 3);
                        for (bool barycentric : {false, true}) Require(Translate(a, barycentric) == Translate(b, barycentric));
                        ++checks;
                    }
                }
            }
        }
    }
    for (std::uint32_t bit : {8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u}) Reject({0xd6000005u | (1u << bit), 0x02020400u});
    for (std::uint32_t bit : {8u, 27u, 28u, 29u, 30u, 31u}) Reject({0xd6000005u, 0x02020400u | (1u << bit)});
    Reject({0xd6000005u, 0x02000400u});
    Reject({0xd6020005u, 0x02000600u});
    Reject({0xd6000005u, 0x02060400u});
    bool truncated = false;
    try { const std::array<std::uint32_t, 1> code{0xd6000005u}; (void)DecodeRdnaVop3(0, code, 0); }
    catch (const std::out_of_range&) { truncated = true; }
    Require(truncated);
    std::printf("PASS: %u VOP3 interpolation encodings; E32/E64 IR parity in both barycentric modes; malformed/modifier/truncation guards\n", checks);
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
}
