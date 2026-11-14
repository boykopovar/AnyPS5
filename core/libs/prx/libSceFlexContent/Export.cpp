#include "prx/libSceFlexContent/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x81B60001);

}

extern "C" {

int APS5_VABI sceFlexContentInitialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFlexContentTerminate(void) {
    return ErrorNotInitialized;
}

APS5_EXPORT("7n4PI+iTKws", sceFlexContentUnknown00);
int APS5_VABI sceFlexContentUnknown00(void) {
    return ErrorNotInitialized;
}

APS5_EXPORT("bgXJD9PYROo", sceFlexContentUnknown01);
int APS5_VABI sceFlexContentUnknown01(void) {
    return ErrorNotInitialized;
}

int APS5_VABI sceFlexContentMount(void) {
    return ErrorNotInitialized;
}

APS5_EXPORT("EP5ByjgbB2Y", sceFlexContentUnknown02);
int APS5_VABI sceFlexContentUnknown02(void) {
    return ErrorNotInitialized;
}

int APS5_VABI sceFlexContentGetDownloadProgress(void) {
    return ErrorNotInitialized;
}

int APS5_VABI sceFlexContentDelete(void) {
    return ErrorNotInitialized;
}

int APS5_VABI sceFlexContentGetLatestVersion(void) {
    return ErrorNotInitialized;
}

int APS5_VABI sceFlexContentGetInstalledVersion(void) {
    return ErrorNotInitialized;
}

APS5_EXPORT("TNwR02XaltA", sceFlexContentUnknown03);
int APS5_VABI sceFlexContentUnknown03(void) {
    return ErrorNotInitialized;
}

APS5_EXPORT("EQWWghng4OI", sceFlexContentUnknown04);
int APS5_VABI sceFlexContentUnknown04(void) {
    return ErrorNotInitialized;
}

}
