#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using anyps5::experimental::GuestCpu;
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
struct Result {
    std::uint64_t value;
    std::uint8_t flags;
};

static std::uint8_t packFlags(unsigned char cf, unsigned char of,
                              unsigned char zf, unsigned char sf,
                              unsigned char pf) {
    return static_cast<std::uint8_t>(cf | (of << 1) | (zf << 2) |
                                     (sf << 3) | (pf << 4));
}

static Result nativeAlu16(std::uint64_t lhs, std::uint64_t rhs, bool carry,
                          unsigned operation) {
    unsigned char cf, of, zf, sf, pf;
    const auto bit = static_cast<std::uint64_t>(carry);
    switch (operation) {
    case 0:
        asm volatile("btq $0,%[bit]; addw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    case 1:
        asm volatile("btq $0,%[bit]; orw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    case 2:
        asm volatile("btq $0,%[bit]; adcw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    case 3:
        asm volatile("btq $0,%[bit]; sbbw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    case 4:
        asm volatile("btq $0,%[bit]; andw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    case 5:
        asm volatile("btq $0,%[bit]; subw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    case 6:
        asm volatile("btq $0,%[bit]; xorw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    case 7:
        asm volatile("btq $0,%[bit]; cmpw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    default:
        asm volatile("btq $0,%[bit]; testw %w[rhs],%w[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [lhs] "+r"(lhs), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [rhs] "r"(rhs), [bit] "r"(bit)
            : "cc");
        break;
    }
    return {lhs & UINT64_C(0xFFFF), packFlags(cf, of, zf, sf, pf)};
}

static Result nativeShift16(std::uint64_t value, unsigned count,
                            unsigned operation) {
    unsigned char cf, of, zf, sf, pf;
    const auto countByte = static_cast<std::uint8_t>(count);
    switch (operation) {
    case 0:
        asm volatile("movb %[count],%%cl; shlw %%cl,%w[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+r"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [count] "r"(countByte)
            : "rcx", "cc");
        break;
    case 1:
        asm volatile("movb %[count],%%cl; shrw %%cl,%w[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+r"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [count] "r"(countByte)
            : "rcx", "cc");
        break;
    default:
        asm volatile("movb %[count],%%cl; sarw %%cl,%w[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+r"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [count] "r"(countByte)
            : "rcx", "cc");
        break;
    }
    return {value & UINT64_C(0xFFFF), packFlags(cf, of, zf, sf, pf)};
}

static Result nativeRotate16(std::uint64_t value, unsigned count,
                             unsigned operation) {
    unsigned char cf, of, zf, sf, pf;
    const auto countByte = static_cast<std::uint8_t>(count);
    if (operation == 0)
        asm volatile("movb %[count],%%cl; rolw %%cl,%w[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+r"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [count] "r"(countByte)
            : "rcx", "cc");
    else
        asm volatile("movb %[count],%%cl; rorw %%cl,%w[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+r"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [count] "r"(countByte)
            : "rcx", "cc");
    return {value & UINT64_C(0xFFFF), packFlags(cf, of, zf, sf, pf)};
}

static Result nativeIncDec16(std::uint64_t value, bool increment, bool carry) {
    unsigned char cf, of, zf, sf, pf;
    const auto bit = static_cast<std::uint64_t>(carry);
    if (increment)
        asm volatile("btq $0,%[bit]; incw %w[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+r"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [bit] "r"(bit)
            : "cc");
    else
        asm volatile("btq $0,%[bit]; decw %w[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+r"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [bit] "r"(bit)
            : "cc");
    return {value & UINT64_C(0xFFFF), packFlags(cf, of, zf, sf, pf)};
}

static Result nativeNeg8(std::uint64_t value, bool high) {
    unsigned char cf, of, zf, sf, pf;
    if (high)
        asm volatile("negb %%ah; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+a"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            :
            : "cc");
    else
        asm volatile("negb %b[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+q"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            :
            : "cc");
    return {value, packFlags(cf, of, zf, sf, pf)};
}

static Result nativeTest8(std::uint64_t value, std::uint8_t immediate,
                          bool high) {
    unsigned char cf, of, zf, sf, pf;
    if (high)
        asm volatile("testb %b[imm],%%ah; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+a"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [imm] "c"(immediate)
            : "cc");
    else
        asm volatile("testb %b[imm],%b[value]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
            : [value] "+q"(value), [cf] "=qm"(cf), [of] "=qm"(of),
              [zf] "=qm"(zf), [sf] "=qm"(sf), [pf] "=qm"(pf)
            : [imm] "r"(immediate)
            : "cc");
    return {value, packFlags(cf, of, zf, sf, pf)};
}

static void put64(std::vector<std::uint8_t>& code, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        code.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

static void appendCapture(std::vector<std::uint8_t>& code) {
    code.insert(code.end(), {
        0x49, 0x89, 0xC0,
        0x49, 0x89, 0xD1,
        0x0F, 0x92, 0xC2,
        0x0F, 0x90, 0xC3,
        0x0F, 0x94, 0xC1,
        0x0F, 0x98, 0xC0,
        0x40, 0x0F, 0x9A, 0xC6,
        0x0F, 0xB6, 0xD2,
        0x0F, 0xB6, 0xDB,
        0x0F, 0xB6, 0xC9,
        0x0F, 0xB6, 0xC0,
        0x40, 0x0F, 0xB6, 0xF6,
        0xD1, 0xE3,
        0xC1, 0xE1, 0x02,
        0xC1, 0xE0, 0x03,
        0xC1, 0xE6, 0x04,
        0x09, 0xDA,
        0x09, 0xCA,
        0x09, 0xC2,
        0x09, 0xF2,
        0x89, 0xD0,
        0xC3
    });
}

static Result runGuest(const std::vector<std::uint8_t>& body) {
    auto code = body;
    appendCapture(code);
    GuestCpu cpu(code, 256);
    const auto flags = cpu.run(4096);
    return {cpu.registerValue(8), static_cast<std::uint8_t>(flags & 0x1F)};
}

static Result guestAlu16(std::uint64_t lhs, std::uint64_t rhs, bool carry,
                         std::uint8_t opcode) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, lhs & UINT64_C(0xFFFF));
    code.insert(code.end(), {0x48, 0xBB});
    put64(code, rhs & UINT64_C(0xFFFF));
    code.insert(code.end(), {
        0xB9, static_cast<std::uint8_t>(carry ? 1 : 2), 0x00, 0x00, 0x00,
        0x83, 0xF9, static_cast<std::uint8_t>(carry ? 2 : 1),
        0x66, opcode, 0xD8
    });
    return runGuest(code);
}

static Result guestShift16(std::uint64_t value, unsigned count,
                           std::uint8_t modrm) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, value & UINT64_C(0xFFFF));
    code.insert(code.end(), {
        0xB1, static_cast<std::uint8_t>(count), 0x66, 0xD3, modrm
    });
    return runGuest(code);
}

static Result guestIncDec16(std::uint64_t value, bool increment, bool carry) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, value & UINT64_C(0xFFFF));
    code.insert(code.end(), {
        0xB9, static_cast<std::uint8_t>(carry ? 1 : 2), 0x00, 0x00, 0x00,
        0x83, 0xF9, static_cast<std::uint8_t>(carry ? 2 : 1),
        0x66, 0xFF, static_cast<std::uint8_t>(increment ? 0xC0 : 0xC8)
    });
    return runGuest(code);
}

static Result guestNeg8(std::uint64_t value, bool high) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, value);
    code.insert(code.end(), {0xF6, static_cast<std::uint8_t>(high ? 0xDC : 0xD8)});
    return runGuest(code);
}

static Result guestTest8(std::uint64_t value, std::uint8_t immediate,
                         bool high) {
    std::vector<std::uint8_t> code{0x48, 0xB8};
    put64(code, value);
    code.insert(code.end(), {
        0xF6, static_cast<std::uint8_t>(high ? 0xC4 : 0xC0), immediate
    });
    return runGuest(code);
}

static bool compare(const char* name, unsigned index, const Result& expected,
                    const Result& actual, unsigned mask) {
    if (expected.value == actual.value &&
        ((expected.flags ^ actual.flags) & mask) == 0)
        return true;
    std::fprintf(stderr,
        "%s mismatch i=%u expected=%llx/%02x actual=%llx/%02x mask=%02x\n",
        name, index,
        static_cast<unsigned long long>(expected.value), expected.flags,
        static_cast<unsigned long long>(actual.value), actual.flags, mask);
    return false;
}

int main() {
    std::uint64_t seed = UINT64_C(0x243F6A8885A308D3);
    constexpr std::array<std::uint16_t, 16> edge{
        0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0x100,
        0x7FFF, 0x8000, 0x8001, 0x00FF, 0xFF00, 0xFFFE, 0xFFFF, 0x1234
    };
    constexpr std::array<std::uint8_t, 9> aluOpcode{
        0x01, 0x09, 0x11, 0x19, 0x21, 0x29, 0x31, 0x39, 0x85
    };
    constexpr std::array<std::uint8_t, 3> shiftModrm{0xE0, 0xE8, 0xF8};
    constexpr std::array<std::uint8_t, 2> rotateModrm{0xC0, 0xC8};
    unsigned comparisons = 0;

    for (unsigned i = 0; i < 256; ++i) {
        seed = seed * UINT64_C(6364136223846793005) +
            UINT64_C(1442695040888963407);
        const auto lhs = i < edge.size()
            ? edge[i] : static_cast<std::uint16_t>(seed);
        const auto rhs = i < edge.size()
            ? edge[edge.size() - 1 - i]
            : static_cast<std::uint16_t>(seed >> 16);
        for (unsigned op = 0; op < aluOpcode.size(); ++op) {
            ++comparisons;
            if (!compare("alu16", i,
                    nativeAlu16(lhs, rhs, false, op),
                    guestAlu16(lhs, rhs, false, aluOpcode[op]), 0x1F))
                return EXIT_FAILURE;
        }
        for (unsigned op = 2; op <= 3; ++op) {
            ++comparisons;
            if (!compare("alu16 carry", i,
                    nativeAlu16(lhs, rhs, true, op),
                    guestAlu16(lhs, rhs, true, aluOpcode[op]), 0x1F))
                return EXIT_FAILURE;
        }
    }

    for (unsigned i = 0; i < 96; ++i) {
        seed = seed * UINT64_C(6364136223846793005) +
            UINT64_C(1442695040888963407);
        const auto value = i < edge.size()
            ? edge[i] : static_cast<std::uint16_t>(seed);
        for (unsigned count = 0; count < 32; ++count) {
            for (unsigned op = 0; op < shiftModrm.size(); ++op) {
                ++comparisons;
                const auto actual = guestShift16(value, count, shiftModrm[op]);
                if (count == 0) {
                    if (actual.value != value || actual.flags != 0x10) {
                        std::fprintf(stderr,
                            "shift16 zero count i=%u op=%u value=%llx/%02x\n",
                            i, op,
                            static_cast<unsigned long long>(actual.value),
                            actual.flags);
                        return EXIT_FAILURE;
                    }
                    continue;
                }
                const unsigned mask = count == 1 ? 0x1F : 0x1D;
                if (!compare("shift16", i,
                        nativeShift16(value, count, op), actual, mask))
                    return EXIT_FAILURE;
            }
            for (unsigned op = 0; op < rotateModrm.size(); ++op) {
                ++comparisons;
                const auto actual = guestShift16(value, count, rotateModrm[op]);
                if (count == 0) {
                    if (actual.value != value || actual.flags != 0x10) {
                        std::fprintf(stderr,
                            "rotate16 zero count i=%u op=%u value=%llx/%02x\n",
                            i, op,
                            static_cast<unsigned long long>(actual.value),
                            actual.flags);
                        return EXIT_FAILURE;
                    }
                    continue;
                }
                const unsigned mask = count == 1 ? 0x03 : 0x01;
                if (!compare("rotate16", i,
                        nativeRotate16(value, count, op), actual, mask))
                    return EXIT_FAILURE;
            }
        }
        for (const bool increment : {true, false}) {
            for (const bool carry : {false, true}) {
                ++comparisons;
                if (!compare("incdec16", i,
                        nativeIncDec16(value, increment, carry),
                        guestIncDec16(value, increment, carry), 0x1F))
                    return EXIT_FAILURE;
            }
        }
    }

    for (unsigned value = 0; value < 256; ++value) {
        const auto alSeed = UINT64_C(0xA5A5A55A5A5A5A00) | value;
        const auto ahSeed = UINT64_C(0xA5A5A55A5A5A5A00) |
            (static_cast<std::uint64_t>(value) << 8);
        const auto immediate = static_cast<std::uint8_t>(value ^ 0x5A);
        for (const bool high : {false, true}) {
            const auto seeded = high ? ahSeed : alSeed;
            ++comparisons;
            if (!compare("neg8", value,
                    nativeNeg8(seeded, high), guestNeg8(seeded, high), 0x1F))
                return EXIT_FAILURE;
            ++comparisons;
            if (!compare("test8", value,
                    nativeTest8(seeded, immediate, high),
                    guestTest8(seeded, immediate, high), 0x1F))
                return EXIT_FAILURE;
        }
    }

    std::printf("Word/byte differential tests passed (%u comparisons)\n",
                comparisons);
    return EXIT_SUCCESS;
}
#else
#error "Native differential tests require x86-64 GCC or Clang"
#endif
