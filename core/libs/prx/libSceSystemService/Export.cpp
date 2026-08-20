#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceSystemServiceDisableNoticeScreenSkipFlagAutoSet(void) {
 return 0;
}

int sceSystemServiceGetDisplaySafeAreaInfo(SystemServiceDisplaySafeAreaInfo* info) {
 (void)info;
 return 0;
}

int sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* luminance) {
 (void)luminance;
 return 0;
}

int sceSystemServiceGetNoticeScreenSkipFlag(bool* value) {
 (void)value;
 return 0;
}

int sceSystemServiceGetStatus(SystemServiceStatus* status) {
 (void)status;
 return 0;
}

int sceSystemServiceHideSplashScreen(void) {
 return 0;
}

int sceSystemServiceParamGetInt(int param_id, int* value) {
 (void)param_id;
 (void)value;
 return 0;
}

int sceSystemServiceParamGetString(int param_id, char* buf, size_t buf_size) {
 (void)param_id;
 (void)buf;
 (void)buf_size;
 return 0;
}

int sceSystemServicePowerTick(void) {
 return 0;
}

int sceSystemServiceReceiveEvent(SystemServiceEvent* event) {
 (void)event;
 return 0;
}

int sceSystemServiceReportAbnormalTermination(const void* info) {
 (void)info;
 return 0;
}

int sceSystemServiceSetNoticeScreenSkipFlag(void) {
 return 0;
}

}
