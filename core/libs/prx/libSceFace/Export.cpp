#include "prx/libSceFace/include/Api.hpp"
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
    auto *out = static_cast<uint32_t *>(param);
    out[0x0] = 56;
    out[0x1] = 0x3f800000;
    out[0x2] = 0x3f574bc7;
    out[0x3] = 13200;
    out[0x4] = 0;
    out[0x5] = 0;
    out[0x6] = 0;
    out[0x7] = 2;
    out[0x8] = 2;
    out[0x9] = 0;
    out[0xa] = 0;
    out[0xb] = 0x3f000000;
    out[0xc] = 1;
    out[0xd] = 0;
    return 0;
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
