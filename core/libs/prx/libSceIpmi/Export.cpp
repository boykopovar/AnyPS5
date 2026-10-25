#include "prx/libSceIpmi/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI sceIpmiMgrGetIpcBlockerPid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIpmiMgrGetIpcBlockerPidByAppId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIpmiMgrPauseDump(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIpmiMgrRaiseExceptionToPid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIpmiMgrStartDump(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
