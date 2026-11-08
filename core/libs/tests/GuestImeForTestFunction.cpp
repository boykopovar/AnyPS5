#include "prx/libSceIme/include/Api.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

} // namespace

int main() {
    const int first = sceImeForTestFunction();
    const int second = sceImeForTestFunction();
    Require(first < 0, "sceImeForTestFunction did not answer an error");
    Require(first == second, "sceImeForTestFunction did not answer the same error on a repeated call");
    return 0;
}
