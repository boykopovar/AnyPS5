#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeTypeOt/include/Calc.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType corner: %s\n", message);
    std::exit(1);
}

template <typename TCall> bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool SameMatrix(const FreeType::GuestMatrix& left, const FreeType::GuestMatrix& right) {
    return left.xx == right.xx && left.xy == right.xy && left.yx == right.yx && left.yy == right.yy;
}

bool SameVector(const FreeType::GuestVector& left, const FreeType::GuestVector& right) {
    return left.x == right.x && left.y == right.y;
}

bool MulDivNoRoundThrows(std::int64_t a, std::int64_t b, std::int64_t c) {
    return Throws([=] { FT_MulDiv_No_Round_nid_postfix(a, b, c); });
}

FreeType::GuestMatrix Multiplied(const FreeType::GuestMatrix& a, FreeType::GuestMatrix b, std::int64_t scaling) {
    FT_Matrix_Multiply_Scaled_nid_postfix(&a, &b, scaling);
    return b;
}

bool MultiplyThrowsUnchanged(const FreeType::GuestMatrix& a, const FreeType::GuestMatrix& b, std::int64_t scaling) {
    FreeType::GuestMatrix target = b;
    return Throws([&] { FT_Matrix_Multiply_Scaled_nid_postfix(&a, &target, scaling); }) && SameMatrix(target, b);
}

FreeType::GuestVector Transformed(FreeType::GuestVector vector, const FreeType::GuestMatrix& matrix, std::int64_t scaling) {
    FT_Vector_Transform_Scaled_nid_postfix(&vector, &matrix, scaling);
    return vector;
}

bool TransformThrowsUnchanged(const FreeType::GuestVector& vector, const FreeType::GuestMatrix& matrix, std::int64_t scaling) {
    FreeType::GuestVector target = vector;
    return Throws([&] { FT_Vector_Transform_Scaled_nid_postfix(&target, &matrix, scaling); }) && SameVector(target, vector);
}

void CheckMulDivNoRound() {
    Require(FT_MulDiv_No_Round_nid_postfix(7, 3, 2) == 10, "FT_MulDiv_No_Round truncates the quotient");
    Require(FT_MulDiv_No_Round_nid_postfix(-7, 3, 2) == -10, "FT_MulDiv_No_Round truncates a negative quotient toward zero");
    Require(FT_MulDiv_No_Round_nid_postfix(7, -3, -2) == 10, "FT_MulDiv_No_Round multiplies the operand signs");
    Require(FT_MulDiv_No_Round_nid_postfix(7, 3, -2) == -10, "FT_MulDiv_No_Round takes the divisor sign");
    Require(FT_MulDiv_No_Round_nid_postfix(0, 3, 5) == 0, "FT_MulDiv_No_Round of a zero factor is 0");
    Require(FT_MulDiv_No_Round_nid_postfix(5, 7, 7) == 5, "FT_MulDiv_No_Round with b equal to c returns a");
    Require(FT_MulDiv_No_Round_nid_postfix(5, 3, 0) == 0x7FFFFFFF, "FT_MulDiv_No_Round saturates a zero divisor");
    Require(FT_MulDiv_No_Round_nid_postfix(-5, 3, 0) == -0x7FFFFFFF, "FT_MulDiv_No_Round keeps the factor sign for a zero divisor");
    Require(FT_MulDiv_No_Round_nid_postfix(-5, -3, 0) == 0x7FFFFFFF, "FT_MulDiv_No_Round multiplies the factor signs for a zero divisor");
    Require(FT_MulDiv_No_Round_nid_postfix(0x7FFFFFFF, 3, 2) == 3221225470, "FT_MulDiv_No_Round accepts the largest operand");
    Require(FT_MulDiv_No_Round_nid_postfix(-0x7FFFFFFF, -0x7FFFFFFF, 0x7FFFFFFF) == 0x7FFFFFFF, "FT_MulDiv_No_Round accepts the largest negative operands");
    Require(FT_MulDiv_No_Round_nid_postfix(0xFFFF, 0x10001, 1) == 0xFFFFFFFF, "FT_MulDiv_No_Round returns the largest quotient");
    Require(MulDivNoRoundThrows(0, 3, 0) && MulDivNoRoundThrows(5, 0, 0), "FT_MulDiv_No_Round throws for a zero divisor with a zero factor");
    Require(MulDivNoRoundThrows(0x80000000LL, 1, 1) && MulDivNoRoundThrows(1, -0x80000000LL, 1) && MulDivNoRoundThrows(1, 1, 0x80000000LL),
        "FT_MulDiv_No_Round throws for an operand outside 0x7FFFFFFF");
    Require(MulDivNoRoundThrows(0x10000, 0x10000, 1) && MulDivNoRoundThrows(-0x7FFFFFFF, 0x7FFFFFFF, 1), "FT_MulDiv_No_Round throws for a quotient above 0xFFFFFFFF");
    Require(MulDivNoRoundThrows(1LL << 40, 1LL << 20, 3), "FT_MulDiv_No_Round throws where the 32-bit build truncates operands");
    Require(MulDivNoRoundThrows(INT64_MAX, 2, 1) && MulDivNoRoundThrows(INT64_MIN, 1, 1) && MulDivNoRoundThrows(1, 1, INT64_MIN), "FT_MulDiv_No_Round throws at the 64-bit limits");
}

void CheckMatrixMultiplyScaled() {
    const FreeType::GuestMatrix identity{0x10000, 0, 0, 0x10000};
    const FreeType::GuestMatrix matrix{2 << 16, 3 << 16, 4 << 16, 5 << 16};
    Require(SameMatrix(Multiplied(identity, matrix, 1), matrix), "FT_Matrix_Multiply_Scaled by the identity with scaling 1 keeps b");
    Require(SameMatrix(Multiplied({1 << 16, 2 << 16, 3 << 16, 4 << 16}, {5 << 16, 6 << 16, 7 << 16, 8 << 16}, 1), {19 << 16, 22 << 16, 43 << 16, 50 << 16}),
        "FT_Matrix_Multiply_Scaled stores a times b");
    Require(SameMatrix(Multiplied(identity, {3, 5, -3, -5}, 2), {2, 3, -2, -3}), "FT_Matrix_Multiply_Scaled rounds half away from zero");
    Require(SameMatrix(Multiplied(identity, matrix, -1), {-(2 << 16), -(3 << 16), -(4 << 16), -(5 << 16)}), "FT_Matrix_Multiply_Scaled takes the scaling sign");
    Require(SameMatrix(Multiplied(identity, matrix, 32767), {4, 6, 8, 10}), "FT_Matrix_Multiply_Scaled accepts scaling 32767");
    Require(SameMatrix(Multiplied({1 << 16, 1 << 16, 1 << 16, 1 << 16}, {1 << 16, -(1 << 16), 1 << 16, 1 << 16}, 0), {0xFFFFFFFE, 0, 0xFFFFFFFE, 0}),
        "FT_Matrix_Multiply_Scaled adds saturated terms for scaling 0");
    FreeType::GuestMatrix square{1 << 16, 2 << 16, 3 << 16, 4 << 16};
    FT_Matrix_Multiply_Scaled_nid_postfix(&square, &square, 1);
    Require(SameMatrix(square, {7 << 16, 10 << 16, 15 << 16, 22 << 16}), "FT_Matrix_Multiply_Scaled reads both matrices before writing b");
    Require(MultiplyThrowsUnchanged(identity, matrix, 0), "FT_Matrix_Multiply_Scaled throws for scaling 0 with a zero factor");
    Require(MultiplyThrowsUnchanged(identity, matrix, 32768) && MultiplyThrowsUnchanged(identity, matrix, -32768), "FT_Matrix_Multiply_Scaled throws for scaling outside 32767");
    Require(MultiplyThrowsUnchanged(identity, matrix, 1LL << 48), "FT_Matrix_Multiply_Scaled throws where 0x10000 * scaling overflows");
    Require(MultiplyThrowsUnchanged({0x10000, 0, 0, 0x7FFFFFFF}, {1, 2, 3, 0x7FFFFFFF}, 1), "FT_Matrix_Multiply_Scaled checks the last term before writing b");
    Require(MultiplyThrowsUnchanged({0x80000000LL, 0, 0, 0x10000}, {1, 2, 3, 4}, 1), "FT_Matrix_Multiply_Scaled throws for an entry outside 0x7FFFFFFF");
    FreeType::GuestMatrix target = matrix;
    FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, 1);
    FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, 32768);
    FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, (1LL << 47) - 1);
    FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, -(1LL << 47));
    FT_Matrix_Multiply_Scaled_nid_postfix(&identity, nullptr, 1);
    Require(SameMatrix(target, matrix), "FT_Matrix_Multiply_Scaled ignores a null matrix");
    Require(Throws([&] { FT_Matrix_Multiply_Scaled_nid_postfix(nullptr, &target, 1LL << 47); }) &&
            Throws([&] { FT_Matrix_Multiply_Scaled_nid_postfix(&identity, nullptr, -(1LL << 47) - 1); }) && SameMatrix(target, matrix),
        "FT_Matrix_Multiply_Scaled checks 0x10000 * scaling before the null test");
}

void CheckVectorTransformScaled() {
    const FreeType::GuestMatrix identity{0x10000, 0, 0, 0x10000};
    const FreeType::GuestVector vector{3 << 16, -(7 << 16)};
    Require(SameVector(Transformed(vector, identity, 1), vector), "FT_Vector_Transform_Scaled by the identity with scaling 1 keeps the vector");
    Require(SameVector(Transformed({100, -200}, {0x18000, 0x8000, -0x10000, 0x20000}, 1), {50, -500}), "FT_Vector_Transform_Scaled applies the matrix");
    Require(SameVector(Transformed({3, -5}, identity, 2), {2, -3}), "FT_Vector_Transform_Scaled rounds half away from zero");
    Require(SameVector(Transformed({2 << 16, 3 << 16}, identity, -1), {-(2 << 16), -(3 << 16)}), "FT_Vector_Transform_Scaled takes the scaling sign");
    Require(SameVector(Transformed({1 << 16, 1 << 16}, {1 << 16, -(1 << 16), 1 << 16, 1 << 16}, 0), {0, 0xFFFFFFFE}), "FT_Vector_Transform_Scaled adds saturated terms for scaling 0");
    Require(TransformThrowsUnchanged(vector, identity, 0), "FT_Vector_Transform_Scaled throws for scaling 0 with a zero factor");
    Require(TransformThrowsUnchanged(vector, identity, 32768) && TransformThrowsUnchanged(vector, identity, -32768), "FT_Vector_Transform_Scaled throws for scaling outside 32767");
    Require(TransformThrowsUnchanged({1, 0x7FFFFFFF}, {1, 0, 0, 0x7FFFFFFF}, 1), "FT_Vector_Transform_Scaled checks the second coordinate before writing");
    Require(TransformThrowsUnchanged({0x80000000LL, 1}, identity, 1), "FT_Vector_Transform_Scaled throws for a coordinate outside 0x7FFFFFFF");
    FreeType::GuestVector target = vector;
    FT_Vector_Transform_Scaled_nid_postfix(&target, nullptr, 1);
    FT_Vector_Transform_Scaled_nid_postfix(&target, nullptr, 32768);
    FT_Vector_Transform_Scaled_nid_postfix(nullptr, &identity, 1);
    Require(SameVector(target, vector), "FT_Vector_Transform_Scaled ignores a null vector or matrix");
    Require(Throws([&] { FT_Vector_Transform_Scaled_nid_postfix(&target, nullptr, INT64_MIN); }) &&
            Throws([&] { FT_Vector_Transform_Scaled_nid_postfix(nullptr, &identity, INT64_MAX); }) && SameVector(target, vector),
        "FT_Vector_Transform_Scaled checks 0x10000 * scaling before the null test");
}

void CheckSqrtFixed() {
    Require(FT_SqrtFixed_nid_postfix(0) == 0, "FT_SqrtFixed of 0 is 0");
    Require(FT_SqrtFixed_nid_postfix(-1) == 0 && FT_SqrtFixed_nid_postfix(-0x10000) == 0 && FT_SqrtFixed_nid_postfix(INT32_MIN) == 0,
        "FT_SqrtFixed returns 0 for negative inputs");
    Require(FT_SqrtFixed_nid_postfix(1) == 256, "FT_SqrtFixed scales the least positive fixed unit by 256");
    Require(FT_SqrtFixed_nid_postfix(0x10000) == 0x10000 && FT_SqrtFixed_nid_postfix(0x40000) == 0x20000 && FT_SqrtFixed_nid_postfix(0x90000) == 0x30000 &&
            FT_SqrtFixed_nid_postfix(0x24000) == 0x18000,
        "FT_SqrtFixed computes exact 16.16 roots");
    Require(FT_SqrtFixed_nid_postfix(2) == 362 && FT_SqrtFixed_nid_postfix(3) == 443 && FT_SqrtFixed_nid_postfix(12) == 886 &&
            FT_SqrtFixed_nid_postfix(0xFFFE) == 0xFFFE && FT_SqrtFixed_nid_postfix(0xFFFF) == 0xFFFF && FT_SqrtFixed_nid_postfix(0x10001) == 0x10000,
        "FT_SqrtFixed truncates non-square roots");
    Require(FT_SqrtFixed_nid_postfix(INT32_MAX) == 0xB504F3, "FT_SqrtFixed handles INT32_MAX");
}

void CheckSqrt32() {
    Require(FT_Sqrt32_nid_postfix(0) == 0 && FT_Sqrt32_nid_postfix(1) == 1, "FT_Sqrt32 handles 0 and 1");
    Require(FT_Sqrt32_nid_postfix(4) == 2 && FT_Sqrt32_nid_postfix(9) == 3 && FT_Sqrt32_nid_postfix(0x10000) == 256 &&
            FT_Sqrt32_nid_postfix(46340 * 46340) == 46340,
        "FT_Sqrt32 computes exact integer roots");
    Require(FT_Sqrt32_nid_postfix(3) == 1 && FT_Sqrt32_nid_postfix(8) == 2 && FT_Sqrt32_nid_postfix(15) == 3 &&
            FT_Sqrt32_nid_postfix(46340 * 46340 - 1) == 46339 && FT_Sqrt32_nid_postfix(INT32_MAX) == 46340,
        "FT_Sqrt32 truncates non-square positive inputs");
    Require(FT_Sqrt32_nid_postfix(INT32_MIN) == 46340 &&
            FT_Sqrt32_nid_postfix(static_cast<std::int32_t>(46341u * 46341u - 1u)) == 46340 &&
            FT_Sqrt32_nid_postfix(static_cast<std::int32_t>(46341u * 46341u)) == 46341 &&
            FT_Sqrt32_nid_postfix(static_cast<std::int32_t>(65535u * 65535u - 1u)) == 65534 &&
            FT_Sqrt32_nid_postfix(static_cast<std::int32_t>(65535u * 65535u)) == 65535 && FT_Sqrt32_nid_postfix(-1) == 65535,
        "FT_Sqrt32 treats negative inputs as unsigned 32-bit values");
}

void CheckHighPow2() {
    Require(ft_highpow2_nid_postfix(0) == 0, "ft_highpow2 of 0 is 0");
    Require(ft_highpow2_nid_postfix(1) == 1 && ft_highpow2_nid_postfix(2) == 2 && ft_highpow2_nid_postfix(0x10) == 0x10, "ft_highpow2 keeps a power of two");
    Require(ft_highpow2_nid_postfix(3) == 2 && ft_highpow2_nid_postfix(6) == 4, "ft_highpow2 clears the lower bits");
    Require(ft_highpow2_nid_postfix(0x12345678) == 0x10000000, "ft_highpow2 keeps the highest set bit");
    Require(ft_highpow2_nid_postfix(0x80000001u) == 0x80000000u && ft_highpow2_nid_postfix(0xFFFFFFFFu) == 0x80000000u, "ft_highpow2 keeps bit 31");
}

}  // namespace

int main() {
    Require(ft_corner_orientation_nid_postfix(1, 0, 0, 1) == 1, "counter-clockwise 90-degree turn is +1");
    Require(ft_corner_orientation_nid_postfix(0, 1, 1, 0) == -1, "clockwise 90-degree turn is -1");
    Require(ft_corner_orientation_nid_postfix(1, 2, 2, 4) == 0, "collinear parallel vectors are 0");
    Require(ft_corner_orientation_nid_postfix(1, 0, -1, 0) == 0, "collinear opposite vectors are 0");
    Require(ft_corner_orientation_nid_postfix(0, 0, 5, 5) == 0, "zero input vector is 0");
    Require(ft_corner_orientation_nid_postfix(5, 5, 0, 0) == 0, "zero output vector is 0");
    Require(ft_corner_orientation_nid_postfix(1000000000LL, 0, 0, 1000000000LL) == 1, "large counter-clockwise coordinates");
    Require(ft_corner_orientation_nid_postfix(0, 1000000000LL, 1000000000LL, 0) == -1, "large clockwise coordinates");

    Require(ft_corner_is_flat_nid_postfix(100, 0, 100, 0) == 1, "parallel vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(1000, 1, 1000, -1) == 1, "nearly parallel vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(100, 0, 0, 100) == 0, "perpendicular vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(100, 0, -100, 0) == 0, "reversing vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(100000, 0, 1, 10) == 1, "dominant in vector is flat");
    Require(ft_corner_is_flat_nid_postfix(1, 10, 100000, 0) == 1, "dominant out vector is flat");
    Require(ft_corner_is_flat_nid_postfix(0, 0, 0, 0) == 0, "zero vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(-100, -50, -200, -100) == 1, "negative coordinates flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 0, 1, 0) == 1, "INT64_MAX boundary does not overflow");
    Require(ft_corner_is_flat_nid_postfix(INT64_MIN, 0, -1, 0) == 1, "INT64_MIN boundary does not overflow");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, INT64_MAX, 1, 1) == 1, "large positive diagonal boundary");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, INT64_MAX, INT64_MAX, INT64_MAX) == 1, "identical full-range vectors are flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 0, 0, INT64_MAX) == 0, "full-range perpendicular vectors are not flat");
    Require(ft_corner_is_flat_nid_postfix(INT64_MAX, 1, 1, INT64_MAX) == 0, "full-range near-perpendicular vectors are not flat");

    CheckMulDivNoRound();
    CheckMatrixMultiplyScaled();
    CheckVectorTransformScaled();
    CheckSqrtFixed();
    CheckSqrt32();
    CheckHighPow2();

    std::puts("Guest FreeType corner tests passed");
    return 0;
}
