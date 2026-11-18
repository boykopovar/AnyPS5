#include "prx/libSceSystemLogger2Game/libSceSystemLogger2Game.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

struct Timestamp {
    unsigned long long tick;
    char clockSource[16];
};

}

int main() {
    Require(sceSystemLogger2GameGetTimestamp(nullptr) == static_cast<int>(0x818C0002u));
    Require(sceSystemLogger2GameSetEvent(nullptr) == static_cast<int>(0x818C0002u));

    Timestamp ts{};
    Require(sceSystemLogger2GameGetTimestamp(&ts) == 0);
    Require(ts.tick != 0);

    int dummyEvent = 1;
    Require(sceSystemLogger2GameSetEvent(&dummyEvent) == static_cast<int>(0x818C000Eu));
    return 0;
}
