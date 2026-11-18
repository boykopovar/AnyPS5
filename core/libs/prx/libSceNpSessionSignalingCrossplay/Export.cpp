#include "prx/libSceNpSessionSignalingCrossplay/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80553D03u);

} // namespace

extern "C" {

int APS5_VABI sceNpSessionSignalingCrossplayInitialize(const void* parameter) {
    if (parameter == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingCrossplayTerminate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionSignalingCrossplayCreateContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("SPHdC4zuXfY", sceNpSessionSignalingCrossplayUnknown00);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown00(void) {
    NotImplemented_nid_no_patch("SPHdC4zuXfY");
    return 0;
}

int APS5_VABI sceNpSessionSignalingCrossplayDestroyContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("2nvt1vCR3qo", sceNpSessionSignalingCrossplayUnknown01);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown01(void) {
    NotImplemented_nid_no_patch("2nvt1vCR3qo");
    return 0;
}

APS5_EXPORT("wypjyjGWaLM", sceNpSessionSignalingCrossplayUnknown02);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown02(std::uint64_t argument1, const void* argument2, std::uint64_t argument3, const void* argument4, void* argument5) {
    if (argument2 == nullptr || argument4 == nullptr || argument5 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch("wypjyjGWaLM");
    return 0;
}

APS5_EXPORT("eGv1EEmON+A", sceNpSessionSignalingCrossplayUnknown03);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown03(void) {
    NotImplemented_nid_no_patch("eGv1EEmON+A");
    return 0;
}

APS5_EXPORT("GMGOCumydk0", sceNpSessionSignalingCrossplayUnknown04);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown04(void) {
    NotImplemented_nid_no_patch("GMGOCumydk0");
    return 0;
}

APS5_EXPORT("LfFgtyCjdbs", sceNpSessionSignalingCrossplayUnknown05);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown05(void) {
    NotImplemented_nid_no_patch("LfFgtyCjdbs");
    return 0;
}

int APS5_VABI sceNpSessionSignalingCrossplayGetMemoryInfo(void* info) {
    if (info == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
