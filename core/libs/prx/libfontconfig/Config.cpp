#include "prx/libfontconfig/include/Config.hpp"

extern "C" {

FcConfig* APS5_VABI FcConfigCreate() {
    return nullptr;
}

FcConfig* APS5_VABI FcConfigReference(FcConfig*) {
    return nullptr;
}

void APS5_VABI FcConfigDestroy(FcConfig*) {
}

}
