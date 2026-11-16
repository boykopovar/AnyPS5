#include "prx/libc/include/Api.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(_ZSt19uncaught_exceptionsv_nid_postfix() == 0);
    return 0;
}
