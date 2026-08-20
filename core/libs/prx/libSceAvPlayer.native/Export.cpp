#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceAvPlayerAddSource(AvPlayerInternal* h, const char* filename) {
 (void)h;
 (void)filename;
 return 0;
}

int sceAvPlayerAddSourceEx(AvPlayerInternal* h, uint32_t uri_type, const void* source_details) {
 (void)h;
 (void)uri_type;
 (void)source_details;
 return 0;
}

int sceAvPlayerChangeStream(AvPlayerInternal* h, uint32_t old_stream_id, uint32_t new_stream_id) {
 (void)h;
 (void)old_stream_id;
 (void)new_stream_id;
 return 0;
}

int sceAvPlayerClose(AvPlayerInternal* h) {
 (void)h;
 return 0;
}

uint64_t sceAvPlayerCurrentTime(AvPlayerInternal* h) {
 (void)h;
 return 0;
}

int sceAvPlayerDisableStream(AvPlayerInternal* h, uint32_t stream_id) {
 (void)h;
 (void)stream_id;
 return 0;
}

int sceAvPlayerEnableStream(AvPlayerInternal* h, uint32_t stream_id) {
 (void)h;
 (void)stream_id;
 return 0;
}

Bool sceAvPlayerGetAudioData(AvPlayerInternal* h, AvPlayerFrameInfo* audio_info) {
 (void)h;
 (void)audio_info;
 return {};
}

int sceAvPlayerGetStreamInfo(AvPlayerInternal* h, uint32_t stream_id, void* info) {
 (void)h;
 (void)stream_id;
 (void)info;
 return 0;
}

int sceAvPlayerGetStreamInfoEx(AvPlayerInternal* h, uint32_t stream_id, void* info) {
 (void)h;
 (void)stream_id;
 (void)info;
 return 0;
}

Bool sceAvPlayerGetVideoDataEx(AvPlayerInternal* h, AvPlayerFrameInfoEx* video_info) {
 (void)h;
 (void)video_info;
 return {};
}

AvPlayerInternal* sceAvPlayerInit(AvPlayerInitData* init) {
 (void)init;
 return nullptr;
}

int sceAvPlayerInitEx(const void* init_ex, AvPlayerInternal** handle) {
 (void)init_ex;
 (void)handle;
 return 0;
}

Bool sceAvPlayerIsActive(AvPlayerInternal* h) {
 (void)h;
 return {};
}

int sceAvPlayerJumpToTime(AvPlayerInternal* h, uint64_t time_ms) {
 (void)h;
 (void)time_ms;
 return 0;
}

int sceAvPlayerPause(AvPlayerInternal* h) {
 (void)h;
 return 0;
}

int sceAvPlayerPostInit(AvPlayerInternal* h, const void* post_init) {
 (void)h;
 (void)post_init;
 return 0;
}

int sceAvPlayerResume(AvPlayerInternal* h) {
 (void)h;
 return 0;
}

int sceAvPlayerSetAvailableBandwidth(AvPlayerInternal* h, uint32_t start_bandwidth, uint32_t minimum_bandwidth, uint32_t maximum_bandwidth) {
 (void)h;
 (void)start_bandwidth;
 (void)minimum_bandwidth;
 (void)maximum_bandwidth;
 return 0;
}

int sceAvPlayerSetAvSyncMode(AvPlayerInternal* h, uint32_t sync_mode) {
 (void)h;
 (void)sync_mode;
 return 0;
}

int sceAvPlayerSetLogCallback(void* callback, void* user_data) {
 (void)callback;
 (void)user_data;
 return 0;
}

int sceAvPlayerSetLooping(AvPlayerInternal* h, Bool loop) {
 (void)h;
 (void)loop;
 return 0;
}

int sceAvPlayerSetTrickSpeed(AvPlayerInternal* h, int32_t trick_speed) {
 (void)h;
 (void)trick_speed;
 return 0;
}

int sceAvPlayerStart(AvPlayerInternal* h) {
 (void)h;
 return 0;
}

int sceAvPlayerStartEx(AvPlayerInternal* h, const void* start_info_ex) {
 (void)h;
 (void)start_info_ex;
 return 0;
}

int sceAvPlayerStop(AvPlayerInternal* h) {
 (void)h;
 return 0;
}

int sceAvPlayerStreamCount(AvPlayerInternal* h) {
 (void)h;
 return 0;
}

}
