#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <limits>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    Require(__isnan_nid_postfix(nan) != 0);
    Require(__isnan_nid_postfix(1.0) == 0);
    Require(__isnan_nid_postfix(-0.0) == 0);
    Require(__isnan_nid_postfix(infinity) == 0);
    const float nanf = std::numeric_limits<float>::quiet_NaN();
    Require(__isnanf_nid_postfix(nanf) != 0);
    Require(__isnanf_nid_postfix(1.0f) == 0);
    Require(__isnanf_nid_postfix(std::numeric_limits<float>::infinity()) == 0);
    return 0;
}
