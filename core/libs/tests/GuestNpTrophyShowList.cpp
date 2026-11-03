#include "prx/libSceNpTrophy/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void RequireThrows(auto function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (std::runtime_error const&) {
        return;
    }
    std::fprintf(stderr, "sceNpTrophyShowTrophyList did not throw at %s:%d\n", location.file_name(), location.line());
    std::abort();
}

}

int main() {
    RequireThrows([] { sceNpTrophyShowTrophyList(1, 2); });
    RequireThrows([] { sceNpTrophyShowTrophyList(0, 1); });
    RequireThrows([] { sceNpTrophyShowTrophyList(0, 0); });
    RequireThrows([] { sceNpTrophyShowTrophyList(-1, 0); });
    return 0;
}
