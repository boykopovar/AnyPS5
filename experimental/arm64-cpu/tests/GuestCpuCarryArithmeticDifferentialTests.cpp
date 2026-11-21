#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using anyps5::experimental::GuestCpu;
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
struct Result { std::uint64_t value; std::uint8_t flags; };
// Bit order: CF, OF, ZF, SF, PF.
static Result native(std::uint64_t lhs, std::uint64_t rhs, bool carry,
                     bool subtract, bool wide) {
    unsigned char cf, of, zf, sf, pf;
    const auto bit = static_cast<std::uint64_t>(carry);
    if (wide) {
        if (subtract)
            asm volatile("btq $0,%[bit]; sbbq %[rhs],%[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
                : [lhs]"+r"(lhs), [cf]"=qm"(cf), [of]"=qm"(of), [zf]"=qm"(zf),
                  [sf]"=qm"(sf), [pf]"=qm"(pf)
                : [rhs]"r"(rhs), [bit]"r"(bit) : "cc");
        else
            asm volatile("btq $0,%[bit]; adcq %[rhs],%[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
                : [lhs]"+r"(lhs), [cf]"=qm"(cf), [of]"=qm"(of), [zf]"=qm"(zf),
                  [sf]"=qm"(sf), [pf]"=qm"(pf)
                : [rhs]"r"(rhs), [bit]"r"(bit) : "cc");
    } else {
        auto a = static_cast<std::uint32_t>(lhs);
        const auto v = static_cast<std::uint32_t>(rhs);
        const auto initial = static_cast<std::uint32_t>(carry);
        if (subtract)
            asm volatile("btl $0,%[bit]; sbbl %[rhs],%[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
                : [lhs]"+r"(a), [cf]"=qm"(cf), [of]"=qm"(of), [zf]"=qm"(zf),
                  [sf]"=qm"(sf), [pf]"=qm"(pf)
                : [rhs]"r"(v), [bit]"r"(initial) : "cc");
        else
            asm volatile("btl $0,%[bit]; adcl %[rhs],%[lhs]; setc %[cf]; seto %[of]; setz %[zf]; sets %[sf]; setp %[pf]"
                : [lhs]"+r"(a), [cf]"=qm"(cf), [of]"=qm"(of), [zf]"=qm"(zf),
                  [sf]"=qm"(sf), [pf]"=qm"(pf)
                : [rhs]"r"(v), [bit]"r"(initial) : "cc");
        lhs = a;
    }
    return {lhs, static_cast<std::uint8_t>(cf | (of << 1) | (zf << 2) |
                                            (sf << 3) | (pf << 4))};
}

static Result guest(std::uint64_t lhs, std::uint64_t rhs, bool carry,
                    bool subtract, bool wide) {
    std::vector<std::uint8_t> code;
    const auto imm = [&](std::uint64_t v, unsigned size) {
        for (unsigned i = 0; i < size; ++i)
            code.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    if (wide) code.push_back(0x48);
    code.push_back(0xB8); imm(lhs, wide ? 8 : 4);
    if (wide) code.push_back(0x48);
    code.push_back(0xB9); imm(rhs, wide ? 8 : 4);
    // CMP edx,0/1 establishes CF=carry without changing EAX/ECX.
    code.insert(code.end(), {0xBA,0,0,0,0, 0x83,0xFA,
                             static_cast<std::uint8_t>(carry ? 1 : 0)});
    if (wide) code.push_back(0x48);
    code.insert(code.end(), {static_cast<std::uint8_t>(subtract ? 0x1B : 0x13),0xC1});
    // Store result in r8 before observing flags; MOV does not modify flags.
    code.insert(code.end(), {0x49,0x89,0xC0});
    // SETcc into separate byte registers: DL, BL, CL, AL, SIL.
    code.insert(code.end(), {
        0x0F,0x92,0xC2, 0x0F,0x90,0xC3, 0x0F,0x94,0xC1,
        0x0F,0x98,0xC0, 0x40,0x0F,0x9A,0xC6
    });
    // Rebuild a flag bitmask in R9 without disturbing the saved R8.
    code.insert(code.end(), {
        0x0F,0xB6,0xD2,       // movzx edx,dl  CF
        0x0F,0xB6,0xDB,       // movzx ebx,bl  OF
        0x0F,0xB6,0xC9,       // movzx ecx,cl  ZF
        0x0F,0xB6,0xC0,       // movzx eax,al  SF
        0x40,0x0F,0xB6,0xF6,  // movzx esi,sil PF
        0xD1,0xE3,           // shl ebx,1
        0xC1,0xE1,2,         // shl ecx,2
        0xC1,0xE0,3,         // shl eax,3
        0xC1,0xE6,4,         // shl esi,4
        0x09,0xDA,           // or edx,ebx
        0x09,0xCA,           // or edx,ecx
        0x09,0xC2,           // or edx,eax
        0x09,0xF2,           // or edx,esi
        0x89,0xD0,0xC3      // mov eax,edx; ret
    });
    GuestCpu cpu(code);
    const auto flags = cpu.run();
    return {cpu.registerValue(8), static_cast<std::uint8_t>(flags)};
}

int main() {
    std::uint64_t seed = UINT64_C(0xC4D391BE7A20F865);
    constexpr std::array<std::uint64_t, 12> edge{
        0,1,2,3,0x7F,0x80,0xFFFFFFFF,
        UINT64_C(0x7FFFFFFF),UINT64_C(0x80000000),
        UINT64_C(0x7FFFFFFFFFFFFFFF),UINT64_C(0x8000000000000000),UINT64_MAX
    };
    unsigned comparisons = 0;
    for (unsigned i = 0; i < 512; ++i) {
        seed = seed * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
        const auto lhs = i < edge.size() ? edge[i] : seed;
        const auto rhs = i < edge.size() ? edge[edge.size()-1-i] : (seed >> 13);
        for (const bool wide : {false,true})
            for (const bool subtract : {false,true})
                for (const bool carry : {false,true}) {
                    const auto expected = native(lhs,rhs,carry,subtract,wide);
                    const auto actual = guest(lhs,rhs,carry,subtract,wide);
                    ++comparisons;
                    if (expected.value != actual.value || expected.flags != actual.flags) {
                        std::fprintf(stderr,
                            "ADC/SBB mismatch i=%u wide=%d sub=%d carry=%d lhs=%llx rhs=%llx expected=%llx/%02x actual=%llx/%02x\n",
                            i,wide,subtract,carry,
                            static_cast<unsigned long long>(lhs),
                            static_cast<unsigned long long>(rhs),
                            static_cast<unsigned long long>(expected.value),expected.flags,
                            static_cast<unsigned long long>(actual.value),actual.flags);
                        return EXIT_FAILURE;
                    }
                }
    }
    std::printf("ADC/SBB differential tests passed (%u native result+flag comparisons)\n",comparisons);
    return EXIT_SUCCESS;
}
#else
#error "Native differential tests require x86-64 GCC or Clang"
#endif
