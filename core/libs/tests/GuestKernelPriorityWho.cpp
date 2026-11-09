#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
}

static constexpr int PrioProcess = 0;
static constexpr int PrioGroup = 1;
static constexpr int PrioUser = 2;

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    const int self = getpid_nid_postfix();
    const int uid = getuid_nid_postfix();

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioProcess, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioProcess, self) == 0);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioGroup, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioGroup, self) == 0);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioUser, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioUser, uid) == 0);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioProcess, self + 1) == -1);
    Require(*__error_nid_postfix() == 3);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioProcess, -1) == -1);
    Require(*__error_nid_postfix() == 3);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioGroup, self + 1) == -1);
    Require(*__error_nid_postfix() == 3);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(PrioUser, uid + 1) == -1);
    Require(*__error_nid_postfix() == 3);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(3, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(PrioProcess, 0, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(PrioProcess, self, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(PrioProcess, self + 1, 0) == -1);
    Require(*__error_nid_postfix() == 3);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(3, 0, 0) == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(PrioProcess, 0, -5) == -1);
    Require(*__error_nid_postfix() == 13);
}
