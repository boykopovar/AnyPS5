#include <pthread.h>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

typedef void (*SceFaceTrackerFixUserIdCallbackFunc)();
typedef void (*SceFaceTrackerLostTargetCallbackFunc)();
typedef void (*SceFaceTrackerRecoveryFromLostTargetCallbackFunc)();
typedef void (*SceFaceTrackerStartTrackingCallbackFunc)();
typedef void (*SceFaceTrackerStopTrackingCallbackFunc)();

extern "C" {

int APS5_VABI sceFaceTrackerAddUserFeature(int userId, const void* featureData, int featureDataSize, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerFinalize(void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetDefaultConfig(void* pConfig) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetResult(int trackId, void* result, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetTrackIdByUserId(int userId, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetUserFeature(int userId, const void** featureData, int* featureDataSize, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetUserIdByTrackId(int trackId, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerGetWorkingMemorySize(int width, int height, int rowstride, int maxTrackingNum, const void* pConfig, int* workSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerInitialize(int width, int height, int rowstride, int maxTrackingNum, pthread_mutex_t* threadFuncMutex, const void* pConfig, void* work, int workSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerRegisterFixUserIdCallback(SceFaceTrackerFixUserIdCallbackFunc func, void* callbackParam, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerRegisterLostTargetCallback(SceFaceTrackerLostTargetCallbackFunc func, void* callbackParam, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerRegisterRecoveryFromLostTargetCallback(SceFaceTrackerRecoveryFromLostTargetCallbackFunc func, void* callbackParam, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerRegisterStartTrackingCallback(SceFaceTrackerStartTrackingCallbackFunc func, void* callbackParam, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerRegisterStopTrackingCallback(SceFaceTrackerStopTrackingCallbackFunc func, void* callbackParam, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerRegisterUser(int userId, const void* featureData, int featureDataSize, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerReset(bool isClearRegisterUser, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerSearchTarget(pthread_mutex_t* threadFuncMutex, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerStartTracking(int trackId, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerStopTracking(int trackId, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerUnregisterUser(int userId, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFaceTrackerUpdate(const unsigned char* imageData, void* work) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
