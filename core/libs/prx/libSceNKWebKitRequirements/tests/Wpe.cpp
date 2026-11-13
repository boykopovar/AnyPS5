#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

struct Case {
    std::uint32_t input;
    std::uint32_t expected;
};

constexpr Case KeyCodeToUnicode[] = {
    {0x00000000, 0x00000000},
    {0x0000001f, 0x00000000},
    {0x00000020, 0x00000020},
    {0x00000041, 0x00000041},
    {0x0000007e, 0x0000007e},
    {0x0000007f, 0x00000000},
    {0x0000009f, 0x00000000},
    {0x000000a0, 0x000000a0},
    {0x000000e9, 0x000000e9},
    {0x000000ff, 0x000000ff},
    {0x00000100, 0x00000000},
    {0x000001a1, 0x00000104},
    {0x000001a4, 0x00000000},
    {0x000001ff, 0x000002d9},
    {0x000006c1, 0x00000430},
    {0x00000aa2, 0x00002002},
    {0x000013bc, 0x00000152},
    {0x000020ac, 0x000020ac},
    {0x000020ad, 0x00000000},
    {0x0000ff08, 0x00000008},
    {0x0000ff0d, 0x0000000d},
    {0x0000ff80, 0x00000020},
    {0x0000ffb5, 0x00000035},
    {0x0000ffbd, 0x0000003d},
    {0x0000ffbe, 0x00000000},
    {0x0000ffff, 0x0000007f},
    {0x00010000, 0x00000000},
    {0x0100263a, 0x0000263a},
    {0x01000000, 0x00000000},
    {0x01ffffff, 0x00ffffff},
    {0x02000041, 0x00000000},
    {0xff000041, 0x00000000},
    {0xffffffff, 0x00000000},
};

constexpr Case UnicodeToKeyCode[] = {
    {0x00000000, 0x01000000},
    {0x00000008, 0x0000ff08},
    {0x00000009, 0x0000ff09},
    {0x0000000a, 0x0000ff0a},
    {0x0000000b, 0x0000ff0b},
    {0x0000000c, 0x0100000c},
    {0x0000000d, 0x0000ff0d},
    {0x0000001b, 0x0000ff1b},
    {0x0000001f, 0x0100001f},
    {0x00000020, 0x00000020},
    {0x00000041, 0x00000041},
    {0x0000007e, 0x0000007e},
    {0x0000007f, 0x0100007f},
    {0x0000009f, 0x0100009f},
    {0x000000a0, 0x000000a0},
    {0x000000e9, 0x000000e9},
    {0x000000ff, 0x000000ff},
    {0x00000100, 0x000003c0},
    {0x00000104, 0x000001a1},
    {0x00000152, 0x000013bc},
    {0x000002d8, 0x000001a2},
    {0x00002002, 0x00000aa2},
    {0x000020ac, 0x000020ac},
    {0x0000263a, 0x0100263a},
    {0x0000318e, 0x00000ef7},
    {0x0000318f, 0x0100318f},
    {0x0000ffff, 0x0100ffff},
    {0x00010000, 0x01010000},
    {0x0010ffff, 0x0110ffff},
    {0x01000041, 0x01000041},
    {0xffffffff, 0xffffffff},
};

template <typename Function>
int Check(const char* name, Function function, const Case* cases, std::size_t count) {
    int failures = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint32_t actual = function(cases[index].input);
        if (actual != cases[index].expected) {
            std::fprintf(stderr, "%s(0x%08x) returned 0x%08x, expected 0x%08x\n", name,
                static_cast<unsigned>(cases[index].input), static_cast<unsigned>(actual),
                static_cast<unsigned>(cases[index].expected));
            ++failures;
        }
    }
    return failures;
}

} // namespace

int main() {
    int failures = Check("wpe_key_code_to_unicode", wpe_key_code_to_unicode_nid_postfix, KeyCodeToUnicode,
        sizeof(KeyCodeToUnicode) / sizeof(KeyCodeToUnicode[0]));
    failures += Check("wpe_unicode_to_key_code", wpe_unicode_to_key_code_nid_postfix, UnicodeToKeyCode,
        sizeof(UnicodeToKeyCode) / sizeof(UnicodeToKeyCode[0]));
    if (failures != 0) {
        std::fprintf(stderr, "%d WPE key code cases failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("Wpe tests passed.\n");
    return EXIT_SUCCESS;
}
