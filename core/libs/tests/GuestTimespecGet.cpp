#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <ctime>
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(timespec_get_nid_postfix(nullptr, TIME_UTC) == 0);
    KernelTimespec ts{};
    Require(timespec_get_nid_postfix(&ts, 0) == 0);
    KernelTimespec filled{-1, -1};
    Require(timespec_get_nid_postfix(&filled, TIME_UTC) == TIME_UTC);
    Require(filled.tv_sec > 0);
    Require(filled.tv_nsec >= 0 && filled.tv_nsec < 1000000000LL);
    KernelTimespec later{-1, -1};
    Require(timespec_get_nid_postfix(&later, TIME_UTC) == TIME_UTC);
    Require(later.tv_nsec >= 0 && later.tv_nsec < 1000000000LL);
    Require(later.tv_sec > filled.tv_sec || (later.tv_sec == filled.tv_sec && later.tv_nsec >= filled.tv_nsec));
    return 0;
}
