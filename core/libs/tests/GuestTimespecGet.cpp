#include "prx/libc/include/general/VabiMacros.hpp"
#include <ctime>
#include <cstdlib>

extern "C" int APS5_VABI timespec_get_nid_postfix(timespec* ts, int base);

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(timespec_get_nid_postfix(nullptr, TIME_UTC) == 0);
    timespec ts{};
    Require(timespec_get_nid_postfix(&ts, 0) == 0);
    Require(timespec_get_nid_postfix(&ts, TIME_UTC) == TIME_UTC);
    Require(ts.tv_sec > 0);
    Require(ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000L);
    timespec later{};
    Require(timespec_get_nid_postfix(&later, TIME_UTC) == TIME_UTC);
    Require(later.tv_sec > ts.tv_sec || (later.tv_sec == ts.tv_sec && later.tv_nsec >= ts.tv_nsec));
    return 0;
}
