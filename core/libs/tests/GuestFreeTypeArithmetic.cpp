#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <iostream>

extern "C" {
std::int64_t APS5_VABI FT_MulDiv_nid_postfix(std::int64_t, std::int64_t, std::int64_t);
std::int64_t APS5_VABI FT_MulFix_nid_postfix(std::int64_t, std::int64_t);
std::int64_t APS5_VABI FT_DivFix_nid_postfix(std::int64_t, std::int64_t);
std::int64_t APS5_VABI FT_RoundFix_nid_postfix(std::int64_t);
std::int64_t APS5_VABI FT_CeilFix_nid_postfix(std::int64_t);
std::int64_t APS5_VABI FT_FloorFix_nid_postfix(std::int64_t);
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout << "FT_ARITHMETIC " << sizeof(FT_Long) << '\n';
    std::int64_t a, b, c;
    while (std::cin >> a >> b >> c) {
        const std::array results{
            FT_MulDiv_nid_postfix(a, b, c), FT_MulFix_nid_postfix(a, b), FT_DivFix_nid_postfix(a, b),
            FT_RoundFix_nid_postfix(a), FT_CeilFix_nid_postfix(a), FT_FloorFix_nid_postfix(a)
        };
        for (const auto value : results) std::cout << value << ' ';
        if constexpr (sizeof(FT_Long) == 8) {
            const std::array native{
                FT_MulDiv(a, b, c), FT_MulFix(a, b), FT_DivFix(a, b), FT_RoundFix(a), FT_CeilFix(a), FT_FloorFix(a)
            };
            for (const auto value : native) std::cout << value << ' ';
        }
        std::cout << '\n';
    }
    return std::cin.eof() ? 0 : 1;
}
