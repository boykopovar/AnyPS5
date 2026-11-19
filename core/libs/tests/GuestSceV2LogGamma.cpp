#include "prx/libc/include/Api.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

constexpr double Infinity = std::numeric_limits<double>::infinity();
constexpr double NotANumber = std::numeric_limits<double>::quiet_NaN();

struct DoubleCase {
    double x;
    std::uint64_t bits;
    int error;
};

struct FloatCase {
    double x;
    std::uint32_t bits;
    int error;
};

struct ExtendedCase {
    double x;
    std::uint16_t top;
    std::uint64_t bits;
    int error;
};

struct PolyCase {
    double x;
    int degree;
    std::uint64_t bits;
};

struct NanPolyCase {
    int infiniteStart;
    int degree;
    std::uint64_t x;
    std::uint64_t bits;
};

const DoubleCase DoubleCases[] = {
    {0x1p+0, 0xbe205057c0000000ull, 0},
    {0x1p+1, 0xbe273ec680000000ull, 0},
    {0x1p-1, 0x3fe250d048244940ull, 0},
    {0x1.8p+1, 0x3fe62e42fd33dcb0ull, 0},
    {0x1.4p+3, 0x40299a8921824b1cull, 0},
    {0x1p-2, 0x3ff49bbd81711584ull, 0},
    {0x1.8p+0, 0xbfbeeb95ba95cca0ull, 0},
    {0x1.9p+6, 0x40767225b487426full, 0},
    {0x1.2a05f2p+33, 0x4249a43710f467c1ull, 0},
    {0x1.7e43c8800759cp+996, 0x7ed017f38e7a1ab5ull, 0},
    {0x1p-20, 0x402bb9d3ac3864daull, 0},
    {0x0p+0, 0x7ff0000000000000ull, 0},
    {-0x0p+0, 0xfff8000000000000ull, EDOM},
    {-0x1p+0, 0xfff8000000000000ull, EDOM},
    {-0x1.4p+1, 0xfff8000000000000ull, EDOM},
    {0x1.56e1fc2f8f359p-997, 0x4085963447f86054ull, 0},
    {0x0.0000000000001p-1022, 0x7ff0000000000000ull, 0},
    {Infinity, 0xfff8000000000000ull, ERANGE},
    {-Infinity, 0xfff8000000000000ull, ERANGE},
    {NotANumber, 0x7ff8000000000000ull, 0},
    {0x1.08p+5, 0x405463b59b9103f6ull, 0},
    {0x1.55p+7, 0x408600091168558bull, 0},
};

const FloatCase FloatCases[] = {
    {0x1p+0, 0x34800000u, 0},
    {0x1p+1, 0x34800000u, 0},
    {0x1p-1, 0x3f128688u, 0},
    {0x1.8p+1, 0x3f317218u, 0},
    {0x1.4p+3, 0x414cd449u, 0},
    {0x1p-2, 0x3fa4ddf0u, 0},
    {0x1.8p+0, 0xbdf75ca0u, 0},
    {0x1.9p+6, 0x43b3912du, 0},
    {0x1.2a05f2p+33, 0x524d21b8u, 0},
    {0x1.7e43c8800759cp+996, 0xffc00000u, ERANGE},
    {0x1p-20, 0x415dce9eu, 0},
    {0x0p+0, 0x7f800000u, 0},
    {-0x0p+0, 0xffc00000u, EDOM},
    {-0x1p+0, 0xffc00000u, EDOM},
    {-0x1.4p+1, 0xffc00000u, EDOM},
    {0x1.56e1fc2f8f359p-997, 0x7f800000u, 0},
    {0x0.0000000000001p-1022, 0x7f800000u, 0},
    {Infinity, 0xffc00000u, ERANGE},
    {-Infinity, 0xffc00000u, ERANGE},
    {NotANumber, 0x7fc00000u, 0},
    {0x1.dp+2, 0x40e1ab81u, 0},
    {0x1.08p+5, 0x42a31dadu, 0},
    {0x1.55p+7, 0x44300049u, 0},
};

const ExtendedCase ExtendedCases[] = {
    {0x1p+1, 0xbfe2, 0xb9f632f800000000ull, 0},
    {0x1p-1, 0x3ffe, 0x92868241224a1078ull, 0},
    {0x1.8p+1, 0x3ffe, 0xb17217e99ee5a0eaull, 0},
    {0x1.4p+3, 0x4002, 0xccd4490c1258e380ull, 0},
    {0x1p-2, 0x3fff, 0xa4ddec0b88ac2050ull, 0},
    {0x1.8p+0, 0xbffb, 0xf75cadd4ae643600ull, 0},
    {0x1.9p+6, 0x4007, 0xb3912da43a137722ull, 0},
    {0x1.7e43c8800759cp+996, 0x43ed, 0x80bf9c73d0d5a7ecull, 0},
    {0x1p-20, 0x4002, 0xddce9d61c326d027ull, 0},
    {0x0p+0, 0x7fff, 0x8000000000000000ull, 0},
    {-0x0p+0, 0x7fff, 0xc000000000000000ull, EDOM},
    {-0x1p+0, 0x7fff, 0xc000000000000000ull, EDOM},
    {-0x1.4p+1, 0x7fff, 0xc000000000000000ull, EDOM},
    {0x1.56e1fc2f8f359p-997, 0x4008, 0xacb1a23fc302a287ull, 0},
    {0x0.0000000000001p-1022, 0x4008, 0xba1c2a236a9313faull, 0},
    {Infinity, 0xffff, 0xc000000000000000ull, ERANGE},
    {-Infinity, 0x7fff, 0xc000000000000000ull, ERANGE},
    {NotANumber, 0x7fff, 0xc000000000000000ull, 0},
    {0x1.dp+2, 0x4001, 0xe1ab80d033bad5b9ull, 0},
    {0x1.08p+5, 0x4005, 0xa31dacdc881fa96bull, 0},
    {0x1.55p+7, 0x4008, 0xb000488b42ac5af7ull, 0},
};

const PolyCase PolyCases[] = {
    {0x0p+0, -1, 0x4008000000000000ull},
    {0x1p+0, -1, 0x4008000000000000ull},
    {-0x1p+0, -1, 0x4008000000000000ull},
    {0x1p-1, -1, 0x4008000000000000ull},
    {0x1.fffffffffffffp+1023, -1, 0x4008000000000000ull},
    {0x1.8p+1, -1, 0x4008000000000000ull},
    {-0x0.0000000000001p-1022, -1, 0x4008000000000000ull},
    {0x1.67e9c127b6e74p-532, -1, 0x4008000000000000ull},
    {0x0p+0, 0, 0x4008000000000000ull},
    {0x1p+0, 0, 0x4008000000000000ull},
    {-0x1p+0, 0, 0x4008000000000000ull},
    {0x1p-1, 0, 0x4008000000000000ull},
    {0x1.fffffffffffffp+1023, 0, 0x4008000000000000ull},
    {0x1.8p+1, 0, 0x4008000000000000ull},
    {-0x0.0000000000001p-1022, 0, 0x4008000000000000ull},
    {0x1.67e9c127b6e74p-532, 0, 0x4008000000000000ull},
    {0x0p+0, 1, 0xbfd0000000000000ull},
    {0x1p+0, 1, 0x4006000000000000ull},
    {-0x1p+0, 1, 0xc00a000000000000ull},
    {0x1p-1, 1, 0x3ff4000000000000ull},
    {0x1.fffffffffffffp+1023, 1, 0x7ff0000000000000ull},
    {0x1.8p+1, 1, 0x4021800000000000ull},
    {-0x0.0000000000001p-1022, 1, 0xbfd0000000000000ull},
    {0x1.67e9c127b6e74p-532, 1, 0xbfd0000000000000ull},
    {0x0p+0, 2, 0x3fc5555555555555ull},
    {0x1p+0, 2, 0x4007555555555555ull},
    {-0x1p+0, 2, 0x400b555555555555ull},
    {0x1p-1, 2, 0x3fe9555555555555ull},
    {0x1.fffffffffffffp+1023, 2, 0x7ff0000000000000ull},
    {0x1.8p+1, 2, 0x403a6aaaaaaaaaabull},
    {-0x0.0000000000001p-1022, 2, 0x3fc5555555555555ull},
    {0x1.67e9c127b6e74p-532, 2, 0x3fc5555555555555ull},
    {0x0p+0, 3, 0xfe37e43c8800759cull},
    {0x1p+0, 3, 0xfe37e43c8800759cull},
    {-0x1p+0, 3, 0xfe37e43c8800759cull},
    {0x1p-1, 3, 0xfe37e43c8800759cull},
    {0x1.fffffffffffffp+1023, 3, 0x7ff0000000000000ull},
    {0x1.8p+1, 3, 0xfe37e43c8800759cull},
    {-0x0.0000000000001p-1022, 3, 0xfe37e43c8800759cull},
    {0x1.67e9c127b6e74p-532, 3, 0xfe37e43c8800759cull},
    {0x0p+0, 4, 0x401c000000000000ull},
    {0x1p+0, 4, 0xfe37e43c8800759cull},
    {-0x1p+0, 4, 0x7e37e43c8800759cull},
    {0x1p-1, 4, 0xfe27e43c8800759cull},
    {0x1.fffffffffffffp+1023, 4, 0x7ff0000000000000ull},
    {0x1.8p+1, 4, 0xfe51eb2d66005835ull},
    {-0x0.0000000000001p-1022, 4, 0x401c000000000000ull},
    {0x1.67e9c127b6e74p-532, 4, 0xdd00cb70d24b7379ull},
    {0x0p+0, 5, 0x0000000000000001ull},
    {0x1p+0, 5, 0xfe37e43c8800759cull},
    {-0x1p+0, 5, 0xfe37e43c8800759cull},
    {0x1p-1, 5, 0xfe17e43c8800759cull},
    {0x1.fffffffffffffp+1023, 5, 0x7ff0000000000000ull},
    {0x1.8p+1, 5, 0xfe6ae0c41900844full},
    {-0x0.0000000000001p-1022, 5, 0x8000000000000006ull},
    {0x1.67e9c127b6e74p-532, 5, 0xbbc79ca10c924224ull},
    {0x0p+0, 6, 0x4004000000000000ull},
    {0x1p+0, 6, 0xfe37e43c8800759cull},
    {-0x1p+0, 6, 0x7e37e43c8800759cull},
    {0x1p-1, 6, 0xfe07e43c8800759cull},
    {0x1.fffffffffffffp+1023, 6, 0x7ff0000000000000ull},
    {0x1.8p+1, 6, 0xfe84289312c0633bull},
    {-0x0.0000000000001p-1022, 6, 0x4004000000000000ull},
    {0x1.67e9c127b6e74p-532, 6, 0x4004000000000000ull},
    {0x0p+0, 7, 0xbfc0000000000000ull},
    {0x1p+0, 7, 0xfe37e43c8800759cull},
    {-0x1p+0, 7, 0xfe37e43c8800759cull},
    {0x1p-1, 7, 0xfdf7e43c8800759cull},
    {0x1.fffffffffffffp+1023, 7, 0x7ff0000000000000ull},
    {0x1.8p+1, 7, 0xfe9e3cdc9c2094d9ull},
    {-0x0.0000000000001p-1022, 7, 0xbfc0000000000000ull},
    {0x1.67e9c127b6e74p-532, 7, 0xbfc0000000000000ull},
};

const NanPolyCase NanPolyCases[] = {
    {0, 0, 0xfff8000000000005ull, 0x7ff8000000000003ull},
    {0, 0, 0x3ff0000000000000ull, 0x7ff8000000000003ull},
    {0, 0, 0x7ff0000000000000ull, 0x7ff8000000000003ull},
    {0, 0, 0x0000000000000000ull, 0x7ff8000000000003ull},
    {0, 0, 0x7ff4000000000000ull, 0x7ff8000000000003ull},
    {0, 1, 0xfff8000000000005ull, 0xfff8000000000005ull},
    {0, 1, 0x3ff0000000000000ull, 0x7ff8000000000003ull},
    {0, 1, 0x7ff0000000000000ull, 0x7ff8000000000003ull},
    {0, 1, 0x0000000000000000ull, 0x7ff8000000000003ull},
    {0, 1, 0x7ff4000000000000ull, 0x7ffc000000000000ull},
    {0, 2, 0xfff8000000000005ull, 0xfff8000000000005ull},
    {0, 2, 0x3ff0000000000000ull, 0x7ff8000000000003ull},
    {0, 2, 0x7ff0000000000000ull, 0x7ff8000000000003ull},
    {0, 2, 0x0000000000000000ull, 0x7ff8000000000003ull},
    {0, 2, 0x7ff4000000000000ull, 0x7ffc000000000000ull},
    {0, 3, 0xfff8000000000005ull, 0xfff8000000000005ull},
    {0, 3, 0x3ff0000000000000ull, 0x7ff8000000000003ull},
    {0, 3, 0x7ff0000000000000ull, 0x7ff8000000000003ull},
    {0, 3, 0x0000000000000000ull, 0x7ff8000000000003ull},
    {0, 3, 0x7ff4000000000000ull, 0x7ffc000000000000ull},
    {1, 0, 0xfff8000000000005ull, 0x7ff0000000000000ull},
    {1, 0, 0x3ff0000000000000ull, 0x7ff0000000000000ull},
    {1, 0, 0x7ff0000000000000ull, 0x7ff0000000000000ull},
    {1, 0, 0x0000000000000000ull, 0x7ff0000000000000ull},
    {1, 0, 0x7ff4000000000000ull, 0x7ff0000000000000ull},
    {1, 1, 0xfff8000000000005ull, 0xfff8000000000005ull},
    {1, 1, 0x3ff0000000000000ull, 0x7ff0000000000000ull},
    {1, 1, 0x7ff0000000000000ull, 0x7ff0000000000000ull},
    {1, 1, 0x0000000000000000ull, 0xfff8000000000000ull},
    {1, 1, 0x7ff4000000000000ull, 0x7ffc000000000000ull},
    {1, 2, 0xfff8000000000005ull, 0xfff8000000000005ull},
    {1, 2, 0x3ff0000000000000ull, 0x7ff8000000000009ull},
    {1, 2, 0x7ff0000000000000ull, 0x7ff8000000000009ull},
    {1, 2, 0x0000000000000000ull, 0xfff8000000000000ull},
    {1, 2, 0x7ff4000000000000ull, 0x7ffc000000000000ull},
    {1, 3, 0xfff8000000000005ull, 0xfff8000000000005ull},
    {1, 3, 0x3ff0000000000000ull, 0x7ff8000000000009ull},
    {1, 3, 0x7ff0000000000000ull, 0x7ff8000000000009ull},
    {1, 3, 0x0000000000000000ull, 0xfff8000000000000ull},
    {1, 3, 0x7ff4000000000000ull, 0x7ffc000000000000ull},
};

const std::uint64_t NanPolyTable[] = {0x7ff8000000000003ull, 0x0000000000000000ull, 0x7ff0000000000009ull, 0xfff8000000000007ull};

const double PolyTable[] = {0x1.8p+1, -0x1p-2, 0x1.5555555555555p-3, -1e300, 7, 0x1p-1074, 2.5, -0.125};

void Require(bool condition, const char* label, double x) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 log gamma: %s x=%a\n", label, x);
    std::abort();
}

}

int main() {
    for (const auto& entry : DoubleCases) {
        errno = 0;
        const double result = _ZNSt8__sce_v28_XLgammaEd_nid_postfix(entry.x);
        std::uint64_t bits;
        std::memcpy(&bits, &result, sizeof(bits));
        Require(bits == entry.bits && errno == entry.error, "double", entry.x);
    }
    for (const auto& entry : FloatCases) {
        errno = 0;
        const float result = _ZNSt8__sce_v28_XLgammaEf_nid_postfix(static_cast<float>(entry.x));
        std::uint32_t bits;
        std::memcpy(&bits, &result, sizeof(bits));
        Require(bits == entry.bits && errno == entry.error, "float", entry.x);
    }
    for (const auto& entry : ExtendedCases) {
        errno = 0;
        const long double result = _ZNSt8__sce_v28_XLgammaEe_nid_postfix(static_cast<long double>(entry.x));
        std::uint64_t bits;
        std::uint16_t top;
        std::memcpy(&bits, &result, sizeof(bits));
        std::memcpy(&top, reinterpret_cast<const unsigned char*>(&result) + sizeof(bits), sizeof(top));
        Require(bits == entry.bits && top == entry.top && errno == entry.error, "long double", entry.x);
    }
    for (const auto& entry : PolyCases) {
        const double result = _ZNSt8__sce_v25_PolyEdPKdi_nid_postfix(entry.x, PolyTable, entry.degree);
        std::uint64_t bits;
        std::memcpy(&bits, &result, sizeof(bits));
        Require(bits == entry.bits, "poly", entry.x);
    }
    for (const auto& entry : NanPolyCases) {
        double table[4];
        std::memcpy(table, NanPolyTable, sizeof(table));
        if (entry.infiniteStart) table[0] = Infinity;
        double x;
        std::memcpy(&x, &entry.x, sizeof(x));
        const double result = _ZNSt8__sce_v25_PolyEdPKdi_nid_postfix(x, table, entry.degree);
        std::uint64_t bits;
        std::memcpy(&bits, &result, sizeof(bits));
        Require(bits == entry.bits, "poly NaN order", x);
    }
    return 0;
}
