#include "prx/libSceCompanionUtil/libSceCompanionUtil.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "libSceCompanionUtil lifecycle check failed at line %u\n", location.line());
    std::abort();
}

}

int main() {
    Require(sceCompanionUtilGetRemoteOskEvent() == 0);
    Require(sceCompanionUtilInitialize() == 0);
    Require(sceCompanionUtilOptParamInitialize() == 0);
    Require(sceCompanionUtilTerminate() == 0);
    return 0;
}
