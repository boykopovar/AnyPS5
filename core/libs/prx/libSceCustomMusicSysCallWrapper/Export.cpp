#include "prx/libSceCustomMusicSysCallWrapper/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceCustomMusicNetInetNtop() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCustomMusicNetShutdown() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCustomMusicNetSocketAbort() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCustomMusicPthreadAttrGet() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCustomMusicPthreadSelf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCustomMusicSelect() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
