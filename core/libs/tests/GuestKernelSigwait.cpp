#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    int sig = 0;
    GuestSignalSet set{};
    *__error_nid_postfix() = 0;
    Require(sigwait_nid_postfix(nullptr, &sig) == -1 && *__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    Require(sigwait_nid_postfix(&set, nullptr) == -1 && *__error_nid_postfix() == 22);

    set.bits[0] = 0;
    *__error_nid_postfix() = 0;
    Require(sigwait_nid_postfix(&set, &sig) == -1 && *__error_nid_postfix() == 22);

    set.bits[0] = (1ULL << 32);
    *__error_nid_postfix() = 0;
    Require(sigwait_nid_postfix(&set, &sig) == -1 && *__error_nid_postfix() == 22);

    set.bits[0] = (1ULL << 1);
    *__error_nid_postfix() = 75;
    Require(sigwait_nid_postfix(&set, reinterpret_cast<int*>(0x10)) == -1 && *__error_nid_postfix() == 14);
}
