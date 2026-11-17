#include "prx/libSceIme/include/Api.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    const int first = sceImeForTestFunction();
    const int second = sceImeForTestFunction();
    Require(first < 0, "sceImeForTestFunction did not answer an error");
    Require(first == second, "sceImeForTestFunction did not answer the same error on a repeated call");
    return 0;
}
