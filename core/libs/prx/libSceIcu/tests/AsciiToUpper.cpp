#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU ASCII uppercase check failed at line %u\n", location.line());
    std::exit(EXIT_FAILURE);
}

}

int main() {
    for (unsigned i = 0; i <= 65535; ++i)
        Require(u_asciiToUpper_nid_postfix(static_cast<char16_t>(i)) == (i >= 'a' && i <= 'z' ? i - 32 : i));
    std::puts("ICU ASCII uppercase tests passed");
}
