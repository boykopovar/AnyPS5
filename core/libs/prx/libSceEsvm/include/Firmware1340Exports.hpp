#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEsvmHandleProvisioningResponse();
int APS5_VABI sceEsvmMediaKeysDecryptAudioData();
int APS5_VABI sceEsvmGetMetrics();
int APS5_VABI sceEsvmUseCustomMusicCoreForDecoderMode();
int APS5_VABI sceEsvmGetServiceCertificateRequest();
int APS5_VABI sceEsvmGetProvisioningRequest();
int APS5_VABI sceEsvmIsProvisioned();
int APS5_VABI sceEsvmSetServiceCertificate();
int APS5_VABI sceEsvmParseAndLoadServiceCertificateResponse();
int APS5_VABI sceEsvmSetMediaKeys();
int APS5_VABI sceEsvmCreateMediaKeys2();

}
