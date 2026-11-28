#include "GuestCpu.hpp"
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <stdexcept>

using anyps5::experimental::GuestCpu;

int main() {
    const std::array<std::uint8_t, 12> addition{
        0xB8, 0x28, 0, 0, 0,
        0x05, 0x02, 0, 0, 0,
        0xC3, 0x90
    };
    if (GuestCpu(addition).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 1> invalid{0x90};
    bool rejected = false;
    try {
        GuestCpu(invalid).run();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 5> truncated{0xB8, 1, 2, 3, 4};
    rejected = false;
    try {
        GuestCpu(truncated).run();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    rejected = false;
    try {
        GuestCpu(addition).run(1);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 18> branch{
        0xB8, 0x2A, 0, 0, 0,
        0x3D, 0x2A, 0, 0, 0,
        0x75, 0x05,
        0xC3,
        0xB8, 0, 0, 0, 0
    };
    if (GuestCpu(branch).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 7> extended{
        0x41, 0xB8, 0x07, 0, 0, 0, 0xC3
    };
    GuestCpu extendedCpu(extended);
    if (extendedCpu.run() != 0 || extendedCpu.registerValue(8) != 7) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 2> loop{0xEB, 0xFE};
    rejected = false;
    try {
        GuestCpu(loop).run(8);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 2> invalidJump{0xEB, 0x7F};
    rejected = false;
    try {
        GuestCpu(invalidJump).run();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 17> nestedCall{
        0xE8, 0x06, 0, 0, 0,
        0xC3,
        0x90, 0x90, 0x90, 0x90, 0x90,
        0xB8, 0x2A, 0, 0, 0, 0xC3
    };
    GuestCpu callCpu(nestedCall);
    if (callCpu.run() != 42 || callCpu.registerValue(4) != 65536) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 8> stackCode{
        0xB8, 0x2A, 0, 0, 0, 0x50, 0x58, 0xC3
    };
    GuestCpu stackCpu(stackCode, 64);
    if (stackCpu.run() != 42 || stackCpu.registerValue(4) != 64) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    GuestCpu memoryCpu(stackCode, 64);
    memoryCpu.writeMemory64(8, 0x123456789ABCDEF0ULL);
    if (memoryCpu.readMemory64(8) != 0x123456789ABCDEF0ULL) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    rejected = false;
    try { memoryCpu.writeMemory64(60, 1); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    rejected = false;
    try { memoryCpu.readMemory64(65); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 10> overflow{
        0xB8, 1, 0, 0, 0, 0x50, 0x50, 0x50, 0x50, 0xC3
    };
    rejected = false;
    try { GuestCpu(overflow, 16).run(); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 1> underflow{0x58};
    rejected = false;
    try { GuestCpu(underflow, 16).run(); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    const std::array<std::uint8_t, 15> registerRoundtrip{
        0xB8, 0x2A, 0, 0, 0,
        0x89, 0xC1,
        0xB8, 0, 0, 0, 0,
        0x8B, 0xC1, 0xC3
    };
    if (GuestCpu(registerRoundtrip).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 19> displacementMov{
        0xB8, 0x2A, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0xB8, 0, 0, 0, 0,
        0x8B, 0x44, 0x24, 0xF8,
        0xC3
    };
    if (GuestCpu(displacementMov, 64).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 11> extendedMov{
        0xB8, 0x2A, 0, 0, 0,
        0x41, 0x89, 0xC0,
        0x41, 0x8B, 0xC0
    };
    GuestCpu rexCpu(extendedMov);
    rejected = false;
    try { rexCpu.run(); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected || rexCpu.registerValue(8) != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 4> invalidAddress{0x8B, 0x04, 0x24, 0xC3};
    rejected = false;
    try { GuestCpu(invalidAddress, 64).run(); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    const std::array<std::uint8_t, 2> truncatedModrm{0x8B, 0x04};
    rejected = false;
    try { GuestCpu(truncatedModrm).run(); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // Use RSP-8 so the full operand fits inside the bounded guest memory.
    const std::array<std::uint8_t, 19> memoryRoundtrip{
        0xB8, 0x2A, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0xB8, 0, 0, 0, 0,
        0x8B, 0x44, 0x24, 0xF8,
        0xC3
    };
    GuestCpu memoryRoundtripCpu(memoryRoundtrip, 64);
    if (memoryRoundtripCpu.run() != 42 || memoryRoundtripCpu.readMemory32(56) != 42)
        { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // REX.W: 64-bit register -> memory -> register roundtrip.
    const std::array<std::uint8_t, 11> wideMemory{
        0x48, 0x89, 0x64, 0x24, 0xF8,
        0x48, 0x8B, 0x44, 0x24, 0xF8,
        0xC3
    };
    GuestCpu wideCpu(wideMemory, 64);
    if (wideCpu.run() != 64) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    if (wideCpu.readMemory64(56) != 64) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // REX.R selects R8 as the destination register.
    const std::array<std::uint8_t, 10> rexRMov{
        0xB8, 0x2A, 0, 0, 0,
        0x44, 0x8B, 0xC0,
        0xC3, 0x90
    };
    GuestCpu rexRCpu(rexRMov);
    if (rexRCpu.run() != 42 || rexRCpu.registerValue(8) != 42)
        { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // SIB addressing: [RSP + RAX*2 - 16] resolves to address 56.
    const std::array<std::uint8_t, 15> sibIndexed{
        0xB8, 0x04, 0, 0, 0,
        0x8B, 0x44, 0x44, 0xF0,
        0xC3, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    GuestCpu sibCpu(sibIndexed, 64);
    sibCpu.writeMemory32(56, 42);
    if (sibCpu.run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // RIP-relative address uses the instruction pointer after disp32.
    const std::array<std::uint8_t, 7> ripRelative{
        0x8B, 0x05, 0x08, 0, 0, 0, 0xC3
    };
    GuestCpu ripCpu(ripRelative, 64);
    ripCpu.writeMemory32(14, 42);
    if (ripCpu.run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // Reject malformed and out-of-range operands.
    const std::array<std::uint8_t, 2> truncatedDisp{0x8B, 0x45};
    rejected = false;
    try { GuestCpu(truncatedDisp).run(); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // Register arithmetic (ADD r32,r/m32 and SUB r32,r/m32).
    // The final ADD immediate is executed before the terminal RET.
    const std::array<std::uint8_t, 20> arithmeticReturn{
        0xB8, 40, 0, 0, 0,
        0xB9, 4, 0, 0, 0,
        0x03, 0xC1,
        0x2B, 0xC1,
        0x05, 2, 0, 0, 0,
        0xC3
    };
    if (GuestCpu(arithmeticReturn).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // Store 40 at [RSP-8], add 2 directly in guest memory, reload 42.
    const std::array<std::uint8_t, 23> memoryArithmetic{
        0xB8, 40, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0xB8, 2, 0, 0, 0,
        0x01, 0x44, 0x24, 0xF8,
        0x8B, 0x44, 0x24, 0xF8,
        0xC3
    };
    GuestCpu memArithmeticCpu(memoryArithmetic, 64);
    if (memArithmeticCpu.run() != 42 || memArithmeticCpu.readMemory32(56) != 42)
        { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // CMP sets ZF without changing the source registers.
    const std::array<std::uint8_t, 16> compareEqual{
        0xB8, 42, 0, 0, 0,
        0xB9, 42, 0, 0, 0,
        0x39, 0xC8,
        0x75, 0x01,
        0xC3, 0x90
    };
    if (GuestCpu(compareEqual).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // TEST updates ZF without writing its operand.
    const std::array<std::uint8_t, 11> testNonzero{
        0xB8, 42, 0, 0, 0,
        0x85, 0xC0,
        0x74, 0x01,
        0xC3, 0x90
    };
    if (GuestCpu(testNonzero).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // REX.W ADD also accepts the 64-bit operand width.
    const std::array<std::uint8_t, 4> wideAdd{
        0x48, 0x03, 0xC1, 0xC3
    };
    GuestCpu wideAddCpu(wideAdd);
    if (wideAddCpu.run() != 0) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // Signed comparison: -1 is less than +1 (JL uses SF != OF).
    const std::array<std::uint8_t, 21> signedLess{
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,
        0xB9, 1, 0, 0, 0,
        0x39, 0xC8,
        0x7C, 0x01,
        0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    // Branch skips an intentionally unsupported NOP when the condition is true.
    if (GuestCpu(signedLess).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // Unsigned 0xFFFFFFFF is above 1 (JA uses !CF && !ZF).
    const std::array<std::uint8_t, 21> unsignedAbove{
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,
        0xB9, 1, 0, 0, 0,
        0x39, 0xC8,
        0x77, 0x01,
        0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    if (GuestCpu(unsignedAbove).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // Near conditional branch (0F 8C rel32) skips an invalid opcode.
    const std::array<std::uint8_t, 25> nearSignedLess{
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,
        0xB9, 1, 0, 0, 0,
        0x39, 0xC8,
        0x0F, 0x8C, 1, 0, 0, 0,
        0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    if (GuestCpu(nearSignedLess).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // Overflow: 0x7FFFFFFF + 1 becomes negative, but signed JL is false (SF == OF).
    const std::array<std::uint8_t, 23> overflowBranch{
        0xB8, 0xFF, 0xFF, 0xFF, 0x7F,
        0xB9, 1, 0, 0, 0,
        0x03, 0xC1,
        0x7C, 0x01,
        0xEB, 0x01,
        0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    if (GuestCpu(overflowBranch).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    // REX.W + B8 encodes a full 64-bit immediate (movabs rax, imm64).
    const std::array<std::uint8_t, 11> movabs{
        0x48, 0xB8, 0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0xC3
    };
    if (GuestCpu(movabs).run() != 0x123456789ABCDEF0ULL) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }

    // 32-bit register destinations must clear the high 32 bits.
    const std::array<std::uint8_t, 18> zeroExtend{
        0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xB9, 42, 0, 0, 0,
        0x8B, 0xC1, 0xC3
    };
    if (GuestCpu(zeroExtend).run() != 42) { std::fprintf(stderr, "GuestCpuTests failed at line %d\n", __LINE__); return EXIT_FAILURE; }
    // LEA calculates RSP-8 without dereferencing the address.
    const std::array<std::uint8_t, 6> leaStack{
        0x48, 0x8D, 0x44, 0x24, 0xF8, 0xC3
    };
    if (GuestCpu(leaStack, 64).run() != 56) {
        std::fprintf(stderr, "LEA stack calculation failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // A conventional frame setup/teardown restores RSP and RBP.
    const std::array<std::uint8_t, 11> framedFunction{
        0x55, 0x48, 0x89, 0xE5,
        0xB8, 42, 0, 0, 0,
        0xC9, 0xC3
    };
    GuestCpu frameCpu(framedFunction, 64);
    if (frameCpu.run() != 42 || frameCpu.registerValue(4) != 64 ||
        frameCpu.registerValue(5) != 0) {
        std::fprintf(stderr, "LEAVE frame restoration failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Realistic function-shaped machine code: CALL, PUSH RBP, MOV RBP,RSP,
    // LEA EAX,[RDI+2], POP RBP, RET. RDI starts at zero.
    const std::array<std::uint8_t, 15> compiledStyleWithRet{
        0xE8, 0x01, 0, 0, 0, 0xC3,
        0x55, 0x48, 0x89, 0xE5,
        0x8D, 0x47, 0x02, 0x5D, 0xC3
    };
    GuestCpu compiledCpu(compiledStyleWithRet, 64);
    if (compiledCpu.run() != 2 || compiledCpu.registerValue(4) != 64) {
        std::fprintf(stderr, "Compiler-style function failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // LEA with ModR/M register-direct addressing is invalid.
    const std::array<std::uint8_t, 3> invalidLea{0x8D, 0xC0, 0xC3};
    rejected = false;
    try { GuestCpu(invalidLea).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "Invalid LEA was accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Compiled from tests/compiler_fixture.c with Clang -O1 targeting x86-64.
    // 8D 47 02 = lea eax,[rdi+2]; C3 = ret.
    // The caller sets EDI=40 and CALLs the compiler-produced function.
    const std::array<std::uint8_t, 15> actualCompilerFixture{
        0xBF, 40, 0, 0, 0,        // mov edi,40
        0xE8, 0x01, 0, 0, 0,      // call guest_add_two
        0xC3,                    // caller ret
        0x8D, 0x47, 0x02, 0xC3  // compiled .text from fixture
    };
    GuestCpu actualCompilerCpu(actualCompilerFixture, 64);
    if (actualCompilerCpu.run() != 42 || actualCompilerCpu.registerValue(4) != 64) {
        std::fprintf(stderr, "Real compiler fixture failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Immediate ADD/SUB/CMP, including negative sign-extended imm8.
    const std::array<std::uint8_t, 18> immediateArithmetic{
        0xB8, 40, 0, 0, 0,
        0x83, 0xC0, 0x05,       // add eax,5
        0x83, 0xE8, 0x03,       // sub eax,3
        0x83, 0xF8, 42,         // cmp eax,42
        0x75, 0x01,             // jne invalid opcode
        0xC3, 0x90
    };
    if (GuestCpu(immediateArithmetic).run() != 42) {
        std::fprintf(stderr, "Immediate arithmetic failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // Signed IMUL: -7 * -6 = 42, 32-bit result zero-extended.
    const std::array<std::uint8_t, 15> signedMultiply{
        0xB8, 0xF9, 0xFF, 0xFF, 0xFF,
        0xB9, 0xFA, 0xFF, 0xFF, 0xFF,
        0x0F, 0xAF, 0xC1,
        0xC3, 0x90
    };
    if (GuestCpu(signedMultiply).run() != 42) {
        std::fprintf(stderr, "Signed multiplication failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // 64-bit signed IMUL: -7 * -6 = 42.
    const std::array<std::uint8_t, 25> wideSignedMultiply{
        0x48, 0xB8, 0xF9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x48, 0xB9, 0xFA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x48, 0x0F, 0xAF, 0xC1, 0xC3
    };
    if (GuestCpu(wideSignedMultiply).run() != 42) {
        std::fprintf(stderr, "64-bit signed multiplication failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // 2^62 * 4 overflows signed 64-bit. JO must skip the unsupported NOP.
    const std::array<std::uint8_t, 28> wideMultiplyOverflow{
        0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0x00, 0x40,
        0xB9, 4, 0, 0, 0,
        0x48, 0x0F, 0xAF, 0xC1,
        0x70, 0x01,
        0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    if (GuestCpu(wideMultiplyOverflow).run() != 42) {
        std::fprintf(stderr, "64-bit IMUL overflow flag failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // -2^63 * -1 overflows even though the truncated product is -2^63.
    const std::array<std::uint8_t, 33> signedBoundaryOverflow{
        0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0x80,
        0x48, 0xB9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x48, 0x0F, 0xAF, 0xC1,
        0x70, 0x01,
        0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    };
    if (GuestCpu(signedBoundaryOverflow).run() != 42) {
        std::fprintf(stderr, "INT64_MIN multiplication overflow failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // M9: loop sum(1..6) using DEC ECX and JNE back-edge.
    const auto sumLoop = std::to_array<std::uint8_t>({
        0xB8, 0, 0, 0, 0,       // mov eax,0
        0xB9, 6, 0, 0, 0,       // mov ecx,6
        0x03, 0xC1,             // add eax,ecx
        0xFF, 0xC9,             // dec ecx
        0x75, 0xFA,             // jne -6 (back to add)
        0xC3
    });
    if (GuestCpu(sumLoop).run() != 21) {
        std::fprintf(stderr, "M9 decrement loop failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // INC a memory operand in a loop, then load the accumulated value.
    const auto memoryLoop = std::to_array<std::uint8_t>({
        0xB8, 0, 0, 0, 0,       // mov eax,0
        0x89, 0x44, 0x24, 0xF8, // mov [rsp-8],eax
        0xB9, 3, 0, 0, 0,       // mov ecx,3
        0xFF, 0x44, 0x24, 0xF8, // inc dword [rsp-8]
        0xFF, 0xC9,             // dec ecx
        0x75, 0xF8,             // jne -8 (back to inc)
        0x8B, 0x44, 0x24, 0xF8, // mov eax,[rsp-8]
        0xC3
    });
    GuestCpu memoryLoopCpu(memoryLoop, 64);
    if (memoryLoopCpu.run() != 3 || memoryLoopCpu.readMemory32(56) != 3) {
        std::fprintf(stderr, "M9 memory loop failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Group 81 imm32: ADD 0x10000 then CMP, including the wide sign extension.
    const auto imm32Arithmetic = std::to_array<std::uint8_t>({
        0xB8, 0, 0, 0, 0,
        0x81, 0xC0, 0, 0, 1, 0,  // add eax,65536
        0x81, 0xF8, 0, 0, 1, 0,  // cmp eax,65536
        0x75, 0x01, 0xC3, 0x90
    });
    if (GuestCpu(imm32Arithmetic).run() != 65536) {
        std::fprintf(stderr, "M9 imm32 arithmetic failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Carry flag must survive INC and DEC (unlike ADD/SUB).
    const auto carryPreserved = std::to_array<std::uint8_t>({
        0xB8, 0, 0, 0, 0,       // mov eax,0
        0x83, 0xE8, 0x01,       // sub eax,1 (CF=1)
        0xFF, 0xC0,             // inc eax (CF must remain 1)
        0x72, 0x01,             // jc +1
        0x90,                   // invalid if CF lost
        0xFF, 0xC8,             // dec eax (CF must remain 1)
        0x72, 0x01,             // jc +1
        0x90,
        0xB8, 42, 0, 0, 0, 0xC3
    });
    if (GuestCpu(carryPreserved).run() != 42) {
        std::fprintf(stderr, "M9 INC/DEC carry preservation failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Common compiler idiom: XOR EAX,EAX clears the register and sets ZF.
    const auto xorZero = std::to_array<std::uint8_t>({
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,
        0x31, 0xC0,             // xor eax,eax
        0x75, 0x01,             // jne -> invalid NOP (must not branch)
        0xC3, 0x90
    });
    if (GuestCpu(xorZero).run() != 0) {
        std::fprintf(stderr, "XOR zero idiom failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // XOR EAX,ECX and reverse opcode 33 must agree; 40 ^ 2 = 42.
    const auto xorRegisters = std::to_array<std::uint8_t>({
        0xB8, 40, 0, 0, 0,
        0xB9, 2, 0, 0, 0,
        0x33, 0xC1, 0xC3
    });
    if (GuestCpu(xorRegisters).run() != 42) {
        std::fprintf(stderr, "XOR register form failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // Memory XOR updates guest memory in place.
    const auto xorMemory = std::to_array<std::uint8_t>({
        0xB8, 40, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0xB9, 2, 0, 0, 0,
        0x31, 0x4C, 0x24, 0xF8,
        0x8B, 0x44, 0x24, 0xF8,
        0xC3
    });
    GuestCpu xorMemoryCpu(xorMemory, 64);
    if (xorMemoryCpu.run() != 42 || xorMemoryCpu.readMemory32(56) != 42) {
        std::fprintf(stderr, "XOR memory form failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M11: compiler-style bit masking and merging.
    const auto andOrRegisters = std::to_array<std::uint8_t>({
        0xB8, 0x3F, 0, 0, 0,    // mov eax,63
        0xB9, 0x0A, 0, 0, 0,    // mov ecx,10
        0x23, 0xC1,             // and eax,ecx -> 10
        0xB9, 0x20, 0, 0, 0,    // mov ecx,32
        0x0B, 0xC1,             // or eax,ecx -> 42
        0xC3
    });
    if (GuestCpu(andOrRegisters).run() != 42) {
        std::fprintf(stderr, "M11 AND/OR register operations failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // The memory-destination forms must preserve the other bits and
    // correctly update the sign/zero flags.
    const auto andOrMemory = std::to_array<std::uint8_t>({
        0xB8, 0x3F, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8, // mov [rsp-8],eax
        0xB9, 0x0A, 0, 0, 0,
        0x21, 0x4C, 0x24, 0xF8, // and [rsp-8],ecx -> 10
        0xB9, 0x20, 0, 0, 0,
        0x09, 0x4C, 0x24, 0xF8, // or [rsp-8],ecx -> 42
        0x8B, 0x44, 0x24, 0xF8,
        0xC3
    });
    GuestCpu andOrMemoryCpu(andOrMemory, 64);
    if (andOrMemoryCpu.run() != 42 || andOrMemoryCpu.readMemory32(56) != 42) {
        std::fprintf(stderr, "M11 AND/OR memory operations failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // AND producing zero sets ZF and clears CF/OF.
    const auto andZero = std::to_array<std::uint8_t>({
        0xB8, 1, 0, 0, 0,
        0xB9, 2, 0, 0, 0,
        0x21, 0xC8,             // and eax,ecx -> zero
        0x75, 0x01,             // jne invalid NOP
        0xC3, 0x90
    });
    if (GuestCpu(andZero).run() != 0) {
        std::fprintf(stderr, "M11 AND zero-flag regression failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M12: signed SETL from -1 < 1, writing only AL.
    const auto setSignedLess = std::to_array<std::uint8_t>({
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF, // mov eax,-1
        0xB9, 1, 0, 0, 0,             // mov ecx,1
        0x39, 0xC8,                   // cmp eax,ecx
        0x0F, 0x9C, 0xC0,             // setl al
        0xC3
    });
    GuestCpu setSignedCpu(setSignedLess);
    if (setSignedCpu.run() != 0xFFFFFF01ULL) {
        std::fprintf(stderr, "M12 signed SETcc or byte preservation failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Unsigned SETA should be true for 0xFFFFFFFF > 1.
    const auto setUnsignedAbove = std::to_array<std::uint8_t>({
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,
        0xB9, 1, 0, 0, 0,
        0x39, 0xC8,
        0x0F, 0x97, 0xC0, // seta al
        0xC3
    });
    if (GuestCpu(setUnsignedAbove).run() != 0xFFFFFF01ULL) {
        std::fprintf(stderr, "M12 unsigned SETcc failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Legacy AH encoding must update bits 8..15 without corrupting AL.
    const auto setHighByte = std::to_array<std::uint8_t>({
        0xB8, 0x78, 0x56, 0x34, 0x12,
        0x39, 0xC0,             // cmp eax,eax (ZF=1)
        0x0F, 0x94, 0xC4,       // sete ah
        0xC3
    });
    if (GuestCpu(setHighByte).run() != 0x12340178ULL) {
        std::fprintf(stderr, "M12 legacy high-byte SETcc failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // REX.B maps the low-byte destination to R8B.
    const auto setR8Byte = std::to_array<std::uint8_t>({
        0x41, 0xB8, 0x78, 0x56, 0x34, 0x12,
        0x39, 0xC0,             // cmp eax,eax
        0x41, 0x0F, 0x94, 0xC0, // sete r8b
        0xC3
    });
    GuestCpu setR8Cpu(setR8Byte);
    if (setR8Cpu.run() != 0 || setR8Cpu.registerValue(8) != 0x12345601ULL) {
        std::fprintf(stderr, "M12 REX extended byte register failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // SETE [rsp-8] must change only the addressed byte.
    const auto setMemoryByte = std::to_array<std::uint8_t>({
        0xB8, 0x78, 0x56, 0x34, 0x12,
        0x89, 0x44, 0x24, 0xF8, // mov [rsp-8],eax
        0x39, 0xC0,             // cmp eax,eax
        0x0F, 0x94, 0x44, 0x24, 0xF8, // sete byte [rsp-8]
        0xC3
    });
    GuestCpu setMemoryCpu(setMemoryByte, 64);
    if (setMemoryCpu.run() != 0x12345678ULL ||
        setMemoryCpu.readMemory32(56) != 0x12345601U) {
        std::fprintf(stderr, "M12 memory SETcc byte write failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M13: zero-extend a byte value (0xFF -> 255) from CL.
    const auto movzxRegister = std::to_array<std::uint8_t>({
        0xB9, 0xFF, 0, 0, 0, // mov ecx,255
        0x0F, 0xB6, 0xC1,   // movzx eax,cl
        0xC3
    });
    if (GuestCpu(movzxRegister).run() != 255) {
        std::fprintf(stderr, "M13 MOVZX register failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Sign-extend 0xFF to -1 in a 32-bit destination (zero-extended RAX).
    const auto movsxRegister = std::to_array<std::uint8_t>({
        0xB9, 0xFF, 0, 0, 0,
        0x0F, 0xBE, 0xC1,   // movsx eax,cl
        0xC3
    });
    if (GuestCpu(movsxRegister).run() != 0xFFFFFFFFULL) {
        std::fprintf(stderr, "M13 MOVSX 32-bit sign extension failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // REX.W MOVSX sign-extends to the full 64-bit destination.
    const auto movsxWide = std::to_array<std::uint8_t>({
        0xB9, 0x80, 0, 0, 0,
        0x48, 0x0F, 0xBE, 0xC1, // movsx rax,cl
        0xC3
    });
    if (GuestCpu(movsxWide).run() != 0xFFFFFFFFFFFFFF80ULL) {
        std::fprintf(stderr, "M13 MOVSX 64-bit sign extension failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Without REX, encoded byte register 4 means AH.
    const auto movzxHighByte = std::to_array<std::uint8_t>({
        0xB8, 0x78, 0xAB, 0x34, 0x12,
        0x0F, 0xB6, 0xC4, // movzx eax,ah -> 0xAB
        0xC3
    });
    if (GuestCpu(movzxHighByte).run() != 0xAB) {
        std::fprintf(stderr, "M13 MOVZX AH failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // With a REX prefix, byte register 4 means SPL, not AH.
    const auto movzxSpl = std::to_array<std::uint8_t>({
        0x40, 0x0F, 0xB6, 0xC4, // movzx eax,spl
        0xC3
    });
    if (GuestCpu(movzxSpl, 64).run() != 64) {
        std::fprintf(stderr, "M13 MOVZX SPL failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Memory source must read exactly one byte, not a full dword.
    const auto movzxMemory = std::to_array<std::uint8_t>({
        0x0F, 0xB6, 0x44, 0x24, 0xF8, // movzx eax,byte [rsp-8]
        0xC3
    });
    GuestCpu movzxMemoryCpu(movzxMemory, 64);
    movzxMemoryCpu.writeMemory32(56, 0x123456FE);
    if (movzxMemoryCpu.run() != 254) {
        std::fprintf(stderr, "M13 MOVZX memory failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // The same memory byte must sign-extend when MOVSX is used.
    const auto movsxMemory = std::to_array<std::uint8_t>({
        0x48, 0x0F, 0xBE, 0x44, 0x24, 0xF8,
        0xC3
    });
    GuestCpu movsxMemoryCpu(movsxMemory, 64);
    movsxMemoryCpu.writeMemory32(56, 0x123456FE);
    if (movsxMemoryCpu.run() != 0xFFFFFFFFFFFFFFFEULL) {
        std::fprintf(stderr, "M13 MOVSX memory failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M14: MOVZX EAX,CX zero-extends the low 16 bits.
    const auto movzxWordRegister = std::to_array<std::uint8_t>({
        0xB9, 0xFF, 0xFF, 0x34, 0x12,
        0x0F, 0xB7, 0xC1,
        0xC3
    });
    if (GuestCpu(movzxWordRegister).run() != 65535) {
        std::fprintf(stderr, "M14 MOVZX word register failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // MOVSX EAX,CX sign-extends 0x8000 to 0xFFFF8000.
    const auto movsxWordRegister = std::to_array<std::uint8_t>({
        0xB9, 0x00, 0x80, 0x34, 0x12,
        0x0F, 0xBF, 0xC1,
        0xC3
    });
    if (GuestCpu(movsxWordRegister).run() != 0xFFFF8000ULL) {
        std::fprintf(stderr, "M14 MOVSX word 32-bit destination failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // REX.W sign-extends the same source across all 64 bits.
    const auto movsxWideWord = std::to_array<std::uint8_t>({
        0xB9, 0x00, 0x80, 0x34, 0x12,
        0x48, 0x0F, 0xBF, 0xC1,
        0xC3
    });
    if (GuestCpu(movsxWideWord).run() != 0xFFFFFFFFFFFF8000ULL) {
        std::fprintf(stderr, "M14 MOVSX word 64-bit destination failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // A word load at the last two bytes of guest memory must succeed.
    const auto movzxWordMemory = std::to_array<std::uint8_t>({
        0x0F, 0xB7, 0x44, 0x24, 0xFE,
        0xC3
    });
    GuestCpu movzxWordMemoryCpu(movzxWordMemory, 64);
    movzxWordMemoryCpu.writeMemory32(60, 0x8001ABCD);
    if (movzxWordMemoryCpu.run() != 0x8001) {
        std::fprintf(stderr, "M14 MOVZX word memory failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Memory reads must reject a 16-bit source crossing the stack boundary.
    const auto invalidWordMemory = std::to_array<std::uint8_t>({
        0x0F, 0xB7, 0x44, 0x24, 0xFF
    });
    rejected = false;
    try { GuestCpu(invalidWordMemory, 64).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M14 out-of-bounds word load accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M15: left shift and right shift by immediate counts.
    const auto shiftLeft = std::to_array<std::uint8_t>({
        0xB8, 21, 0, 0, 0, 0xD1, 0xE0, 0xC3 // shl eax,1 -> 42
    });
    if (GuestCpu(shiftLeft).run() != 42) {
        std::fprintf(stderr, "M15 SHL failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto shiftRight = std::to_array<std::uint8_t>({
        0xB8, 84, 0, 0, 0, 0xC1, 0xE8, 1, 0xC3 // shr eax,1
    });
    if (GuestCpu(shiftRight).run() != 42) {
        std::fprintf(stderr, "M15 SHR failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // SAR must preserve the sign bit (0xFFFFFFFC >> 1 = -2).
    const auto shiftArithmetic = std::to_array<std::uint8_t>({
        0xB8, 0xFC, 0xFF, 0xFF, 0xFF, 0xD1, 0xF8, 0xC3
    });
    if (GuestCpu(shiftArithmetic).run() != 0xFFFFFFFEULL) {
        std::fprintf(stderr, "M15 SAR sign preservation failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // D3 reads the shift count from CL.
    const auto shiftByCl = std::to_array<std::uint8_t>({
        0xB8, 7, 0, 0, 0,
        0xB9, 2, 0, 0, 0,
        0xD3, 0xE0, // shl eax,cl -> 28
        0x05, 14, 0, 0, 0,
        0xC3
    });
    if (GuestCpu(shiftByCl).run() != 42) {
        std::fprintf(stderr, "M15 CL shift failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // REX.W: 64-bit shift with a value above the 32-bit range.
    const auto wideShift = std::to_array<std::uint8_t>({
        0xB8, 1, 0, 0, 0,
        0x48, 0xC1, 0xE0, 0x21, // shl rax,33
        0xC3
    });
    if (GuestCpu(wideShift).run() != (std::uint64_t{1} << 33)) {
        std::fprintf(stderr, "M15 wide shift failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // Memory operand: shift the value at [rsp-8] and reload.
    const auto memoryShift = std::to_array<std::uint8_t>({
        0xB8, 21, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0xD1, 0x64, 0x24, 0xF8, // shl dword [rsp-8],1
        0x8B, 0x44, 0x24, 0xF8,
        0xC3
    });
    GuestCpu memoryShiftCpu(memoryShift, 64);
    if (memoryShiftCpu.run() != 42 || memoryShiftCpu.readMemory32(56) != 42) {
        std::fprintf(stderr, "M15 memory shift failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // A masked shift count of zero must not alter the operand.
    const auto zeroShift = std::to_array<std::uint8_t>({
        0xB8, 42, 0, 0, 0,
        0xC1, 0xE0, 32, // 32-bit count masked to zero
        0xC3
    });
    if (GuestCpu(zeroShift).run() != 42) {
        std::fprintf(stderr, "M15 zero-count shift failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M16: real Clang -O1 x86-64 output from compiler_fixture.c.
    // Prefix sets the SysV integer argument in EDI before entering the function.
    const auto compiledSumSix = std::to_array<std::uint8_t>({
        0xBF, 6, 0, 0, 0,       // mov edi,6
        0x85, 0xFF, 0x7E, 0x13, // test edi,edi; jle +19
        0x8D, 0x47, 0xFF,       // lea eax,[rdi-1]
        0x8D, 0x4F, 0xFE,       // lea ecx,[rdi-2]
        0x48, 0x0F, 0xAF, 0xC8, // imul rcx,rax
        0x48, 0xD1, 0xE9,       // shr rcx,1
        0x8D, 0x04, 0x79,       // lea eax,[rcx+rdi*2]
        0xFF, 0xC8, 0xC3,       // dec eax; ret
        0x31, 0xC0, 0xC3        // xor eax,eax; ret (n <= 0)
    });
    if (GuestCpu(compiledSumSix).run() != 21) {
        std::fprintf(stderr, "M16 compiled sum positive case failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    auto compiledSumZero = compiledSumSix;
    compiledSumZero[1] = 0;
    if (GuestCpu(compiledSumZero).run() != 0) {
        std::fprintf(stderr, "M16 compiled sum zero case failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    auto compiledSumNegative = compiledSumSix;
    compiledSumNegative[1] = 0xFF;
    compiledSumNegative[2] = 0xFF;
    compiledSumNegative[3] = 0xFF;
    compiledSumNegative[4] = 0xFF;
    if (GuestCpu(compiledSumNegative).run() != 0) {
        std::fprintf(stderr, "M16 compiled sum negative case failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // Clang -O1 output uses LEA with a SIB displacement, SHR, OR,
    // and MOVZX DIL to implement this bit-manipulation function.
    const auto compiledBitmix = std::to_array<std::uint8_t>({
        0xBF, 0x23, 0x01, 0, 0,       // mov edi,0x123
        0x8D, 0x04, 0xBD, 0, 0, 0, 0, // lea eax,[rdi*4]
        0xC1, 0xEF, 3,               // shr edi,3
        0x09, 0xC7,                  // or edi,eax
        0x40, 0x0F, 0xB6, 0xC7,     // movzx eax,dil
        0xC3
    });
    if (GuestCpu(compiledBitmix).run() != 0xAC) {
        std::fprintf(stderr, "M16 compiled bitmix failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M17: three-operand IMUL with signed 8-bit immediate.
    const auto imulImmediate8 = std::to_array<std::uint8_t>({
        0xB9, 14, 0, 0, 0,
        0x6B, 0xC1, 3, // imul eax,ecx,3 -> 42
        0xC3
    });
    if (GuestCpu(imulImmediate8).run() != 42) {
        std::fprintf(stderr, "M17 IMUL imm8 failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto imulNegative = std::to_array<std::uint8_t>({
        0xB9, 0xF2, 0xFF, 0xFF, 0xFF, // ecx=-14
        0x6B, 0xC1, 0xFD,             // imul eax,ecx,-3 -> 42
        0xC3
    });
    if (GuestCpu(imulNegative).run() != 42) {
        std::fprintf(stderr, "M17 signed IMUL imm8 failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto imulImmediate32 = std::to_array<std::uint8_t>({
        0xB9, 2, 0, 0, 0,
        0x69, 0xC1, 21, 0, 0, 0, // imul eax,ecx,21
        0xC3
    });
    if (GuestCpu(imulImmediate32).run() != 42) {
        std::fprintf(stderr, "M17 IMUL imm32 failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto imulMemory = std::to_array<std::uint8_t>({
        0xB8, 14, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0x6B, 0x44, 0x24, 0xF8, 3, // imul eax,[rsp-8],3
        0xC3
    });
    if (GuestCpu(imulMemory, 64).run() != 42) {
        std::fprintf(stderr, "M17 memory IMUL failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // 32-bit overflow must set OF, observable through SETO AL.
    const auto imulOverflow = std::to_array<std::uint8_t>({
        0xB9, 0, 0, 1, 0,           // mov ecx,65536
        0x69, 0xC1, 0, 0, 1, 0,     // imul eax,ecx,65536 (overflow)
        0x0F, 0x90, 0xC0,           // seto al
        0xC3
    });
    if (GuestCpu(imulOverflow).run() != 1) {
        std::fprintf(stderr, "M17 IMUL overflow flag failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // REX.W: 64-bit multiplication by a sign-extended imm8.
    const auto imulWide = std::to_array<std::uint8_t>({
        0x48, 0xB9, 0, 0, 0, 0, 1, 0, 0, 0, // mov rcx,2^32
        0x48, 0x6B, 0xC1, 2,              // imul rax,rcx,2
        0xC3
    });
    if (GuestCpu(imulWide).run() != (std::uint64_t{1} << 33)) {
        std::fprintf(stderr, "M17 wide IMUL failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M20: RIP-relative ModR/M with an immediate uses the address
    // after the entire instruction, not the address before the immediate.
    const auto ripAddImmediate = std::to_array<std::uint8_t>({
        0x81, 0x05, 0xF6, 0xFF, 0xFF, 0xFF, // [RIP-10] -> guest address 0
        21, 0, 0, 0, 0xC3
    });
    GuestCpu ripAddCpu(ripAddImmediate, 64);
    ripAddCpu.writeMemory32(0, 21);
    if (ripAddCpu.run() != 0 || ripAddCpu.readMemory32(0) != 42) {
        std::fprintf(stderr, "M20 RIP-relative imm32 address failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto ripAddByte = std::to_array<std::uint8_t>({
        0x83, 0x05, 0xF9, 0xFF, 0xFF, 0xFF, // [RIP-7] -> address 0
        21, 0xC3
    });
    GuestCpu ripByteCpu(ripAddByte, 64);
    ripByteCpu.writeMemory32(0, 21);
    if (ripByteCpu.run() != 0 || ripByteCpu.readMemory32(0) != 42) {
        std::fprintf(stderr, "M20 RIP-relative imm8 address failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto ripShiftImmediate = std::to_array<std::uint8_t>({
        0xC1, 0x25, 0xF9, 0xFF, 0xFF, 0xFF, // shl dword [RIP-7],1
        1, 0xC3
    });
    GuestCpu ripShiftCpu(ripShiftImmediate, 64);
    ripShiftCpu.writeMemory32(0, 21);
    if (ripShiftCpu.run() != 0 || ripShiftCpu.readMemory32(0) != 42) {
        std::fprintf(stderr, "M20 RIP-relative shift address failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // A truncated immediate must fail before any memory mutation.
    const auto truncatedRipImmediate = std::to_array<std::uint8_t>({
        0x81, 0x05, 0xF6, 0xFF, 0xFF, 0xFF, 21, 0
    });
    GuestCpu truncatedRipCpu(truncatedRipImmediate, 64);
    truncatedRipCpu.writeMemory32(0, 21);
    rejected = false;
    try { truncatedRipCpu.run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected || truncatedRipCpu.readMemory32(0) != 21) {
        std::fprintf(stderr, "M20 truncated RIP immediate failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    // A valid RIP-relative address may still fail the guest-memory bounds check.
    const auto invalidRipAddress = std::to_array<std::uint8_t>({
        0x81, 0x05, 0x3B, 0, 0, 0, // [RIP+59] -> address 69, beyond 64 bytes
        21, 0, 0, 0, 0xC3
    });
    rejected = false;
    try { GuestCpu(invalidRipAddress, 64).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M20 invalid RIP address accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M21: compiler-common unary NOT/NEG operations and flag semantics.
    const auto unaryNot = std::to_array<std::uint8_t>({
        0xB8, 0xD5, 0xFF, 0xFF, 0xFF, // eax=-43
        0xF7, 0xD0,                   // not eax -> 42
        0xC3
    });
    if (GuestCpu(unaryNot).run() != 42) {
        std::fprintf(stderr, "M21 NOT failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto unaryNeg = std::to_array<std::uint8_t>({
        0xB8, 0xD6, 0xFF, 0xFF, 0xFF, // eax=-42
        0xF7, 0xD8,                   // neg eax -> 42
        0xC3
    });
    if (GuestCpu(unaryNeg).run() != 42) {
        std::fprintf(stderr, "M21 NEG failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto negCarry = std::to_array<std::uint8_t>({
        0xB8, 1, 0, 0, 0, 0xF7, 0xD8, // neg eax -> -1, CF=1
        0x0F, 0x92, 0xC0, 0xC3       // setc al
    });
    if (GuestCpu(negCarry).run() != 0xFFFFFF01u) {
        std::fprintf(stderr, "M21 NEG carry failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto negOverflow = std::to_array<std::uint8_t>({
        0xB8, 0, 0, 0, 0x80, 0xF7, 0xD8, // neg INT32_MIN -> overflow
        0x0F, 0x90, 0xC1,                 // seto cl
        0x0F, 0xB6, 0xC1, 0xC3            // movzx eax,cl
    });
    if (GuestCpu(negOverflow).run() != 1) {
        std::fprintf(stderr, "M21 NEG overflow failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto notPreservesCarry = std::to_array<std::uint8_t>({
        0xB8, 1, 0, 0, 0, 0xF7, 0xD8, // neg eax -> CF=1
        0xF7, 0xD0,                   // not eax; flags unchanged
        0x0F, 0x92, 0xC1,             // setc cl
        0x0F, 0xB6, 0xC1, 0xC3
    });
    if (GuestCpu(notPreservesCarry).run() != 1) {
        std::fprintf(stderr, "M21 NOT flags failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto negWide = std::to_array<std::uint8_t>({
        0x48, 0xB8, 0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x48, 0xF7, 0xD8, 0xC3
    });
    if (GuestCpu(negWide).run() != 42) {
        std::fprintf(stderr, "M21 wide NEG failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto negMemory = std::to_array<std::uint8_t>({
        0xB8, 42, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8, // mov [rsp-8],eax
        0xF7, 0x54, 0x24, 0xF8, // not [rsp-8]
        0xF7, 0x54, 0x24, 0xF8, // not [rsp-8] again
        0x8B, 0x44, 0x24, 0xF8, 0xC3
    });
    if (GuestCpu(negMemory, 64).run() != 42) {
        std::fprintf(stderr, "M21 memory NOT failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto unsupportedF7 = std::to_array<std::uint8_t>({0xF7, 0xF0, 0xC3});
    rejected = false;
    try { GuestCpu(unsupportedF7).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M21 unsupported F7 accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M22: CMOVcc compiler-style branchless selection.
    const auto cmovEqual = std::to_array<std::uint8_t>({
        0xB8, 7, 0, 0, 0,            // mov eax,7
        0xB9, 42, 0, 0, 0,           // mov ecx,42
        0x83, 0xF8, 7,              // cmp eax,7 -> ZF=1
        0x0F, 0x44, 0xC1,            // cmove eax,ecx
        0xC3
    });
    if (GuestCpu(cmovEqual).run() != 42) {
        std::fprintf(stderr, "M22 CMOVE true failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto cmovNotEqual = std::to_array<std::uint8_t>({
        0xB8, 42, 0, 0, 0,
        0xB9, 99, 0, 0, 0,
        0x83, 0xF8, 42,             // ZF=1
        0x0F, 0x45, 0xC1,            // cmovne eax,ecx -> not taken
        0xC3
    });
    if (GuestCpu(cmovNotEqual).run() != 42) {
        std::fprintf(stderr, "M22 CMOVNE false failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto cmovSigned = std::to_array<std::uint8_t>({
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF, // mov eax,-1
        0xB9, 42, 0, 0, 0,
        0x83, 0xF8, 0,              // cmp eax,0 (SF=1)
        0x0F, 0x4C, 0xC1,            // cmovl eax,ecx
        0xC3
    });
    if (GuestCpu(cmovSigned).run() != 42) {
        std::fprintf(stderr, "M22 signed CMOVL failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto cmovWide = std::to_array<std::uint8_t>({
        0x48, 0xB8, 7, 0, 0, 0, 0, 0, 0, 0,
        0x48, 0xB9, 42, 0, 0, 0, 1, 0, 0, 0, // rcx=2^32+42
        0x83, 0xF8, 7,
        0x48, 0x0F, 0x44, 0xC1,
        0xC3
    });
    if (GuestCpu(cmovWide).run() != ((std::uint64_t{1} << 32) + 42)) {
        std::fprintf(stderr, "M22 wide CMOV failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto cmovMemory = std::to_array<std::uint8_t>({
        0xB8, 7, 0, 0, 0,
        0x83, 0xF8, 7,
        0x0F, 0x44, 0x44, 0x24, 0xF8, // cmove eax,[rsp-8]
        0xC3
    });
    GuestCpu cmovMemoryCpu(cmovMemory, 64);
    cmovMemoryCpu.writeMemory32(56, 42);
    if (cmovMemoryCpu.run() != 42) {
        std::fprintf(stderr, "M22 memory CMOV failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto cmovFalseInvalidMemory = std::to_array<std::uint8_t>({
        0xB8, 42, 0, 0, 0,
        0x83, 0xF8, 42,            // ZF=1; CMOVNE condition false
        0x0F, 0x45, 0x44, 0x24, 0x00, // cmovne eax,[rsp] -> out of bounds
        0xC3
    });
    rejected = false;
    try { GuestCpu(cmovFalseInvalidMemory, 64).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M22 false CMOV memory fault skipped at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M23: MOVSXD sign extends 32-bit source into a 64-bit destination.
    const auto movsxdNegative = std::to_array<std::uint8_t>({
        0xB9, 0xD6, 0xFF, 0xFF, 0xFF, // ecx=-42
        0x48, 0x63, 0xC1,             // movsxd rax,ecx
        0xC3
    });
    if (GuestCpu(movsxdNegative).run() != UINT64_C(0xFFFFFFFFFFFFFFD6)) {
        std::fprintf(stderr, "M23 MOVSXD negative failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movsxdPositive = std::to_array<std::uint8_t>({
        0xB9, 42, 0, 0, 0,
        0x48, 0x63, 0xC1,
        0xC3
    });
    if (GuestCpu(movsxdPositive).run() != 42) {
        std::fprintf(stderr, "M23 MOVSXD positive failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movsxdMin = std::to_array<std::uint8_t>({
        0xB9, 0, 0, 0, 0x80,
        0x48, 0x63, 0xC1,
        0xC3
    });
    if (GuestCpu(movsxdMin).run() != UINT64_C(0xFFFFFFFF80000000)) {
        std::fprintf(stderr, "M23 MOVSXD INT32_MIN failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movsxdMemory = std::to_array<std::uint8_t>({
        0x48, 0x63, 0x44, 0x24, 0xF8, // movsxd rax,dword [rsp-8]
        0xC3
    });
    GuestCpu movsxdMemoryCpu(movsxdMemory, 64);
    movsxdMemoryCpu.writeMemory32(56, 0xFFFFFFD6u);
    if (movsxdMemoryCpu.run() != UINT64_C(0xFFFFFFFFFFFFFFD6)) {
        std::fprintf(stderr, "M23 MOVSXD memory failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movsxdRexRegisters = std::to_array<std::uint8_t>({
        0x41, 0xB8, 0xD6, 0xFF, 0xFF, 0xFF, // mov r8d,-42
        0x49, 0x63, 0xC0,                   // movsxd rax,r8d
        0xC3
    });
    if (GuestCpu(movsxdRexRegisters).run() != UINT64_C(0xFFFFFFFFFFFFFFD6)) {
        std::fprintf(stderr, "M23 MOVSXD REX.B failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movsxdTruncated = std::to_array<std::uint8_t>({0x48, 0x63});
    rejected = false;
    try { GuestCpu(movsxdTruncated).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M23 truncated MOVSXD accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movsxdInvalidMemory = std::to_array<std::uint8_t>({
        0x48, 0x63, 0x44, 0x24, 0x00, 0xC3 // [rsp] beyond guest memory
    });
    rejected = false;
    try { GuestCpu(movsxdInvalidMemory, 64).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M23 out-of-range MOVSXD accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movsxdNoRexW = std::to_array<std::uint8_t>({0x63, 0xC1, 0xC3});
    rejected = false;
    try { GuestCpu(movsxdNoRexW).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M23 unsupported width accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M24: C7 /0 immediate writes, including REX.W sign extension.
    const auto movImmediateRegister = std::to_array<std::uint8_t>({
        0xC7, 0xC0, 42, 0, 0, 0, 0xC3 // mov eax,42
    });
    if (GuestCpu(movImmediateRegister).run() != 42) {
        std::fprintf(stderr, "M24 C7 register failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movImmediateWideNegative = std::to_array<std::uint8_t>({
        0x48, 0xC7, 0xC0, 0xD6, 0xFF, 0xFF, 0xFF, 0xC3
    });
    if (GuestCpu(movImmediateWideNegative).run() != UINT64_C(0xFFFFFFFFFFFFFFD6)) {
        std::fprintf(stderr, "M24 C7 wide sign extension failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movImmediateZeroExtend = std::to_array<std::uint8_t>({
        0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xC7, 0xC0, 42, 0, 0, 0, 0xC3
    });
    if (GuestCpu(movImmediateZeroExtend).run() != 42) {
        std::fprintf(stderr, "M24 C7 32-bit zero extend failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movImmediateMemory = std::to_array<std::uint8_t>({
        0xC7, 0x44, 0x24, 0xF8, 42, 0, 0, 0, // mov dword [rsp-8],42
        0x8B, 0x44, 0x24, 0xF8, 0xC3
    });
    if (GuestCpu(movImmediateMemory, 64).run() != 42) {
        std::fprintf(stderr, "M24 C7 memory failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movImmediateRip = std::to_array<std::uint8_t>({
        0xC7, 0x05, 0xF6, 0xFF, 0xFF, 0xFF, 42, 0, 0, 0, 0xC3
    });
    GuestCpu movImmediateRipCpu(movImmediateRip, 64);
    if (movImmediateRipCpu.run() != 0 || movImmediateRipCpu.readMemory32(0) != 42) {
        std::fprintf(stderr, "M24 C7 RIP-relative failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movImmediateRexB = std::to_array<std::uint8_t>({
        0x41, 0xC7, 0xC0, 42, 0, 0, 0, // mov r8d,42
        0x44, 0x89, 0xC0,             // mov eax,r8d
        0xC3
    });
    if (GuestCpu(movImmediateRexB).run() != 42) {
        std::fprintf(stderr, "M24 C7 REX.B failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movImmediateBadGroup = std::to_array<std::uint8_t>({
        0xC7, 0xC8, 42, 0, 0, 0, 0xC3 // invalid /1
    });
    rejected = false;
    try { GuestCpu(movImmediateBadGroup).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M24 unsupported C7 accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movImmediateTruncated = std::to_array<std::uint8_t>({
        0xC7, 0xC0, 42, 0
    });
    rejected = false;
    try { GuestCpu(movImmediateTruncated).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M24 truncated C7 accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movImmediateOutOfBounds = std::to_array<std::uint8_t>({
        0xC7, 0x44, 0x24, 0, 42, 0, 0, 0, 0xC3
    });
    rejected = false;
    try { GuestCpu(movImmediateOutOfBounds, 64).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M24 out-of-bounds C7 accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M25: 16-bit MOV preserves the upper bits of register destinations.
    const auto movWordRegister = std::to_array<std::uint8_t>({
        0x48, 0xB8, 0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,
        0xB9, 42, 0, 0, 0,
        0x66, 0x89, 0xC8, // mov ax,cx
        0xC3
    });
    if (GuestCpu(movWordRegister).run() != UINT64_C(0x1234002A)) {
        std::fprintf(stderr, "M25 word register write failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movWordRead = std::to_array<std::uint8_t>({
        0x48, 0xB8, 42, 0, 0, 0, 1, 0, 0, 0,
        0x48, 0xB9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x66, 0x8B, 0xC8, // mov cx,ax (preserve upper 48 bits)
        0x48, 0x89, 0xC8, // mov rax,rcx
        0xC3
    });
    if (GuestCpu(movWordRead).run() != UINT64_C(0xFFFFFFFFFFFF002A)) {
        std::fprintf(stderr, "M25 word register read failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movWordMemory = std::to_array<std::uint8_t>({
        0xB8, 42, 0, 0, 0,
        0x66, 0x89, 0x44, 0x24, 0xFE, // mov [rsp-2],ax
        0xB8, 0, 0, 0, 0,
        0x66, 0x8B, 0x44, 0x24, 0xFE, // mov ax,[rsp-2]
        0xC3
    });
    if (GuestCpu(movWordMemory, 64).run() != 42) {
        std::fprintf(stderr, "M25 word memory roundtrip failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movWordOutOfBounds = std::to_array<std::uint8_t>({
        0x66, 0x8B, 0x44, 0x24, 0xFF, 0xC3 // word read at rsp-1
    });
    rejected = false;
    try { GuestCpu(movWordOutOfBounds, 64).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M25 word memory bounds failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto movWordTruncated = std::to_array<std::uint8_t>({0x66, 0x8B});
    rejected = false;
    try { GuestCpu(movWordTruncated).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M25 truncated word MOV accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto wordXchgUnsupported = std::to_array<std::uint8_t>({0x66, 0x87, 0xC0, 0xC3});
    rejected = false;
    try { GuestCpu(wordXchgUnsupported).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M25 unsupported word xchg accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M26: 66 C7 /0 writes exactly 16 bits from an imm16.
    const auto wordImmediateRegister = std::to_array<std::uint8_t>({
        0x48, 0xB8, 0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0,
        0x66, 0xC7, 0xC0, 42, 0, // mov ax,42
        0xC3
    });
    if (GuestCpu(wordImmediateRegister).run() != UINT64_C(0x1234002A)) {
        std::fprintf(stderr, "M26 word immediate register failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto wordImmediateMemory = std::to_array<std::uint8_t>({
        0x66, 0xC7, 0x44, 0x24, 0xFE, 42, 0,
        0x66, 0x8B, 0x44, 0x24, 0xFE,
        0xC3
    });
    if (GuestCpu(wordImmediateMemory, 64).run() != 42) {
        std::fprintf(stderr, "M26 word immediate memory failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto wordImmediateRip = std::to_array<std::uint8_t>({
        0x66, 0xC7, 0x05, 0xF7, 0xFF, 0xFF, 0xFF, 42, 0, 0xC3
    });
    GuestCpu wordImmediateRipCpu(wordImmediateRip, 64);
    if (wordImmediateRipCpu.run() != 0 ||
        (wordImmediateRipCpu.readMemory32(0) & 0xFFFFu) != 42) {
        std::fprintf(stderr, "M26 word immediate RIP failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto wordImmediateRexB = std::to_array<std::uint8_t>({
        0x66, 0x41, 0xC7, 0xC0, 42, 0, // mov r8w,42
        0x66, 0x44, 0x89, 0xC0,        // mov ax,r8w
        0xC3
    });
    if (GuestCpu(wordImmediateRexB).run() != 42) {
        std::fprintf(stderr, "M26 word immediate REX.B failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto wordImmediateBadGroup = std::to_array<std::uint8_t>({
        0x66, 0xC7, 0xC8, 42, 0, 0xC3
    });
    rejected = false;
    try { GuestCpu(wordImmediateBadGroup).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M26 invalid word immediate group accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto wordImmediateTruncated = std::to_array<std::uint8_t>({
        0x66, 0xC7, 0xC0, 42
    });
    rejected = false;
    try { GuestCpu(wordImmediateTruncated).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M26 truncated word immediate accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto wordImmediateOutOfBounds = std::to_array<std::uint8_t>({
        0x66, 0xC7, 0x44, 0x24, 0xFF, 42, 0, 0xC3
    });
    rejected = false;
    try { GuestCpu(wordImmediateOutOfBounds, 64).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M26 out-of-bounds word write accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M27: compiler-oriented byte moves, sign extension and NOP alignment.
    const auto byteMoveRegister = std::to_array<std::uint8_t>({
        0xB8, 0x34, 0x12, 0, 0, // eax=0x1234
        0xB9, 42, 0, 0, 0,    // ecx=42
        0x88, 0xC8,          // mov al,cl; preserve AH
        0xC3
    });
    if (GuestCpu(byteMoveRegister).run() != 0x122A) {
        std::fprintf(stderr, "M27 byte register MOV failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto byteHighRegister = std::to_array<std::uint8_t>({
        0xB8, 0, 0, 0, 0,
        0xB9, 42, 0, 0, 0,
        0x88, 0xCC, // mov ah,cl
        0xC3
    });
    if (GuestCpu(byteHighRegister).run() != 0x2A00) {
        std::fprintf(stderr, "M27 AH MOV failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto byteMoveMemory = std::to_array<std::uint8_t>({
        0xB8, 42, 0, 0, 0,
        0x88, 0x44, 0x24, 0xFF, // mov [rsp-1],al
        0xB8, 0, 0, 0, 0,
        0x8A, 0x44, 0x24, 0xFF, // mov al,[rsp-1]
        0xC3
    });
    if (GuestCpu(byteMoveMemory, 64).run() != 42) {
        std::fprintf(stderr, "M27 byte memory roundtrip failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto byteImmediate = std::to_array<std::uint8_t>({
        0xC6, 0xC0, 42, 0xC3 // mov al,42
    });
    if (GuestCpu(byteImmediate).run() != 42) {
        std::fprintf(stderr, "M27 C6 register failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto byteImmediateRip = std::to_array<std::uint8_t>({
        0xC6, 0x05, 0xF9, 0xFF, 0xFF, 0xFF, 42, 0xC3
    });
    GuestCpu byteImmediateRipCpu(byteImmediateRip, 64);
    if (byteImmediateRipCpu.run() != 0 ||
        (byteImmediateRipCpu.readMemory32(0) & 0xFFu) != 42) {
        std::fprintf(stderr, "M27 C6 RIP-relative failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto byteRexRegister = std::to_array<std::uint8_t>({
        0x41, 0xB8, 42, 0, 0, 0, // mov r8d,42
        0x44, 0x88, 0xC0,       // mov al,r8b
        0xC3
    });
    if (GuestCpu(byteRexRegister).run() != 42) {
        std::fprintf(stderr, "M27 REX byte register failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto cdqeNegative = std::to_array<std::uint8_t>({
        0xB8, 0xD6, 0xFF, 0xFF, 0xFF, 0x48, 0x98, 0xC3
    });
    if (GuestCpu(cdqeNegative).run() != UINT64_C(0xFFFFFFFFFFFFFFD6)) {
        std::fprintf(stderr, "M27 CDQE failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto cwdeNegative = std::to_array<std::uint8_t>({
        0xB8, 0xD6, 0xFF, 0, 0, 0x98, 0xC3
    });
    if (GuestCpu(cwdeNegative).run() != 0xFFFFFFD6u) {
        std::fprintf(stderr, "M27 CWDE failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto alignmentNops = std::to_array<std::uint8_t>({
        0x90, 0x0F, 0x1F, 0x00, // nop; nop dword [rax]
        0x0F, 0x1F, 0x44, 0x00, 0x00, // nop dword [rax+rax]
        0xB8, 42, 0, 0, 0, 0xC3
    });
    if (GuestCpu(alignmentNops).run() != 42) {
        std::fprintf(stderr, "M27 alignment NOP failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto invalidC6 = std::to_array<std::uint8_t>({0xC6, 0xC8, 42, 0xC3});
    rejected = false;
    try { GuestCpu(invalidC6).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M27 unsupported C6 accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto truncatedC6 = std::to_array<std::uint8_t>({0xC6, 0xC0});
    rejected = false;
    try { GuestCpu(truncatedC6).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M27 truncated C6 accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto invalidByteMemory = std::to_array<std::uint8_t>({
        0x8A, 0x44, 0x24, 0x00, 0xC3 // mov al,[rsp] out of bounds
    });
    rejected = false;
    try { GuestCpu(invalidByteMemory, 64).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M27 out-of-bounds byte read accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    // M28: immediate OR/AND/XOR and TEST, including flags and memory.
    const auto immediateLogic = std::to_array<std::uint8_t>({
        0xB8, 0x0F, 0, 0, 0,   // eax=15
        0x83, 0xC8, 0x20,     // or eax,32 -> 47
        0x83, 0xE0, 0x2A,     // and eax,42 -> 42
        0x83, 0xF0, 0x00,     // xor eax,0 -> 42
        0xC3
    });
    if (GuestCpu(immediateLogic).run() != 42) {
        std::fprintf(stderr, "M28 imm8 logic failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto immediateLogic32 = std::to_array<std::uint8_t>({
        0xB8, 0xFF, 0, 0, 0,
        0x81, 0xE0, 0x2A, 0, 0, 0, // and eax,42
        0x81, 0xC8, 0, 0, 0, 0,    // or eax,0
        0x81, 0xF0, 0, 0, 0, 0,    // xor eax,0
        0xC3
    });
    if (GuestCpu(immediateLogic32).run() != 42) {
        std::fprintf(stderr, "M28 imm32 logic failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto immediateLogicWide = std::to_array<std::uint8_t>({
        0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x48, 0x81, 0xE0, 42, 0, 0, 0, // and rax,42
        0xC3
    });
    if (GuestCpu(immediateLogicWide).run() != 42) {
        std::fprintf(stderr, "M28 wide immediate AND failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto immediateLogicMemory = std::to_array<std::uint8_t>({
        0xB8, 15, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8, // mov [rsp-8],eax
        0x83, 0x4C, 0x24, 0xF8, 0x20, // or [rsp-8],32
        0x83, 0x64, 0x24, 0xF8, 0x2A, // and [rsp-8],42
        0x8B, 0x44, 0x24, 0xF8, 0xC3
    });
    if (GuestCpu(immediateLogicMemory, 64).run() != 42) {
        std::fprintf(stderr, "M28 immediate memory logic failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto testImmediate = std::to_array<std::uint8_t>({
        0xB8, 42, 0, 0, 0,
        0xF7, 0xC0, 1, 0, 0, 0, // test eax,1 -> ZF=1
        0x0F, 0x94, 0xC1,       // sete cl
        0x0F, 0xB6, 0xC1, 0xC3
    });
    if (GuestCpu(testImmediate).run() != 1) {
        std::fprintf(stderr, "M28 TEST imm32 flags failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto testImmediateWide = std::to_array<std::uint8_t>({
        0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x48, 0xF7, 0xC0, 42, 0, 0, 0,
        0xC3
    });
    if (GuestCpu(testImmediateWide).run() != UINT64_MAX) {
        std::fprintf(stderr, "M28 TEST changed destination at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto testImmediateRip = std::to_array<std::uint8_t>({
        0xF7, 0x05, 0xF6, 0xFF, 0xFF, 0xFF, 1, 0, 0, 0,
        0x0F, 0x94, 0xC0, 0xC3
    });
    GuestCpu testImmediateRipCpu(testImmediateRip, 64);
    testImmediateRipCpu.writeMemory32(0, 42);
    if (testImmediateRipCpu.run() != 1 ||
        testImmediateRipCpu.readMemory32(0) != 42) {
        std::fprintf(stderr, "M28 RIP-relative TEST failed at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }
    const auto truncatedTest = std::to_array<std::uint8_t>({0xF7, 0xC0, 1, 0});
    rejected = false;
    try { GuestCpu(truncatedTest).run(); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) {
        std::fprintf(stderr, "M28 truncated TEST accepted at line %d\\n", __LINE__);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
