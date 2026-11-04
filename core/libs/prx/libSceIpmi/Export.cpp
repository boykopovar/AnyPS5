#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceIpmi/include/InventoryExports.hpp"
#include "prx/libSceIpmi/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI sceIpmiMgrGetIpcBlockerPid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIpmiMgrGetIpcBlockerPidByAppId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIpmiMgrPauseDump(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIpmiMgrRaiseExceptionToPid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIpmiMgrStartDump(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl10disconnectEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl10initializeEPKNS_6Client6ConfigEPv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl11getUserDataEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl12tryGetResultEjPiPvPmm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl12tryGetResultEjjPiPNS_10BufferInfoEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl13pollEventFlagEjmjPm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl13waitEventFlagEjmjPmPj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl16invokeSyncMethodEjPKNS_8DataInfoEjPiPNS_10BufferInfoEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl16invokeSyncMethodEjPKvmPiPvPmm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl17invokeAsyncMethodEjPKNS_8DataInfoEjPjPKNS_6Client12EventNotifeeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl17invokeAsyncMethodEjPKvmPjPKNS_6Client12EventNotifeeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl19terminateConnectionEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl6getMsgEjPvPmmPj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl7connectEPKvmPi() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl7destroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImpl9tryGetMsgEjPvPmm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImplC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImplC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImplD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImplD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ClientImplD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImpl11getUserDataEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImpl11tryDispatchEPvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImpl13createSessionEPPNS_7SessionEPvS5_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImpl13runDispatcherEPvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImpl18shutdownDispatcherEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImpl7destroyEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImplC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImplC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImplD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImplD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl10ServerImplD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl11SessionImpl10trySendMsgEjPKvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl11SessionImpl11getUserDataEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl11SessionImpl11tryDispatchEPvm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl11SessionImpl12getClientPidEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl11SessionImpl12setEventFlagEjm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl11SessionImpl13emptyMsgQueueEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl11SessionImpl26respondToSyncMethodRequestEiPKNS_10BufferInfoEj() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN4IPMI4impl11SessionImpl26respondToSyncMethodRequestEiPKvm() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceIpmiUnknown_fjPNqzuUop8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fjPNqzuUop8", libSceIpmiUnknown_fjPNqzuUop8_nid_no_patch);

int APS5_VABI libSceIpmiUnknown_gL4bo5h2nwU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gL4bo5h2nwU", libSceIpmiUnknown_gL4bo5h2nwU_nid_no_patch);

int APS5_VABI libSceIpmiUnknown_rq3L3xKnt3Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rq3L3xKnt3Y", libSceIpmiUnknown_rq3L3xKnt3Y_nid_no_patch);

}
