#include "prx/libSceNKWeb/include/Api.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    Require(sceNKWebInitialize() == 0);
    Require(sceNKWebTerminate() == 0);
    return 0;
}
