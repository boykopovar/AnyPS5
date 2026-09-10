#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAvPlayerAddSource(AvPlayerInternal* h, const char* filename) {
 (void)h;
 (void)filename;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerAddSourceEx(AvPlayerInternal* h, uint32_t uri_type, const void* source_details) {
 (void)h;
 (void)uri_type;
 (void)source_details;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerChangeStream(AvPlayerInternal* h, uint32_t old_stream_id, uint32_t new_stream_id) {
 (void)h;
 (void)old_stream_id;
 (void)new_stream_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerClose(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t APS5_VABI sceAvPlayerCurrentTime(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerDisableStream(AvPlayerInternal* h, uint32_t stream_id) {
 (void)h;
 (void)stream_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerEnableStream(AvPlayerInternal* h, uint32_t stream_id) {
 (void)h;
 (void)stream_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

Bool APS5_VABI sceAvPlayerGetAudioData(AvPlayerInternal* h, AvPlayerFrameInfo* audio_info) {
 (void)h;
 (void)audio_info;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

int APS5_VABI sceAvPlayerGetStreamInfo(AvPlayerInternal* h, uint32_t stream_id, void* info) {
 (void)h;
 (void)stream_id;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

Bool APS5_VABI sceAvPlayerGetVideoData(AvPlayerInternal* h, AvPlayerFrameInfo* video_info) {
 (void)h;
 (void)video_info;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

Bool APS5_VABI sceAvPlayerGetVideoDataEx(AvPlayerInternal* h, AvPlayerFrameInfoEx* video_info) {
 (void)h;
 (void)video_info;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

AvPlayerInternal* APS5_VABI sceAvPlayerInit(AvPlayerInitData* init) {
 (void)init;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI sceAvPlayerInitEx(const void* init_ex, AvPlayerInternal** handle) {
 (void)init_ex;
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

Bool APS5_VABI sceAvPlayerIsActive(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

int APS5_VABI sceAvPlayerJumpToTime(AvPlayerInternal* h, uint64_t time_ms) {
 (void)h;
 (void)time_ms;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerPause(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerPostInit(AvPlayerInternal* h, const void* post_init) {
 (void)h;
 (void)post_init;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerResume(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerSetAvSyncMode(AvPlayerInternal* h, uint32_t sync_mode) {
 (void)h;
 (void)sync_mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerSetLogCallback(void* callback, void* user_data) {
 (void)callback;
 (void)user_data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerSetLooping(AvPlayerInternal* h, Bool loop) {
 (void)h;
 (void)loop;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerSetTrickSpeed(AvPlayerInternal* h, int32_t trick_speed) {
 (void)h;
 (void)trick_speed;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerStart(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerStop(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAvPlayerStreamCount(AvPlayerInternal* h) {
 (void)h;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
