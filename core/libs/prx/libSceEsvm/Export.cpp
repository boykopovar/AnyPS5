#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

}
extern "C" {

int APS5_VABI sceEsvmAppAudioSyncCreateContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmAppAudioSyncFlush() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmSyncVideo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
