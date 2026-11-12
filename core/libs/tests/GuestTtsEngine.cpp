#include "prx/libSceTtsEngine/libSceTtsEngine.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x81A10004u);
    for (int call = 0; call < 2; ++call) {
        Require(sceTtsEngineUnknown00() == notInitialized);
        Require(sceTtsEngineUnknown01() == notInitialized);
        Require(sceTtsEngineSetText() == notInitialized);
        Require(sceTtsEngineGetAudio() == notInitialized);
        Require(sceTtsEngineClear() == notInitialized);
        Require(sceTtsEngineSetPitch() == notInitialized);
        Require(sceTtsEngineSetSpeed() == notInitialized);
        Require(sceTtsEngineSetVolume() == notInitialized);
        Require(sceTtsEngineSetDepth() == notInitialized);
        Require(sceTtsEngineTerminate() == 0);
        Require(sceTtsEngineInitialize() == 0);
        Require(sceTtsEngineSetText() == notInitialized);
    }
    return 0;
}
