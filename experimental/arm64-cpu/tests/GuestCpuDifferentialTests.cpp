#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using anyps5::experimental::GuestCpu;

// Differential testing uses native x86-64 instructions as an independent
// reference. No executable memory or arbitrary guest code execution is needed.
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
static std::uint32_t nativeShift(std::uint32_t value, std::uint8_t count,
                                 unsigned kind) {
    if (kind == 0) {
        asm volatile("shll %%cl, %0" : "+r"(value) : "c"(count) : "cc");
    } else if (kind == 1) {
        asm volatile("shrl %%cl, %0" : "+r"(value) : "c"(count) : "cc");
    } else {
        asm volatile("sarl %%cl, %0" : "+r"(value) : "c"(count) : "cc");
    }
    return value;
}

static std::uint32_t nativeMultiply(std::uint32_t value, std::int32_t factor) {
    asm volatile("imull %1, %0" : "+r"(value) : "r"(factor) : "cc");
    return value;
}

static std::uint64_t nativeShift64(std::uint64_t value, std::uint8_t count,
                                   unsigned kind) {
    if (kind == 0) {
        asm volatile("shlq %%cl, %0" : "+r"(value) : "c"(count) : "cc");
    } else if (kind == 1) {
        asm volatile("shrq %%cl, %0" : "+r"(value) : "c"(count) : "cc");
    } else {
        asm volatile("sarq %%cl, %0" : "+r"(value) : "c"(count) : "cc");
    }
    return value;
}

struct NativeMultiplyFlags {
    std::uint64_t result;
    bool overflow;
    bool carry;
};

static NativeMultiplyFlags nativeMultiplyFlags32(std::uint32_t input, std::int32_t factor) {
    std::uint32_t value = input;
    unsigned char overflow = 0, carry = 0;
    asm volatile(
        "imull %3, %0; seto %1; setc %2"
        : "+r"(value), "=qm"(overflow), "=qm"(carry)
        : "r"(factor) : "cc");
    return {value, overflow != 0, carry != 0};
}

static NativeMultiplyFlags nativeMultiplyFlags64(std::uint64_t input, std::int64_t factor) {
    std::uint64_t value = input;
    unsigned char overflow = 0, carry = 0;
    asm volatile(
        "imulq %3, %0; seto %1; setc %2"
        : "+r"(value), "=qm"(overflow), "=qm"(carry)
        : "r"(factor) : "cc");
    return {value, overflow != 0, carry != 0};
}

// Return the flags via SETO/SETC and MOVZX, avoiding any direct access
// to the interpreter's private flag storage.
static std::uint64_t guestMultiplyFlags(std::uint64_t input,
                                         std::int8_t factor, bool wide) {
    std::vector<std::uint8_t> code;
    if (wide) code.push_back(0x48);
    code.push_back(0xB9); // mov ecx/rcx, immediate
    const unsigned bytes = wide ? 8u : 4u;
    for (unsigned i = 0; i < bytes; ++i)
        code.push_back(static_cast<std::uint8_t>(input >> (i * 8)));
    if (wide) code.push_back(0x48);
    code.insert(code.end(), {0x6B, 0xC1, static_cast<std::uint8_t>(factor)});
    code.insert(code.end(), {
        0x0F, 0x90, 0xC2, // seto dl
        0x0F, 0x92, 0xC1, // setc cl
        0x0F, 0xB6, 0xD2, // movzx edx,dl
        0x0F, 0xB6, 0xC9, // movzx ecx,cl
        0xD1, 0xE1,       // shl ecx,1
        0x31, 0xC0,       // xor eax,eax (clear product)
        0x09, 0xC8,       // or eax,ecx (CF in bit 1)
        0x09, 0xD0,       // or eax,edx (OF in bit 0)
        0xC3
    });
    return GuestCpu(code).run();
}

static bool nativeParity(std::uint32_t value) {
    unsigned char parity = 0;
    asm volatile("testl %1, %1; setp %0" : "=qm"(parity) : "r"(value) : "cc");
    return parity != 0;
}

static bool guestParity(std::uint32_t value) {
    std::array<std::uint8_t, 14> code{
        0xB8, 0, 0, 0, 0,
        0x85, 0xC0,
        0x0F, 0x9A, 0xC1,
        0x0F, 0xB6, 0xC1,
        0xC3
    };
    for (unsigned i = 0; i < 4; ++i)
        code[1 + i] = static_cast<std::uint8_t>(value >> (i * 8));
    return GuestCpu(code).run() != 0;
}

static bool nativeParity64(std::uint64_t value) {
    unsigned char parity = 0;
    asm volatile("testq %1, %1; setp %0" : "=qm"(parity) : "r"(value) : "cc");
    return parity != 0;
}

static bool guestParity64(std::uint64_t value) {
    std::array<std::uint8_t, 21> code{
        0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,
        0x48, 0x85, 0xC0,
        0x0F, 0x9A, 0xC1,
        0x0F, 0xB6, 0xC1,
        0xC3, 0x90
    };
    for (unsigned i = 0; i < 8; ++i)
        code[2 + i] = static_cast<std::uint8_t>(value >> (i * 8));
    return GuestCpu(code).run() != 0;
}

int main() {
    std::uint32_t seed = 0xA17C9E53u;
    constexpr std::array<std::uint32_t, 9> boundaries{
        0, 1, 2, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu,
        0x55555555u, 0xAAAAAAAAu, 0x12345678u
    };
    for (unsigned sample = 0; sample < 256; ++sample) {
        seed = seed * 1664525u + 1013904223u;
        const std::uint32_t value = sample < boundaries.size()
            ? boundaries[sample] : seed;
        const auto count = static_cast<std::uint8_t>(sample % 65);
        for (unsigned kind = 0; kind < 3; ++kind) {
            // mov eax,imm32; mov ecx,imm32; shl/shr/sar eax,cl; ret
            std::array<std::uint8_t, 13> code{
                0xB8, 0, 0, 0, 0, 0xB9, count, 0, 0, 0,
                0xD3, static_cast<std::uint8_t>(kind == 0 ? 0xE0 :
                                                 kind == 1 ? 0xE8 : 0xF8), 0xC3
            };
            for (unsigned i = 0; i < 4; ++i)
                code[1 + i] = static_cast<std::uint8_t>(value >> (i * 8));
            const auto expected = nativeShift(value, count, kind);
            const auto actual = GuestCpu(code).run();
            if (actual != expected) {
                std::fprintf(stderr,
                    "32-bit shift mismatch sample=%u kind=%u count=%u value=%08x expected=%08x actual=%llx\\n",
                    sample, kind, count, value, expected,
                    static_cast<unsigned long long>(actual));
                return EXIT_FAILURE;
            }
            // mov rax,imm64; mov ecx,imm32; REX.W shift rax,cl; ret
            const std::uint64_t wideValue =
                (static_cast<std::uint64_t>(value) << 32) | (~value);
            std::array<std::uint8_t, 20> wideCode{
                0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,
                0xB9, count, 0, 0, 0,
                0x48, 0xD3, static_cast<std::uint8_t>(kind == 0 ? 0xE0 :
                                                       kind == 1 ? 0xE8 : 0xF8),
                0xC3, 0x90
            };
            for (unsigned i = 0; i < 8; ++i)
                wideCode[2 + i] = static_cast<std::uint8_t>(wideValue >> (i * 8));
            const auto wideExpected = nativeShift64(wideValue, count, kind);
            const auto wideActual = GuestCpu(wideCode).run();
            if (wideActual != wideExpected) {
                std::fprintf(stderr,
                    "64-bit shift mismatch sample=%u kind=%u count=%u expected=%llx actual=%llx\\n",
                    sample, kind, count,
                    static_cast<unsigned long long>(wideExpected),
                    static_cast<unsigned long long>(wideActual));
                return EXIT_FAILURE;
            }
        }
        if (nativeParity(value) != guestParity(value)) {
            std::fprintf(stderr, "Parity mismatch sample=%u value=%08x\\n", sample, value);
            return EXIT_FAILURE;
        }
        const std::uint64_t parityWideValue =
            (static_cast<std::uint64_t>(value) << 32) | (~value);
        if (nativeParity64(parityWideValue) != guestParity64(parityWideValue)) {
            std::fprintf(stderr, "Wide parity mismatch sample=%u\\n", sample);
            return EXIT_FAILURE;
        }
        // Differential OF/CF checks on both operand widths. x86 IMUL
        // specifies CF == OF; SETcc exposes them to the guest program.
        const auto signedFactor = static_cast<std::int8_t>(sample);
        const auto native32 = nativeMultiplyFlags32(value, signedFactor);
        const auto guestFlags32 = guestMultiplyFlags(value, signedFactor, false);
        const auto expectedFlags32 = static_cast<std::uint64_t>(
            native32.overflow | (static_cast<unsigned>(native32.carry) << 1));
        if (guestFlags32 != expectedFlags32) {
            std::fprintf(stderr,
                "IMUL 32-bit flags mismatch sample=%u expected=%llu actual=%llu\\n",
                sample, static_cast<unsigned long long>(expectedFlags32),
                static_cast<unsigned long long>(guestFlags32));
            return EXIT_FAILURE;
        }
        const std::uint64_t wideInput =
            (static_cast<std::uint64_t>(value) << 32) | (~value);
        const auto native64 = nativeMultiplyFlags64(wideInput, signedFactor);
        const auto guestFlags64 = guestMultiplyFlags(wideInput, signedFactor, true);
        const auto expectedFlags64 = static_cast<std::uint64_t>(
            native64.overflow | (static_cast<unsigned>(native64.carry) << 1));
        if (guestFlags64 != expectedFlags64) {
            std::fprintf(stderr,
                "IMUL 64-bit flags mismatch sample=%u expected=%llu actual=%llu\\n",
                sample, static_cast<unsigned long long>(expectedFlags64),
                static_cast<unsigned long long>(guestFlags64));
            return EXIT_FAILURE;
        }
        const auto factor = static_cast<std::int8_t>(sample);
        std::array<std::uint8_t, 9> imul{
            0xB9, 0, 0, 0, 0, 0x6B, 0xC1,
            static_cast<std::uint8_t>(factor), 0xC3
        };
        for (unsigned i = 0; i < 4; ++i)
            imul[1 + i] = static_cast<std::uint8_t>(value >> (i * 8));
        const auto expected = nativeMultiply(value, factor);
        const auto actual = GuestCpu(imul).run();
        if (actual != expected) {
            std::fprintf(stderr,
                "IMUL mismatch sample=%u value=%08x factor=%d expected=%08x actual=%llx\\n",
                sample, value, static_cast<int>(factor), expected,
                static_cast<unsigned long long>(actual));
            return EXIT_FAILURE;
        }
    }
    std::puts("Differential CPU tests passed (256 deterministic samples; native x86-64 reference)");
    return EXIT_SUCCESS;
}
#else
#error "Differential tests require x86-64 with GCC or Clang"
#endif
