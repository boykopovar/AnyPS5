#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEsvmCreateMediaKeys2_nid_no_patch(void);
int APS5_VABI sceEsvmGetMetrics_nid_no_patch(void);
int APS5_VABI sceEsvmGetProvisioningRequest_nid_no_patch(void);
int APS5_VABI sceEsvmGetServiceCertificateRequest_nid_no_patch(void);
int APS5_VABI sceEsvmHandleProvisioningResponse_nid_no_patch(void);
int APS5_VABI sceEsvmIsProvisioned_nid_no_patch(void);
int APS5_VABI sceEsvmMediaKeysDecryptAudioData_nid_no_patch(void);
int APS5_VABI sceEsvmParseAndLoadServiceCertificateResponse_nid_no_patch(void);
int APS5_VABI sceEsvmSetMediaKeys_nid_no_patch(void);
int APS5_VABI sceEsvmSetServiceCertificate_nid_no_patch(void);

}
