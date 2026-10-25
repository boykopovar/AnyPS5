#pragma once

#include "SceTypes.hpp"

struct MoveTrackerCameraFrame;

struct MoveTrackerControllerInput;

struct MoveTrackerPlaybackInfo;

struct MoveTrackerState;

extern "C" {

int APS5_VABI sceMoveTrackerCalibrateReset();
int APS5_VABI sceMoveTrackerCalibrateYaw(std::int32_t handle, const float* direction);
int APS5_VABI sceMoveTrackerCameraUpdate(const MoveTrackerCameraFrame* frames);
int APS5_VABI sceMoveTrackerCaptureInit(void* imageMemory, std::int32_t capacity, std::int32_t interval, const char* path, std::int32_t mode);
int APS5_VABI sceMoveTrackerCaptureTerm();
int APS5_VABI sceMoveTrackerControllersUpdate(const MoveTrackerControllerInput* controllers);
int APS5_VABI sceMoveTrackerGetState(std::int32_t handle, std::uint64_t timestamp, MoveTrackerState* state);
int APS5_VABI sceMoveTrackerGetVersion();
int APS5_VABI sceMoveTrackerGetWorkingMemorySize(std::uint32_t* totalSize, std::uint32_t* processingSize);
int APS5_VABI sceMoveTrackerInit(void* workMemory, std::uint64_t requiredArgument, std::uint32_t pipeId, std::uint32_t queueId);
int APS5_VABI sceMoveTrackerPlayGetImages(MoveTrackerCameraFrame* frames);
int APS5_VABI sceMoveTrackerPlayGetStates(MoveTrackerPlaybackInfo* info, MoveTrackerState* states);
int APS5_VABI sceMoveTrackerPlayStart();
int APS5_VABI sceMoveTrackerPlayStep();
int APS5_VABI sceMoveTrackerRecordSave();
int APS5_VABI sceMoveTrackerRecordStart();
int APS5_VABI sceMoveTrackerRecordStop();
int APS5_VABI sceMoveTrackerTerm();

}
