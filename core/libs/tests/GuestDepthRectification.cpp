#include "prx/libSceDepth/libSceDepth.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    Require(sceDepthCalculateRectificationInformation() == 0, "sceDepthCalculateRectificationInformation result");
    Require(sceDepthGetRectificationInformation() == 0, "sceDepthGetRectificationInformation result");
    Require(sceDepthSetRectificationInformation() == 0, "sceDepthSetRectificationInformation result");
    std::puts("Depth rectification tests passed");
    return 0;
}
