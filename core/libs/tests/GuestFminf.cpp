#include "prx/libc/include/general/VabiMacros.hpp"
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

extern "C" {
double APS5_VABI fmin_nid_postfix(double, double);
float APS5_VABI fminf_nid_postfix(float, float);
long double APS5_VABI fminl_nid_postfix(long double, long double);
double APS5_VABI fmax_nid_postfix(double, double);
float APS5_VABI fmaxf_nid_postfix(float, float);
long double APS5_VABI fmaxl_nid_postfix(long double, long double);
}

static void Check(bool condition, const char* name, int line) {
    if (!condition) {
        std::fprintf(stderr, "%s check failed at line %d\n", name, line);
        std::abort();
    }
}
#define Require(value) Check((value), name, __LINE__)

template <typename T>
static void CheckMinAndMax(T (APS5_VABI *minimum)(T, T), T (APS5_VABI *maximum)(T, T), const char* name) {
    const auto nan = std::numeric_limits<T>::quiet_NaN();
    const auto infinity = std::numeric_limits<T>::infinity();

    Require(minimum(T(3.0), T(-2.0)) == T(-2.0));
    Require(minimum(T(-2.0), T(3.0)) == T(-2.0));
    Require(minimum(T(1.5), T(2.5)) == T(1.5));
    Require(maximum(T(3.0), T(-2.0)) == T(3.0));
    Require(maximum(T(-2.0), T(3.0)) == T(3.0));
    Require(maximum(T(1.5), T(2.5)) == T(2.5));

    Require(minimum(nan, T(4.0)) == T(4.0));
    Require(minimum(T(4.0), nan) == T(4.0));
    Require(std::isnan(minimum(nan, nan)));
    Require(maximum(nan, T(4.0)) == T(4.0));
    Require(maximum(T(4.0), nan) == T(4.0));
    Require(std::isnan(maximum(nan, nan)));

    const auto signaling = std::numeric_limits<T>::signaling_NaN();
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(minimum(signaling, T(4.0)) == T(4.0) && minimum(T(4.0), signaling) == T(4.0));
    Require(maximum(signaling, T(4.0)) == T(4.0) && maximum(T(4.0), signaling) == T(4.0));
    Require(std::fetestexcept(FE_INVALID) == 0);

    Require(std::signbit(minimum(T(-0.0), T(+0.0))));
    Require(std::signbit(minimum(T(+0.0), T(-0.0))));
    Require(std::signbit(minimum(T(-0.0), T(-0.0))));
    Require(!std::signbit(minimum(T(+0.0), T(+0.0))));
    Require(!std::signbit(maximum(T(-0.0), T(+0.0))));
    Require(!std::signbit(maximum(T(+0.0), T(-0.0))));
    Require(std::signbit(maximum(T(-0.0), T(-0.0))));
    Require(!std::signbit(maximum(T(+0.0), T(+0.0))));

    Require(minimum(infinity, -infinity) == -infinity);
    Require(minimum(-infinity, infinity) == -infinity);
    Require(minimum(infinity, T(5.0)) == T(5.0));
    Require(minimum(T(5.0), infinity) == T(5.0));
    Require(minimum(-infinity, T(5.0)) == -infinity);
    Require(minimum(T(5.0), -infinity) == -infinity);
    Require(maximum(infinity, -infinity) == infinity);
    Require(maximum(-infinity, infinity) == infinity);
    Require(maximum(infinity, T(5.0)) == infinity);
    Require(maximum(T(5.0), infinity) == infinity);
    Require(maximum(-infinity, T(5.0)) == T(5.0));
    Require(maximum(T(5.0), -infinity) == T(5.0));
}

int main() {
    CheckMinAndMax<double>(fmin_nid_postfix, fmax_nid_postfix, "fmin/fmax");
    CheckMinAndMax<float>(fminf_nid_postfix, fmaxf_nid_postfix, "fminf/fmaxf");
    CheckMinAndMax<long double>(fminl_nid_postfix, fmaxl_nid_postfix, "fminl/fmaxl");

    return 0;
}
