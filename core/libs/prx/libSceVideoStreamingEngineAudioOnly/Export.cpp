#include "prx/libSceVideoStreamingEngineAudioOnly/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int sceVseErrorNotInitialized = static_cast<int>(0x8178000a);

}

extern "C" {

APS5_EXPORT("KAkO+-Xbfws", sceVideoStreamingEngineAudioOnlyUnknown00);
int APS5_VABI sceVideoStreamingEngineAudioOnlyUnknown00(void) {
    return sceVseErrorNotInitialized;
}

APS5_EXPORT("JOKi6pWGGAg", sceVideoStreamingEngineAudioOnlyUnknown01);
int APS5_VABI sceVideoStreamingEngineAudioOnlyUnknown01(void) {
    return sceVseErrorNotInitialized;
}

APS5_EXPORT("1dWjQLhvcIg", sceVideoStreamingEngineAudioOnlyUnknown02);
int APS5_VABI sceVideoStreamingEngineAudioOnlyUnknown02(void) {
    return sceVseErrorNotInitialized;
}

APS5_EXPORT("5Fg-EMN0TMw", sceVideoStreamingEngineAudioOnlyUnknown03);
int APS5_VABI sceVideoStreamingEngineAudioOnlyUnknown03(void) {
    return sceVseErrorNotInitialized;
}

}
