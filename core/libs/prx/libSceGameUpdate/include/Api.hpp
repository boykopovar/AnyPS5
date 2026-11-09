#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceGameUpdateAbortRequest(int requestId);
int APS5_VABI sceGameUpdateCheck(int requestId, const GameUpdateCheckParam* param, GameUpdateCheckResult* result);
int APS5_VABI sceGameUpdateCreateRequest(void);
int APS5_VABI sceGameUpdateDeleteRequest(int requestId);
int APS5_VABI sceGameUpdateGetAddcontLatestVersion(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, GameUpdateAddcontVersionInfo* info);
int APS5_VABI sceGameUpdateInitialize(void);
int APS5_VABI sceGameUpdateTerminate(void);
int APS5_VABI sceGameUpdateCheckTitle();

}
