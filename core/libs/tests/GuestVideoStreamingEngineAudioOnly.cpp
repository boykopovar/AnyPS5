#include "prx/libSceVideoStreamingEngineAudioOnly/libSceVideoStreamingEngineAudioOnly.hpp"
#include <cstdlib>

namespace {

constexpr int notInitialized = static_cast<int>(0x8178000a);

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(sceVideoStreamingEngineAudioOnlyUnknown00() == notInitialized);
    Require(sceVideoStreamingEngineAudioOnlyUnknown01() == notInitialized);
    Require(sceVideoStreamingEngineAudioOnlyUnknown02() == notInitialized);
    Require(sceVideoStreamingEngineAudioOnlyUnknown03() == notInitialized);
    return 0;
}
