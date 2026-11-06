#include "prx/libSceAudioLatencyEstimation/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioLatencyEstimationAddData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioLatencyEstimationGetDelayTime() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioLatencyEstimationInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioLatencyEstimationRelease() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
