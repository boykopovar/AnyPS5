#include "prx/libc/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace {

constexpr auto SignedInt = _ZNSt3__19to_stringEi_nid_postfix;
constexpr auto UnsignedInt = _ZNSt3__19to_stringEj_nid_postfix;
constexpr auto UnsignedLong = _ZNSt3__19to_stringEm_nid_postfix;
constexpr auto SignedLongLong = _ZNSt3__19to_stringEx_nid_postfix;

void Require(bool condition, const char* failure) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", failure);
    std::abort();
}

template<class Convert, class Number>
void Check(Convert convert, Number value, std::string_view expected) {
    struct alignas(8) Result {
        std::uint64_t before{UINT64_C(0x1122334455667788)};
        std::array<unsigned char, 24> string;
        std::uint64_t after{UINT64_C(0x8877665544332211)};
    } result;
    result.string.fill(0xa5);
    Require(convert(result.string.data(), value) == result.string.data(), "incorrect SysV hidden-return address");
    Require(result.before == UINT64_C(0x1122334455667788) && result.after == UINT64_C(0x8877665544332211),
        "conversion wrote past the 24-byte guest string");
    Require(result.string[0] == expected.size() * 2u, "incorrect inline guest string size");
    const auto* text = reinterpret_cast<const char*>(result.string.data() + 1);
    Require(std::string_view(text, expected.size()) == expected, "incorrect decimal conversion");
    Require(text[expected.size()] == '\0', "guest result has no terminator");
}

}

int main() {
    Check(SignedInt, std::int32_t{0}, "0");
    Check(SignedInt, std::int32_t{-72}, "-72");
    Check(SignedInt, std::numeric_limits<std::int32_t>::min(), "-2147483648");
    Check(SignedInt, std::numeric_limits<std::int32_t>::max(), "2147483647");
    Check(UnsignedInt, std::uint32_t{3000}, "3000");
    Check(UnsignedInt, std::numeric_limits<std::uint32_t>::max(), "4294967295");
    Check(UnsignedLong, UINT64_C(4294967297), "4294967297");
    Check(UnsignedLong, std::numeric_limits<std::uint64_t>::max(), "18446744073709551615");
    Check(SignedLongLong, std::int64_t{-4294967297LL}, "-4294967297");
    Check(SignedLongLong, std::numeric_limits<std::int64_t>::min(), "-9223372036854775808");
    Check(SignedLongLong, std::numeric_limits<std::int64_t>::max(), "9223372036854775807");
    std::puts("libc++ integer to_string ABI checks passed");
    return 0;
}
