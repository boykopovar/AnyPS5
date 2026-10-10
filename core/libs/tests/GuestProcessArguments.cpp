#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" int APS5_VABI getargc_nid_postfix(void);
extern "C" const char** APS5_VABI getargv_nid_postfix(void);

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    const char* const expected[] = {"plain", "two words", "ü日"};
    constexpr int expectedCount = 1 + static_cast<int>(sizeof(expected) / sizeof(expected[0]));
    const int count = getargc_nid_postfix();
    const char** values = getargv_nid_postfix();
    Require(count == expectedCount, "getargc does not count every command-line argument");
    Require(values != nullptr && values[0] != nullptr && values[0][0] != '\0', "getargv has no program name");
    for (int index = 1; index < expectedCount; ++index) {
        Require(values[index] != nullptr && std::strcmp(values[index], expected[index - 1]) == 0,
            "getargv does not return a command-line argument as UTF-8");
    }
    Require(values[count] == nullptr, "getargv is not terminated by a null pointer");
    return 0;
}
