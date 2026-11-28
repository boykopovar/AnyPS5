#include "prx/libc/include/Api.hpp"

#include <bit>
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
    const char* label;
    double x;
    double y;
    int scale;
    std::uint64_t bits;
    short code;
};

struct FloatCase {
    const char* label;
    double x;
    double y;
    int scale;
    std::uint32_t bits;
    short code;
};

const DoubleCase DoubleCases[] = {
    {"zero", 0x0p+0, 0x1p+0, 0, 0x3ff0000000000000ull, -1},
    {"one", 0x1p+0, 0x1p+0, 0, 0x4005bf0a8b145769ull, -1},
    {"minus one", -0x1p+0, 0x1p+0, 0, 0x3fd78b56362cef38ull, -1},
    {"half", 0x1p-1, 0x1p+0, 0, 0x3ffa61298e1e069dull, -1},
    {"ten", 0x1.4p+3, 0x1p+0, 0, 0x40d5829dcf950560ull, -1},
    {"scaled factor", 0x1p+1, 0x1.8p+1, 0, 0x40362acb8a9fa642ull, -1},
    {"negative factor", 0x1.8p+0, -0x1p+1, 0, 0xc021ed3fe64fc542ull, -1},
    {"zero factor", 0x1.4p+2, 0x0p+0, 0, 0x0000000000000000ull, 0},
    {"negative zero factor", 0x1.4p+2, -0x0p+0, 0, 0x8000000000000000ull, 0},
    {"tiny argument", 0x1p-60, 0x1p+0, 0, 0x3ff0000000000000ull, -1},
    {"tiny argument with factor", -0x1p-56, 0x1.cp+2, 0, 0x401c000000000000ull, -1},
    {"scale up", 0x1p+0, 0x1p+0, 10, 0x40a5bf0a8b145769ull, -1},
    {"scale down", 0x1p+0, 0x1p+0, -10, 0x3f65bf0a8b145769ull, -1},
    {"large", 0x1.5ep+9, 0x1p+0, 0, 0x7f0d945df4f8ec8eull, -1},
    {"overflowing scale", 0x1.5ep+9, 0x1p+0, 400, 0x7ff0000000000000ull, 1},
    {"underflowing scale", -0x1.5ep+9, 0x1p+0, -400, 0x0000000000000000ull, 0},
    {"subnormal result", -0x1.5ep+9, 0x1p+0, -300, 0x0000000000000000ull, 0},
    {"below the lower limit", -0x1.cccp+10, 0x1p+0, 0, 0x0000000000000000ull, 0},
    {"above the upper limit", 0x1.cccp+10, -0x1p+0, 0, 0xfff0000000000000ull, 1},
    {"at the upper limit", 0x1.cc8p+10, 0x1p+0, -1500, 0x7ff0000000000000ull, 1},
    {"infinity", Infinity, 0x1p+0, 0, 0x7ff0000000000000ull, 1},
    {"minus infinity", -Infinity, 0x1p+0, 0, 0x0000000000000000ull, 0},
    {"nan argument", NotANumber, 0x1p+0, 0, 0x7ff8000000000000ull, 2},
    {"nan factor", 0x1p+0, NotANumber, 0, 0x7ff8000000000000ull, 2},
    {"nan both", NotANumber, -NotANumber, 3, 0xfff8000000000000ull, 2},
};

const FloatCase FloatCases[] = {
    {"zero", 0x0p+0, 0x1p+0, 0, 0x3f800000u, -1},
    {"one", 0x1p+0, 0x1p+0, 0, 0x402df853u, -1},
    {"minus one", -0x1p+0, 0x1p+0, 0, 0x3ebc5ab3u, -1},
    {"half", 0x1p-1, 0x1p+0, 0, 0x3fd3094cu, -1},
    {"ten", 0x1.4p+3, 0x1p+0, 0, 0x46ac14eeu, -1},
    {"scaled factor", 0x1p+1, 0x1.8p+1, 0, 0x41b1565du, -1},
    {"negative factor", 0x1.8p+0, -0x1p+1, 0, 0xc10f6a00u, -1},
    {"zero factor", 0x1.4p+2, 0x0p+0, 0, 0x00000000u, 0},
    {"negative zero factor", 0x1.4p+2, -0x0p+0, 0, 0x80000000u, 0},
    {"tiny argument", 0x1p-60, 0x1p+0, 0, 0x3f800000u, -1},
    {"tiny argument with factor", -0x1p-56, 0x1.cp+2, 0, 0x40e00000u, -1},
    {"scale up", 0x1p+0, 0x1p+0, 1, 0x40adf853u, -1},
    {"scale down", 0x1p+0, 0x1p+0, -1, 0x3fadf853u, -1},
    {"large", 0x1.5ep+9, 0x1p+0, 0, 0x7f800000u, 1},
    {"overflowing scale", 0x1.5ep+9, 0x1p+0, 50, 0x7f800000u, 1},
    {"underflowing scale", -0x1.5ep+9, 0x1p+0, -50, 0x00000000u, 0},
    {"subnormal result", -0x1.5ep+9, 0x1p+0, -37, 0x00000000u, 0},
    {"below the lower limit", -0x1.cccp+10, 0x1p+0, 0, 0x00000000u, 0},
    {"above the upper limit", 0x1.cccp+10, -0x1p+0, 0, 0xff800000u, 1},
    {"at the upper limit", 0x1.cc8p+10, 0x1p+0, -187, 0x7f800000u, 1},
    {"infinity", Infinity, 0x1p+0, 0, 0x7f800000u, 1},
    {"minus infinity", -Infinity, 0x1p+0, 0, 0x00000000u, 0},
    {"nan argument", NotANumber, 0x1p+0, 0, 0x7fc00000u, 2},
    {"nan factor", 0x1p+0, NotANumber, 0, 0x7fc00000u, 2},
};

struct ConsoleCase {
    const char* label;
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t bits;
    short code;
};

const ConsoleCase ConsoleCases[] = {
    {"minus ten", 0xc1200000u, 0x3f800000u, 0x383e6bceu, -1},
    {"just below the overflow", 0x42b19999u, 0x3f800000u, 0x7f800000u, 1},
    {"at the overflow", 0x42b20000u, 0x3f800000u, 0x7f800000u, 1},
    {"minus 87", 0xc2ae0000u, 0x3f800000u, 0x00b33687u, -1},
    {"nan argument payload", 0x7fc00001u, 0x3f800000u, 0x7fc00001u, 2},
    {"second nan argument payload", 0x7fc00002u, 0x3f800000u, 0x7fc00002u, 2},
    {"nan factor against a different nan argument", 0x7fc00001u, 0x7fc00002u, 0x7fc00001u, 2},
    {"finite argument with a nan factor payload", 0x3f800000u, 0x7fc00003u, 0x7fc00003u, 2},
};

void Require(bool condition, const char* kind, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 exp: %s %s\n", kind, label);
    std::abort();
}

}

int main() {
    for (const auto& entry : DoubleCases) {
        double value = entry.x;
        errno = 0;
        const short code = _ZNSt8__sce_v24_ExpEPdds_nid_postfix(&value, entry.y, static_cast<short>(entry.scale));
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        Require(code == entry.code && bits == entry.bits && errno == 0, "double", entry.label);
    }
    for (const auto& entry : FloatCases) {
        float value = static_cast<float>(entry.x);
        errno = 0;
        const short code = _ZNSt8__sce_v25_FExpEPffs_nid_postfix(&value, static_cast<float>(entry.y), static_cast<short>(entry.scale));
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        Require(code == entry.code && bits == entry.bits && errno == 0, "float", entry.label);
    }
    for (const auto& entry : ConsoleCases) {
        const float x = std::bit_cast<float>(entry.x);
        float value = x;
        errno = 0;
        const short code = _ZNSt8__sce_v25_FExpEPffs_nid_postfix(&value, std::bit_cast<float>(entry.y), 0);
        Require(code == entry.code && std::bit_cast<std::uint32_t>(value) == entry.bits && errno == 0, "console", entry.label);
    }
    return 0;
}
