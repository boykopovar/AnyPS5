#include "prx/libSceAvPlayerStreaming/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI MvpHttpAbort_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpDeInit_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpGet_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpGetLastError_nid_postfix(const void* context) {
    if (context == nullptr) return -1;
    std::int32_t error;
    std::memcpy(&error, static_cast<const unsigned char*>(context) + 0x30, sizeof(error));
    return error;
}

int APS5_VABI MvpHttpHead_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpHeadCleanup_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpInit_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpPost_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpSetCert_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpSetRecvTimeout_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI MvpHttpSetRedirectCb_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAvPlayerStreamingRegisterVprinf() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceTsCloseFile(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsDisableRepresentation(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsEnableRepresentation(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsEnableStream(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetAvailableBW(void* file, void* bandwidth) {
    if (file == nullptr || bandwidth == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetLiveStatus(void* file, void* status) {
    if (file == nullptr || status == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetNextAuInfo(void* file, void* info) {
    if (file == nullptr || info == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetNextUnitData(void* file, void* data) {
    if (file == nullptr || data == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetRepresentationCount(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetRepresentationInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetSeekWindow(void* file, void* start, void* end) {
    if (file == nullptr || start == nullptr || end == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetStreamInfo(void* file, std::uint32_t index, void* info) {
    if (file == nullptr || info == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsGetUrlEncryptionType(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsJumpPTS(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsOpenFile(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsOpenFileEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsReleaseBuffer(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsRepresentationIsEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsSetAvailableBW(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsSetStartingBW(void* file) {
    if (file == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsStartFileStreaming(void* file, void* first, void* second) {
    if (file == nullptr || first == nullptr || second == nullptr) return -1;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsStopFileStreaming(void* file) {
    if (file == nullptr) return static_cast<int>(0xF0000200u);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
