#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int scePadClose_nid_postfix(int handle) {
 (void)handle;
 return 0;
}

int scePadDeviceClassGetExtendedInformation(int handle, PadDeviceClassExtendedInformation* info) {
 (void)handle;
 (void)info;
 return 0;
}

int scePadDeviceClassParseData(int handle, const PadData* data, PadDeviceClassData* class_data) {
 (void)handle;
 (void)data;
 (void)class_data;
 return 0;
}

int scePadGetControllerInformation(int handle, PadControllerInformation* info) {
 (void)handle;
 (void)info;
 return 0;
}

int scePadGetHandle(int user_id, int type, int index) {
 (void)user_id;
 (void)type;
 (void)index;
 return 0;
}

int scePadGetTriggerEffectState(int handle, PadTriggerEffectStateInformation* info) {
 (void)handle;
 (void)info;
 return 0;
}

int scePadInit_nid_postfix(void) {
 return 0;
}

int scePadOpen_nid_postfix(int user_id, int type, int index, const void* param) {
 (void)user_id;
 (void)type;
 (void)index;
 (void)param;
 return 0;
}

int scePadRead_nid_postfix(int handle, PadData* data, int num) {
 (void)handle;
 (void)data;
 (void)num;
 return 0;
}

int scePadReadState(int handle, PadData* data) {
 (void)handle;
 (void)data;
 return 0;
}

int scePadResetLightBar(int handle) {
 (void)handle;
 return 0;
}

int scePadResetOrientation(int handle) {
 (void)handle;
 return 0;
}

int scePadSetAngularVelocityDeadbandState(int handle, bool enable) {
 (void)handle;
 (void)enable;
 return 0;
}

int scePadSetLightBar(int handle, const PadLightBarParam* param) {
 (void)handle;
 (void)param;
 return 0;
}

int scePadSetMotionSensorState(int handle, bool enable) {
 (void)handle;
 (void)enable;
 return 0;
}

int scePadSetTiltCorrectionState(int handle, bool enabled) {
 (void)handle;
 (void)enabled;
 return 0;
}

int scePadSetTriggerEffect(int handle, const void* param) {
 (void)handle;
 (void)param;
 return 0;
}

int scePadSetVibration(int handle, const PadVibrationParam* param) {
 (void)handle;
 (void)param;
 return 0;
}

int scePadSetVibrationMode(int handle, int mode) {
 (void)handle;
 (void)mode;
 return 0;
}

int scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool enabled) {
 (void)enabled;
 return 0;
}

}
