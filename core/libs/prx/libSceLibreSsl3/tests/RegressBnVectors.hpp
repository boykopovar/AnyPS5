#pragma once

#include <array>

namespace LibreSslRegressBn {

struct ConvertCase {
    const char* hex;
    int neg;
    const char* binHex;
    int binLen;
    int numBits;
};

inline constexpr std::array<ConvertCase, 15> ConvertCases {{
    {"0", 0, "", 0, 0},
    {"01", 0, "01", 1, 1},
    {"-01", 1, "01", 1, 1},
    {"7FFFFF", 0, "7fffff", 3, 23},
    {"-7FFFFF", 1, "7fffff", 3, 23},
    {"FFFFFFFF", 0, "ffffffff", 4, 32},
    {"-FFFFFFFF", 1, "ffffffff", 4, 32},
    {"FFFFFFFF00000000", 0, "ffffffff00000000", 8, 64},
    {"-FFFFFFFF00000000", 1, "ffffffff00000000", 8, 64},
    {"8001800180018001", 0, "8001800180018001", 8, 64},
    {"-8001800180018001", 1, "8001800180018001", 8, 64},
    {"018001800180018001", 0, "018001800180018001", 9, 65},
    {"-018001800180018001", 1, "018001800180018001", 9, 65},
    {"7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF", 0, "7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff", 32, 255},
    {"-7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF7FFF", 1, "7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff7fff", 32, 255},
}};

}
