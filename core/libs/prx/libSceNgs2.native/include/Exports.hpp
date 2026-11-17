#pragma once

#include "prx/libSceNgs2.native/include/Api.hpp"
#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"
#include "prx/libSceNgs2/include/Api.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {
int APS5_VABI sceNgs2ParseWaveformFile(const char*, uint32_t, Ngs2WaveformInfo*);
int APS5_VABI sceNgs2CalcWaveformBlock(const Ngs2WaveformFormat*, uint32_t, uint32_t, Ngs2WaveformBlock*);
int APS5_VABI sceNgs2GetWaveformFrameInfo(const Ngs2WaveformFormat*, uint32_t*, uint32_t*, uint32_t*, uint32_t*);
int APS5_VABI sceNgs2SystemResetOption(Ngs2SystemOption*);
int APS5_VABI sceNgs2SystemQueryBufferSize(const Ngs2SystemOption*, Ngs2ContextBufferInfo*);
int APS5_VABI sceNgs2SystemCreate(const Ngs2SystemOption*, const Ngs2ContextBufferInfo*, uintptr_t*);
int APS5_VABI sceNgs2SystemCreateWithAllocator(const Ngs2SystemOption*, const Ngs2BufferAllocator*, uintptr_t*);
int APS5_VABI sceNgs2SystemDestroy(uintptr_t, Ngs2ContextBufferInfo*);
int APS5_VABI sceNgs2SystemGetInfo(uintptr_t, Ngs2SystemInfo*, size_t);
int APS5_VABI sceNgs2SystemSetGrainSamples(uintptr_t, uint32_t);
int APS5_VABI sceNgs2SystemSetSampleRate(uintptr_t, uint32_t);
int APS5_VABI sceNgs2SystemSetUserData(uintptr_t, uintptr_t);
int APS5_VABI sceNgs2SystemGetUserData(uintptr_t, uintptr_t*);
int APS5_VABI sceNgs2SystemLock(uintptr_t);
int APS5_VABI sceNgs2SystemUnlock(uintptr_t);
int APS5_VABI sceNgs2SystemRender(uintptr_t, const Ngs2RenderBufferInfo*, uint32_t);
int APS5_VABI sceNgs2PanInit(Ngs2PanWork*, const float*, float, uint32_t);
int APS5_VABI sceNgs2PanGetVolumeMatrix(Ngs2PanWork*, const Ngs2PanParam*, uint32_t, uint32_t, float*);
int APS5_VABI sceNgs2RackQueryBufferSize(uint32_t, const Ngs2RackOption*, Ngs2ContextBufferInfo*);
int APS5_VABI sceNgs2RackLock(uintptr_t);
int APS5_VABI sceNgs2RackUnlock(uintptr_t);
int APS5_VABI sceNgs2RackCreate(uintptr_t, uint32_t, const Ngs2RackOption*, const Ngs2ContextBufferInfo*, uintptr_t*);
int APS5_VABI sceNgs2RackCreateWithAllocator(uintptr_t, uint32_t, const Ngs2RackOption*, const Ngs2BufferAllocator*, uintptr_t*);
int APS5_VABI sceNgs2RackDestroy(uintptr_t, Ngs2ContextBufferInfo*);
int APS5_VABI sceNgs2RackGetVoiceHandle(uintptr_t, uint32_t, uintptr_t*);
int APS5_VABI sceNgs2RackGetInfo(uintptr_t, Ngs2RackInfo*, size_t);
int APS5_VABI sceNgs2RackSetUserData(uintptr_t, uintptr_t);
int APS5_VABI sceNgs2RackGetUserData(uintptr_t, uintptr_t*);
int APS5_VABI sceNgs2VoiceControl(uintptr_t, const Ngs2VoiceParamHeader*);
int APS5_VABI sceNgs2VoiceRunCommands(uintptr_t, const Ngs2VoiceCommand*, size_t);
int APS5_VABI sceNgs2VoiceGetState(uintptr_t, Ngs2VoiceState*, size_t);
int APS5_VABI sceNgs2VoiceGetStateFlags(uintptr_t, uint32_t*);
int APS5_VABI sceNgs2VoiceGetOwner(uintptr_t, uintptr_t*, uint32_t*);
int APS5_VABI sceNgs2VoiceGetMatrixInfo(uintptr_t, uint32_t, Ngs2VoiceMatrixInfo*, size_t);
int APS5_VABI sceNgs2CustomRackGetModuleInfo(uintptr_t, uint32_t, Ngs2CustomModuleInfo*, size_t);
int APS5_VABI sceNgs2VoiceGetPortInfo(uintptr_t, uint32_t, Ngs2VoicePortInfo*, size_t);
int APS5_VABI sceNgs2VoiceQueryInfo(uintptr_t, uint32_t, void*, size_t);
int APS5_VABI sceNgs2GeomResetListenerParam(Ngs2GeomListenerParam*);
int APS5_VABI sceNgs2GeomCalcListener(const Ngs2GeomListenerParam*, Ngs2GeomListenerWork*, std::uint32_t);
int APS5_VABI sceNgs2GeomResetSourceParam(Ngs2GeomSourceParam*);
int APS5_VABI sceNgs2GeomApply(const Ngs2GeomListenerWork*, const Ngs2GeomSourceParam*, Ngs2GeomAttribute*, std::uint32_t);
int APS5_VABI sceNgs2ReportRegisterHandler(std::uint32_t, Ngs2ReportHandler, std::uintptr_t, Ngs2Handle*);
int APS5_VABI sceNgs2ReportUnregisterHandler(Ngs2Handle);
}
