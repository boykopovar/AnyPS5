#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    GuestResourceUsage usage{};
    *__error_nid_postfix() = 0;
    Require(getrusage_nid_postfix(0, &usage) == 0);

    *__error_nid_postfix() = 0;
    Require(getrusage_nid_postfix(1, &usage) == 0);

    *__error_nid_postfix() = 0;
    Require(getrusage_nid_postfix(-1, &usage) == 0);

    for (int invalidWho : {-2, 2, 5, 100, -99, 999}) {
        *__error_nid_postfix() = 0;
        Require(getrusage_nid_postfix(invalidWho, &usage) == -1);
        Require(*__error_nid_postfix() == 22);
    }

    for (int who : {0, 1, -1}) {
        *__error_nid_postfix() = 0;
        Require(getrusage_nid_postfix(who, nullptr) == -1);
        Require(*__error_nid_postfix() == 14);
    }

    return 0;
}
