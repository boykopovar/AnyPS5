#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoCoreAbortRequest();
int APS5_VABI sceVideoCoreAcquireDecoderResource();
int APS5_VABI sceVideoCoreAcquireDecoderResourceForBeWrapper();
int APS5_VABI sceVideoCoreAddStream();
int APS5_VABI sceVideoCoreAsyncAddStream();
int APS5_VABI sceVideoCoreAsyncSendRequest();
int APS5_VABI sceVideoCoreCancelThumbnail();
int APS5_VABI sceVideoCoreChangeActiveStatus();
int APS5_VABI sceVideoCoreChangeAudioStream();
int APS5_VABI sceVideoCoreChangeDisplaySize();
int APS5_VABI sceVideoCoreChangeHdcpSetting();
int APS5_VABI sceVideoCoreChangeSubtitle();
int APS5_VABI sceVideoCoreDeleteStream();
int APS5_VABI sceVideoCoreGenerateThumbnail();
int APS5_VABI sceVideoCoreGenerateThumbnail2();
int APS5_VABI sceVideoCoreGetCanvasHandle();
int APS5_VABI sceVideoCoreGetPlayStatus();
int APS5_VABI sceVideoCoreGetPlayStatus2();
int APS5_VABI sceVideoCoreGetStreamUid();
int APS5_VABI sceVideoCoreGetThumbnail();
int APS5_VABI sceVideoCoreGetThumbnail2();
int APS5_VABI sceVideoCoreLaunchVideoCore();
int APS5_VABI sceVideoCoreMediaSourceAbortSourceBuffer();
int APS5_VABI sceVideoCoreMediaSourceAddSourceBuffer();
int APS5_VABI sceVideoCoreMediaSourceAppendData();
int APS5_VABI sceVideoCoreMediaSourceEndOfStream();
int APS5_VABI sceVideoCoreMediaSourceGetBufferedTimeRanges();
int APS5_VABI sceVideoCoreMediaSourceGetInputBufferEmptySize();
int APS5_VABI sceVideoCoreMediaSourceGetPlaybackQuality();
int APS5_VABI sceVideoCoreMediaSourceGetSourceBufferEvent();
int APS5_VABI sceVideoCoreMediaSourceRemoveSourceBuffer();
int APS5_VABI sceVideoCoreMediaSourceSetDuration();
int APS5_VABI sceVideoCoreMediaSourceSetTimestampOffset();
int APS5_VABI sceVideoCorePlayControl();
int APS5_VABI sceVideoCoreRegistEventCallback();
int APS5_VABI sceVideoCoreReleaseDecoderResource();
int APS5_VABI sceVideoCoreReleaseDecoderResourceForBeWrapper();
int APS5_VABI sceVideoCoreSetAudioMute();
int APS5_VABI sceVideoCoreSetAudioVolume();
int APS5_VABI sceVideoCoreSetFullScreen();
int APS5_VABI sceVideoCoreSetInitialInfo();
int APS5_VABI sceVideoCoreSetInitializeInfo();
int APS5_VABI sceVideoCoreSetPlaybackRange();
int APS5_VABI sceVideoCoreSetThumbnailInfo();
int APS5_VABI sceVideoCoreSyncAddStream();
int APS5_VABI sceVideoCoreSyncSendRequest();
int APS5_VABI sceVideoCoreUnregistEventCallback();

}
