#include "prx/libc/include/general/VabiMacros.hpp"
#include <pthread.h>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceFaceTrackerAddUserFeature(int userId, const void* featureData, int featureDataSize, void* work);
int APS5_VABI sceFaceTrackerFinalize(void* work);
int APS5_VABI sceFaceTrackerGetDefaultConfig(void* pConfig);
int APS5_VABI sceFaceTrackerGetResult(int trackId, void* result, void* work);
int APS5_VABI sceFaceTrackerGetTrackIdByUserId(int userId, void* work);
int APS5_VABI sceFaceTrackerGetUserFeature(int userId, const void** featureData, int* featureDataSize, void* work);
int APS5_VABI sceFaceTrackerGetUserIdByTrackId(int trackId, void* work);
int APS5_VABI sceFaceTrackerGetWorkingMemorySize(int width, int height, int rowstride, int maxTrackingNum, const void* pConfig, int* workSize);
int APS5_VABI sceFaceTrackerInitialize(int width, int height, int rowstride, int maxTrackingNum, pthread_mutex_t* threadFuncMutex, const void* pConfig, void* work, int workSize);
int APS5_VABI sceFaceTrackerRegisterFixUserIdCallback(void (*func)(), void* callbackParam, void* work);
int APS5_VABI sceFaceTrackerRegisterLostTargetCallback(void (*func)(), void* callbackParam, void* work);
int APS5_VABI sceFaceTrackerRegisterRecoveryFromLostTargetCallback(void (*func)(), void* callbackParam, void* work);
int APS5_VABI sceFaceTrackerRegisterStartTrackingCallback(void (*func)(), void* callbackParam, void* work);
int APS5_VABI sceFaceTrackerRegisterStopTrackingCallback(void (*func)(), void* callbackParam, void* work);
int APS5_VABI sceFaceTrackerRegisterUser(int userId, const void* featureData, int featureDataSize, void* work);
int APS5_VABI sceFaceTrackerReset(bool isClearRegisterUser, void* work);
int APS5_VABI sceFaceTrackerSearchTarget(pthread_mutex_t* threadFuncMutex, void* work);
int APS5_VABI sceFaceTrackerStartTracking(int trackId, void* work);
int APS5_VABI sceFaceTrackerStopTracking(int trackId, void* work);
int APS5_VABI sceFaceTrackerUnregisterUser(int userId, void* work);
int APS5_VABI sceFaceTrackerUpdate(const unsigned char* imageData, void* work);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename Call>
bool Throws(Call call) {
    try {
        static_cast<void>(call());
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    pthread_mutex_t mutex = {};
    int featureDataSize = 7;
    int workSize = 9;
    const void* featureData = &workSize;

    Require(Throws([&] { return sceFaceTrackerAddUserFeature(0, nullptr, 0, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerFinalize(nullptr); }));
    Require(Throws([&] { return sceFaceTrackerGetDefaultConfig(nullptr); }));
    Require(Throws([&] { return sceFaceTrackerGetResult(0, nullptr, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerGetTrackIdByUserId(0, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerGetUserFeature(0, &featureData, &featureDataSize, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerGetUserIdByTrackId(0, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerGetWorkingMemorySize(0, 0, 0, 0, nullptr, &workSize); }));
    Require(Throws([&] { return sceFaceTrackerInitialize(0, 0, 0, 0, &mutex, nullptr, nullptr, 0); }));
    Require(Throws([&] { return sceFaceTrackerRegisterFixUserIdCallback(nullptr, nullptr, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerRegisterLostTargetCallback(nullptr, nullptr, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerRegisterRecoveryFromLostTargetCallback(nullptr, nullptr, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerRegisterStartTrackingCallback(nullptr, nullptr, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerRegisterStopTrackingCallback(nullptr, nullptr, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerRegisterUser(0, nullptr, 0, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerReset(false, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerSearchTarget(&mutex, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerStartTracking(0, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerStopTracking(0, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerUnregisterUser(0, nullptr); }));
    Require(Throws([&] { return sceFaceTrackerUpdate(nullptr, nullptr); }));

    Require(featureDataSize == 7);
    Require(workSize == 9);
    Require(featureData == &workSize);
}
