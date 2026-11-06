#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI MvpHttpAbort_nid_postfix(void);
int APS5_VABI MvpHttpDeInit_nid_postfix(void);
int APS5_VABI MvpHttpGet_nid_postfix(void);
int APS5_VABI MvpHttpGetLastError_nid_postfix(void);
int APS5_VABI MvpHttpHead_nid_postfix(void);
int APS5_VABI MvpHttpHeadCleanup_nid_postfix(void);
int APS5_VABI MvpHttpInit_nid_postfix(void);
int APS5_VABI MvpHttpPost_nid_postfix(void);
int APS5_VABI MvpHttpSetCert_nid_postfix(void);
int APS5_VABI MvpHttpSetRecvTimeout_nid_postfix(void);
int APS5_VABI MvpHttpSetRedirectCb_nid_postfix(void);
int APS5_VABI sceAvPlayerStreamingRegisterVprinf();
int APS5_VABI sceTsCloseFile(void);
int APS5_VABI sceTsDisableRepresentation(void);
int APS5_VABI sceTsEnableRepresentation(void);
int APS5_VABI sceTsEnableStream(void);
int APS5_VABI sceTsGetAvailableBW(void);
int APS5_VABI sceTsGetLiveStatus(void);
int APS5_VABI sceTsGetNextAuInfo(void);
int APS5_VABI sceTsGetNextUnitData(void);
int APS5_VABI sceTsGetRepresentationCount(void);
int APS5_VABI sceTsGetRepresentationInfo(void);
int APS5_VABI sceTsGetSeekWindow(void);
int APS5_VABI sceTsGetStreamInfo(void);
int APS5_VABI sceTsGetUrlEncryptionType(void);
int APS5_VABI sceTsJumpPTS(void);
int APS5_VABI sceTsOpenFile(void);
int APS5_VABI sceTsOpenFileEx(void);
int APS5_VABI sceTsReleaseBuffer(void);
int APS5_VABI sceTsRepresentationIsEnabled(void);
int APS5_VABI sceTsSetAvailableBW(void);
int APS5_VABI sceTsSetStartingBW(void);
int APS5_VABI sceTsStartFileStreaming(void);
int APS5_VABI sceTsStopFileStreaming(void);

}
