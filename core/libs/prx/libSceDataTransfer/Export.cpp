#include "prx/libSceDataTransfer/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int DataTransferNotInitialized = static_cast<int>(0x81620000);

} // namespace

extern "C" {

int APS5_VABI sceDataTransferAbortFgTransfer() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferAbortRequest() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferAbortSearchPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferCheckBgTransferRunning() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferGetFgTransferProgress() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferGetPrepareFgTransferProgress() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferRequestGetAppInfoPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestGetSavedataInfoPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestGetUsersPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestLaunchPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestPrepareFgTransfer() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestRebootAuthPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestSearchPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestStartFgTransfer() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestTransferTimePS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferTargetAbortBindSavedata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortDeactivate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortEth0() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortGetDeviceInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortGetDeviceInfoApplication() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortGetTitles() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortGetUsers() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortLaunch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortPrepareTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortPwrReq() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortReboot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortSendSsoNew2Old() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortSendSsoOld2New() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetAbortTransferSpeed() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetEventIsAuthReady() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetEventIsIPv6Ready() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetEventIsPwrReqReady() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetGetFailedUsers() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetGetRebootData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetGetTransferProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestAbortSearch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestActivate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestAuth() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestBindSavedata() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestComplete() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestCreateRebootData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestDeactivate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestEndTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestGetDeviceInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestGetDeviceInfoApplication() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestGetTitles() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestGetUsers() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestLaunch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestPrepareTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestPrepareTransferProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestPwrReq() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestReboot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestSearch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestSendSsoNew2Old() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestSendSsoOld2New() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestStartTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTargetRequestTransferSpeed() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferAbortAuthPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferAbortFgTransferPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferAbortLaunchPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferAbortLaunchPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferAbortPS5Request() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferAbortRequestBeforeFgTransferPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferAbortSearchPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferEventIsAuthReadyPS4() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceDataTransferEventIsPwrReqReadyPS4() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceDataTransferEventIsPwrReqReadyPS5() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceDataTransferGetFgTransferProgressPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferGetPS4RebootData() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferHostNotifyEasySignInReady() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestAuthPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestAuthPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestCalcTransferSizePS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestCalcTransferSizePS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestEndFgTransfer() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestEndFgTransferPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestGetDeviceInfoPS4() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestGetPS4Info() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestGetPS5Info() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestLaunchPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestPrepareFgTransferPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestRebootAuthPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestSearchPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestStartFgTransferPS5() {
    return DataTransferNotInitialized;
}

int APS5_VABI sceDataTransferRequestTransferTimePS5() {
    return DataTransferNotInitialized;
}

}

extern "C" {

int APS5_VABI sceDataTransferEventIsAuthReady() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDataTransferEventIsPwrReqReady() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
