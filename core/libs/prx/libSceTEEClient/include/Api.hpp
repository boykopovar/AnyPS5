#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI AMDTEE_DLM_FetchDebugStrings_nid_postfix(void);
int APS5_VABI AMDTEE_DLM_GetDebugToken_nid_postfix(void);
int APS5_VABI AMDTEE_DLM_StopTADebug_nid_postfix(void);
int APS5_VABI TEECI_InitASD_nid_postfix(void);
int APS5_VABI TEECI_SetTimeout_nid_postfix(void);
int APS5_VABI TEEC_CloseSession_nid_postfix(void);
int APS5_VABI TEEC_InvokeCommand_nid_postfix(void);
int APS5_VABI TEEC_OpenSession_nid_postfix(void);
int APS5_VABI TEEC_ReleaseSharedMemory_nid_postfix(void);
int APS5_VABI TEEC_RequestCancellation_nid_postfix(void);

}
