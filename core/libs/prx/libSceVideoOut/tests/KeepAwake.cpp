#include "prx/libSceVideoOut/include/KeepAwake.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

int main() {
    Require(KeepAwakeEnabledFromValue(nullptr), "unset ANYPS5_KEEP_AWAKE did not keep the screen awake");
    Require(!KeepAwakeEnabledFromValue("0"), "ANYPS5_KEEP_AWAKE=0 did not turn keep-awake off");
    for (const char* value : {"", "1", "00", "off", "false", " 0", "0 ", "2", "yes"}) {
        Require(KeepAwakeEnabledFromValue(value), "a value other than 0 did not keep the screen awake");
    }
    return 0;
}
