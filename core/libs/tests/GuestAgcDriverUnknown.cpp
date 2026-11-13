#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "AgcDriver unknown-export check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(0, 0) == static_cast<int>(0x8A6D0003u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(2, 0) == static_cast<int>(0x8A6D0003u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(-1, 0) == static_cast<int>(0x8A6D0003u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(1, 1) == static_cast<int>(0x8A6D0005u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(1, -1) == static_cast<int>(0x8A6D0005u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(1, 0) == static_cast<int>(0x8A6D0003u));
    return 0;
}
