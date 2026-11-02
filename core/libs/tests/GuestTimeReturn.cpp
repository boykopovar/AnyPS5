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
    return 0;
}
