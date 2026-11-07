#include "prx/libSceDepth/libSceDepth.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "DEPTH RECTIFICATION: %s\n", message);
    std::abort();
}

}

int main() {
    Require(sceDepthCalculateRectificationInformation() == 0, "sceDepthCalculateRectificationInformation result");
    Require(sceDepthGetRectificationInformation() == 0, "sceDepthGetRectificationInformation result");
    Require(sceDepthSetRectificationInformation() == 0, "sceDepthSetRectificationInformation result");
    std::puts("Depth rectification tests passed");
    return 0;
}
