#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

int main() {
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(0, 0) == static_cast<int>(0x8A6D0003u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(2, 0) == static_cast<int>(0x8A6D0003u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(-1, 0) == static_cast<int>(0x8A6D0003u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(1, 1) == static_cast<int>(0x8A6D0005u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(1, -1) == static_cast<int>(0x8A6D0005u));
    Require(sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(1, 0) == static_cast<int>(0x8A6D0003u));
    return 0;
}
