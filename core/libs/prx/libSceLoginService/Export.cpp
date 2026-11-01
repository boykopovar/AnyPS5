#include "prx/libSceLoginService/include/Api.hpp"
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceLoginServiceInitialize(void) {
    return 0;
}

int APS5_VABI sceLoginServiceTerminate(void) {
    return 0;
}

int APS5_VABI sceLoginServiceRequestDevices(const LoginServiceDeviceRequest* param) {
    (void)param;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
