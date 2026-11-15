#pragma once

#include <cstdint>

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI MvpHttpAbort_nid_postfix(void);
int APS5_VABI MvpHttpDeInit_nid_postfix(void);
int APS5_VABI MvpHttpGet_nid_postfix(void);
int APS5_VABI MvpHttpGetLastError_nid_postfix(const void* context);
int APS5_VABI MvpHttpHead_nid_postfix(void);
int APS5_VABI MvpHttpHeadCleanup_nid_postfix(void);
int APS5_VABI MvpHttpInit_nid_postfix(void);
int APS5_VABI MvpHttpPost_nid_postfix(void);
int APS5_VABI MvpHttpSetCert_nid_postfix(void);
int APS5_VABI MvpHttpSetRecvTimeout_nid_postfix(void);
int APS5_VABI MvpHttpSetRedirectCb_nid_postfix(void);
int APS5_VABI sceAvPlayerStreamingRegisterVprinf();
int APS5_VABI sceTsCloseFile(void* file);
int APS5_VABI sceTsDisableRepresentation(void* file);
int APS5_VABI sceTsEnableRepresentation(void* file);
int APS5_VABI sceTsEnableStream(void* file);
int APS5_VABI sceTsGetAvailableBW(void* file, void* bandwidth);
int APS5_VABI sceTsGetLiveStatus(void* file, void* status);
int APS5_VABI sceTsGetNextAuInfo(void* file, void* info);
int APS5_VABI sceTsGetNextUnitData(void* file, void* data);
int APS5_VABI sceTsGetRepresentationCount(void* file);
int APS5_VABI sceTsGetRepresentationInfo(void);
int APS5_VABI sceTsGetSeekWindow(void* file, void* start, void* end);
int APS5_VABI sceTsGetStreamInfo(void* file, std::uint32_t index, void* info);
int APS5_VABI sceTsGetUrlEncryptionType(void* file);
int APS5_VABI sceTsJumpPTS(void* file);
int APS5_VABI sceTsOpenFile(void);
int APS5_VABI sceTsOpenFileEx(void);
int APS5_VABI sceTsReleaseBuffer(void);
int APS5_VABI sceTsRepresentationIsEnabled(void);
int APS5_VABI sceTsSetAvailableBW(void* file);
int APS5_VABI sceTsSetStartingBW(void* file);
int APS5_VABI sceTsStartFileStreaming(void* file, void* first, void* second);
int APS5_VABI sceTsStopFileStreaming(void* file);

}
