#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "AffineTransform construct and inverse check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool AlmostEqual(double a, double b) {
    return std::abs(a - b) <= 1e-12;
}

static bool Equals(const WebCore::AffineTransform& t, const std::array<double, 6>& expected) {
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (!AlmostEqual(t.m_transform[i], expected[i])) return false;
    }
    return true;
}

static WebCore::AffineTransform Garbage() {
    WebCore::AffineTransform t;
    std::memset(&t, 0xA5, sizeof(t));
    return t;
}

static WebCore::AffineTransform Make(double a, double b, double c, double d, double e, double f) {
    WebCore::AffineTransform t = Garbage();
    _ZN7WebCore15AffineTransformC1Edddddd(&t, a, b, c, d, e, f);
    return t;
}

static void TestConstructors() {
    WebCore::AffineTransform complete = Garbage();
    _ZN7WebCore15AffineTransformC1Ev(&complete);
    Require(Equals(complete, {1.0, 0.0, 0.0, 1.0, 0.0, 0.0}));

    WebCore::AffineTransform base = Garbage();
    _ZN7WebCore15AffineTransformC2Ev(&base);
    Require(Equals(base, {1.0, 0.0, 0.0, 1.0, 0.0, 0.0}));

    WebCore::AffineTransform values = Make(2.0, -3.0, 0.5, 4.0, 10.0, -20.0);
    Require(Equals(values, {2.0, -3.0, 0.5, 4.0, 10.0, -20.0}));

    WebCore::AffineTransform baseValues = Garbage();
    _ZN7WebCore15AffineTransformC2Edddddd(&baseValues, -1.0, 0.25, 7.0, -8.0, 0.0, 3.5);
    Require(Equals(baseValues, {-1.0, 0.25, 7.0, -8.0, 0.0, 3.5}));
}

static void TestInverseGeneral() {
    const auto t = Make(2.0, 1.0, -1.0, 3.0, 5.0, -7.0);
    const auto inverse = _ZNK7WebCore15AffineTransform7inverseEv(&t);
    Require(inverse.m_engaged);
    Require(Equals(inverse.m_value, {3.0 / 7.0, -1.0 / 7.0, 1.0 / 7.0, 2.0 / 7.0, -8.0 / 7.0, 19.0 / 7.0}));

    auto product = t;
    _ZN7WebCore15AffineTransform8multiplyERKS0_(&product, inverse.m_value);
    Require(Equals(product, {1.0, 0.0, 0.0, 1.0, 0.0, 0.0}));
}

static void TestInverseTranslation() {
    const auto t = Make(1.0, 0.0, 0.0, 1.0, 12.5, -4.0);
    const auto inverse = _ZNK7WebCore15AffineTransform7inverseEv(&t);
    Require(inverse.m_engaged);
    Require(Equals(inverse.m_value, {1.0, 0.0, 0.0, 1.0, -12.5, 4.0}));

    WebCore::AffineTransform identity;
    _ZN7WebCore15AffineTransformC1Ev(&identity);
    const auto identityInverse = _ZNK7WebCore15AffineTransform7inverseEv(&identity);
    Require(identityInverse.m_engaged);
    Require(Equals(identityInverse.m_value, {1.0, 0.0, 0.0, 1.0, 0.0, 0.0}));
}

static void TestInverseSingular() {
    const auto singular = Make(2.0, 4.0, 1.0, 2.0, 3.0, 3.0);
    Require(!_ZNK7WebCore15AffineTransform7inverseEv(&singular).m_engaged);

    const auto zero = Make(0.0, 0.0, 0.0, 0.0, 1.0, 1.0);
    Require(!_ZNK7WebCore15AffineTransform7inverseEv(&zero).m_engaged);

    const double infinity = std::numeric_limits<double>::infinity();
    const auto infinite = Make(infinity, 0.0, 0.0, 1.0, 0.0, 0.0);
    Require(!_ZNK7WebCore15AffineTransform7inverseEv(&infinite).m_engaged);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto notANumber = Make(1.0, nan, 0.0, 1.0, 0.0, 0.0);
    Require(!_ZNK7WebCore15AffineTransform7inverseEv(&notANumber).m_engaged);

    const double huge = std::numeric_limits<double>::max();
    const auto overflow = Make(huge, 0.0, 0.0, huge, 0.0, 0.0);
    Require(!_ZNK7WebCore15AffineTransform7inverseEv(&overflow).m_engaged);
}

static void TestInverseRawBytes() {
    alignas(WebCore::OptionalAffineTransform) unsigned char raw[sizeof(WebCore::OptionalAffineTransform)];
    std::memset(raw, 0xA5, sizeof(raw));
    const auto t = Make(2.0, 1.0, -1.0, 3.0, 5.0, -7.0);
    ::new (static_cast<void*>(raw)) WebCore::OptionalAffineTransform(_ZNK7WebCore15AffineTransform7inverseEv(&t));
    Require(raw[0] == 1);
    for (std::size_t i = 1; i < 8; ++i) Require(raw[i] == 0);
    std::array<double, 6> value{};
    std::memcpy(value.data(), raw + 8, sizeof(value));
    const std::array<double, 6> expected{3.0 / 7.0, -1.0 / 7.0, 1.0 / 7.0, 2.0 / 7.0, -8.0 / 7.0, 19.0 / 7.0};
    for (std::size_t i = 0; i < value.size(); ++i) Require(AlmostEqual(value[i], expected[i]));

    std::memset(raw, 0xA5, sizeof(raw));
    const auto singular = Make(2.0, 4.0, 1.0, 2.0, 3.0, 3.0);
    ::new (static_cast<void*>(raw)) WebCore::OptionalAffineTransform(_ZNK7WebCore15AffineTransform7inverseEv(&singular));
    Require(raw[0] == 0);
    for (std::size_t i = 1; i < 8; ++i) Require(raw[i] == 0);
}

int main() {
    TestConstructors();
    TestInverseGeneral();
    TestInverseTranslation();
    TestInverseSingular();
    TestInverseRawBytes();
    return 0;
}
