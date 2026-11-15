#include "prx/libSceGnmDriver/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}

int main() {
    Require(sceGnmAreSubmitsAllowed() == 1);
    Require(!sceGnmIsUserPaEnabled());
    Require(!sceGnmDriverCaptureInProgress());
    Require(!sceRazorIsLoaded());
    return 0;
}
