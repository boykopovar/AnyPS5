#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

using anyps5::experimental::GuestCpu;

static void put32(std::vector<std::uint8_t>& code, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        code.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}

static bool check(const char* name, std::uint64_t actual, std::uint64_t expected) {
    if (actual == expected) return true;
    std::fprintf(stderr, "%s: expected %llu, got %llu\n", name,
        static_cast<unsigned long long>(expected),
        static_cast<unsigned long long>(actual));
    return false;
}

int main() {
    // A complete loop: sum 1..n, with a signed conditional branch.
    // mov edi,n; xor eax,eax; mov ecx,1; add eax,ecx;
    // inc ecx; cmp ecx,edi; jle loop; ret.
    for (std::uint32_t n = 1; n <= 128; ++n) {
        std::vector<std::uint8_t> code{0xBF};
        put32(code, n);
        const std::array<std::uint8_t, 14> body{
            0x31, 0xC0,             // xor eax,eax
            0xB9, 1, 0, 0, 0,       // mov ecx,1
            0x01, 0xC8,             // add eax,ecx
            0xFF, 0xC1,             // inc ecx
            0x39, 0xF9,             // cmp ecx,edi
            0x7E                    // jle rel8 (displacement appended below)
        };
        code.insert(code.end(), body.begin(), body.end());
        code.push_back(0xF8); // jump back 8 bytes to add eax,ecx
        code.push_back(0xC3);
        if (!check("sum loop", GuestCpu(code).run(1000),
                   static_cast<std::uint64_t>(n) * (n + 1) / 2))
            return EXIT_FAILURE;
    }

    // Combined bitwise data path: shifts, OR, AND and return.
    // mov edi,x; lea eax,[rdi*4]; shr edi,3; or eax,edi;
    // and eax,255; ret.
    for (std::uint32_t x = 0; x < 512; ++x) {
        std::vector<std::uint8_t> code{0xBF};
        put32(code, x);
        const std::array<std::uint8_t, 17> body{
            0x8D, 0x04, 0xBD, 0, 0, 0, 0,
            0xC1, 0xEF, 0x03,
            0x09, 0xF8,
            0x81, 0xE0, 0xFF, 0, 0
        };
        code.insert(code.end(), body.begin(), body.end());
        code.push_back(0);
        code.push_back(0xC3);
        if (!check("bitmix", GuestCpu(code).run(),
                   ((x << 2) | (x >> 3)) & 255u))
            return EXIT_FAILURE;
    }

    // Full stack-frame call and return, including 16-bit and byte writes.
    // call function; ret; function: push rbp; mov rbp,rsp;
    // sub rsp,16; mov word [rbp-2],42; movzx eax,word [rbp-2];
    // leave; ret.
    const std::array<std::uint8_t, 25> stackFunction{
        0xE8, 1, 0, 0, 0, 0xC3,
        0x55, 0x48, 0x89, 0xE5,
        0x48, 0x83, 0xEC, 0x10,
        0x66, 0xC7, 0x45, 0xFE, 42, 0,
        0x0F, 0xB7, 0x45, 0xFE, 0xC9
    };
    std::vector<std::uint8_t> stackCode(stackFunction.begin(), stackFunction.end());
    stackCode.push_back(0xC3);
    GuestCpu stackCpu(stackCode, 128);
    if (!check("stack-frame function", stackCpu.run(), 42) ||
        !check("stack restored", stackCpu.registerValue(4), 128))
        return EXIT_FAILURE;

    // Diagnostics must include the unsupported opcode and its offset.
    const std::array<std::uint8_t, 2> unsupported{0x90, 0xF4};
    try {
        (void)GuestCpu(unsupported).run();
        std::fprintf(stderr, "unsupported opcode unexpectedly executed\n");
        return EXIT_FAILURE;
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        if (message.find("244") == std::string::npos ||
            message.find("offset 1") == std::string::npos) {
            std::fprintf(stderr, "insufficient opcode diagnostics: %s\n", error.what());
            return EXIT_FAILURE;
        }
    }
    // Phase 2: parity flags from logical and arithmetic operations.
    const std::array<std::uint8_t, 15> parityEven{
        0xB8, 3, 0, 0, 0,  // eax=3 (two set bits, even parity)
        0x85, 0xC0,       // test eax,eax
        0x0F, 0x9A, 0xC1, // setp cl
        0x0F, 0xB6, 0xC1, // movzx eax,cl
        0xC3, 0x90
    };
    if (!check("even parity", GuestCpu(parityEven).run(), 1))
        return EXIT_FAILURE;
    const std::array<std::uint8_t, 14> parityOdd{
        0xB8, 1, 0, 0, 0,
        0x85, 0xC0,
        0x0F, 0x9B, 0xC1, // setnp cl
        0x0F, 0xB6, 0xC1,
        0xC3
    };
    if (!check("odd parity", GuestCpu(parityOdd).run(), 1))
        return EXIT_FAILURE;

    // Repeated operand prefixes and 16-bit immediate register writes.
    const std::array<std::uint8_t, 17> wordImmediate{
        0x48, 0xB8, 0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0,
        0x66, 0x66, 0xB8, 42, 0, 0xC3, 0x90
    };
    if (!check("word immediate preserves upper register", GuestCpu(wordImmediate).run(),
               UINT64_C(0x1234002A)))
        return EXIT_FAILURE;
    const std::array<std::uint8_t, 17> rexWideOverridesWord{
        0x66, 0x48, 0xB8, 42, 0, 0, 0, 0, 0, 0, 0, 0xC3,
        0x90, 0x90, 0x90, 0x90, 0x90
    };
    if (!check("REX.W overrides 66", GuestCpu(rexWideOverridesWord).run(), 42))
        return EXIT_FAILURE;
    const std::array<std::uint8_t, 16> tooManyPrefixes{
        0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
        0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x90
    };
    try {
        (void)GuestCpu(tooManyPrefixes).run();
        std::fprintf(stderr, "overlong prefixes unexpectedly accepted\\n");
        return EXIT_FAILURE;
    } catch (const std::runtime_error&) {}

    // XCHG opcode-register forms, including REX.B and operand-size override.
    const std::array<std::uint8_t, 13> xchg32{
        0xB8, 42, 0, 0, 0, 0xB9, 7, 0, 0, 0,
        0x91, 0xC3, 0x90
    };
    GuestCpu xchg32Cpu(xchg32);
    if (!check("XCHG eax,ecx", xchg32Cpu.run(), 7) ||
        !check("XCHG ecx,eax", xchg32Cpu.registerValue(1), 42))
        return EXIT_FAILURE;

    const std::array<std::uint8_t, 14> xchgR8{
        0xB8, 42, 0, 0, 0,
        0x41, 0xB8, 7, 0, 0, 0,
        0x41, 0x90, 0xC3
    };
    GuestCpu xchgR8Cpu(xchgR8);
    if (!check("REX.B 90 is XCHG", xchgR8Cpu.run(), 7) ||
        !check("REX.B 90 updates r8d", xchgR8Cpu.registerValue(8), 42))
        return EXIT_FAILURE;

    const std::array<std::uint8_t, 15> xchgWord{
        0xB8, 42, 0, 0, 0,
        0xB9, 7, 0, 0, 0,
        0x66, 0x91, 0xC3, 0x90, 0x90
    };
    GuestCpu xchgWordCpu(xchgWord);
    if (!check("16-bit XCHG", xchgWordCpu.run(), 7) ||
        !check("16-bit XCHG second register", xchgWordCpu.registerValue(1), 42))
        return EXIT_FAILURE;

    const std::array<std::uint8_t, 15> xchgModrm{
        0xB8, 42, 0, 0, 0,
        0xB9, 7, 0, 0, 0,
        0x87, 0xC1, 0xC3, 0x90, 0x90
    };
    GuestCpu xchgModrmCpu(xchgModrm);
    if (!check("ModRM XCHG", xchgModrmCpu.run(), 7) ||
        !check("ModRM XCHG other register", xchgModrmCpu.registerValue(1), 42))
        return EXIT_FAILURE;

    const std::array<std::uint8_t, 23> xchgMemory{
        0xB8, 42, 0, 0, 0,
        0x89, 0x44, 0x24, 0xF8,
        0xB8, 7, 0, 0, 0,
        0x87, 0x44, 0x24, 0xF8,
        0x8B, 0x4C, 0x24, 0xF8, 0xC3
    };
    GuestCpu xchgMemoryCpu(xchgMemory, 64);
    if (!check("XCHG memory returns previous value", xchgMemoryCpu.run(), 42) ||
        !check("XCHG memory writes replacement", xchgMemoryCpu.registerValue(1), 7))
        return EXIT_FAILURE;

    const std::array<std::uint8_t, 12> rexWideNop{
        0x48, 0xB8, 42, 0, 0, 0, 0, 0, 0, 0,
        0x48, 0x90
    };
    std::vector<std::uint8_t> rexWideNopCode(rexWideNop.begin(), rexWideNop.end());
    rexWideNopCode.push_back(0xC3);
    if (!check("REX.W 90 is NOP", GuestCpu(rexWideNopCode).run(), 42))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
