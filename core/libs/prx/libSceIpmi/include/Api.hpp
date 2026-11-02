#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceIpmiMgrGetIpcBlockerPid(void);
int APS5_VABI sceIpmiMgrGetIpcBlockerPidByAppId(void);
int APS5_VABI sceIpmiMgrPauseDump(void);
int APS5_VABI sceIpmiMgrRaiseExceptionToPid(void);
int APS5_VABI sceIpmiMgrStartDump(void);
int APS5_VABI _ZN4IPMI4impl11SessionImplC1Ev(void);
int APS5_VABI _ZN4IPMI6Client6Config14MsgQueueConfig20estimateMsgQueueSizeEmj(void);
int APS5_VABI _ZN4IPMI6Server12EventHandler21onAsyncMethodDispatchEPNS_7SessionEjjPvmS4_m(void);
int APS5_VABI _ZN4IPMI6Client6Config26SyncMethodInvocationConfigC1Ev(void);
int APS5_VABI _ZN4IPMI6Server12EventHandler20onSyncMethodDispatchEPNS_7SessionEjPvmmS4_m(void);
int APS5_VABI _ZN4IPMI7SessionD1Ev(void);
int APS5_VABI _ZN4IPMI4impl11SessionImpl9tryGetMsgEjPvPmm(void);
int APS5_VABI _ZN4IPMI6Server12EventHandler20onSyncMethodDispatchEPNS_7SessionEjPKNS_8DataInfoEjPNS_10BufferInfoEj(void);
int APS5_VABI _ZN4IPMI6Client6Config14MsgQueueConfigC1Ev(void);
int APS5_VABI _ZN4IPMI6ServerD2Ev(void);
int APS5_VABI _ZN4IPMI6ServerD0Ev(void);
int APS5_VABI _ZN4IPMI6Client6Config24estimateClientMemorySizeEv(void);
int APS5_VABI _ZN4IPMI6Server6createEPPS0_PKNS0_6ConfigEPvS6_(void);
int APS5_VABI _ZN4IPMI6Client6ConfigC2Ev(void);
int APS5_VABI _ZN4IPMI6Server12EventHandler15onSessionKilledEPNS_7SessionE(void);
int APS5_VABI _ZN4IPMI4impl11SessionImplD1Ev(void);
int APS5_VABI _ZN4IPMI4impl11SessionImpl27respondToAsyncMethodRequestEjiPKvm(void);
int APS5_VABI _ZN4IPMI6Client6Config27AsyncMethodInvocationConfigC2Ev(void);
int APS5_VABI _ZN4IPMI6Server12EventHandler21onAsyncMethodDispatchEPNS_7SessionEjjPKNS_8DataInfoEj(void);
int APS5_VABI _ZN4IPMI6ClientD2Ev(void);
int APS5_VABI _ZN4IPMI4impl11SessionImpl7sendMsgEjPKvmPj(void);
int APS5_VABI _ZN4IPMI6ClientC2Ev(void);
int APS5_VABI _ZN4IPMI7SessionD2Ev(void);
int APS5_VABI _ZNK4IPMI4impl11SessionImpl16isPeerPrivilegedEv(void);
int APS5_VABI _ZN4IPMI7SessionC2Ev(void);
int APS5_VABI _ZN4IPMI4impl11SessionImplD2Ev(void);
int APS5_VABI _ZN4IPMI6ClientD1Ev(void);
int APS5_VABI _ZN4IPMI6Server12EventHandlerD0Ev(void);
int APS5_VABI _ZN4IPMI7Session6Config25estimateSessionMemorySizeEv(void);
int APS5_VABI _ZNK4IPMI6Server6Config29estimateTempWorkingMemorySizeEv(void);
int APS5_VABI _ZN4IPMI4impl11SessionImplD0Ev(void);
int APS5_VABI _ZN4IPMI4impl11SessionImpl7destroyEv(void);
int APS5_VABI _ZN4IPMI6Client6Config27AsyncMethodInvocationConfigC1Ev(void);
int APS5_VABI _ZN4IPMI4impl11SessionImpl6getMsgEjPvPmmPj(void);
int APS5_VABI _ZN4IPMI4impl11SessionImpl27respondToAsyncMethodRequestEjjiPKNS_8DataInfoEj(void);
int APS5_VABI _ZN4IPMI6Server12EventHandlerD2Ev(void);
int APS5_VABI _ZN4IPMI6ClientD0Ev(void);
int APS5_VABI _ZN4IPMI6Client6createEPPS0_PKNS0_6ConfigEPvS6_(void);
int APS5_VABI _ZN4IPMI6ServerC2Ev(void);
int APS5_VABI _ZN4IPMI4impl11SessionImplC2Ev(void);
int APS5_VABI _ZN4IPMI7SessionD0Ev(void);
int APS5_VABI _ZN4IPMI6ServerD1Ev(void);
int APS5_VABI _ZN4IPMI6Server6ConfigC1Ev(void);
int APS5_VABI _ZN4IPMI6Server12EventHandlerD1Ev(void);
int APS5_VABI _ZN4IPMI4impl11SessionImpl9getServerEv(void);
int APS5_VABI _ZN4IPMI6Client6ConfigC1Ev(void);
int APS5_VABI _ZN4IPMI6Client6Config26SyncMethodInvocationConfigC2Ev(void);
int APS5_VABI _ZN4IPMI6Client6Config14MsgQueueConfigC2Ev(void);
int APS5_VABI _ZN4IPMI6Server6ConfigC2Ev(void);

}
