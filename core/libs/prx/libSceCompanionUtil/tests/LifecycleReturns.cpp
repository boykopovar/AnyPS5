#include "prx/libSceCompanionUtil/libSceCompanionUtil.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

int main() {
    Require(sceCompanionUtilGetRemoteOskEvent() == 0);
    Require(sceCompanionUtilInitialize() == 0);
    Require(sceCompanionUtilOptParamInitialize() == 0);
    Require(sceCompanionUtilTerminate() == 0);
    return 0;
}
