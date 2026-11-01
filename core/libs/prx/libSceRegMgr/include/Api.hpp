#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceRegMgrGetBin(std::int64_t optionId, void* data, int size);
int APS5_VABI sceRegMgrGetInt(std::int64_t optionId, int32_t* outInt);
int APS5_VABI sceRegMgrGetStr(std::int64_t optionId, char* outString, int stringLength);
int APS5_VABI sceRegMgrSetBin(std::uint32_t optionId, const void* data, std::size_t size);
int APS5_VABI sceRegMgrSetInt(int32_t optionId, int32_t inInt);
int APS5_VABI sceRegMgrSetStr(int32_t optionId, const char* inStirng, std::size_t stringLength);
std::int32_t APS5_VABI sceRegMgrNonSysGetInt(std::int64_t key, std::int32_t unk, std::int32_t* data);
int APS5_VABI sceRegMgrCheckError(void);
int APS5_VABI sceRegMgrCheckIntVal(void);
int APS5_VABI sceRegMgrGetBinInitVal(void);
int APS5_VABI sceRegMgrGetIntInitVal(void);
int APS5_VABI sceRegMgrGetStrInitVal(void);
int APS5_VABI sceRegMgrGetVersion(void);
int APS5_VABI sceRegMgrIsChange(void);
int APS5_VABI sceRegMgrEvtGetCnt(void);
int APS5_VABI sceRegMgrEvtGetRegId(void);
int APS5_VABI sceRegMgrResetVal(void);
int APS5_VABI sceRegMgrSetInitLevel(void);
int APS5_VABI sceRegMgrSrvCnvRegionInt(void);
int APS5_VABI sceRegMgrSrvCnvRegionStr(void);
int APS5_VABI sceRegMgrSrvGetMachineType(void);
int APS5_VABI sceRegMgrSrvGetQAFforReg(void);
int APS5_VABI sceRegMgrSrvGetRealMachineType(void);
int APS5_VABI sceRegMgrSrvGetRegion(void);
int APS5_VABI sceRegMgrSrvGetRegionStr(void);
int APS5_VABI sceRegMgrBackupNeedMem(void);
int APS5_VABI sceRegMgrBackupPullData(void);
int APS5_VABI sceRegMgrBackupPushData(void);
int APS5_VABI sceRegMgrBackupPullDataGkp(void);
int APS5_VABI sceRegMgrBackupPushDataGkp(void);
int APS5_VABI sceRegMgrCntlStart(void);
int APS5_VABI sceRegMgrCntlDeleteReg(void);
int APS5_VABI sceRegMgrCntlGetFilesCount(void);
int APS5_VABI sceRegMgrCntlGetFileName(void);
int APS5_VABI sceRegMgrCntSetInitReinst(void);
int APS5_VABI sceRegMgrIsInitOK(void);
int APS5_VABI sceRegMgrGetInitError(void);
int APS5_VABI sceRegMgrRecoverRegNvs(void);
int APS5_VABI sceRegMgrNonSysCheckError(void);
int APS5_VABI sceRegMgrNonSysGetBin(void);
int APS5_VABI sceRegMgrNonSysGetStr(void);
int APS5_VABI sceRegMgrNonSysSetBin(void);
int APS5_VABI sceRegMgrNonSysSetInt(void);
int APS5_VABI sceRegMgrNonSysSetStr(void);
int APS5_VABI sceRegMgrGetVersionForPS4(void);
int APS5_VABI sceRegMgrBackupNeedMemForPS4(void);
int APS5_VABI sceRegMgrBackupPullDataForPS4(void);
int APS5_VABI sceRegMgrBackupPushDataForPS4(void);
int APS5_VABI sceRegMgrEvtGetCntForPS4(void);
int APS5_VABI sceRegMgrEvtGetRegIdForPS4(void);
int APS5_VABI sceRegMgrStrncpy(void);
int APS5_VABI sceRegMgrMemset(void);
int APS5_VABI sceRegMgrSetbin(void);
int APS5_VABI sceRegMgrToolGetUpdateCnt(void);
int APS5_VABI sceRegMgrToolGetEntryCnt(void);
int APS5_VABI sceRegMgrToolDataCheckGet(void);
int APS5_VABI sceRegMgrDrvGetUpdateCnt(void);
int APS5_VABI sceRegMgrDrvGetEntCnt(void);
int APS5_VABI sceRegMgrDrvDataOpen(void);
int APS5_VABI sceRegMgrDrvDataCheckGet(void);
int APS5_VABI sceRegMgrDrvDataClose(void);
int APS5_VABI sceRegMgrToolGetInfo(void);
int APS5_VABI sceRegMgrLogPull(void);
int APS5_VABI sceRegMgrCntlStopHdd(void);
int APS5_VABI sceRegMgrPrintInfo(void);
int APS5_VABI sceRegMgrLogStart(void);

}
