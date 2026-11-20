#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSysUtilSendSystemNotificationWithText(int32_t type, const char* message);
int APS5_VABI sceSysUtilSendSystemNotification(int32_t type);
int APS5_VABI sceSysUtilSendSystemNotificationWithUserId(int32_t type, int32_t userId);
int APS5_VABI sceSysUtilSendSystemNotificationWithDeviceId(int32_t type, const void* deviceId);
int APS5_VABI sceSysUtilSendSystemNotificationWithErrorCode(int32_t type, int32_t errorCode);
int APS5_VABI sceSysUtilSendSystemNotificationWithAppId(int32_t type, int32_t appId);
int APS5_VABI sceSysUtilSendSystemNotificationWithAppName(int32_t type, const char* appName);
int APS5_VABI sceSysUtilSendSystemNotificationWithAppInfo(int32_t type, const char* appName, const char* info);
int APS5_VABI sceSysUtilSendSystemNotificationWithDeviceIdRelatedToUser(int32_t type, int32_t userId, const void* deviceId);
int APS5_VABI sceSysUtilSendSystemNotificationWithTextRelatedToUser(int32_t type, int32_t userId, const char* message);
int APS5_VABI sceSysUtilSendSystemNotificationWithUserName(int32_t type, const char* userName);
int APS5_VABI sceSysUtilSendSystemNotificationWithUserNameInfo(int32_t type, const char* userName, const char* info);
int APS5_VABI sceSysUtilSendSystemNotificationWithAppNameRelatedToUser(int32_t type, int32_t userId, const char* appName);
int APS5_VABI sceSysUtilSendNpDebugNotificationRequest(const char* message, int32_t flags);
int APS5_VABI sceSysUtilSendSystemNotification2(const void* param);
void APS5_VABI sceSysUtilSendSystemNotificationParamInit(void* param, int32_t type);
int APS5_VABI sceSysUtilSendAddressingSystemNotification(int32_t type, int32_t addressingType);
int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithDeviceId(int32_t type, int32_t addressingType, const void* deviceId);
int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithUserName(int32_t type, int32_t addressingType, const char* userName);
int APS5_VABI sceSysUtilSendAddressingSystemNotificationWithUserId(int32_t type, int32_t addressingType, int32_t userId);
int APS5_VABI sceSysUtilSendNotificationRequest(const char* message, int32_t flags);
int APS5_VABI sceSysUtilSendSystemNotificationWithParams();
int APS5_VABI sceSysUtilSendTrcCheckNotificationRequest(const char* message, int32_t flags);
int APS5_VABI sceSysUtilSendWebDebugNotificationRequest(const char* message, int32_t flags);

}
