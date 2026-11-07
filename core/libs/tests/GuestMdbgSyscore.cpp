#include "prx/libmdbg_syscore/libmdbg_syscore.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(sceDebugGetProcessEventCntlFlag() == 1);
    Require(sceDebugSetProcessEventCntlFlag() == 1);
    return 0;
}
