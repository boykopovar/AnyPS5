#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceDataTransferAbortFgTransfer();
int APS5_VABI sceDataTransferAbortRequest();
int APS5_VABI sceDataTransferAbortSearchPS4();
int APS5_VABI sceDataTransferCheckBgTransferRunning();
int APS5_VABI sceDataTransferGetFgTransferProgress();
int APS5_VABI sceDataTransferGetPrepareFgTransferProgress();
int APS5_VABI sceDataTransferInitialize();
int APS5_VABI sceDataTransferRequestGetAppInfoPS4();
int APS5_VABI sceDataTransferRequestGetSavedataInfoPS4();
int APS5_VABI sceDataTransferRequestGetUsersPS4();
int APS5_VABI sceDataTransferRequestLaunchPS4();
int APS5_VABI sceDataTransferRequestPrepareFgTransfer();
int APS5_VABI sceDataTransferRequestRebootAuthPS4();
int APS5_VABI sceDataTransferRequestSearchPS4();
int APS5_VABI sceDataTransferRequestStartFgTransfer();
int APS5_VABI sceDataTransferRequestTransferTimePS4();
int APS5_VABI sceDataTransferTargetAbortBindSavedata();
int APS5_VABI sceDataTransferTargetAbortDeactivate();
int APS5_VABI sceDataTransferTargetAbortEth0();
int APS5_VABI sceDataTransferTargetAbortGetDeviceInfo();
int APS5_VABI sceDataTransferTargetAbortGetDeviceInfoApplication();
int APS5_VABI sceDataTransferTargetAbortGetTitles();
int APS5_VABI sceDataTransferTargetAbortGetUsers();
int APS5_VABI sceDataTransferTargetAbortLaunch();
int APS5_VABI sceDataTransferTargetAbortPrepareTransfer();
int APS5_VABI sceDataTransferTargetAbortPwrReq();
int APS5_VABI sceDataTransferTargetAbortReboot();
int APS5_VABI sceDataTransferTargetAbortSendSsoNew2Old();
int APS5_VABI sceDataTransferTargetAbortSendSsoOld2New();
int APS5_VABI sceDataTransferTargetAbortTransfer();
int APS5_VABI sceDataTransferTargetAbortTransferSpeed();
int APS5_VABI sceDataTransferTargetEventIsAuthReady();
int APS5_VABI sceDataTransferTargetEventIsIPv6Ready();
int APS5_VABI sceDataTransferTargetEventIsPwrReqReady();
int APS5_VABI sceDataTransferTargetGetFailedUsers();
int APS5_VABI sceDataTransferTargetGetRebootData();
int APS5_VABI sceDataTransferTargetGetTransferProgress();
int APS5_VABI sceDataTransferTargetRequestAbortSearch();
int APS5_VABI sceDataTransferTargetRequestActivate();
int APS5_VABI sceDataTransferTargetRequestAuth();
int APS5_VABI sceDataTransferTargetRequestBindSavedata();
int APS5_VABI sceDataTransferTargetRequestComplete();
int APS5_VABI sceDataTransferTargetRequestCreateRebootData();
int APS5_VABI sceDataTransferTargetRequestDeactivate();
int APS5_VABI sceDataTransferTargetRequestEndTransfer();
int APS5_VABI sceDataTransferTargetRequestGetDeviceInfo();
int APS5_VABI sceDataTransferTargetRequestGetDeviceInfoApplication();
int APS5_VABI sceDataTransferTargetRequestGetTitles();
int APS5_VABI sceDataTransferTargetRequestGetUsers();
int APS5_VABI sceDataTransferTargetRequestLaunch();
int APS5_VABI sceDataTransferTargetRequestPrepareTransfer();
int APS5_VABI sceDataTransferTargetRequestPrepareTransferProgress();
int APS5_VABI sceDataTransferTargetRequestPwrReq();
int APS5_VABI sceDataTransferTargetRequestReboot();
int APS5_VABI sceDataTransferTargetRequestSearch();
int APS5_VABI sceDataTransferTargetRequestSendSsoNew2Old();
int APS5_VABI sceDataTransferTargetRequestSendSsoOld2New();
int APS5_VABI sceDataTransferTargetRequestStartTransfer();
int APS5_VABI sceDataTransferTargetRequestTransferSpeed();
int APS5_VABI sceDataTransferTerminate();

}
