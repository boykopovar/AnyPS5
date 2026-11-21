#include "prx/libSceLibcInternal/include/MathInternals.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal quad: %s\n", message);
    std::abort();
}

struct DoubleCase {
    std::uint64_t input;
    int retcode;
    unsigned int quadrant;
    std::uint64_t expected;
};

struct DoublePhaseCase {
    std::uint64_t input;
    std::uint64_t phase;
    unsigned int quadrant;
    std::uint64_t expected;
};

struct FloatCase {
    std::uint32_t input;
    int retcode;
    unsigned int quadrant;
    std::uint32_t expected;
};

struct FloatPhaseCase {
    std::uint32_t input;
    std::uint32_t phase;
    unsigned int quadrant;
    std::uint32_t expected;
};

struct ExtendedCase {
    std::uint64_t inputMantissa;
    std::uint16_t inputExponent;
    int retcode;
    unsigned int quadrant;
    std::uint64_t expectedMantissa;
    std::uint16_t expectedExponent;
};

struct ExtendedPhaseCase {
    std::uint64_t inputMantissa;
    std::uint16_t inputExponent;
    std::uint64_t phaseMantissa;
    std::uint16_t phaseExponent;
    unsigned int quadrant;
    std::uint64_t expectedMantissa;
    std::uint16_t expectedExponent;
};

long double Extended(std::uint64_t mantissa, std::uint16_t exponent) {
    long double value = 0;
    std::memcpy(&value, &mantissa, sizeof(mantissa));
    std::memcpy(reinterpret_cast<unsigned char*>(&value) + sizeof(mantissa), &exponent, sizeof(exponent));
    return value;
}

bool SameExtended(long double value, std::uint64_t mantissa, std::uint16_t exponent) {
    std::uint64_t actualMantissa = 0;
    std::uint16_t actualExponent = 0;
    std::memcpy(&actualMantissa, &value, sizeof(actualMantissa));
    std::memcpy(&actualExponent, reinterpret_cast<const unsigned char*>(&value) + sizeof(actualMantissa), sizeof(actualExponent));
    return actualMantissa == mantissa && actualExponent == exponent;
}

void TestDouble() {
    static constexpr DoubleCase Cases[] = {
        {0x8000000000000000ull, 0, 0x00000000u, 0x8000000000000000ull},
        {0x3fe921fb54442d18ull, 0, 0x00000001u, 0xbfe921fb54442d19ull},
        {0xbfe921fb54442d18ull, 0, 0x00000003u, 0x3fe921fb54442d19ull},
        {0x410fecc4017fa6ecull, 0, 0x00000002u, 0x3fd59305093f375bull},
        {0x4553789898dd65d8ull, 0, 0x00000003u, 0x3fc26c3974e11d0bull},
        {0x7fa33e5e599f2f4aull, 0, 0x00000000u, 0xbfe89f5dd15c2c9dull},
        {0x7f83dcb3445d26beull, 0, 0x00000001u, 0xbf83d92bcd68baa9ull},
        {0x3fe0000000000000ull, 0, 0x00000000u, 0x3fe0000000000000ull},
        {0x3ff0000000000000ull, 0, 0x00000001u, 0xbfe243f6a8885a31ull},
        {0xbff0000000000000ull, 0, 0x00000003u, 0x3fe243f6a8885a31ull},
        {0x4008000000000000ull, 0, 0x00000002u, 0xbfc21fb54442d184ull},
        {0x412e848000000000ull, 0, 0x00000000u, 0xbfd6e254d0f6b398ull},
        {0x41945f306dc9c883ull, 0, 0x00000000u, 0x3fdad7034e4045faull},
        {0xc1945f306dc9c883ull, 0, 0x00000000u, 0xbfdad7034e4045faull},
        {0x41945f306dc9b424ull, 0, 0x00000000u, 0x3fdad5bd5e4045faull},
        {0x419329fe7c6815e6ull, 0, 0x00000003u, 0xbfe921fb563e3385ull},
        {0xc177bad6cdce02ccull, 0, 0x00000002u, 0x3fe921fb5531d235ull},
        {0x4480f0cf064dd592ull, 0, 0x00000003u, 0x3fe19eab99633cd8ull},
        {0x7fe0000000000000ull, 0, 0x00000002u, 0xbfe3242cd2724cceull},
        {0x7506ac5b262ca1ffull, 0, 0x00000001u, 0x3c214ae72e6ba22full},
        {0x3ff0000000000000ull, 2, 0x00000001u, 0xbfe243f6a8885a31ull},
        {0x3ffc000000000000ull, 1, 0x00000003u, 0x3fe921fb54442d18ull},
        {0x4014000000000000ull, 1, 0x00000002u, 0x0000000000000000ull},
        {0xc014000000000000ull, 1, 0xfffffffeu, 0x0000000000000000ull},
        {0xbffe666666666666ull, 1, 0xfffffffcu, 0x3fd41b2f769cf0e5ull},
        {0x43b0000000000000ull, 1, 0x00000000u, 0x0000000000000000ull},
        {0x3fd3333333333333ull, 1, 0x00000001u, 0xbfe41b2f769cf0e0ull},
        {0x0000000000000001ull, 1, 0x00000000u, 0x0000000000000003ull},
        {0xbfd0000000000000ull, 1, 0x00000000u, 0xbfe921fb54442d18ull},
        {0x3ffc000000000000ull, 3, 0x00000003u, 0x3fe921fb54442d18ull},
        {0x3ffc000000000000ull, -1, 0x00000003u, 0x3fe921fb54442d18ull},
    };
    for (const DoubleCase& item : Cases) {
        double x = std::bit_cast<double>(item.input);
        Require(_Quad_nid_postfix(&x, item.retcode) == item.quadrant, "_Quad quadrant");
        Require(std::bit_cast<std::uint64_t>(x) == item.expected, "_Quad reduced argument bits");
    }
    static constexpr DoublePhaseCase PhaseCases[] = {
        {0x3ff0000000000000ull, 0x3fd0000000000000ull, 0x00000001u, 0x3fcb7812aeef4b9cull},
        {0x3ff0000000000000ull, 0x3ffc000000000000ull, 0x00000004u, 0x3fcb7812aeef4b9cull},
        {0x3ff0000000000000ull, 0xbfe8000000000000ull, 0x00000003u, 0x3fcb7812aeef4b9cull},
        {0x3ff0000000000000ull, 0x43d0000000000000ull, 0x00000001u, 0xbfe243f6a8885a31ull},
        {0x3ff0000000000000ull, 0x43e0000000000000ull, 0x00000001u, 0xbfe243f6a8885a31ull},
        {0x3ff0000000000000ull, 0xbfe0000000000000ull, 0x00000004u, 0xbfe243f6a8885a31ull},
        {0x4008000000000000ull, 0x3fd0000000000000ull, 0x00000002u, 0x3fe49a0e033378b7ull},
        {0x3fb999999999999aull, 0xbfd0000000000000ull, 0x00000000u, 0xbfe5eec82110f9e5ull},
    };
    for (const DoublePhaseCase& item : PhaseCases) {
        double x = std::bit_cast<double>(item.input);
        Require(_Quadph_nid_postfix(&x, std::bit_cast<double>(item.phase)) == item.quadrant, "_Quadph quadrant");
        Require(std::bit_cast<std::uint64_t>(x) == item.expected, "_Quadph reduced argument bits");
    }
}

void TestFloat() {
    static constexpr FloatCase Cases[] = {
        {0x80000000u, 0, 0x00000000u, 0x80000000u},
        {0x3f490fdbu, 0, 0x00000001u, 0xbf490fdau},
        {0xbf490fdbu, 0, 0x00000003u, 0x3f490fdau},
        {0x7d606a63u, 0, 0x00000000u, 0x3ab398edu},
        {0x3f000000u, 0, 0x00000000u, 0x3f000000u},
        {0x3f800000u, 0, 0x00000001u, 0xbf121fb5u},
        {0xbf800000u, 0, 0x00000003u, 0x3f121fb5u},
        {0x40400000u, 0, 0x00000002u, 0xbe10fdaau},
        {0x447a0000u, 0, 0x00000001u, 0xbf18e60bu},
        {0x4522f983u, 0, 0x00000000u, 0x3d94a4cbu},
        {0xc522f983u, 0, 0x00000000u, 0xbd94a4cbu},
        {0x4522f97fu, 0, 0x00000000u, 0x3d92a4cbu},
        {0x44b1cb65u, 0, 0x00000001u, 0x3f491032u},
        {0xc4671a79u, 0, 0x00000003u, 0x3f49100eu},
        {0x71800000u, 0, 0x00000003u, 0x3f02da95u},
        {0x4b7fffffu, 0, 0x00000003u, 0xbea576dfu},
        {0x3f800000u, 2, 0x00000001u, 0xbf121fb5u},
        {0x3fe00000u, 1, 0x00000003u, 0x3f490fdbu},
        {0x40a00000u, 1, 0x00000002u, 0x00000000u},
        {0xc0a00000u, 1, 0xfffffffeu, 0x00000000u},
        {0xbff33333u, 1, 0xfffffffcu, 0x3ea0d97fu},
        {0x5d800000u, 1, 0x00000000u, 0x00000000u},
        {0x3e99999au, 1, 0x00000001u, 0xbf20d97bu},
        {0x00000001u, 1, 0x00000000u, 0x00000003u},
        {0xbe800000u, 1, 0x00000000u, 0xbf490fdbu},
        {0x3fe00000u, 3, 0x00000003u, 0x3f490fdbu},
        {0x3fe00000u, -1, 0x00000003u, 0x3f490fdbu},
    };
    for (const FloatCase& item : Cases) {
        float x = std::bit_cast<float>(item.input);
        Require(_FQuad_nid_postfix(&x, item.retcode) == item.quadrant, "_FQuad quadrant");
        Require(std::bit_cast<std::uint32_t>(x) == item.expected, "_FQuad reduced argument bits");
    }
    static constexpr FloatPhaseCase PhaseCases[] = {
        {0x3f800000u, 0x3e800000u, 0x00000001u, 0x3e5bc098u},
        {0x3f800000u, 0x3fe00000u, 0x00000004u, 0x3e5bc098u},
        {0x3f800000u, 0xbf400000u, 0x00000003u, 0x3e5bc098u},
        {0x3f800000u, 0x5e800000u, 0x00000001u, 0xbf121fb5u},
        {0x3f800000u, 0x5f000000u, 0x00000001u, 0xbf121fb5u},
        {0x3f800000u, 0xbf000000u, 0x00000004u, 0xbf121fb5u},
        {0x40400000u, 0x3e800000u, 0x00000002u, 0x3f24d070u},
        {0x3dcccccdu, 0xbe800000u, 0x00000000u, 0xbf2f7641u},
    };
    for (const FloatPhaseCase& item : PhaseCases) {
        float x = std::bit_cast<float>(item.input);
        Require(_FQuadph_nid_postfix(&x, std::bit_cast<float>(item.phase)) == item.quadrant, "_FQuadph quadrant");
        Require(std::bit_cast<std::uint32_t>(x) == item.expected, "_FQuadph reduced argument bits");
    }
}

void TestExtended() {
    static constexpr ExtendedCase Cases[] = {
        {0x0000000000000000u, 0x8000u, 0, 0x00000000u, 0x0000000000000000u, 0x8000u},
        {0xc90fdaa22168c235u, 0x3ffeu, 0, 0x00000001u, 0xc90fdaa22168c235u, 0xbffeu},
        {0xc90fdaa22168c235u, 0xbffeu, 0, 0x00000003u, 0xc90fdaa22168c235u, 0x3ffeu},
        {0xb93ccd30476a8b45u, 0x7fc6u, 0, 0x00000002u, 0xb0da7704b931914au, 0x3ffcu},
        {0x8000000000000000u, 0x3ffeu, 0, 0x00000000u, 0x8000000000000000u, 0x3ffeu},
        {0x8000000000000000u, 0x3fffu, 0, 0x00000001u, 0x921fb54442d1846au, 0xbffeu},
        {0x8000000000000000u, 0xbfffu, 0, 0x00000003u, 0x921fb54442d1846au, 0x3ffeu},
        {0xc000000000000000u, 0x4000u, 0, 0x00000002u, 0x90fdaa22168c234cu, 0xbffcu},
        {0xf424000000000000u, 0x4012u, 0, 0x00000000u, 0xb712a687b59cc3adu, 0xbffdu},
        {0xa2f9836e4e44152au, 0x401eu, 0, 0x00000001u, 0xb79bbb1c366093e8u, 0xbffeu},
        {0xa2f9836e4e44152au, 0xc01eu, 0, 0x00000003u, 0xb79bbb1c366093e8u, 0x3ffeu},
        {0xa2f9836e4da11ba7u, 0x401eu, 0, 0x00000001u, 0xb83eb49f366093e8u, 0xbffeu},
        {0x87b1d0d85831c953u, 0x401eu, 0, 0x00000000u, 0xc90fdaa236396e79u, 0xbffeu},
        {0xe3916ff77e4806fau, 0xc01cu, 0, 0x00000002u, 0xc90fdaa252aeba7du, 0x3ffeu},
        {0x878678326eac9000u, 0x4048u, 0, 0x00000003u, 0x8cf55ccb19e6bf6du, 0x3ffeu},
        {0x8000000000000000u, 0x7e7fu, 0, 0x00000000u, 0xc63b481a5aa19604u, 0x3ffeu},
        {0xb562d931650ff800u, 0x4350u, 0, 0x00000001u, 0x8a5739735d1177a3u, 0x3fc2u},
        {0x8000000000000000u, 0x3fffu, 2, 0x00000001u, 0x921fb54442d1846au, 0xbffeu},
        {0xe000000000000000u, 0x3fffu, 1, 0x00000003u, 0xc90fdaa22168c235u, 0x3ffeu},
        {0xa000000000000000u, 0x4001u, 1, 0x00000002u, 0x0000000000000000u, 0x0000u},
        {0xa000000000000000u, 0xc001u, 1, 0xfffffffeu, 0x0000000000000000u, 0x0000u},
        {0xf333333333333333u, 0xbfffu, 1, 0xfffffffcu, 0xa0d97bb4e78701c7u, 0x3ffdu},
        {0x8000000000000000u, 0x403bu, 1, 0x00000000u, 0x0000000000000000u, 0x0000u},
        {0x999999999999999au, 0x3ffdu, 1, 0x00000001u, 0xa0d97bb4e78701c3u, 0xbffeu},
        {0x0000000000000001u, 0x0000u, 1, 0x00000000u, 0x0000000000000003u, 0x0000u},
        {0x8000000000000000u, 0xbffdu, 1, 0x00000000u, 0xc90fdaa22168c235u, 0xbffeu},
        {0xe000000000000000u, 0x3fffu, 3, 0x00000003u, 0xc90fdaa22168c235u, 0x3ffeu},
        {0xe000000000000000u, 0x3fffu, -1, 0x00000003u, 0xc90fdaa22168c235u, 0x3ffeu},
    };
    for (const ExtendedCase& item : Cases) {
        long double x = Extended(item.inputMantissa, item.inputExponent);
        Require(_LQuad_nid_postfix(&x, item.retcode) == item.quadrant, "_LQuad quadrant");
        Require(SameExtended(x, item.expectedMantissa, item.expectedExponent), "_LQuad reduced argument bits");
    }
    static constexpr ExtendedPhaseCase PhaseCases[] = {
        {0x8000000000000000u, 0x3fffu, 0x8000000000000000u, 0x3ffdu, 0x00000001u, 0xdbc095777a5cf72cu, 0x3ffcu},
        {0x8000000000000000u, 0x3fffu, 0xe000000000000000u, 0x3fffu, 0x00000004u, 0xdbc095777a5cf72cu, 0x3ffcu},
        {0x8000000000000000u, 0x3fffu, 0xc000000000000000u, 0xbffeu, 0x00000003u, 0xdbc095777a5cf72cu, 0x3ffcu},
        {0x8000000000000000u, 0x3fffu, 0x8000000000000000u, 0x403du, 0x00000001u, 0x921fb54442d1846au, 0xbffeu},
        {0x8000000000000000u, 0x3fffu, 0x8000000000000001u, 0x403du, 0x00000002u, 0x921fb54442d1846au, 0xbffeu},
        {0x8000000000000000u, 0x3fffu, 0x8000000000000000u, 0x403eu, 0x00000001u, 0x921fb54442d1846au, 0xbffeu},
        {0x8000000000000000u, 0x3fffu, 0x8000000000000000u, 0xbffeu, 0x00000004u, 0x921fb54442d1846au, 0xbffeu},
        {0xc000000000000000u, 0x4000u, 0x8000000000000000u, 0x3ffdu, 0x00000002u, 0xa4d070199bc5b962u, 0x3ffeu},
        {0xcccccccccccccccdu, 0x3ffbu, 0x8000000000000000u, 0xbffdu, 0x00000000u, 0xaf76410887cf289bu, 0xbffeu},
    };
    for (const ExtendedPhaseCase& item : PhaseCases) {
        long double x = Extended(item.inputMantissa, item.inputExponent);
        Require(_LQuadph_nid_postfix(&x, Extended(item.phaseMantissa, item.phaseExponent)) == item.quadrant, "_LQuadph quadrant");
        Require(SameExtended(x, item.expectedMantissa, item.expectedExponent), "_LQuadph reduced argument bits");
    }
}

template <typename Exception, typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

template <typename T, typename Quad, typename Quadph>
void TestRejected(Quad quad, Quadph quadph, const char* name) {
    for (const T bad : {std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity()}) {
        for (const int retcode : {0, 1}) {
            T x = bad;
            Require(Throws<std::runtime_error>([&] { quad(&x, retcode); }), name);
        }
        T x = bad;
        Require(Throws<std::runtime_error>([&] { quadph(&x, T(0.25)); }), name);
        T finite = T(1);
        Require(Throws<std::runtime_error>([&] { quadph(&finite, bad); }), name);
        Require(finite == T(1), name);
    }
    Require(Throws<std::invalid_argument>([&] { quad(nullptr, 0); }), name);
    Require(Throws<std::invalid_argument>([&] { quadph(nullptr, T(0)); }), name);
}

}

int main() {
    TestDouble();
    TestFloat();
    TestExtended();
    TestRejected<double>(_Quad_nid_postfix, _Quadph_nid_postfix, "_Quad and _Quadph reject a NaN, an infinity and a null pointer");
    TestRejected<float>(_FQuad_nid_postfix, _FQuadph_nid_postfix, "_FQuad and _FQuadph reject a NaN, an infinity and a null pointer");
    TestRejected<long double>(_LQuad_nid_postfix, _LQuadph_nid_postfix, "_LQuad and _LQuadph reject a NaN, an infinity and a null pointer");
    return 0;
}
