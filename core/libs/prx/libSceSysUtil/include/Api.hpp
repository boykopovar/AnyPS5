#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSysUtilSendSystemNotificationWithText(int32_t type, const char* message);
int APS5_VABI sceSysUtilSendAddressingSystemNotification();
int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithDeviceId();
int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithUserId();
int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithUserName();
int APS5_VABI sceSysUtilSendNotificationRequest();
int APS5_VABI sceSysUtilSendNpDebugNotificationRequest();
int APS5_VABI sceSysUtilSendSystemNotification();
int APS5_VABI sceSysUtilSendSystemNotification2();
int APS5_VABI sceSysUtilSendSystemNotificationParamInit();
int APS5_VABI sceSysUtilSendSystemNotificationWithAppId();
int APS5_VABI sceSysUtilSendSystemNotificationWithAppInfo();
int APS5_VABI sceSysUtilSendSystemNotificationWithAppName();
int APS5_VABI sceSysUtilSendSystemNotificationWithAppNameRelatedToUser();
int APS5_VABI sceSysUtilSendSystemNotificationWithDeviceId();
int APS5_VABI sceSysUtilSendSystemNotificationWithDeviceIdRelatedToUser();
int APS5_VABI sceSysUtilSendSystemNotificationWithErrorCode();
int APS5_VABI sceSysUtilSendSystemNotificationWithParams();
int APS5_VABI sceSysUtilSendSystemNotificationWithTextRelatedToUser();
int APS5_VABI sceSysUtilSendSystemNotificationWithUserId();
int APS5_VABI sceSysUtilSendSystemNotificationWithUserName();
int APS5_VABI sceSysUtilSendSystemNotificationWithUserNameInfo();
int APS5_VABI sceSysUtilSendTrcCheckNotificationRequest();
int APS5_VABI sceSysUtilSendWebDebugNotificationRequest();

}
