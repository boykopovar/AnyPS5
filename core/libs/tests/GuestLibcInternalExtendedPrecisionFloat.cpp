#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr float Filler = 12345.0f;
using Parts = std::array<float, 4>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal extended precision float: %s\n", message);
    std::abort();
}

template<typename TCall>
bool Rejects(const TCall& call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

double Sum(const Parts& parts) {
    double sum = 0.0;
    for (const float part : parts) {
        if (part == 0.0f) break;
        sum += part;
    }
    return sum;
}

Parts Filled() {
    Parts parts;
    parts.fill(Filler);
    return parts;
}

void TestSetWhole() {
    Parts parts = Filled();
    const float value = 1.0f + std::ldexp(1.0f, -20);
    Require(_FXp_setw_nid_postfix(parts.data(), 4, value) == parts.data(), "setw returns its array");
    Require(parts[0] == 1.0f && parts[1] == std::ldexp(1.0f, -20), "setw splits the value into two 12-bit parts");
    Require(parts[2] == 0.0f && parts[3] == Filler, "setw ends two parts with one zero");
    Require(_FXp_getw_nid_postfix(parts.data(), 4) == value, "getw sums what setw stored");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 4, 3.0f);
    Require(parts[0] == 3.0f && parts[1] == 0.0f && parts[2] == 0.0f && parts[3] == Filler, "setw of a short value writes two zeros");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 4, std::numeric_limits<float>::infinity());
    Require(std::isinf(parts[0]) && parts[1] == 0.0f && parts[2] == Filler, "setw of an infinity writes one zero");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 4, 0.0f);
    Require(parts[0] == 0.0f && parts[1] == Filler, "setw of zero writes one part");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 1, value);
    Require(parts[0] == value && parts[1] == Filler, "setw into one part stores the value whole");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 0, value);
    _FXp_setw_nid_postfix(parts.data(), -1, value);
    Require(parts[0] == Filler, "setw into no parts writes nothing");
    Require(Rejects([&] { _FXp_setw_nid_postfix(nullptr, 2, 1.0f); }), "setw rejects a null array");
}

void TestAddHalf() {
    Parts parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 4, 1.0f);
    Require(_FXp_addh_nid_postfix(parts.data(), 4, std::ldexp(1.0f, -40)) == parts.data(), "addh returns its array");
    Require(parts[0] == 1.0f && parts[1] == std::ldexp(1.0f, -40) && parts[2] == 0.0f, "addh keeps a far smaller addend as its own part");
    _FXp_addh_nid_postfix(parts.data(), 4, -1.0f);
    Require(parts[0] == std::ldexp(1.0f, -40) && parts[1] == 0.0f, "addh moves the parts up when the first one cancels");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 4, std::ldexp(1.0f, 30));
    _FXp_addh_nid_postfix(parts.data(), 4, 1.0f);
    _FXp_addh_nid_postfix(parts.data(), 4, std::ldexp(1.0f, -20));
    Require(Sum(parts) == std::ldexp(1.0, 30) + 1.0 + std::ldexp(1.0, -20), "addh keeps bits a float sum would lose");

    parts = Filled();
    parts[0] = std::numeric_limits<float>::infinity();
    _FXp_addh_nid_postfix(parts.data(), 4, -std::numeric_limits<float>::infinity());
    Require(std::isnan(parts[0]) && parts[1] == 0.0f && parts[2] == Filler, "addh of opposite infinities stores a NaN and a zero");

    parts = Filled();
    parts[0] = 5.0f;
    _FXp_addh_nid_postfix(parts.data(), 4, std::numeric_limits<float>::infinity());
    Require(std::isinf(parts[0]) && parts[1] == Filler, "addh of an infinity to a finite value stores it in the first part");

    parts = Filled();
    _FXp_addh_nid_postfix(parts.data(), 4, 0.0f);
    Require(parts[0] == Filler, "addh of zero changes nothing");

    Require(Rejects([&] { _FXp_addh_nid_postfix(nullptr, 1, 1.0f); }), "addh rejects a null array");
    Require(Rejects([&] { _FXp_addh_nid_postfix(parts.data(), -1, 1.0f); }), "addh rejects a negative count");
}

void TestMultiplyHalfAndInteger() {
    Parts parts = Filled();
    const float left = 1.0f + std::ldexp(1.0f, -20);
    const float right = 1.0f + std::ldexp(1.0f, -11);
    _FXp_setw_nid_postfix(parts.data(), 4, left);
    Require(_FXp_mulh_nid_postfix(parts.data(), 4, right) == parts.data(), "mulh returns its array");
    Require(Sum(parts) == static_cast<double>(left) * right, "mulh keeps the exact product of a 12-bit factor");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 4, 3.0f);
    _FXp_mulh_nid_postfix(parts.data(), 4, 0.0f);
    Require(parts[0] == 0.0f, "mulh by zero stores zero");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 4, 3.0f);
    _FXp_mulh_nid_postfix(parts.data(), 4, std::numeric_limits<float>::infinity());
    Require(std::isinf(parts[0]) && parts[1] == 0.0f, "mulh by an infinity stores it and a zero");
    Require(Rejects([&] { _FXp_mulh_nid_postfix(nullptr, 1, 1.0f); }), "mulh rejects a null array");

    parts = Filled();
    Require(_FXp_setn_nid_postfix(parts.data(), 4, 123456789) == parts.data(), "setn returns its array");
    Require(Sum(parts) == 123456789.0, "setn loads an integer wider than a float");
    parts = Filled();
    _FXp_setn_nid_postfix(parts.data(), 4, -123456789);
    Require(Sum(parts) == -123456789.0, "setn loads a negative integer");
    Require(Rejects([&] { _FXp_setn_nid_postfix(nullptr, 1, 1); }), "setn rejects a null array");
}

void TestMoveAddSubtractScale() {
    Parts source = Filled();
    Parts parts = Filled();
    _FXp_setw_nid_postfix(source.data(), 4, 1.0f);
    _FXp_addh_nid_postfix(source.data(), 4, std::ldexp(1.0f, -40));
    Require(_FXp_movx_nid_postfix(parts.data(), 4, source.data()) == parts.data(), "movx returns its array");
    Require(parts == source, "movx copies every part");
    Require(_FXp_addx_nid_postfix(parts.data(), 4, source.data(), 4) == parts.data(), "addx returns its array");
    Require(Sum(parts) == 2.0 + std::ldexp(1.0, -39), "addx adds every part");
    Require(_FXp_subx_nid_postfix(parts.data(), 4, source.data(), 4) == parts.data(), "subx returns its array");
    Require(Sum(parts) == 1.0 + std::ldexp(1.0, -40), "subx subtracts every part");
    Require(_FXp_ldexpx_nid_postfix(parts.data(), 4, -3) == parts.data(), "ldexpx returns its array");
    Require(Sum(parts) == 0.125 + std::ldexp(1.0, -43), "ldexpx scales every part");

    Require(Rejects([&] { _FXp_movx_nid_postfix(parts.data(), 4, nullptr); }), "movx rejects a null source");
    Require(Rejects([&] { _FXp_movx_nid_postfix(parts.data(), -1, source.data()); }), "movx rejects a negative count");
    Require(Rejects([&] { _FXp_addx_nid_postfix(parts.data(), 4, nullptr, 4); }), "addx rejects a null addend");
    Require(Rejects([&] { _FXp_subx_nid_postfix(parts.data(), 4, source.data(), -1); }), "subx rejects a negative count");
    Require(Rejects([&] { _FXp_ldexpx_nid_postfix(nullptr, 1, 1); }), "ldexpx rejects a null array");
}

void TestMultiplyExtendedAndInvert() {
    Parts parts = Filled();
    Parts factor = Filled();
    std::array<float, 8> productScratch;
    std::array<float, 16> scratch;
    productScratch.fill(Filler);
    scratch.fill(Filler);
    _FXp_setw_nid_postfix(parts.data(), 4, 1.0f);
    _FXp_addh_nid_postfix(parts.data(), 4, std::ldexp(1.0f, -30));
    factor = parts;
    Require(_FXp_mulx_nid_postfix(parts.data(), 4, factor.data(), 4, productScratch.data()) == parts.data(), "mulx returns its array");
    Require(Sum(parts) == 1.0 + std::ldexp(1.0, -29), "mulx keeps the two largest terms");
    Require(parts[2] == std::ldexp(1.0f, -60), "mulx keeps the smallest term in its own part");

    parts = Filled();
    factor = Filled();
    productScratch.fill(Filler);
    _FXp_setw_nid_postfix(parts.data(), 4, 3.0f);
    _FXp_setw_nid_postfix(factor.data(), 4, 5.0f);
    _FXp_mulx_nid_postfix(parts.data(), 4, factor.data(), 4, productScratch.data());
    Require(Sum(parts) == 15.0, "mulx by a one-part factor multiplies");
    Require(productScratch[0] == Filler, "mulx by a one-part factor does not use the scratch");

    const float one[] = {2.0f};
    Require(Rejects([&] { _FXp_mulx_nid_postfix(parts.data(), 4, one, 1, productScratch.data()); }), "mulx rejects one nonzero factor part");
    Require(Rejects([&] { _FXp_mulx_nid_postfix(parts.data(), 4, nullptr, 4, productScratch.data()); }), "mulx rejects a null factor");
    factor = Filled();
    _FXp_setw_nid_postfix(factor.data(), 4, 1.0f + std::ldexp(1.0f, -20));
    Require(Rejects([&] { _FXp_mulx_nid_postfix(parts.data(), 4, factor.data(), 4, nullptr); }), "mulx rejects a null scratch it would use");

    parts = Filled();
    _FXp_setw_nid_postfix(parts.data(), 4, 3.0f);
    const Parts three = parts;
    Require(_FXp_invx_nid_postfix(parts.data(), 4, scratch.data()) == parts.data(), "invx returns its array");
    Require(parts[0] != 0.0f && parts[1] != 0.0f, "invx of 3 needs more than one part");
    _FXp_mulx_nid_postfix(parts.data(), 4, three.data(), 4, productScratch.data());
    Require(std::fabs(Sum(parts) - 1.0) < std::ldexp(1.0, -30), "invx of 3 times 3 is 1 beyond float precision");

    parts = Filled();
    parts[0] = 0.0f;
    _FXp_invx_nid_postfix(parts.data(), 4, scratch.data());
    Require(std::isinf(parts[0]) && parts[0] > 0.0f && parts[1] == Filler, "invx of zero is infinity");
    parts[0] = -std::numeric_limits<float>::infinity();
    _FXp_invx_nid_postfix(parts.data(), 4, scratch.data());
    Require(parts[0] == 0.0f, "invx of an infinity is zero");

    parts[0] = 3.0f;
    Require(Rejects([&] { _FXp_invx_nid_postfix(parts.data(), 4, nullptr); }), "invx rejects a null scratch it would use");
    Require(Rejects([&] { _FXp_invx_nid_postfix(nullptr, 4, scratch.data()); }), "invx rejects a null array");
}

}

int main() {
    TestSetWhole();
    TestAddHalf();
    TestMultiplyHalfAndInteger();
    TestMoveAddSubtractScale();
    TestMultiplyExtendedAndInvert();
    return 0;
}
