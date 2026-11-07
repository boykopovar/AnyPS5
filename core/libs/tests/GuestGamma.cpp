#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

extern int signgam_nid_postfix;

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "gamma check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

static bool Near(double actual, double expected, double tolerance) { return std::abs(actual - expected) <= tolerance; }

static int SignOf(double value) {
    int sign = 7;
    static_cast<void>(lgamma_r_nid_postfix(value, &sign));
    return sign;
}

static int SignOfFloat(float value) {
    int sign = 7;
    static_cast<void>(lgammaf_r_nid_postfix(value, &sign));
    return sign;
}

static void Values() {
    Require(lgamma_r_nid_postfix(1.0, nullptr) == 0.0);
    Require(lgamma_r_nid_postfix(2.0, nullptr) == 0.0);
    Require(Near(lgamma_r_nid_postfix(0.5, nullptr), 0.5723649429247001, 1e-15));
    Require(Near(lgamma_r_nid_postfix(10.0, nullptr), 12.801827480081469, 1e-13));
    Require(Near(lgamma_r_nid_postfix(-0.5, nullptr), 1.2655121234846454, 1e-15));
    Require(std::isinf(lgamma_r_nid_postfix(0.0, nullptr)));
}

static void FloatValues() {
    Require(lgammaf_r_nid_postfix(1.0f, nullptr) == 0.0f);
    Require(lgammaf_r_nid_postfix(2.0f, nullptr) == 0.0f);
    Require(Near(lgammaf_r_nid_postfix(5.0f, nullptr), 3.1780538303479458, 1e-6));
    Require(Near(lgammaf_r_nid_postfix(0.5f, nullptr), 0.5723649268155060, 1e-6));
    Require(Near(lgammaf_r_nid_postfix(1.0e-30f, nullptr), 69.07755278982137, 1e-4));
    Require(std::isinf(lgammaf_r_nid_postfix(0.0f, nullptr)));
    Require(std::isinf(lgammaf_r_nid_postfix(-0.0f, nullptr)));
}

static void Signs() {
    Require(SignOf(0.5) == 1 && SignOf(10.0) == 1 && SignOf(0.0) == 1 && SignOf(-0.0) == -1);
    Require(SignOf(-0.5) == -1 && SignOf(-1.5) == 1 && SignOf(-2.5) == -1);
    Require(SignOf(-1.0) == 1 && SignOf(-2.0) == 1);
    Require(SignOf(std::numeric_limits<double>::infinity()) == 1);
    Require(SignOf(-std::numeric_limits<double>::infinity()) == 1);
    Require(SignOf(std::numeric_limits<double>::quiet_NaN()) == 1);
    Require(SignOfFloat(0.5f) == 1 && SignOfFloat(0.0f) == 1 && SignOfFloat(-0.0f) == -1);
    Require(SignOfFloat(-0.5f) == -1 && SignOfFloat(-1.5f) == 1 && SignOfFloat(-2.5f) == -1);
    Require(SignOfFloat(-1.0f) == 1 && SignOfFloat(-2.0f) == 1);
}

static void Aliases() {
    const double values[] = {0.5, 1.0, 2.0, 7.25, -0.5, -1.5, -2.5, 1.0e-30, -1.0e-30, 30.0};
    for (const double x : values) {
        int throughGamma = 7;
        int throughLgamma = 7;
        const double alias = gamma_r_nid_postfix(x, &throughGamma);
        const double named = lgamma_r_nid_postfix(x, &throughLgamma);
        Require(std::bit_cast<std::uint64_t>(alias) == std::bit_cast<std::uint64_t>(named));
        Require(throughGamma == throughLgamma);
        Require(std::bit_cast<std::uint64_t>(gamma_nid_postfix(x)) == std::bit_cast<std::uint64_t>(lgamma_nid_postfix(x)));
    }
    const float narrow[] = {0.5f, 1.0f, 5.0f, -0.5f, -1.5f, 1.0e-30f, 20.0f};
    for (const float x : narrow) {
        int throughGamma = 7;
        int throughLgamma = 7;
        const float alias = gammaf_r_nid_postfix(x, &throughGamma);
        const float named = lgammaf_r_nid_postfix(x, &throughLgamma);
        Require(std::bit_cast<std::uint32_t>(alias) == std::bit_cast<std::uint32_t>(named));
        Require(throughGamma == throughLgamma);
        Require(std::bit_cast<std::uint32_t>(gammaf_nid_postfix(x)) == std::bit_cast<std::uint32_t>(lgammaf_nid_postfix(x)));
    }
    signgam_nid_postfix = 7;
    static_cast<void>(gamma_nid_postfix(-0.5));
    Require(signgam_nid_postfix == -1);
    signgam_nid_postfix = 7;
    static_cast<void>(gamma_nid_postfix(2.5));
    Require(signgam_nid_postfix == 1);
    signgam_nid_postfix = 7;
    static_cast<void>(gammaf_nid_postfix(-0.5f));
    Require(signgam_nid_postfix == -1);
    signgam_nid_postfix = 7;
    static_cast<void>(lgammaf_nid_postfix(-2.5f));
    Require(signgam_nid_postfix == -1);
}

static void WideValues() {
    Require(tgamma_nid_postfix(1.0) == 1.0 && tgamma_nid_postfix(2.0) == 1.0);
    Require(tgamma_nid_postfix(3.0) == 2.0 && tgamma_nid_postfix(5.0) == 24.0 && tgamma_nid_postfix(7.0) == 720.0);
    Require(Near(tgamma_nid_postfix(0.5), 1.7724538509055160273, 1e-15));
    Require(Near(tgamma_nid_postfix(-0.5), -3.5449077018110320546, 1e-14));
    Require(std::isinf(tgamma_nid_postfix(0.0)) && !std::signbit(tgamma_nid_postfix(0.0)));
    Require(std::isinf(tgamma_nid_postfix(-0.0)) && std::signbit(tgamma_nid_postfix(-0.0)));
}

static void WidePoles() {
    for (const double pole : {-1.0, -2.0, -3.0, -1.0e6, -std::numeric_limits<double>::infinity()}) {
        errno = 7;
        Require(std::bit_cast<std::uint64_t>(tgamma_nid_postfix(pole)) == 0xfff8000000000000ull);
        Require(errno == (std::isfinite(pole) ? EDOM : 7));
    }
    for (const double x : {0.0, -0.0, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                           40.0, 1.0e5, 1.0e300, 0.5}) {
        errno = 7;
        static_cast<void>(tgamma_nid_postfix(x));
        Require(errno == 7);
    }
}

int main() {
    Values();
    FloatValues();
    Signs();
    Aliases();
    WideValues();
    WidePoles();
    std::puts("lgamma_r, lgammaf, gamma and tgamma answer the console's family");
}
