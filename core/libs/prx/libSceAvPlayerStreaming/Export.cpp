#include "prx/libSceAvPlayerStreaming/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void* g_vprintfCallback = nullptr;

}

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
    std::int32_t error = 0;
    const bool copied = GuestCopyIn_nid_no_patch(&error, static_cast<const unsigned char*>(context) + 0x30, sizeof(error));
    if (!copied) return -1;
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

int APS5_VABI sceAvPlayerStreamingRegisterVprinf(void* fn) {
    g_vprintfCallback = fn;
    return 0;
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

int APS5_VABI sceTsGetRepresentationInfo(void* ts, std::uint32_t a2, std::uint32_t a3, void* a4) {
    (void)a2;
    (void)a3;
    if (ts == nullptr || a4 == nullptr) return -1;
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
    return 0;
}

int APS5_VABI sceTsOpenFileEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTsReleaseBuffer(void* ts, void* b) {
    (void)ts;
    if (b == nullptr) {
        return 0;
    }
    std::uint64_t zero64 = 0;
    std::uint32_t zero32 = 0;
    if (!GuestCopyOut_nid_no_patch(b, &zero64, sizeof(zero64)) ||
        !GuestCopyOut_nid_no_patch(static_cast<char*>(b) + 8, &zero32, sizeof(zero32))) {
        return -1;
    }
    return 0;
}

int APS5_VABI sceTsRepresentationIsEnabled(void* ts, std::uint32_t a2, std::uint32_t a3, void* a4) {
    (void)a2;
    (void)a3;
    if (ts == nullptr || a4 == nullptr) return -1;
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
