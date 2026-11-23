#include "prx/libSceAppChecker/libSceAppChecker.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    Require(sceAppCheckerAbort() == 0);
    Require(sceAppCheckerExecute() == 0);
    Require(sceAppCheckerExecuteEx() == 0);
    return 0;
}
