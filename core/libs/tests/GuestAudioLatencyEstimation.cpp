#include "prx/libSceAudioLatencyEstimation/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    Require(sceAudioLatencyEstimationInitialize() == 0);
    Require(sceAudioLatencyEstimationAddData() == 0);
    Require(sceAudioLatencyEstimationRelease() == 0);

    Require(sceAudioLatencyEstimationInitialize() == 0);
    Require(sceAudioLatencyEstimationAddData() == 0);
    Require(sceAudioLatencyEstimationRelease() == 0);

    return 0;
}
