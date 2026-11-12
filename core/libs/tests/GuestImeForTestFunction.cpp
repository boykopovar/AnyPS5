#include "prx/libSceIme/include/Api.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

}

int main() {
    constexpr int expected = static_cast<int>(0x80bc00ffu);
    for (int call = 0; call < 3; ++call) {
        const int result = sceImeForTestFunction();
        Require(result < 0, "sceImeForTestFunction did not answer an error");
        Require(result == expected, "sceImeForTestFunction did not answer 0x80bc00ff");
    }
    return 0;
}
