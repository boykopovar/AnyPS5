#include "prx/libc/include/NumericLimits.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace {

template <typename TValue, typename TExpected>
void Check(const TValue& value, TExpected expected, const char* name) {
    static_assert(std::is_same_v<TValue, bool> == std::is_same_v<TExpected, bool>);
    if (static_cast<std::int64_t>(value) == static_cast<std::int64_t>(expected)) return;
    std::fprintf(stderr, "%s is %lld, expected %lld\n", name, static_cast<long long>(value),
        static_cast<long long>(expected));
    std::abort();
}

} // namespace

int main() {
    static_assert(sizeof(_ZNSt14numeric_limitsIiE6digitsE_nid_postfix) == 4);
    static_assert(sizeof(_ZNSt14numeric_limitsIiE9is_signedE_nid_postfix) == 1);
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);
    Check(_ZNSt14numeric_limitsIbE6digitsE_nid_postfix, std::numeric_limits<bool>::digits, "numeric_limits<bool>::digits");
    Check(_ZNSt14numeric_limitsIbE8digits10E_nid_postfix, std::numeric_limits<bool>::digits10, "numeric_limits<bool>::digits10");
    Check(_ZNSt14numeric_limitsIbE9is_signedE_nid_postfix, std::numeric_limits<bool>::is_signed, "numeric_limits<bool>::is_signed");
    Check(_ZNSt14numeric_limitsIbE9is_moduloE_nid_postfix, std::numeric_limits<bool>::is_modulo, "numeric_limits<bool>::is_modulo");
    Check(_ZNSt14numeric_limitsIcE6digitsE_nid_postfix, std::numeric_limits<signed char>::digits, "numeric_limits<char>::digits");
    Check(_ZNSt14numeric_limitsIcE8digits10E_nid_postfix, std::numeric_limits<signed char>::digits10, "numeric_limits<char>::digits10");
    Check(_ZNSt14numeric_limitsIcE9is_signedE_nid_postfix, std::numeric_limits<signed char>::is_signed, "numeric_limits<char>::is_signed");
    Check(_ZNSt14numeric_limitsIaE6digitsE_nid_postfix, std::numeric_limits<signed char>::digits, "numeric_limits<signed char>::digits");
    Check(_ZNSt14numeric_limitsIaE8digits10E_nid_postfix, std::numeric_limits<signed char>::digits10, "numeric_limits<signed char>::digits10");
    Check(_ZNSt14numeric_limitsIaE9is_signedE_nid_postfix, std::numeric_limits<signed char>::is_signed, "numeric_limits<signed char>::is_signed");
    Check(_ZNSt14numeric_limitsIhE6digitsE_nid_postfix, std::numeric_limits<unsigned char>::digits, "numeric_limits<unsigned char>::digits");
    Check(_ZNSt14numeric_limitsIhE8digits10E_nid_postfix, std::numeric_limits<unsigned char>::digits10, "numeric_limits<unsigned char>::digits10");
    Check(_ZNSt14numeric_limitsIhE9is_signedE_nid_postfix, std::numeric_limits<unsigned char>::is_signed, "numeric_limits<unsigned char>::is_signed");
    Check(_ZNSt14numeric_limitsIsE6digitsE_nid_postfix, std::numeric_limits<std::int16_t>::digits, "numeric_limits<short>::digits");
    Check(_ZNSt14numeric_limitsIsE8digits10E_nid_postfix, std::numeric_limits<std::int16_t>::digits10, "numeric_limits<short>::digits10");
    Check(_ZNSt14numeric_limitsIsE9is_signedE_nid_postfix, std::numeric_limits<std::int16_t>::is_signed, "numeric_limits<short>::is_signed");
    Check(_ZNSt14numeric_limitsItE6digitsE_nid_postfix, std::numeric_limits<std::uint16_t>::digits, "numeric_limits<unsigned short>::digits");
    Check(_ZNSt14numeric_limitsItE8digits10E_nid_postfix, std::numeric_limits<std::uint16_t>::digits10, "numeric_limits<unsigned short>::digits10");
    Check(_ZNSt14numeric_limitsItE9is_signedE_nid_postfix, std::numeric_limits<std::uint16_t>::is_signed, "numeric_limits<unsigned short>::is_signed");
    Check(_ZNSt14numeric_limitsIiE6digitsE_nid_postfix, std::numeric_limits<std::int32_t>::digits, "numeric_limits<int>::digits");
    Check(_ZNSt14numeric_limitsIiE8digits10E_nid_postfix, std::numeric_limits<std::int32_t>::digits10, "numeric_limits<int>::digits10");
    Check(_ZNSt14numeric_limitsIiE9is_signedE_nid_postfix, std::numeric_limits<std::int32_t>::is_signed, "numeric_limits<int>::is_signed");
    Check(_ZNSt14numeric_limitsIjE6digitsE_nid_postfix, std::numeric_limits<std::uint32_t>::digits, "numeric_limits<unsigned int>::digits");
    Check(_ZNSt14numeric_limitsIjE8digits10E_nid_postfix, std::numeric_limits<std::uint32_t>::digits10, "numeric_limits<unsigned int>::digits10");
    Check(_ZNSt14numeric_limitsIjE9is_signedE_nid_postfix, std::numeric_limits<std::uint32_t>::is_signed, "numeric_limits<unsigned int>::is_signed");
    Check(_ZNSt14numeric_limitsIlE6digitsE_nid_postfix, std::numeric_limits<std::int64_t>::digits, "numeric_limits<long>::digits");
    Check(_ZNSt14numeric_limitsIlE8digits10E_nid_postfix, std::numeric_limits<std::int64_t>::digits10, "numeric_limits<long>::digits10");
    Check(_ZNSt14numeric_limitsIlE9is_signedE_nid_postfix, std::numeric_limits<std::int64_t>::is_signed, "numeric_limits<long>::is_signed");
    Check(_ZNSt14numeric_limitsImE6digitsE_nid_postfix, std::numeric_limits<std::uint64_t>::digits, "numeric_limits<unsigned long>::digits");
    Check(_ZNSt14numeric_limitsImE8digits10E_nid_postfix, std::numeric_limits<std::uint64_t>::digits10, "numeric_limits<unsigned long>::digits10");
    Check(_ZNSt14numeric_limitsImE9is_signedE_nid_postfix, std::numeric_limits<std::uint64_t>::is_signed, "numeric_limits<unsigned long>::is_signed");
    Check(_ZNSt14numeric_limitsIxE6digitsE_nid_postfix, std::numeric_limits<std::int64_t>::digits, "numeric_limits<long long>::digits");
    Check(_ZNSt14numeric_limitsIxE8digits10E_nid_postfix, std::numeric_limits<std::int64_t>::digits10, "numeric_limits<long long>::digits10");
    Check(_ZNSt14numeric_limitsIxE9is_signedE_nid_postfix, std::numeric_limits<std::int64_t>::is_signed, "numeric_limits<long long>::is_signed");
    Check(_ZNSt14numeric_limitsIyE6digitsE_nid_postfix, std::numeric_limits<std::uint64_t>::digits, "numeric_limits<unsigned long long>::digits");
    Check(_ZNSt14numeric_limitsIyE8digits10E_nid_postfix, std::numeric_limits<std::uint64_t>::digits10, "numeric_limits<unsigned long long>::digits10");
    Check(_ZNSt14numeric_limitsIyE9is_signedE_nid_postfix, std::numeric_limits<std::uint64_t>::is_signed, "numeric_limits<unsigned long long>::is_signed");
    Check(_ZNSt14numeric_limitsIwE6digitsE_nid_postfix, std::numeric_limits<char16_t>::digits, "numeric_limits<wchar_t>::digits");
    Check(_ZNSt14numeric_limitsIwE8digits10E_nid_postfix, std::numeric_limits<char16_t>::digits10, "numeric_limits<wchar_t>::digits10");
    Check(_ZNSt14numeric_limitsIwE9is_signedE_nid_postfix, std::numeric_limits<char16_t>::is_signed, "numeric_limits<wchar_t>::is_signed");
    Check(_ZNSt14numeric_limitsIDsE6digitsE_nid_postfix, std::numeric_limits<char16_t>::digits, "numeric_limits<char16_t>::digits");
    Check(_ZNSt14numeric_limitsIDsE8digits10E_nid_postfix, std::numeric_limits<char16_t>::digits10, "numeric_limits<char16_t>::digits10");
    Check(_ZNSt14numeric_limitsIDsE9is_signedE_nid_postfix, std::numeric_limits<char16_t>::is_signed, "numeric_limits<char16_t>::is_signed");
    Check(_ZNSt14numeric_limitsIDiE6digitsE_nid_postfix, std::numeric_limits<char32_t>::digits, "numeric_limits<char32_t>::digits");
    Check(_ZNSt14numeric_limitsIDiE8digits10E_nid_postfix, std::numeric_limits<char32_t>::digits10, "numeric_limits<char32_t>::digits10");
    Check(_ZNSt14numeric_limitsIDiE9is_signedE_nid_postfix, std::numeric_limits<char32_t>::is_signed, "numeric_limits<char32_t>::is_signed");
    Check(_ZNSt14numeric_limitsIfE6digitsE_nid_postfix, std::numeric_limits<float>::digits, "numeric_limits<float>::digits");
    Check(_ZNSt14numeric_limitsIfE8digits10E_nid_postfix, std::numeric_limits<float>::digits10, "numeric_limits<float>::digits10");
    Check(_ZNSt14numeric_limitsIfE12max_digits10E_nid_postfix, std::numeric_limits<float>::max_digits10, "numeric_limits<float>::max_digits10");
    Check(_ZNSt14numeric_limitsIfE12max_exponentE_nid_postfix, std::numeric_limits<float>::max_exponent, "numeric_limits<float>::max_exponent");
    Check(_ZNSt14numeric_limitsIfE14max_exponent10E_nid_postfix, std::numeric_limits<float>::max_exponent10, "numeric_limits<float>::max_exponent10");
    Check(_ZNSt14numeric_limitsIfE12min_exponentE_nid_postfix, std::numeric_limits<float>::min_exponent, "numeric_limits<float>::min_exponent");
    Check(_ZNSt14numeric_limitsIfE14min_exponent10E_nid_postfix, std::numeric_limits<float>::min_exponent10, "numeric_limits<float>::min_exponent10");
    Check(_ZNSt14numeric_limitsIdE6digitsE_nid_postfix, std::numeric_limits<double>::digits, "numeric_limits<double>::digits");
    Check(_ZNSt14numeric_limitsIdE8digits10E_nid_postfix, std::numeric_limits<double>::digits10, "numeric_limits<double>::digits10");
    Check(_ZNSt14numeric_limitsIdE12max_digits10E_nid_postfix, std::numeric_limits<double>::max_digits10, "numeric_limits<double>::max_digits10");
    Check(_ZNSt14numeric_limitsIdE12max_exponentE_nid_postfix, std::numeric_limits<double>::max_exponent, "numeric_limits<double>::max_exponent");
    Check(_ZNSt14numeric_limitsIdE14max_exponent10E_nid_postfix, std::numeric_limits<double>::max_exponent10, "numeric_limits<double>::max_exponent10");
    Check(_ZNSt14numeric_limitsIdE12min_exponentE_nid_postfix, std::numeric_limits<double>::min_exponent, "numeric_limits<double>::min_exponent");
    Check(_ZNSt14numeric_limitsIdE14min_exponent10E_nid_postfix, std::numeric_limits<double>::min_exponent10, "numeric_limits<double>::min_exponent10");
    Check(_ZNSt14numeric_limitsIeE6digitsE_nid_postfix, 64, "numeric_limits<long double>::digits");
    Check(_ZNSt14numeric_limitsIeE8digits10E_nid_postfix, 18, "numeric_limits<long double>::digits10");
    Check(_ZNSt14numeric_limitsIeE12max_digits10E_nid_postfix, 21, "numeric_limits<long double>::max_digits10");
    Check(_ZNSt14numeric_limitsIeE12max_exponentE_nid_postfix, 16384, "numeric_limits<long double>::max_exponent");
    Check(_ZNSt14numeric_limitsIeE14max_exponent10E_nid_postfix, 4932, "numeric_limits<long double>::max_exponent10");
    Check(_ZNSt14numeric_limitsIeE12min_exponentE_nid_postfix, -16381, "numeric_limits<long double>::min_exponent");
    Check(_ZNSt14numeric_limitsIeE14min_exponent10E_nid_postfix, -4931, "numeric_limits<long double>::min_exponent10");
    return 0;
}
