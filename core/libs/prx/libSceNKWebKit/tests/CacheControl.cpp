#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/CacheControlAbi.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace {

using NKWebKit::HttpAbi::CacheControlDirectives;
using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::HttpAbi::CommonHeader;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

using Parse = CacheControlDirectives (APS5_VABI *)(const HTTPHeaderMap&);
const auto parse = std::bit_cast<Parse>(&_ZN7WebCore27parseCacheControlDirectivesERKNS_13HTTPHeaderMapE);

constexpr double Unset = std::numeric_limits<double>::quiet_NaN();
constexpr double Infinity = std::numeric_limits<double>::infinity();

struct Case {
    const char* control;
    const char* pragma;
    double age = Unset;
    double stale = Unset;
    double revalidate = Unset;
    unsigned flags = 0;
};

bool Same(double a, double b) {
    return std::isnan(b) ? std::isnan(a) : a == b && (a != 0 || std::signbit(a) == std::signbit(b));
}

void Check(const Case& test, bool wide) {
    std::u16string control16(test.control, test.control + std::strlen(test.control));
    std::u16string pragma16(test.pragma, test.pragma + std::strlen(test.pragma));
    StringImpl control {1, static_cast<unsigned>(std::strlen(test.control)), wide ? static_cast<const void*>(control16.data()) : test.control, wide ? 0u : 4u};
    StringImpl pragma {1, static_cast<unsigned>(std::strlen(test.pragma)), wide ? static_cast<const void*>(pragma16.data()) : test.pragma, wide ? 0u : 4u};
    CommonHeader entries[] {{HTTPHeaderName::CacheControl, String(&control)}, {HTTPHeaderName::Pragma, String(&pragma)}};
    HTTPHeaderMap map {{entries, 2, 2}, {nullptr, 0, 0}};
    auto result = parse(map);
    unsigned flags = result.noCache | (result.noStore << 1) | (result.mustRevalidate << 2) | (result.immutable << 3);
    if (!Same(result.maxAge, test.age) || !Same(result.maxStale, test.stale) || !Same(result.staleWhileRevalidate, test.revalidate) || flags != test.flags) {
        std::fprintf(stderr, "CacheControl (%s) [%s] pragma [%s]: got %g %g %g flags %u\n", wide ? "16" : "8", test.control, test.pragma, result.maxAge, result.maxStale, result.staleWhileRevalidate, flags);
        std::abort();
    }
    auto bytes = std::bit_cast<std::array<unsigned char, 32>>(result);
    if ((bytes[24] & 15) != test.flags) std::abort();
}

}

int main() {
    HTTPHeaderMap empty {};
    auto result = parse(empty);
    if (!std::isnan(result.maxAge) || !std::isnan(result.maxStale) || !std::isnan(result.staleWhileRevalidate) || result.noCache || result.noStore || result.mustRevalidate || result.immutable) std::abort();
    const Case cases[] {
        {"", ""},
        {"MaX-AgE=12, MAX-StAlE=34,StAlE-WhIlE-ReVaLiDaTe=56,No-CaChE,No-StOrE,MuSt-ReVaLiDaTe,ImMuTaBlE", "", 12, 34, 56, 15},
        {"no-cache", "", Unset, Unset, Unset, 1},
        {"no-store", "", Unset, Unset, Unset, 2},
        {"must-revalidate", "", Unset, Unset, Unset, 4},
        {"immutable", "", Unset, Unset, Unset, 8},
        {"max-age=12", "", 12},
        {"max-stale=34", "", Unset, 34},
        {"stale-while-revalidate=56", "", Unset, Unset, 56},
        {"max-age=\"12.5\",max-stale=\"34.25\",stale-while-revalidate=\"56.75\"", "", 12.5, 34.25, 56.75},
        {"max-age=1,max-age=2,max-stale=3,max-stale=4,stale-while-revalidate=5,stale-while-revalidate=6", "", 1, 3, 5},
        {"max-age=x,max-age=-2.5,max-stale=bad,max-stale=+3.5,stale-while-revalidate=,stale-while-revalidate=4e2", "", -2.5, 3.5, 400},
        {"max-age=1x,max-stale=NaN,stale-while-revalidate=Infinity", ""},
        {"max-age=1e309,max-stale=-1e309,stale-while-revalidate=1e-999", "", Infinity, -Infinity, 0},
        {"max-age=-0,max-stale=.5,stale-while-revalidate=1.", "", -0.0, .5, 1},
        {"max-age=1e,max-stale=0x12,stale-while-revalidate=--1", ""},
        {"no-cache=\"field\"", ""},
        {"no-cache=\"\"", "", Unset, Unset, Unset, 1},
        {"max-stale", "", Unset, Infinity},
        {"max-stale=,max-stale=7", "", Unset, Infinity},
        {"max-stale=\"\"", "", Unset, Infinity},
        {"max-age=,stale-while-revalidate=", ""},
        {"", "prefixNO-CACHEsuffix", Unset, Unset, Unset, 1},
        {"max-age=10", "no-cache", 10, Unset, Unset, 1},
        {"no-cache=field", "no-cache", Unset, Unset, Unset, 1},
        {"no-cache", "other", Unset, Unset, Unset, 1},
        {"", "no- cache"},
        {"m\r\na\tx-age = 1 2\177, no-\001store", "", 12, Unset, Unset, 2},
        {"max-age=\"12", "", 12},
        {"max-age=\"12,34", "", 12},
        {"max-age=\"12\"junk,max-stale=34", "", 12, 34},
        {"max-age=\"12\"junk max-stale=34", "", 12},
        {"max-age=12;garbage,no-store=anything,must-revalidate=x,immutable=y", "", 12, Unset, Unset, 14},
        {"unknown=12,,max-age=7", "", 7},
        {"max-age=\"1;2\",max-stale=\"3,4\"", ""},
        {"no-cache=field,no-cache", "", Unset, Unset, Unset, 1},
    };
    for (const auto& test : cases) {
        Check(test, false);
        Check(test, true);
    }
    std::printf("%zu cache control cases in both string widths passed\n", std::size(cases));
}
