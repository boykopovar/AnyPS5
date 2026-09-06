#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceShareCaptureScreenshot(const void* param, int32_t* req_id) {
 (void)param;
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareCaptureVideoClip(const void* param, int32_t* req_id) {
 (void)param;
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareFeaturePermit(uint32_t feature_flags) {
 (void)feature_flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareFeatureProhibit(uint32_t feature_flags) {
 (void)feature_flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareGetCurrentStatus(uint32_t feature_flag, ShareCurrentStatus* status) {
 (void)feature_flag;
 (void)status;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareInitialize(size_t heap_size, int thread_priority, uint64_t affinity_mask) {
 (void)heap_size;
 (void)thread_priority;
 (void)affinity_mask;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareOpenMenuForContent(const void* content_id) {
 (void)content_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareRegisterContentEventCallback(void* callback, void* user_data) {
 (void)callback;
 (void)user_data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareSetCaptureSource(uint32_t feature_flags, const void* tap_point) {
 (void)feature_flags;
 (void)tap_point;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareSetContentParam(const char* content_param) {
 (void)content_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareSetScreenshotOverlayImage(const char* file_path, int32_t margin_x, int32_t margin_y, int32_t origin) {
 (void)file_path;
 (void)margin_x;
 (void)margin_y;
 (void)origin;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceShareUnregisterContentEventCallback(void* callback) {
 (void)callback;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
