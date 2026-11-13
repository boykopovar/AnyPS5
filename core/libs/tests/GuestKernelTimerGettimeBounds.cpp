#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int GuestEfault = 14;
constexpr int GuestEinval = 22;
constexpr int ClockMonotonic = 4;

const GuestSigevent None{0, 0, 0, {}};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

int GetError() {
    return *__error_nid_postfix();
}

void TestTimerGettimeBounds() {
    int id = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &None, &id) == 0);

    GuestItimerspec value{};
    Require(ktimer_gettime_nid_postfix(id, &value) == 0);
    GuestItimerspec secondValue{};
    Require(ktimer_gettime_nid_postfix(id, &secondValue) == 0);
    Require(ktimer_gettime_nid_postfix(id, nullptr) == -1);
    Require(GetError() == GuestEfault);

    for (const int badId : {-1, 0, 2, id + 1, 32, 999}) {
        Require(ktimer_gettime_nid_postfix(badId, &value) == -1);
        Require(GetError() == GuestEinval);
        Require(ktimer_gettime_nid_postfix(badId, nullptr) == -1);
        Require(GetError() == GuestEinval);
    }

    Require(ktimer_delete_nid_postfix(id) == 0);
    Require(ktimer_gettime_nid_postfix(id, &value) == -1);
    Require(GetError() == GuestEinval);
}

} // namespace

int main() {
    TestTimerGettimeBounds();
    std::puts("GuestKernelTimerGettimeBounds passed");
    return 0;
}
