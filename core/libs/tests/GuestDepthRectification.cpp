#include "prx/libSceDepth/libSceDepth.hpp"

#include <cerrno>
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
    errno = 37;
    for (int i = 0; i < 3; ++i) {
        Require(sceDepthCalculateRectificationInformation() == 0, "sceDepthCalculateRectificationInformation result");
        Require(sceDepthGetRectificationInformation() == 0, "sceDepthGetRectificationInformation result");
        Require(sceDepthSetRectificationInformation() == 0, "sceDepthSetRectificationInformation result");
        Require(errno == 37, "errno modified");
    }
    std::puts("Depth rectification tests passed");
    return 0;
}
