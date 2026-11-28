#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestCpu.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>

struct ExpectedExp {
    unsigned rounding;
    std::uint32_t bits;
    int flags;
    int error;
};

struct ExpCase {
    std::uint32_t input;
    std::array<ExpectedExp, 4> expected;
};

static constexpr ExpCase Cases[] = {
    {0x00000000u, {{
        {0x000u, 0x3f800000u, 0, 73},
        {0x400u, 0x3f800000u, 0, 73},
        {0x800u, 0x3f800000u, 0, 73},
        {0xc00u, 0x3f800000u, 0, 73},
    }}},
    {0x80000000u, {{
        {0x000u, 0x3f800000u, 0, 73},
        {0x400u, 0x3f800000u, 0, 73},
        {0x800u, 0x3f800000u, 0, 73},
        {0xc00u, 0x3f800000u, 0, 73},
    }}},
    {0x00000001u, {{
        {0x000u, 0x3f800000u, 34, 73},
        {0x400u, 0x3f800000u, 34, 73},
        {0x800u, 0x3f800001u, 34, 73},
        {0xc00u, 0x3f800000u, 34, 73},
    }}},
    {0x80000001u, {{
        {0x000u, 0x3f800000u, 34, 73},
        {0x400u, 0x3f7fffffu, 34, 73},
        {0x800u, 0x3f800000u, 34, 73},
        {0xc00u, 0x3f7fffffu, 34, 73},
    }}},
    {0x007fffffu, {{
        {0x000u, 0x3f800000u, 34, 73},
        {0x400u, 0x3f800000u, 34, 73},
        {0x800u, 0x3f800001u, 34, 73},
        {0xc00u, 0x3f800000u, 34, 73},
    }}},
    {0x807fffffu, {{
        {0x000u, 0x3f800000u, 34, 73},
        {0x400u, 0x3f7fffffu, 34, 73},
        {0x800u, 0x3f800000u, 34, 73},
        {0xc00u, 0x3f7fffffu, 34, 73},
    }}},
    {0x00800000u, {{
        {0x000u, 0x3f800000u, 32, 73},
        {0x400u, 0x3f800000u, 32, 73},
        {0x800u, 0x3f800001u, 32, 73},
        {0xc00u, 0x3f800000u, 32, 73},
    }}},
    {0x80800000u, {{
        {0x000u, 0x3f800000u, 32, 73},
        {0x400u, 0x3f7fffffu, 32, 73},
        {0x800u, 0x3f800000u, 32, 73},
        {0xc00u, 0x3f7fffffu, 32, 73},
    }}},
    {0x33800000u, {{
        {0x000u, 0x3f800000u, 32, 73},
        {0x400u, 0x3f800000u, 32, 73},
        {0x800u, 0x3f800001u, 32, 73},
        {0xc00u, 0x3f800000u, 32, 73},
    }}},
    {0x34000000u, {{
        {0x000u, 0x3f800001u, 32, 73},
        {0x400u, 0x3f800001u, 32, 73},
        {0x800u, 0x3f800001u, 32, 73},
        {0xc00u, 0x3f800001u, 32, 73},
    }}},
    {0xb3800000u, {{
        {0x000u, 0x3f7fffffu, 32, 73},
        {0x400u, 0x3f7fffffu, 32, 73},
        {0x800u, 0x3f7fffffu, 32, 73},
        {0xc00u, 0x3f7fffffu, 32, 73},
    }}},
    {0xb4000000u, {{
        {0x000u, 0x3f7ffffeu, 32, 73},
        {0x400u, 0x3f7ffffeu, 32, 73},
        {0x800u, 0x3f7ffffeu, 32, 73},
        {0xc00u, 0x3f7ffffeu, 32, 73},
    }}},
    {0x3e800000u, {{
        {0x000u, 0x3fa45af2u, 32, 73},
        {0x400u, 0x3fa45af1u, 32, 73},
        {0x800u, 0x3fa45af2u, 32, 73},
        {0xc00u, 0x3fa45af1u, 32, 73},
    }}},
    {0x3f000000u, {{
        {0x000u, 0x3fd3094cu, 32, 73},
        {0x400u, 0x3fd3094cu, 32, 73},
        {0x800u, 0x3fd3094du, 32, 73},
        {0xc00u, 0x3fd3094cu, 32, 73},
    }}},
    {0x3f7ffff0u, {{
        {0x000u, 0x402df84au, 32, 73},
        {0x400u, 0x402df849u, 32, 73},
        {0x800u, 0x402df84au, 32, 73},
        {0xc00u, 0x402df849u, 32, 73},
    }}},
    {0x3f800000u, {{
        {0x000u, 0x402df854u, 0, 73},
        {0x400u, 0x402df854u, 0, 73},
        {0x800u, 0x402df854u, 0, 73},
        {0xc00u, 0x402df854u, 0, 73},
    }}},
    {0x3f800006u, {{
        {0x000u, 0x402df85cu, 32, 73},
        {0x400u, 0x402df85cu, 32, 73},
        {0x800u, 0x402df85du, 32, 73},
        {0xc00u, 0x402df85cu, 32, 73},
    }}},
    {0x3f800010u, {{
        {0x000u, 0x402df86au, 32, 73},
        {0x400u, 0x402df86au, 32, 73},
        {0x800u, 0x402df86bu, 32, 73},
        {0xc00u, 0x402df86au, 32, 73},
    }}},
    {0xbf800000u, {{
        {0x000u, 0x3ebc5ab2u, 32, 73},
        {0x400u, 0x3ebc5ab1u, 32, 73},
        {0x800u, 0x3ebc5ab2u, 32, 73},
        {0xc00u, 0x3ebc5ab2u, 32, 73},
    }}},
    {0x40480000u, {{
        {0x000u, 0x41b61444u, 32, 73},
        {0x400u, 0x41b61444u, 32, 73},
        {0x800u, 0x41b61444u, 32, 73},
        {0xc00u, 0x41b61444u, 32, 73},
    }}},
    {0xc0400000u, {{
        {0x000u, 0x3d4bed86u, 32, 73},
        {0x400u, 0x3d4bed86u, 32, 73},
        {0x800u, 0x3d4bed87u, 32, 73},
        {0xc00u, 0x3d4bed86u, 32, 73},
    }}},
    {0x42800000u, {{
        {0x000u, 0x6da12cc2u, 32, 73},
        {0x400u, 0x6da12cc1u, 32, 73},
        {0x800u, 0x6da12cc2u, 32, 73},
        {0xc00u, 0x6da12cc1u, 32, 73},
    }}},
    {0x42b00000u, {{
        {0x000u, 0x7ef882b7u, 32, 73},
        {0x400u, 0x7ef882b6u, 32, 73},
        {0x800u, 0x7ef882b7u, 32, 73},
        {0xc00u, 0x7ef882b6u, 32, 73},
    }}},
    {0xc2c80000u, {{
        {0x000u, 0x0000001bu, 48, 73},
        {0x400u, 0x0000001au, 48, 73},
        {0x800u, 0x0000001bu, 48, 73},
        {0xc00u, 0x0000001au, 48, 73},
    }}},
    {0x7f7fffffu, {{
        {0x000u, 0x7f800000u, 40, 34},
        {0x400u, 0x7f800000u, 40, 34},
        {0x800u, 0x7f800000u, 40, 34},
        {0xc00u, 0x7f800000u, 40, 34},
    }}},
    {0xff7fffffu, {{
        {0x000u, 0x00000000u, 48, 34},
        {0x400u, 0x00000000u, 48, 34},
        {0x800u, 0x00000001u, 48, 34},
        {0xc00u, 0x00000000u, 48, 34},
    }}},
    {0x7f800000u, {{
        {0x000u, 0x7f800000u, 0, 73},
        {0x400u, 0x7f800000u, 0, 73},
        {0x800u, 0x7f800000u, 0, 73},
        {0xc00u, 0x7f800000u, 0, 73},
    }}},
    {0xff800000u, {{
        {0x000u, 0x00000000u, 0, 73},
        {0x400u, 0x00000000u, 0, 73},
        {0x800u, 0x00000000u, 0, 73},
        {0xc00u, 0x00000000u, 0, 73},
    }}},
    {0x7fc00000u, {{
        {0x000u, 0x7fc00000u, 0, 73},
        {0x400u, 0x7fc00000u, 0, 73},
        {0x800u, 0x7fc00000u, 0, 73},
        {0xc00u, 0x7fc00000u, 0, 73},
    }}},
    {0xffc00000u, {{
        {0x000u, 0xffc00000u, 0, 73},
        {0x400u, 0xffc00000u, 0, 73},
        {0x800u, 0xffc00000u, 0, 73},
        {0xc00u, 0xffc00000u, 0, 73},
    }}},
    {0x7fc12345u, {{
        {0x000u, 0x7fc12345u, 0, 73},
        {0x400u, 0x7fc12345u, 0, 73},
        {0x800u, 0x7fc12345u, 0, 73},
        {0xc00u, 0x7fc12345u, 0, 73},
    }}},
    {0xffc12345u, {{
        {0x000u, 0xffc12345u, 0, 73},
        {0x400u, 0xffc12345u, 0, 73},
        {0x800u, 0xffc12345u, 0, 73},
        {0xc00u, 0xffc12345u, 0, 73},
    }}},
    {0x7f812345u, {{
        {0x000u, 0x7fc12345u, 1, 73},
        {0x400u, 0x7fc12345u, 1, 73},
        {0x800u, 0x7fc12345u, 1, 73},
        {0xc00u, 0x7fc12345u, 1, 73},
    }}},
    {0xff812345u, {{
        {0x000u, 0xffc12345u, 1, 73},
        {0x400u, 0xffc12345u, 1, 73},
        {0x800u, 0xffc12345u, 1, 73},
        {0xc00u, 0xffc12345u, 1, 73},
    }}},
    {0x42b17216u, {{
        {0x000u, 0x7f7fff04u, 32, 73},
        {0x400u, 0x7f7fff04u, 32, 73},
        {0x800u, 0x7f7fff05u, 32, 73},
        {0xc00u, 0x7f7fff04u, 32, 73},
    }}},
    {0x42b17217u, {{
        {0x000u, 0x7f7fff84u, 32, 73},
        {0x400u, 0x7f7fff84u, 32, 73},
        {0x800u, 0x7f7fff85u, 32, 73},
        {0xc00u, 0x7f7fff84u, 32, 73},
    }}},
    {0x42b17218u, {{
        {0x000u, 0x7f800000u, 40, 34},
        {0x400u, 0x7f800000u, 40, 34},
        {0x800u, 0x7f800000u, 40, 34},
        {0xc00u, 0x7f800000u, 40, 34},
    }}},
    {0x42b17219u, {{
        {0x000u, 0x7f800000u, 40, 34},
        {0x400u, 0x7f800000u, 40, 34},
        {0x800u, 0x7f800000u, 40, 34},
        {0xc00u, 0x7f800000u, 40, 34},
    }}},
    {0x42b1721au, {{
        {0x000u, 0x7f800000u, 40, 34},
        {0x400u, 0x7f800000u, 40, 34},
        {0x800u, 0x7f800000u, 40, 34},
        {0xc00u, 0x7f800000u, 40, 34},
    }}},
    {0xc2cff1b3u, {{
        {0x000u, 0x00000001u, 48, 73},
        {0x400u, 0x00000000u, 48, 73},
        {0x800u, 0x00000001u, 48, 73},
        {0xc00u, 0x00000000u, 48, 73},
    }}},
    {0xc2cff1b4u, {{
        {0x000u, 0x00000001u, 48, 73},
        {0x400u, 0x00000000u, 48, 73},
        {0x800u, 0x00000001u, 48, 73},
        {0xc00u, 0x00000000u, 48, 73},
    }}},
    {0xc2cff1b5u, {{
        {0x000u, 0x00000000u, 48, 73},
        {0x400u, 0x00000000u, 48, 73},
        {0x800u, 0x00000001u, 48, 73},
        {0xc00u, 0x00000000u, 48, 73},
    }}},
    {0xc2cff1b6u, {{
        {0x000u, 0x00000000u, 48, 34},
        {0x400u, 0x00000000u, 48, 34},
        {0x800u, 0x00000001u, 48, 34},
        {0xc00u, 0x00000000u, 48, 34},
    }}},
    {0xc2cff1b7u, {{
        {0x000u, 0x00000000u, 48, 34},
        {0x400u, 0x00000000u, 48, 34},
        {0x800u, 0x00000001u, 48, 34},
        {0xc00u, 0x00000000u, 48, 34},
    }}},
    {0xc2aeac4fu, {{
        {0x000u, 0x00800026u, 32, 73},
        {0x400u, 0x00800025u, 32, 73},
        {0x800u, 0x00800026u, 32, 73},
        {0xc00u, 0x00800025u, 32, 73},
    }}},
    {0xc2aeac50u, {{
        {0x000u, 0x007fffe6u, 32, 73},
        {0x400u, 0x007fffe5u, 48, 73},
        {0x800u, 0x007fffe6u, 32, 73},
        {0xc00u, 0x007fffe5u, 48, 73},
    }}},
    {0xc2aeac51u, {{
        {0x000u, 0x007fffa6u, 32, 73},
        {0x400u, 0x007fffa5u, 48, 73},
        {0x800u, 0x007fffa6u, 32, 73},
        {0xc00u, 0x007fffa5u, 48, 73},
    }}},
};

int main() {
    const auto savedControl = GuestCpu::X87Control();
    const auto savedSse = GuestCpu::Mxcsr();
    const int savedError = *__error_nid_postfix();
    unsigned checked = 0;
    for (const auto& item : Cases) {
        for (const auto& expected : item.expected) {
            for (const unsigned precision : {0u, 0x200u, 0x300u}) {
                for (const int pending : {0, 4}) {
                    const auto control = static_cast<std::uint16_t>((savedControl & ~0xf00u) | precision | expected.rounding | 0x3fu);
                    const unsigned mxcsr = (savedSse & ~(0x6000u | 0x8040u | 0x3fu)) | 0x1f80u | (expected.rounding << 3);
                    GuestCpu::SetX87Control(control);
                    feclearexcept_nid_postfix(0x3f);
                    GuestCpu::SetMxcsr(mxcsr | static_cast<unsigned>(pending));
                    *__error_nid_postfix() = 73;
                    const float result = expf_nid_postfix(std::bit_cast<float>(item.input));
                    const int flags = fetestexcept_nid_postfix(0x3f);
                    const auto bits = std::bit_cast<std::uint32_t>(result);
                    if (bits != expected.bits || flags != (expected.flags | pending) || *__error_nid_postfix() != expected.error || GuestCpu::X87Control() != control || (GuestCpu::Mxcsr() & ~0x3fu) != mxcsr) {
                        std::fprintf(stderr, "expf environment regression failed: input=%08x rc=%x pc=%x pending=%x expected=%08x flags=%x errno=%d actual=%08x flags=%x errno=%d\n",item.input,expected.rounding,precision,pending,expected.bits,expected.flags|pending,expected.error,bits,flags,*__error_nid_postfix());
                        return 1;
                    }
                    ++checked;
                }
            }
        }
    }
    feclearexcept_nid_postfix(0x3f);
    GuestCpu::SetX87Control(savedControl);
    GuestCpu::SetMxcsr(savedSse);
    *__error_nid_postfix() = savedError;
    std::printf("expf: %u result/flags/errno/rounding/precision/pending checks passed\n",checked);
    return 0;
}
