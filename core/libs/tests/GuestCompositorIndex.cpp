#include "prx/libSceComposite/libSceComposite.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    for (int i = 0; i < 16; ++i) Require(sceCompositorAllocateIndex() == i);
    Require(sceCompositorAllocateIndex() == -1);

    sceCompositorReleaseIndex(5);
    sceCompositorReleaseIndex(2);
    Require(sceCompositorAllocateIndex() == 2);
    Require(sceCompositorAllocateIndex() == 5);
    Require(sceCompositorAllocateIndex() == -1);

    sceCompositorReleaseIndex(16);
    sceCompositorReleaseIndex(0xffffffffu);
    Require(sceCompositorAllocateIndex() == -1);

    sceCompositorReleaseIndex(7);
    sceCompositorReleaseIndex(7);
    Require(sceCompositorAllocateIndex() == 7);
    Require(sceCompositorAllocateIndex() == -1);

    for (std::uint32_t i = 0; i < 16; ++i) sceCompositorReleaseIndex(i);
    Require(sceCompositorAllocateIndex() == 0);
    return 0;
}
