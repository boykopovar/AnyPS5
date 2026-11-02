#include "prx/libSceAudioLatencyEstimation/include/Api.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename TAction>
bool ThrowsMessage(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::logic_error& error) {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

}

int main() {
    Require(ThrowsMessage([] { sceAudioLatencyEstimationRelease(); }, "not initialized"));
    Require(ThrowsMessage([] { sceAudioLatencyEstimationAddData(); }, "not initialized"));
    Require(ThrowsMessage([] { sceAudioLatencyEstimationGetDelayTime(); }, "not initialized"));
    Require(sceAudioLatencyEstimationInitialize() == 0);
    Require(ThrowsMessage([] { sceAudioLatencyEstimationInitialize(); }, "already initialized"));
    Require(sceAudioLatencyEstimationRelease() == 0);
    Require(ThrowsMessage([] { sceAudioLatencyEstimationRelease(); }, "not initialized"));
    Require(ThrowsMessage([] { sceAudioLatencyEstimationAddData(); }, "not initialized"));
    Require(ThrowsMessage([] { sceAudioLatencyEstimationGetDelayTime(); }, "not initialized"));
    Require(sceAudioLatencyEstimationInitialize() == 0);
    Require(sceAudioLatencyEstimationRelease() == 0);
    return 0;
}
