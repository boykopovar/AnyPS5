#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePlayReadyBufferInitWithData();
int APS5_VABI scePlayReadyCloneDecryptContext();
int APS5_VABI scePlayReadyDebugPrintf();
int APS5_VABI scePlayReadyDebugSetLevel();
int APS5_VABI scePlayReadyLicenseDelete();
int APS5_VABI scePlayReadyLicenseDeleteInMemory();
int APS5_VABI scePlayReadyLicenseGetExpirationTime();
int APS5_VABI scePlayReadyLicenseGetProperty();
int APS5_VABI scePlayReadyReaderClose();
int APS5_VABI scePlayReadyReaderDecryptMultiple();
int APS5_VABI scePlayReadyReaderDecryptMultipleOpaque();
int APS5_VABI scePlayReadyReaderDecryptOpaque();
int APS5_VABI scePlayReadyReaderDecrypt_Legacy();
int APS5_VABI scePlayReadyReaderFreeOpaqueContent();
int APS5_VABI scePlayReadyReaderRetrieveOpaqueContent();
int APS5_VABI scePlayReadyReinitialize();

int APS5_VABI scePlayReadyApiSetInitParamFlags();

}
