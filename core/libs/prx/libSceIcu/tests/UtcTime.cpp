#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace {


static_assert(std::is_same_v<decltype(&uprv_getUTCtime_nid_postfix), double (APS5_VABI*)()>);
static_assert(std::is_same_v<decltype(&uprv_getRawUTCtime_nid_postfix), double (APS5_VABI*)()>);

constexpr double Year2020Millis = 1577836800000.0;

double KernelMillis() {
    KernelTimeval now{};
    Require(gettimeofday_nid_postfix(&now, nullptr) == 0);
    return static_cast<double>(now.tv_sec * 1000 + now.tv_usec / 1000);
}

void Check(double (APS5_VABI* clock)()) {
    for (int round = 0; round < 1000; ++round) {
        const double before = KernelMillis();
        const double value = clock();
        const double after = KernelMillis();
        Require(value == std::floor(value));
        Require(value > Year2020Millis);
        Require(before <= value && value <= after);
    }
}

}

int main() {
    Check(uprv_getRawUTCtime_nid_postfix);
    Check(uprv_getUTCtime_nid_postfix);
    const double raw = uprv_getRawUTCtime_nid_postfix();
    const double utc = uprv_getUTCtime_nid_postfix();
    const double later = uprv_getRawUTCtime_nid_postfix();
    Require(raw <= utc && utc <= later);
    return 0;
}
