#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceSystemServiceDisableNoticeScreenSkipFlagAutoSet(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceGetDisplaySafeAreaInfo(SystemServiceDisplaySafeAreaInfo* info) {
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* luminance) {
 (void)luminance;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceGetNoticeScreenSkipFlag(bool* value) {
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceGetStatus(SystemServiceStatus* status) {
 (void)status;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceHideSplashScreen(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceParamGetInt(int param_id, int* value) {
 (void)param_id;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceParamGetString(int param_id, char* buf, size_t buf_size) {
 (void)param_id;
 (void)buf;
 (void)buf_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServicePowerTick(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceReceiveEvent(SystemServiceEvent* event) {
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceReportAbnormalTermination(const void* info) {
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSystemServiceSetNoticeScreenSkipFlag(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
