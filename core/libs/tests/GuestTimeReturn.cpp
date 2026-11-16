#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static std::int64_t HostSeconds() {
    return static_cast<std::int64_t>(std::time(nullptr));
}

int main() {
    auto before = HostSeconds();
    const std::int64_t now = time_nid_postfix(nullptr);
    auto after = HostSeconds();
    Require(now >= before && now <= after, "time(NULL) is between two host clock reads");

    constexpr std::int64_t sentinel = 0x5a5a5a5a5a5a5a5aLL;
    std::int64_t stored = sentinel;
    before = HostSeconds();
    const std::int64_t returned = time_nid_postfix(&stored);
    after = HostSeconds();
    Require(stored == returned, "time(&t) stores the value it returns");
    Require(returned >= before && returned <= after, "time(&t) is between two host clock reads");

    std::int64_t viaAlias = sentinel;
    before = HostSeconds();
    const std::int64_t aliasReturned = libc_time_nid_postfix(&viaAlias);
    after = HostSeconds();
    Require(viaAlias == aliasReturned, "libc_time(&t) stores the value it returns");
    Require(aliasReturned >= before && aliasReturned <= after, "libc_time(&t) is between two host clock reads");

    *__error_nid_postfix() = 61;
    before = HostSeconds();
    const std::int64_t nullTime = time_nid_postfix(nullptr);
    after = HostSeconds();
    Require(nullTime >= before && nullTime <= after, "time(NULL) still reads host seconds");
    Require(nullTime >= aliasReturned, "time(NULL) is monotonic across calls");
    Require(*__error_nid_postfix() == 61, "time(NULL) preserves guest errno");

    before = HostSeconds();
    const std::int64_t aliasNull = libc_time_nid_postfix(nullptr);
    after = HostSeconds();
    Require(aliasNull >= before && aliasNull <= after, "libc_time(NULL) is between two host clock reads");
    Require(aliasNull >= nullTime, "libc_time(NULL) is non-decreasing");
    Require(*__error_nid_postfix() == 61, "libc_time(NULL) preserves guest errno");

    std::int64_t checked = sentinel;
    const std::int64_t storedAgain = time_nid_postfix(&checked);
    Require(checked == storedAgain && storedAgain >= aliasNull, "time(&t) remains non-decreasing");
    Require(*__error_nid_postfix() == 61, "time(&t) preserves guest errno");

    std::int64_t aliasChecked = sentinel;
    const std::int64_t aliasStoredAgain = libc_time_nid_postfix(&aliasChecked);
    Require(aliasChecked == aliasStoredAgain && aliasStoredAgain >= storedAgain, "libc_time(&t) remains non-decreasing");
    Require(*__error_nid_postfix() == 61, "libc_time(&t) preserves guest errno");
    return 0;
}
