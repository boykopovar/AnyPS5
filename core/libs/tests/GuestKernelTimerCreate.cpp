#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int Efault = 14;
constexpr int Einval = 22;
constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;
constexpr int ClockUptime = 5;

const GuestSigevent None{0, 0, 0, {}};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

void AuditClockIds() {
    int id = -1;
    Require(ktimer_create_nid_postfix(ClockRealtime, &None, &id) == 0);
    Require(ktimer_delete_nid_postfix(id) == 0);

    Require(ktimer_create_nid_postfix(ClockMonotonic, &None, &id) == 0);
    Require(ktimer_delete_nid_postfix(id) == 0);

    Require(ktimer_create_nid_postfix(ClockUptime, &None, &id) == -1 && Error() == Einval && id == -1);
    Require(ktimer_create_nid_postfix(-1, &None, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(999, &None, &id) == -1 && Error() == Einval);
}

void AuditNullTimerId() {
    Require(ktimer_create_nid_postfix(ClockMonotonic, &None, nullptr) == -1 && Error() == Efault);
}

} // namespace

int main() {
    AuditClockIds();
    AuditNullTimerId();
    std::puts("guest_kernel_timer_create passed");
    return 0;
}
