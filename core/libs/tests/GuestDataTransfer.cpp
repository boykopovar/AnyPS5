#include "prx/libSceDataTransfer/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

struct Export {
    const char* name;
    int (APS5_VABI* call)();
};

static const Export exports[] = {
    {"sceDataTransferAbortFgTransfer", sceDataTransferAbortFgTransfer},
    {"sceDataTransferAbortRequest", sceDataTransferAbortRequest},
    {"sceDataTransferAbortSearchPS4", sceDataTransferAbortSearchPS4},
    {"sceDataTransferCheckBgTransferRunning", sceDataTransferCheckBgTransferRunning},
    {"sceDataTransferGetFgTransferProgress", sceDataTransferGetFgTransferProgress},
    {"sceDataTransferGetPrepareFgTransferProgress", sceDataTransferGetPrepareFgTransferProgress},
    {"sceDataTransferInitialize", sceDataTransferInitialize},
    {"sceDataTransferRequestGetAppInfoPS4", sceDataTransferRequestGetAppInfoPS4},
    {"sceDataTransferRequestGetSavedataInfoPS4", sceDataTransferRequestGetSavedataInfoPS4},
    {"sceDataTransferRequestGetUsersPS4", sceDataTransferRequestGetUsersPS4},
    {"sceDataTransferRequestLaunchPS4", sceDataTransferRequestLaunchPS4},
    {"sceDataTransferRequestPrepareFgTransfer", sceDataTransferRequestPrepareFgTransfer},
    {"sceDataTransferRequestRebootAuthPS4", sceDataTransferRequestRebootAuthPS4},
    {"sceDataTransferRequestSearchPS4", sceDataTransferRequestSearchPS4},
    {"sceDataTransferRequestStartFgTransfer", sceDataTransferRequestStartFgTransfer},
    {"sceDataTransferRequestTransferTimePS4", sceDataTransferRequestTransferTimePS4},
    {"sceDataTransferTargetAbortBindSavedata", sceDataTransferTargetAbortBindSavedata},
    {"sceDataTransferTargetAbortDeactivate", sceDataTransferTargetAbortDeactivate},
    {"sceDataTransferTargetAbortEth0", sceDataTransferTargetAbortEth0},
    {"sceDataTransferTargetAbortGetDeviceInfo", sceDataTransferTargetAbortGetDeviceInfo},
    {"sceDataTransferTargetAbortGetDeviceInfoApplication", sceDataTransferTargetAbortGetDeviceInfoApplication},
    {"sceDataTransferTargetAbortGetTitles", sceDataTransferTargetAbortGetTitles},
    {"sceDataTransferTargetAbortGetUsers", sceDataTransferTargetAbortGetUsers},
    {"sceDataTransferTargetAbortLaunch", sceDataTransferTargetAbortLaunch},
    {"sceDataTransferTargetAbortPrepareTransfer", sceDataTransferTargetAbortPrepareTransfer},
    {"sceDataTransferTargetAbortPwrReq", sceDataTransferTargetAbortPwrReq},
    {"sceDataTransferTargetAbortReboot", sceDataTransferTargetAbortReboot},
    {"sceDataTransferTargetAbortSendSsoNew2Old", sceDataTransferTargetAbortSendSsoNew2Old},
    {"sceDataTransferTargetAbortSendSsoOld2New", sceDataTransferTargetAbortSendSsoOld2New},
    {"sceDataTransferTargetAbortTransfer", sceDataTransferTargetAbortTransfer},
    {"sceDataTransferTargetAbortTransferSpeed", sceDataTransferTargetAbortTransferSpeed},
    {"sceDataTransferTargetEventIsAuthReady", sceDataTransferTargetEventIsAuthReady},
    {"sceDataTransferTargetEventIsIPv6Ready", sceDataTransferTargetEventIsIPv6Ready},
    {"sceDataTransferTargetEventIsPwrReqReady", sceDataTransferTargetEventIsPwrReqReady},
    {"sceDataTransferTargetGetFailedUsers", sceDataTransferTargetGetFailedUsers},
    {"sceDataTransferTargetGetRebootData", sceDataTransferTargetGetRebootData},
    {"sceDataTransferTargetGetTransferProgress", sceDataTransferTargetGetTransferProgress},
    {"sceDataTransferTargetRequestAbortSearch", sceDataTransferTargetRequestAbortSearch},
    {"sceDataTransferTargetRequestActivate", sceDataTransferTargetRequestActivate},
    {"sceDataTransferTargetRequestAuth", sceDataTransferTargetRequestAuth},
    {"sceDataTransferTargetRequestBindSavedata", sceDataTransferTargetRequestBindSavedata},
    {"sceDataTransferTargetRequestComplete", sceDataTransferTargetRequestComplete},
    {"sceDataTransferTargetRequestCreateRebootData", sceDataTransferTargetRequestCreateRebootData},
    {"sceDataTransferTargetRequestDeactivate", sceDataTransferTargetRequestDeactivate},
    {"sceDataTransferTargetRequestEndTransfer", sceDataTransferTargetRequestEndTransfer},
    {"sceDataTransferTargetRequestGetDeviceInfo", sceDataTransferTargetRequestGetDeviceInfo},
    {"sceDataTransferTargetRequestGetDeviceInfoApplication", sceDataTransferTargetRequestGetDeviceInfoApplication},
    {"sceDataTransferTargetRequestGetTitles", sceDataTransferTargetRequestGetTitles},
    {"sceDataTransferTargetRequestGetUsers", sceDataTransferTargetRequestGetUsers},
    {"sceDataTransferTargetRequestLaunch", sceDataTransferTargetRequestLaunch},
    {"sceDataTransferTargetRequestPrepareTransfer", sceDataTransferTargetRequestPrepareTransfer},
    {"sceDataTransferTargetRequestPrepareTransferProgress", sceDataTransferTargetRequestPrepareTransferProgress},
    {"sceDataTransferTargetRequestPwrReq", sceDataTransferTargetRequestPwrReq},
    {"sceDataTransferTargetRequestReboot", sceDataTransferTargetRequestReboot},
    {"sceDataTransferTargetRequestSearch", sceDataTransferTargetRequestSearch},
    {"sceDataTransferTargetRequestSendSsoNew2Old", sceDataTransferTargetRequestSendSsoNew2Old},
    {"sceDataTransferTargetRequestSendSsoOld2New", sceDataTransferTargetRequestSendSsoOld2New},
    {"sceDataTransferTargetRequestStartTransfer", sceDataTransferTargetRequestStartTransfer},
    {"sceDataTransferTargetRequestTransferSpeed", sceDataTransferTargetRequestTransferSpeed},
    {"sceDataTransferTerminate", sceDataTransferTerminate},
    {"sceDataTransferAbortAuthPS5", sceDataTransferAbortAuthPS5},
    {"sceDataTransferAbortFgTransferPS5", sceDataTransferAbortFgTransferPS5},
    {"sceDataTransferAbortLaunchPS4", sceDataTransferAbortLaunchPS4},
    {"sceDataTransferAbortLaunchPS5", sceDataTransferAbortLaunchPS5},
    {"sceDataTransferAbortPS5Request", sceDataTransferAbortPS5Request},
    {"sceDataTransferAbortRequestBeforeFgTransferPS5", sceDataTransferAbortRequestBeforeFgTransferPS5},
    {"sceDataTransferAbortSearchPS5", sceDataTransferAbortSearchPS5},
    {"sceDataTransferEventIsAuthReady", sceDataTransferEventIsAuthReady},
    {"sceDataTransferEventIsPwrReqReady", sceDataTransferEventIsPwrReqReady},
    {"sceDataTransferEventIsAuthReadyPS4", sceDataTransferEventIsAuthReadyPS4},
    {"sceDataTransferEventIsPwrReqReadyPS4", sceDataTransferEventIsPwrReqReadyPS4},
    {"sceDataTransferEventIsPwrReqReadyPS5", sceDataTransferEventIsPwrReqReadyPS5},
    {"sceDataTransferGetFgTransferProgressPS5", sceDataTransferGetFgTransferProgressPS5},
    {"sceDataTransferGetPS4RebootData", sceDataTransferGetPS4RebootData},
    {"sceDataTransferHostNotifyEasySignInReady", sceDataTransferHostNotifyEasySignInReady},
    {"sceDataTransferRequestAuthPS4", sceDataTransferRequestAuthPS4},
    {"sceDataTransferRequestAuthPS5", sceDataTransferRequestAuthPS5},
    {"sceDataTransferRequestCalcTransferSizePS4", sceDataTransferRequestCalcTransferSizePS4},
    {"sceDataTransferRequestCalcTransferSizePS5", sceDataTransferRequestCalcTransferSizePS5},
    {"sceDataTransferRequestEndFgTransfer", sceDataTransferRequestEndFgTransfer},
    {"sceDataTransferRequestEndFgTransferPS5", sceDataTransferRequestEndFgTransferPS5},
    {"sceDataTransferRequestGetDeviceInfoPS4", sceDataTransferRequestGetDeviceInfoPS4},
    {"sceDataTransferRequestGetPS4Info", sceDataTransferRequestGetPS4Info},
    {"sceDataTransferRequestGetPS5Info", sceDataTransferRequestGetPS5Info},
    {"sceDataTransferRequestLaunchPS5", sceDataTransferRequestLaunchPS5},
    {"sceDataTransferRequestPrepareFgTransferPS5", sceDataTransferRequestPrepareFgTransferPS5},
    {"sceDataTransferRequestRebootAuthPS5", sceDataTransferRequestRebootAuthPS5},
    {"sceDataTransferRequestSearchPS5", sceDataTransferRequestSearchPS5},
    {"sceDataTransferRequestStartFgTransferPS5", sceDataTransferRequestStartFgTransferPS5},
    {"sceDataTransferRequestTransferTimePS5", sceDataTransferRequestTransferTimePS5},
};

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    for (const Export& entry : exports) {
        bool threw = false;
        try {
            entry.call();
        } catch (const std::runtime_error& error) {
            threw = std::string(error.what()) == std::string(entry.name) + " not implemented";
        }
        Require(threw);
    }
    return 0;
}
