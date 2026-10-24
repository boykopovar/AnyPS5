#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <cstdint>
#include <cstdlib>
#include <iostream>

using namespace FreeType;

extern "C" {
void APS5_VABI FT_Vector_Transform_nid_postfix(GuestVector*, const GuestMatrix*);
void APS5_VABI FT_Matrix_Multiply_nid_postfix(const GuestMatrix*, GuestMatrix*);
int APS5_VABI FT_Matrix_Invert_nid_postfix(GuestMatrix*);
}

static bool Equal(const GuestMatrix& a, const GuestMatrix& b) {
    return a.xx == b.xx && a.xy == b.xy && a.yx == b.yx && a.yy == b.yy;
}

template <typename TMatrix>
static void Print(const TMatrix& matrix) {
    std::cout << ' ' << matrix.xx << ' ' << matrix.xy << ' ' << matrix.yx << ' ' << matrix.yy;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    GuestMatrix unchanged{65536, -32768, 131072, 65536};
    const auto original = unchanged;
    GuestVector untouched{123, -456};
    FT_Vector_Transform_nid_postfix(nullptr, nullptr);
    FT_Vector_Transform_nid_postfix(nullptr, &unchanged);
    FT_Vector_Transform_nid_postfix(&untouched, nullptr);
    FT_Matrix_Multiply_nid_postfix(nullptr, nullptr);
    FT_Matrix_Multiply_nid_postfix(&unchanged, nullptr);
    FT_Matrix_Multiply_nid_postfix(nullptr, &unchanged);
    if (!Equal(unchanged, original) || untouched.x != 123 || untouched.y != -456 ||
        FT_Matrix_Invert_nid_postfix(nullptr) != FT_Err_Invalid_Argument) std::abort();
    std::cout << "FT_GEOMETRY " << sizeof(FT_Long) << '\n';
    GuestVector input{};
    GuestMatrix a{}, b{};
    while (std::cin >> input.x >> input.y >> a.xx >> a.xy >> a.yx >> a.yy >> b.xx >> b.xy >> b.yx >> b.yy) {
        struct {
            std::uint64_t before;
            GuestMatrix value;
            std::uint64_t after;
        } product{0x123456789ABCDEF0, b, 0xFEDCBA9876543210};
        struct {
            std::uint64_t before;
            GuestVector value;
            std::uint64_t after;
        } vector{0x123456789ABCDEF0, input, 0xFEDCBA9876543210};
        const auto left = a;
        auto square = a;
        auto inverse = a;
        FT_Vector_Transform_nid_postfix(&vector.value, &a);
        FT_Matrix_Multiply_nid_postfix(&a, &product.value);
        FT_Matrix_Multiply_nid_postfix(&square, &square);
        const int error = FT_Matrix_Invert_nid_postfix(&inverse);
        if (!Equal(a, left) || product.before != 0x123456789ABCDEF0 || product.after != 0xFEDCBA9876543210 ||
            vector.before != 0x123456789ABCDEF0 || vector.after != 0xFEDCBA9876543210) std::abort();
        std::cout << vector.value.x << ' ' << vector.value.y;
        Print(product.value);
        Print(square);
        std::cout << ' ' << error;
        Print(inverse);
        if constexpr (sizeof(FT_Long) == 8) {
            FT_Vector hostVector{static_cast<FT_Pos>(input.x), static_cast<FT_Pos>(input.y)};
            const FT_Matrix hostA{static_cast<FT_Fixed>(a.xx), static_cast<FT_Fixed>(a.xy), static_cast<FT_Fixed>(a.yx), static_cast<FT_Fixed>(a.yy)};
            FT_Matrix hostB{static_cast<FT_Fixed>(b.xx), static_cast<FT_Fixed>(b.xy), static_cast<FT_Fixed>(b.yx), static_cast<FT_Fixed>(b.yy)};
            auto hostSquare = hostA;
            auto hostInverse = hostA;
            FT_Vector_Transform(&hostVector, &hostA);
            FT_Matrix_Multiply(&hostA, &hostB);
            FT_Matrix_Multiply(&hostSquare, &hostSquare);
            const int hostError = FT_Matrix_Invert(&hostInverse);
            std::cout << ' ' << hostVector.x << ' ' << hostVector.y;
            Print(hostB);
            Print(hostSquare);
            std::cout << ' ' << hostError;
            Print(hostInverse);
        }
        std::cout << '\n';
    }
    if (!std::cin.eof()) return 1;
}
