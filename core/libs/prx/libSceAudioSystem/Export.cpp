#include "prx/libSceAudioSystem/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioSystemSystemResume() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioSystemSystemSuspend() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioSystemInit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioSystemProcessAdd() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioSystemProcessRemove() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioSystemProcessResume() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioSystemProcessSuspend() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioSystemTerminate() {
    NotImplemented_nid_no_patch(__func__);
}

}
