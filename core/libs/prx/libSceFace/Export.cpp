#include "prx/libSceFace/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN

}
extern "C" {

int APS5_VABI sceFaceAgeRangeEstimate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceAgeRangeIntegrate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceAllParts() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceAllPartsGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceAttribute() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceAttributeGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceAgeGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceDetection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceDetectionEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceDetectionGetDefaultParam(void *param) {
    if (param == nullptr) {
        return 0x808b0002;
    }
    const std::uint32_t defaults[14] = {56u, 0x3f800000u, 0x3f574bc7u, 13200u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0x3f000000u, 1u, 0u};
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(param, defaults, sizeof(defaults));
    errno = savedErrno;
    return copied ? 0 : static_cast<int>(0x808b0002u);
}

int APS5_VABI sceFaceDetectionGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceDetectionLocal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceEstimatePoseRegion() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifyExGetFeature() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifyExGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifyExSimilarity() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifyGetFeature() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifyGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifyLiteGetFeature() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifyLiteGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifyLiteSimilarity() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceIdentifySimilarity() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceParts() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFacePartsEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFacePartsGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFacePartsResultCheck() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceShapeFit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceShapeGetWorkingMemorySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceShapeTrack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
