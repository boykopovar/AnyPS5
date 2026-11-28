#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceFaceTrackerRegisterStartTrackingCallback(void *callback, void *userArg, void *context);
int APS5_VABI sceFaceTrackerRegisterFixUserIdCallback(void *callback, void *userArg, void *context);
int APS5_VABI sceFaceTrackerRegisterStopTrackingCallback(void *callback, void *userArg, void *context);
int APS5_VABI sceFaceTrackerRegisterLostTargetCallback(void *callback, void *userArg, void *context);
int APS5_VABI sceFaceTrackerRegisterRecoveryFromLostTargetCallback(void *callback, void *userArg, void *context);
int APS5_VABI sceFaceTrackerStartTracking();
int APS5_VABI sceFaceTrackerStopTracking();
int APS5_VABI sceFaceTrackerReset();
int APS5_VABI sceFaceTrackerRegisterUser(int userId, const void *feature, int featureSize, void *context);
int APS5_VABI sceFaceTrackerUnregisterUser(int userId, void *context);
int APS5_VABI sceFaceTrackerAddUserFeature();
int APS5_VABI sceFaceTrackerGetUserFeature(int userId, void **feature, int *featureSize, void *context);
int APS5_VABI sceFaceTrackerGetUserIdByTrackId(int trackId, void *context);
int APS5_VABI sceFaceTrackerGetTrackIdByUserId(int userId, void *context);
int APS5_VABI sceFaceTrackerGetDefaultConfig(void *config);
int APS5_VABI sceFaceTrackerGetWorkingMemorySize();
int APS5_VABI sceFaceTrackerInitialize();
int APS5_VABI sceFaceTrackerFinalize();
int APS5_VABI sceFaceTrackerUpdate();
int APS5_VABI sceFaceTrackerSearchTarget();
int APS5_VABI sceFaceTrackerGetResult();

}
