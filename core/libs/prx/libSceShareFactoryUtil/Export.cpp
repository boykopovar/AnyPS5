#include "prx/libSceShareFactoryUtil/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceShareFactoryUtilBackToGameFromShareFactory() {
    return 0;
}

int APS5_VABI sceShareFactoryUtilGetLaunchStatus() {
    return 0;
}

int APS5_VABI sceShareFactoryUtilGetLaunchStatusOfShareFactory() {
    return 0;
}

int APS5_VABI sceShareFactoryUtilInitialize() {
    return 0;
}

int APS5_VABI sceShareFactoryUtilLaunchShareFactory() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareFactoryUtilTerminate() {
    return 0;
}

}
