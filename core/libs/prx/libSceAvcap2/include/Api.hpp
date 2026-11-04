#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAvcap2Initialize(void);
int APS5_VABI sceAvcap2Terminate(void);
int APS5_VABI sceAvcap2OpenAudio(void);
int APS5_VABI sceAvcap2Close(void);
int APS5_VABI sceAvcap2Start(void);
int APS5_VABI sceAvcap2Stop(void);
int APS5_VABI sceAvcap2ReadAudio(void);

int APS5_VABI sceAvcap2AllocateVideoEncoderMemory();
int APS5_VABI sceAvcap2AllocateVideoEncoderMemory2();
int APS5_VABI sceAvcap2FreeEncoderVideoMemory();
int APS5_VABI sceAvcap2GetAudioOutVolume();
int APS5_VABI sceAvcap2GetFramePitch();
int APS5_VABI sceAvcap2GetImagePasteMemory();
int APS5_VABI sceAvcap2GetMicVolume();
int APS5_VABI sceAvcap2GetNonVclNalUnitStream();
int APS5_VABI sceAvcap2GetNonVclNalUnitStream2();
int APS5_VABI sceAvcap2GetNonVclNalUnitStream3();
int APS5_VABI sceAvcap2GetRecProhibit();
int APS5_VABI sceAvcap2GetSupportInformation();
int APS5_VABI sceAvcap2GetVideoOutMode();
int APS5_VABI sceAvcap2LockInputDataBuffer();
int APS5_VABI sceAvcap2MapMemory();
int APS5_VABI sceAvcap2OpenPadSpk();
int APS5_VABI sceAvcap2OpenVideo();
int APS5_VABI sceAvcap2QueryVideoEncoderMemorySize();
int APS5_VABI sceAvcap2ReadPadSpk();
int APS5_VABI sceAvcap2ReadVideo();
int APS5_VABI sceAvcap2ReadVideo2();
int APS5_VABI sceAvcap2RequestKeyFrame();
int APS5_VABI sceAvcap2Select();
int APS5_VABI sceAvcap2SetAudioOutVolume();
int APS5_VABI sceAvcap2SetAudioOutVolumeForRec();
int APS5_VABI sceAvcap2SetChapterOpenLevel();
int APS5_VABI sceAvcap2SetChapterPermissionLevel();
int APS5_VABI sceAvcap2SetImagePaste();
int APS5_VABI sceAvcap2SetInvalidFrame();
int APS5_VABI sceAvcap2SetMicVolume();
int APS5_VABI sceAvcap2SetMicVolumeForRec();
int APS5_VABI sceAvcap2SetPrivacyGuard();
int APS5_VABI sceAvcap2SetPrivacyImage();
int APS5_VABI sceAvcap2SetVideoEncodeConfig();
int APS5_VABI sceAvcap2UnlockInputDataBuffer();

}
