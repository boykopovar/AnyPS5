#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libScePad/Pad.hpp"

extern "C" {

int scePadClose_nid_postfix(int handle) {
 if (handle != PAD_HANDLE) {
  return PAD_ERROR_INVALID_HANDLE;
 }
 return PAD_OK;
}

int scePadDeviceClassGetExtendedInformation(int handle, PadDeviceClassExtendedInformation* info) {
 (void)handle;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadDeviceClassParseData(int handle, const PadData* data, PadDeviceClassData* class_data) {
 (void)handle;
 (void)data;
 (void)class_data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadGetControllerInformation(int handle, PadControllerInformation* info) {
 if (handle != PAD_HANDLE) {
  return PAD_ERROR_INVALID_HANDLE;
 }
 if (info == nullptr) {
  return PAD_ERROR_INVALID_ARG;
 }
 std::memset(info, 0, sizeof(*info));
 info->touchPadInfo.pixelDensity = 44.86f;
 info->touchPadInfo.resolution.x = 1920;
 info->touchPadInfo.resolution.y = 943;
 info->stickInfo.deadZoneLeft = 2;
 info->stickInfo.deadZoneRight = 2;
 info->connectionType = PAD_CONNECTION_TYPE_LOCAL;
 info->connectedCount = 1;
 info->connected = true;
 info->deviceClass = PAD_DEVICE_CLASS_STANDARD;
 return PAD_OK;
}

int scePadGetHandle(int user_id, int type, int index) {
 (void)user_id;
 (void)type;
 (void)index;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadGetTriggerEffectState(int handle, PadTriggerEffectStateInformation* info) {
 (void)handle;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadInit_nid_postfix(void) {
 return PAD_OK;
}

int scePadOpen_nid_postfix(int userId, int type, int index, const void* param) {
 (void)param;
 if (index != 0) {
  return PAD_ERROR_INVALID_ARG;
 }
 const bool personalPort = (type == PAD_PORT_TYPE_STANDARD || type == PAD_PORT_TYPE_SPECIAL);
 const bool systemRemote = (userId == PAD_USER_ID_SYSTEM && type == PAD_PORT_TYPE_REMOTE);
 if (!personalPort && !systemRemote) {
  return PAD_ERROR_INVALID_ARG;
 }
 return PAD_HANDLE;
}

int scePadRead_nid_postfix(int handle, PadData* data, int num) {
 (void)handle;
 (void)data;
 (void)num;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadReadState(int handle, PadData* data) {
 (void)handle;
 (void)data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadResetLightBar(int handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadResetOrientation(int handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadSetAngularVelocityDeadbandState(int handle, bool enable) {
 (void)handle;
 (void)enable;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadSetLightBar(int handle, const PadLightBarParam* param) {
 (void)handle;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadSetMotionSensorState(int handle, bool enable) {
 (void)handle;
 if (enable) {
  throw std::runtime_error("scePadSetMotionSensorState: motion sensor not supported");
 }
 return PAD_OK;
}

int scePadSetTiltCorrectionState(int handle, bool enabled) {
 (void)handle;
 (void)enabled;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadSetTriggerEffect(int handle, const void* param) {
 (void)handle;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadSetVibration(int handle, const PadVibrationParam* param) {
 (void)handle;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadSetVibrationMode(int handle, int mode) {
 (void)handle;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int scePadSetVibrationTriggerEffectWeakWhileEmbeddedMicInUse(bool enabled) {
 (void)enabled;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
