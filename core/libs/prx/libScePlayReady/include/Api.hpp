#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceBufferAlloc();
int APS5_VABI sceBufferDup();
int APS5_VABI sceBufferFree();
void APS5_VABI sceBufferInit(void* buffer);
int APS5_VABI sceBufferRealloc();
void APS5_VABI scePlayReadyApiInitializeInitParams(void* params);
int APS5_VABI scePlayReadyApiIsStarted();
int APS5_VABI scePlayReadyApiStart();
int APS5_VABI scePlayReadyApiStop();
int APS5_VABI scePlayReadyApiVersion(uint32_t index, uint64_t* version);
const char* APS5_VABI scePlayReadyApiVersionString(uint32_t index);
int APS5_VABI scePlayReadyBufferFree();
int APS5_VABI scePlayReadyBufferInit(void* buffer);
int APS5_VABI scePlayReadyBufferMalloc();
int APS5_VABI scePlayReadyBufferRealloc();
int APS5_VABI scePlayReadyContentGetProperty();
int APS5_VABI scePlayReadyContentSetProperty();
int APS5_VABI scePlayReadyDomainCertEnumFini();
int APS5_VABI scePlayReadyDomainCertEnumInit();
int APS5_VABI scePlayReadyDomainCertEnumNext();
int APS5_VABI scePlayReadyDomainCertFind();
int APS5_VABI scePlayReadyDomainJoinGenerateChallenge();
int APS5_VABI scePlayReadyDomainJoinProcessResponse();
int APS5_VABI scePlayReadyDomainLeaveGenerateChallenge();
int APS5_VABI scePlayReadyDomainLeaveProcessResponse();
int APS5_VABI scePlayReadyEnvelopeClose();
int APS5_VABI scePlayReadyEnvelopeGetSize();
int APS5_VABI scePlayReadyEnvelopeInitializeRead();
int APS5_VABI scePlayReadyEnvelopeOpen();
int APS5_VABI scePlayReadyEnvelopeRead();
int APS5_VABI scePlayReadyEnvelopeSeek();
int APS5_VABI scePlayReadyFinalize();
int APS5_VABI scePlayReadyFreeDecryptContext();
int APS5_VABI scePlayReadyInitialize();
void APS5_VABI scePlayReadyInitializeInitParams(void* params);
int APS5_VABI scePlayReadyLicenseAcqGenerateAck();
int APS5_VABI scePlayReadyLicenseAcqGenerateChallenge();
int APS5_VABI scePlayReadyLicenseAcqGetAdditionalResponseData();
int APS5_VABI scePlayReadyLicenseAcqProcessAckResponse();
int APS5_VABI scePlayReadyLicenseAcqProcessResponse();
int APS5_VABI scePlayReadyMemAlloc();
int APS5_VABI scePlayReadyMemFree();
int APS5_VABI scePlayReadyMemGetStats();
int APS5_VABI scePlayReadyMemRealloc();
int APS5_VABI scePlayReadyNewDecryptContext();
int APS5_VABI scePlayReadyReaderBind();
int APS5_VABI scePlayReadyReaderCommit();
int APS5_VABI scePlayReadyReaderDecrypt();
int APS5_VABI scePlayReadyReset();
int APS5_VABI scePlayReadyStoreCleanup();

}
