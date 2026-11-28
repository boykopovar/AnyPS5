#include "prx/libSceAudioPropagation/AudioPropagation.hpp"
#include "prx/libSceAudioPropagation/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace {

bool isLiveSystem(AudioPropagationHandle system) {
    AudioPropagation::SystemInfo info{};
    return sceAudioPropagationSystemQueryInfo(system, &info, sizeof(info)) == 0;
}

}

extern "C" {

int APS5_VABI libSceAudioPropagationUnknown_TrQMW7ARZXY_nid_no_patch(AudioPropagationHandle system, AudioPropagationHandle* outMaterials, std::size_t size) {
    static_cast<void>(size);
    if (!outMaterials || !isLiveSystem(system)) {
        return AudioPropagation::ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TrQMW7ARZXY", libSceAudioPropagationUnknown_TrQMW7ARZXY_nid_no_patch);

int APS5_VABI sceAudioPropagationMaterialQueryInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioPropagationUnknown_fOTrQvqzABM_nid_no_patch(AudioPropagationHandle system, AudioPropagationHandle* outPortals, std::size_t size) {
    static_cast<void>(size);
    if (!outPortals || !isLiveSystem(system)) {
        return AudioPropagation::ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fOTrQvqzABM", libSceAudioPropagationUnknown_fOTrQvqzABM_nid_no_patch);

int APS5_VABI sceAudioPropagationAudioPathQueryInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioPropagationPortalQueryInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioPropagationUnknown_mdH4mANi9GI_nid_no_patch(AudioPropagationHandle system, AudioPropagationHandle* outRooms, std::uint32_t capacity) {
    static_cast<void>(outRooms);
    static_cast<void>(capacity);
    if (!isLiveSystem(system)) {
        return AudioPropagation::ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mdH4mANi9GI", libSceAudioPropagationUnknown_mdH4mANi9GI_nid_no_patch);

int APS5_VABI libSceAudioPropagationUnknown_o0gGIp1vd6A_nid_no_patch(AudioPropagationHandle system, AudioPropagationHandle* outSources, std::uint32_t capacity) {
    static_cast<void>(outSources);
    static_cast<void>(capacity);
    if (!isLiveSystem(system)) {
        return AudioPropagation::ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("o0gGIp1vd6A", libSceAudioPropagationUnknown_o0gGIp1vd6A_nid_no_patch);

}
