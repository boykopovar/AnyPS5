#pragma once

#include "prx/libSceAvPlayer/include/AvPlayer.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

extern "C" {

int APS5_VABI sceAvPlayerAddSource(AvPlayerInternal* h, const char* filename);
int APS5_VABI sceAvPlayerAddSourceEx(AvPlayerInternal* h, uint32_t uriType, const AvPlayerSourceDetails* sourceDetails);
int APS5_VABI sceAvPlayerChangeStream(AvPlayerInternal* h, uint32_t oldStreamId, uint32_t newStreamId);
int APS5_VABI sceAvPlayerClose(AvPlayerInternal* h);
uint64_t APS5_VABI sceAvPlayerCurrentTime(AvPlayerInternal* h);
int APS5_VABI sceAvPlayerDisableStream(AvPlayerInternal* h, uint32_t streamId);
int APS5_VABI sceAvPlayerEnableStream(AvPlayerInternal* h, uint32_t streamId);
Bool APS5_VABI sceAvPlayerGetAudioData(AvPlayerInternal* h, AvPlayerFrameInfo* audioInfo);
int APS5_VABI sceAvPlayerGetStreamInfo(AvPlayerInternal* h, uint32_t streamId, AvPlayerStreamInfo* info);
int APS5_VABI sceAvPlayerGetStreamInfoEx(AvPlayerInternal* h, uint32_t streamId, AvPlayerStreamInfoEx* info);
Bool APS5_VABI sceAvPlayerGetVideoData(AvPlayerInternal* h, AvPlayerFrameInfo* videoInfo);
Bool APS5_VABI sceAvPlayerGetVideoDataEx(AvPlayerInternal* h, AvPlayerFrameInfoEx* videoInfo);
AvPlayerInternal* APS5_VABI sceAvPlayerInit(AvPlayerInitData* init);
int APS5_VABI sceAvPlayerInitEx(const AvPlayerInitDataEx* initEx, AvPlayerInternal** handle);
Bool APS5_VABI sceAvPlayerIsActive(AvPlayerInternal* h);
int APS5_VABI sceAvPlayerJumpToTime(AvPlayerInternal* h, uint64_t timeMs);
int APS5_VABI sceAvPlayerPause(AvPlayerInternal* h);
int APS5_VABI sceAvPlayerPostInit(AvPlayerInternal* h, const AvPlayerPostInitData* postInit);
int APS5_VABI sceAvPlayerResume(AvPlayerInternal* h);
int APS5_VABI sceAvPlayerSetAvSyncMode(AvPlayerInternal* h, uint32_t syncMode);
int APS5_VABI sceAvPlayerSetAvailableBandwidth(AvPlayerInternal* h, uint32_t startBandwidth, uint32_t minimumBandwidth, uint32_t maximumBandwidth);
int APS5_VABI sceAvPlayerSetLogCallback(void* callback, void* userData);
int APS5_VABI sceAvPlayerSetLooping(AvPlayerInternal* h, Bool loop);
int APS5_VABI sceAvPlayerSetTrickSpeed(AvPlayerInternal* h, int32_t trickSpeed);
int APS5_VABI sceAvPlayerStart(AvPlayerInternal* h);
int APS5_VABI sceAvPlayerStartEx(AvPlayerInternal* handle, const AvPlayer::StartInfo* startInfo);
int APS5_VABI sceAvPlayerStop(AvPlayerInternal* h);
int APS5_VABI sceAvPlayerStreamCount(AvPlayerInternal* h);

}
