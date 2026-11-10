#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEsvmCreateMediaKeys2(void);
int APS5_VABI sceEsvmGetMetrics(void);
int APS5_VABI sceEsvmGetProvisioningRequest(void);
int APS5_VABI sceEsvmGetServiceCertificateRequest(void);
int APS5_VABI sceEsvmHandleProvisioningResponse(void);
int APS5_VABI sceEsvmIsProvisioned(void);
int APS5_VABI sceEsvmMediaKeysDecryptAudioData(void);
int APS5_VABI sceEsvmParseAndLoadServiceCertificateResponse(void);
int APS5_VABI sceEsvmSetMediaKeys(void);
int APS5_VABI sceEsvmSetServiceCertificate(void);

}
