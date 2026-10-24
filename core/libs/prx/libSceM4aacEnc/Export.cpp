#include "prx/libSceM4aacEnc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int32_t APS5_VABI sceM4aacEncClearContext(int32_t m4aacEncHandle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncClearContextEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceM4aacEncCreateEncoder(int32_t channels, int32_t samplingRate, int32_t bitRate, int32_t inputFormat, int32_t outputFormat, int32_t *pInternalError) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncCreateEncoderEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceM4aacEncDeleteEncoder(int32_t m4aacEncHandle, int32_t *pInternalError) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncDeleteEncoderEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceM4aacEncEncode(int32_t m4aacEncHandle, void *pInput, uint32_t inputByte, void *pOutput, uint32_t *pOutputByte, int32_t *pInternalError) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncEncodeEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncEncodeWithPriority() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncEncodeWithPriorityEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceM4aacEncFlush(int32_t m4aacEncHandle, void *pOutput, uint32_t *pOutputByte, int32_t *pInternalError) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncFlushEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncFlushWithPriority() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncFlushWithPriorityEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncGetContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceM4aacEncTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
