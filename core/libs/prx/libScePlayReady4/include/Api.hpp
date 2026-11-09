#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePlayReadyApiPlatform();
int APS5_VABI scePlayReadyCdmiClose();
int APS5_VABI scePlayReadyCdmiCreateMediaKeySession();
int APS5_VABI scePlayReadyCdmiCreateMediaKeySystemAccess();
int APS5_VABI scePlayReadyCdmiCreateMediaKeys();
int APS5_VABI scePlayReadyCdmiDecryptMultipleOpaque();
int APS5_VABI scePlayReadyCdmiDecryptOpaque();
int APS5_VABI scePlayReadyCdmiDecryptOpaque2();
int APS5_VABI scePlayReadyCdmiDestroyMediaKeySession();
int APS5_VABI scePlayReadyCdmiDestroyMediaKeySystemAccess();
int APS5_VABI scePlayReadyCdmiDestroyMediaKeys();
int APS5_VABI scePlayReadyCdmiFreeOpaqueDecryptedContent();
int APS5_VABI scePlayReadyCdmiGenerateRequest();
int APS5_VABI scePlayReadyCdmiGetConfiguration();
int APS5_VABI scePlayReadyCdmiGetExpiration();
int APS5_VABI scePlayReadyCdmiGetKeySystem();
int APS5_VABI scePlayReadyCdmiGetMediaKeyCount();
int APS5_VABI scePlayReadyCdmiGetMediaKeyStatus();
int APS5_VABI scePlayReadyCdmiGetSessionAppContext();
int APS5_VABI scePlayReadyCdmiGetSessionId();
int APS5_VABI scePlayReadyCdmiIsLicenseAckRequired();
int APS5_VABI scePlayReadyCdmiLoad();
int APS5_VABI scePlayReadyCdmiRemove();
int APS5_VABI scePlayReadyCdmiRetrieveOpaqueDecryptedContent();
int APS5_VABI scePlayReadyCdmiSetServerCertificate();
int APS5_VABI scePlayReadyCdmiSetSessionCallbacks();
int APS5_VABI scePlayReadyCdmiUpdate();
int APS5_VABI scePlayReadyLicenseCount();
int APS5_VABI scePlayReadyProfileClear();
int APS5_VABI scePlayReadyProfileEnter();
int APS5_VABI scePlayReadyProfileLeave();
int APS5_VABI scePlayReadyProfilePause();
int APS5_VABI scePlayReadyProfileResume();
int APS5_VABI scePlayReadyProfileStart();
int APS5_VABI scePlayReadyProfileStop();

int APS5_VABI scePlayReadyCdmiProcessLicenseAckResponse();
int APS5_VABI scePlayReadyCdmiGetSessionId2();
int APS5_VABI scePlayReadyCdmiGenerateLicenseAckRequest();
int APS5_VABI scePlayReadyCdmiGenerateRequest2();
int APS5_VABI scePlayReadyDomainJoinProcessResponse2();

}
