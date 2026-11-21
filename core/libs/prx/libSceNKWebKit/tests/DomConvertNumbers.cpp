#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "convertToInteger mismatch at line %d\n", line);
        std::abort();
    }
}

#define REQUIRE(x) Require((x), __LINE__)

static std::uint64_t I(std::int32_t value) {
    return 0xfffe000000000000ull | static_cast<std::uint32_t>(value);
}

static std::uint64_t D(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits + (1ull << 49);
}

static constexpr std::uint64_t Null = 0x02;
static constexpr std::uint64_t False = 0x06;
static constexpr std::uint64_t True = 0x07;
static constexpr std::uint64_t Undefined = 0x0a;
static constexpr double Nan = std::numeric_limits<double>::quiet_NaN();
static constexpr double Inf = std::numeric_limits<double>::infinity();
static int globalObject;

static std::int8_t S8(std::uint64_t v) { return _ZN7WebCore16convertToIntegerIaEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::uint8_t U8(std::uint64_t v) { return _ZN7WebCore16convertToIntegerIhEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::int16_t S16(std::uint64_t v) { return _ZN7WebCore16convertToIntegerIsEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::uint16_t U16(std::uint64_t v) { return _ZN7WebCore16convertToIntegerItEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::int32_t S32(std::uint64_t v) { return _ZN7WebCore16convertToIntegerIiEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::uint32_t U32(std::uint64_t v) { return _ZN7WebCore16convertToIntegerIjEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::int64_t S64(std::uint64_t v) { return _ZN7WebCore16convertToIntegerIlEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::uint64_t U64(std::uint64_t v) { return _ZN7WebCore16convertToIntegerImEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::int8_t CS8(std::uint64_t v) { return _ZN7WebCore21convertToIntegerClampIaEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::uint8_t CU8(std::uint64_t v) { return _ZN7WebCore21convertToIntegerClampIhEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::int16_t CS16(std::uint64_t v) { return _ZN7WebCore21convertToIntegerClampIsEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::uint16_t CU16(std::uint64_t v) { return _ZN7WebCore21convertToIntegerClampItEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::int32_t CS32(std::uint64_t v) { return _ZN7WebCore21convertToIntegerClampIiEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::uint32_t CU32(std::uint64_t v) { return _ZN7WebCore21convertToIntegerClampIjEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::int64_t CS64(std::uint64_t v) { return _ZN7WebCore21convertToIntegerClampIlEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }
static std::uint64_t CU64(std::uint64_t v) { return _ZN7WebCore21convertToIntegerClampImEET_RN3JSC14JSGlobalObjectENS2_7JSValueE(&globalObject, v); }

static void TestNormal() {
    REQUIRE(S8(I(5)) == 5);
    REQUIRE(S8(I(200)) == -56);
    REQUIRE(S8(I(-200)) == 56);
    REQUIRE(S8(D(300.7)) == 44);
    REQUIRE(S8(D(-1.9)) == -1);
    REQUIRE(S8(D(4294967299.0)) == 3);
    REQUIRE(S8(D(Nan)) == 0 && S8(D(Inf)) == 0 && S8(D(-Inf)) == 0);
    REQUIRE(S8(Undefined) == 0 && S8(Null) == 0 && S8(False) == 0 && S8(True) == 1);
    REQUIRE(U8(I(-1)) == 255);
    REQUIRE(U8(I(263)) == 7);
    REQUIRE(U8(D(-1.5)) == 255);
    REQUIRE(U8(D(511.9)) == 255);
    REQUIRE(S16(I(40000)) == -25536);
    REQUIRE(S16(D(-32769.0)) == 32767);
    REQUIRE(U16(I(-1)) == 65535);
    REQUIRE(U16(D(65536.5)) == 0);
    REQUIRE(S32(I(-7)) == -7);
    REQUIRE(S32(D(2147483648.0)) == std::numeric_limits<std::int32_t>::min());
    REQUIRE(S32(D(-2147483649.0)) == std::numeric_limits<std::int32_t>::max());
    REQUIRE(S32(D(4294967297.0)) == 1);
    REQUIRE(S32(D(-0.0)) == 0 && S32(D(Nan)) == 0 && S32(Undefined) == 0 && S32(True) == 1);
    REQUIRE(U32(I(-1)) == 4294967295u);
    REQUIRE(U32(D(4294967301.0)) == 5);
    REQUIRE(U32(D(-1.0)) == 4294967295u);
    REQUIRE(S64(I(-3)) == -3);
    REQUIRE(S64(D(-1.5)) == -1);
    REQUIRE(S64(D(9007199254740992.0)) == 9007199254740992ll);
    REQUIRE(S64(D(9223372036854775808.0)) == std::numeric_limits<std::int64_t>::min());
    REQUIRE(S64(D(18446744073709551616.0)) == 0);
    REQUIRE(S64(D(Inf)) == 0 && S64(Undefined) == 0 && S64(True) == 1);
    REQUIRE(U64(I(-1)) == std::numeric_limits<std::uint64_t>::max());
    REQUIRE(U64(D(-2.0)) == std::numeric_limits<std::uint64_t>::max() - 1);
    REQUIRE(U64(D(9223372036854775808.0)) == 9223372036854775808ull);
    REQUIRE(U64(D(Nan)) == 0 && U64(Null) == 0);
}

static void TestClamp() {
    REQUIRE(CS8(I(200)) == 127);
    REQUIRE(CS8(I(-200)) == -128);
    REQUIRE(CS8(I(-5)) == -5);
    REQUIRE(CS8(D(-1.9)) == -1);
    REQUIRE(CS8(D(1e10)) == 127 && CS8(D(-Inf)) == -128);
    REQUIRE(CS8(D(Nan)) == 0 && CS8(Undefined) == 0 && CS8(True) == 1);
    REQUIRE(CU8(I(-5)) == 0);
    REQUIRE(CU8(I(300)) == 255);
    REQUIRE(CU8(D(2.5)) == 2 && CU8(D(Inf)) == 255);
    REQUIRE(CS16(I(40000)) == 32767);
    REQUIRE(CS16(D(-40000.5)) == -32768);
    REQUIRE(CU16(D(-0.5)) == 0);
    REQUIRE(CU16(I(70000)) == 65535);
    REQUIRE(CS32(I(-5)) == -5);
    REQUIRE(CS32(D(3e9)) == std::numeric_limits<std::int32_t>::max());
    REQUIRE(CS32(D(-3e9)) == std::numeric_limits<std::int32_t>::min());
    REQUIRE(CS32(D(1.99)) == 1 && CS32(D(Nan)) == 0);
    REQUIRE(CU32(I(-5)) == 0);
    REQUIRE(CU32(D(5e9)) == 4294967295u);
    REQUIRE(CU32(D(7.9)) == 7);
    REQUIRE(CS64(I(-9)) == -9);
    REQUIRE(CS64(D(1e20)) == 9007199254740991ll);
    REQUIRE(CS64(D(-1e20)) == -9007199254740991ll);
    REQUIRE(CS64(D(-2.7)) == -2 && CS64(D(Nan)) == 0);
    REQUIRE(CU64(I(-9)) == 0 && CU64(I(9)) == 9);
    REQUIRE(CU64(D(-3.0)) == 0);
    REQUIRE(CU64(D(1e20)) == 9007199254740991ull);
}

static void TestUnsupported() {
    for (std::uint64_t value : {0x00007f0000001000ull, 0x0000000500000012ull}) {
        const auto calls = NotImplementedCalls_nid_no_patch();
        bool thrown = false;
        try {
            S32(value);
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        REQUIRE(thrown && NotImplementedCalls_nid_no_patch() == calls + 1);
    }
}

int main() {
    TestNormal();
    TestClamp();
    TestUnsupported();
    return 0;
}
