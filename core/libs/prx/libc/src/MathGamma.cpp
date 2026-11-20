#include "prx/libc/include/Api.hpp"
#include <cmath>
#include <limits>

namespace {

long double LargeLogGamma(long double x) {
    const volatile long double inverse = 1.0L / x;
    const volatile long double logarithm = _LLog_nid_postfix(x, 0) - 1.0L;
    const volatile long double argument = x - 0.5L;
    const volatile long double square = inverse * inverse;
    long double product = argument;
    short exponent = 0;
    _LDunscale_nid_postfix(&exponent, &product);
    product = product * logarithm;
    if (_LDscale_nid_postfix(&product, exponent) >= 0) {
        _Feraise_nid_postfix(0x08);
        return product;
    }
    long double correction = 0xdca8f158c7f91ab8p-74L * square;
    correction = correction - 0x9c09c09c09c09c0ap-74L;
    correction = correction * square + 0xd00d00d00d00d00dp-74L;
    correction = correction * square - 0xb60b60b60b60b60bp-72L;
    correction = correction * square + 0xaaaaaaaaaaaaaaabp-67L;
    correction = correction * inverse + 0xd67f1c864beb4a69p-65L;
    return correction + product;
}

}

extern "C" {

long double APS5_VABI lgammal_nid_postfix(long double x) {
    long double integral = x;
    const short kind = _LDint_nid_postfix(&integral, 0);
    if (kind == 0 && x <= 0.0L) {
        _Feraise_nid_postfix(0x04);
        return std::numeric_limits<long double>::infinity();
    }
    if (kind <= 0) {
        if (x >= 130.0L) return LargeLogGamma(x);
        if (x < -130.0L) {
            constexpr long double GammaPi = 0xc90fdaa22168c235p-62L;
            const long double sine = _LSin_nid_postfix((x - integral) * GammaPi, 0);
            const long double reflected = _LLog_nid_postfix(GammaPi / (x * sine), 0);
            return reflected - LargeLogGamma(-x);
        }
    }
    return std::lgamma(x);
}
}
