#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <cstdio>
#include <source_location>

namespace {

constexpr int Einval = 22;
const GuestSigevent None{0, 0, 0, {}};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

} // namespace

int main() {
    int id = -1;
    Require(ktimer_create_nid_postfix(4, &None, &id) == 0);
    Require(ktimer_delete_nid_postfix(id) == 0);

    for (const int badId : {-999, -1, 0, 1, 2, id, 9999}) {
        Require(ktimer_delete_nid_postfix(badId) == -1);
        Require(Error() == Einval);
    }

    return 0;
}
