#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceAvPlayerAddSource(AvPlayerInternal* h, const char* filename) {
 (void)h;
 (void)filename;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerAddSourceEx(AvPlayerInternal* h, uint32_t uri_type, const void* source_details) {
 (void)h;
 (void)uri_type;
 (void)source_details;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerChangeStream(AvPlayerInternal* h, uint32_t old_stream_id, uint32_t new_stream_id) {
 (void)h;
 (void)old_stream_id;
 (void)new_stream_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerClose(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t sceAvPlayerCurrentTime(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerDisableStream(AvPlayerInternal* h, uint32_t stream_id) {
 (void)h;
 (void)stream_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerEnableStream(AvPlayerInternal* h, uint32_t stream_id) {
 (void)h;
 (void)stream_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

Bool sceAvPlayerGetAudioData(AvPlayerInternal* h, AvPlayerFrameInfo* audio_info) {
 (void)h;
 (void)audio_info;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

int sceAvPlayerGetStreamInfo(AvPlayerInternal* h, uint32_t stream_id, void* info) {
 (void)h;
 (void)stream_id;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerGetStreamInfoEx(AvPlayerInternal* h, uint32_t stream_id, void* info) {
 (void)h;
 (void)stream_id;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

Bool sceAvPlayerGetVideoDataEx(AvPlayerInternal* h, AvPlayerFrameInfoEx* video_info) {
 (void)h;
 (void)video_info;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

AvPlayerInternal* sceAvPlayerInit(AvPlayerInitData* init) {
 (void)init;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int sceAvPlayerInitEx(const void* init_ex, AvPlayerInternal** handle) {
 (void)init_ex;
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

Bool sceAvPlayerIsActive(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

int sceAvPlayerJumpToTime(AvPlayerInternal* h, uint64_t time_ms) {
 (void)h;
 (void)time_ms;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerPause(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerPostInit(AvPlayerInternal* h, const void* post_init) {
 (void)h;
 (void)post_init;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerResume(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerSetAvailableBandwidth(AvPlayerInternal* h, uint32_t start_bandwidth, uint32_t minimum_bandwidth, uint32_t maximum_bandwidth) {
 (void)h;
 (void)start_bandwidth;
 (void)minimum_bandwidth;
 (void)maximum_bandwidth;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerSetAvSyncMode(AvPlayerInternal* h, uint32_t sync_mode) {
 (void)h;
 (void)sync_mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerSetLogCallback(void* callback, void* user_data) {
 (void)callback;
 (void)user_data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerSetLooping(AvPlayerInternal* h, Bool loop) {
 (void)h;
 (void)loop;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerSetTrickSpeed(AvPlayerInternal* h, int32_t trick_speed) {
 (void)h;
 (void)trick_speed;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerStart(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerStartEx(AvPlayerInternal* h, const void* start_info_ex) {
 (void)h;
 (void)start_info_ex;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerStop(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAvPlayerStreamCount(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
