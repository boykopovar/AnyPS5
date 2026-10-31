#include "prx/libSceFrontPanelDisplay/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(sceFrontPanelDisplayInitialize() == 0);
    Require(sceFrontPanelDisplaySetOutputMode() == 0);

    return 0;
}
