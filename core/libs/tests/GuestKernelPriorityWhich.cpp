#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    const int self = getpid_nid_postfix();
    for (int invalidWhich : {-1, 3, 4, 100, -99, 999}) {
        *__error_nid_postfix() = 0;
        Require(getpriority_nid_postfix(invalidWhich, 0) == -1);
        Require(*__error_nid_postfix() == 22);

        *__error_nid_postfix() = 0;
        Require(getpriority_nid_postfix(invalidWhich, self) == -1);
        Require(*__error_nid_postfix() == 22);

        *__error_nid_postfix() = 0;
        Require(setpriority_nid_postfix(invalidWhich, 0, 0) == -1);
        Require(*__error_nid_postfix() == 22);

        *__error_nid_postfix() = 0;
        Require(setpriority_nid_postfix(invalidWhich, self, 0) == -1);
        Require(*__error_nid_postfix() == 22);
    }
    return 0;
}
