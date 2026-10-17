#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);

extern "C" {

int SceNpUtilityModuleLoaded_nid_no_patch = 1;

int APS5_VABI sceNpBandwidthTestAbort(int context_id) {
    (void)context_id;
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpBandwidthTestGetStatus(int context_id, int* status) {
    (void)context_id;
    if (status) *status = 0;
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpBandwidthTestInitStartDownload(const void* param) {
    (void)param;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpBandwidthTestInitStartUpload(const void* param) {
    (void)param;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpBandwidthTestShutdown(int context_id, void* result) {
    (void)context_id;
    (void)result;
    return SCE_NP_ERROR_INVALID_ARGUMENT;
}

}

extern "C" {

int APS5_VABI sceNpBandwidthTestInitStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpLookupCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpLookupCreateTitleCtx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpLookupDeleteRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpLookupDeleteTitleCtx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpLookupNpId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterCensorComment() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterCreateAsyncRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterCreateTitleCtx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterDeleteRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterDeleteTitleCtx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterPollAsync() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWordFilterSanitizeComment() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
