#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVdecCoreAnalyzeStream();
int APS5_VABI sceVdecCoreCreateDecoder();
int APS5_VABI sceVdecCoreDeleteDecoder();
int APS5_VABI sceVdecCoreFinalizeComputeResource();
int APS5_VABI sceVdecCoreFlushDecodeOutput();
int APS5_VABI sceVdecCoreGetDecodeOutput();
int APS5_VABI sceVdecCoreInitializeComputeResource();
int APS5_VABI sceVdecCoreMapMemoryBlock();
int APS5_VABI sceVdecCoreQueryComputeResourceInfo();
int APS5_VABI sceVdecCoreQueryFrameBufferInfo();
int APS5_VABI sceVdecCoreQueryInstanceSize();
int APS5_VABI sceVdecCoreResetDecoder();
int APS5_VABI sceVdecCoreSetDecodeInput();
int APS5_VABI sceVdecCoreSetDecodeOutputSw();
int APS5_VABI sceVdecCoreSyncDecode();
int APS5_VABI sceVdecCoreSyncDecodeOutputSw();
int APS5_VABI sceVdecCoreSyncDecodeWptr();
int APS5_VABI sceVdecCoreTrySyncDecode();
int APS5_VABI sceVdecCoreTrySyncDecodeOutputSw();
int APS5_VABI sceVdecCoreTrySyncDecodeWptr();

}
