#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceGameLiveStreamingInitialize(std::size_t heap_size);
int APS5_VABI sceGameLiveStreamingTerminate(void);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    bool threw = false;
    try {
        sceGameLiveStreamingInitialize(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    Require(sceGameLiveStreamingInitialize(0x100000) == 0);
    Require(sceGameLiveStreamingTerminate() == 0);
}
