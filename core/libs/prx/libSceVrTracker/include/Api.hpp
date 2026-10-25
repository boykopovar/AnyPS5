#pragma once

#include "SceTypes.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

struct VrTrackerMemoryBlock {
    void* address;
    std::uint32_t size;
    std::uint32_t alignment;
};

struct VrTrackerInitConfig {
    std::uint32_t type;
    std::uint32_t option;
    std::uint32_t mode;
    std::uint32_t parameter;
    std::uint32_t reserved[4];
};

struct VrTrackerInitParam {
    std::uint32_t size;
    std::int32_t mode;
    std::uint32_t reserved[6];
    VrTrackerInitConfig config;
    VrTrackerMemoryBlock memory[3];
    std::uint32_t option112;
    std::uint32_t option116;
    std::uint32_t reservedTail[6];
};

struct VrTrackerGpuSubmitParam;

struct VrTrackerResult;

struct VrTrackerCpuProcessParam {
    std::uint32_t size;
    std::uint32_t mode;
    std::uint32_t argument;
    std::uint32_t reserved[5];
};

struct VrTrackerGpuWaitParam {
    std::uint32_t size;
    std::byte payload[28];
};

struct VrTrackerGetResultParam {
    std::uint32_t size;
    std::int32_t handle;
    std::uint32_t mode;
    std::uint32_t extendedResult;
    std::uint64_t time;
    std::uint32_t statusMode;
    std::uint32_t reserved0;
    std::uint32_t option32;
    std::byte outputValue[4];
    std::uint32_t option40;
    std::uint32_t reserved1[2];
    std::byte unused[4];
};

namespace VrTrackerLegacy {

struct CalibrationSettings {
    std::uint32_t hmd_position;
    std::uint32_t pad_position;
    std::uint32_t move_position;
    std::uint32_t gun_position;
    std::uint32_t reserved[4];
};

struct QueryMemoryParam {
    std::uint32_t size;
    std::uint32_t profile;
    std::uint32_t reserved[6];
    CalibrationSettings calibration_settings;
};

struct QueryMemoryResult {
    std::uint32_t size;
    std::uint32_t direct_memory_onion_size;
    std::uint32_t direct_memory_onion_alignment;
    std::uint32_t direct_memory_garlic_size;
    std::uint32_t direct_memory_garlic_alignment;
    std::uint32_t work_memory_size;
    std::uint32_t work_memory_alignment;
    std::uint32_t reserved[9];
};

struct InitParam {
    std::uint32_t size;
    std::uint32_t profile;
    std::uint32_t execution_mode;
    std::int32_t hmd_thread_priority;
    std::int32_t pad_thread_priority;
    std::int32_t move_thread_priority;
    std::int32_t gun_thread_priority;
    std::int32_t reserved;
    std::uint64_t cpu_mask;
    CalibrationSettings calibration_settings;
    void* direct_memory_onion;
    std::uint32_t direct_memory_onion_size;
    std::uint32_t direct_memory_onion_alignment;
    void* direct_memory_garlic;
    std::uint32_t direct_memory_garlic_size;
    std::uint32_t direct_memory_garlic_alignment;
    void* work_memory;
    std::uint32_t work_memory_size;
    std::uint32_t work_memory_alignment;
    std::int32_t gpu_pipe_id;
    std::int32_t gpu_queue_id;
};

struct RecalibrateParam {
    std::uint32_t size;
    std::uint32_t device_type;
    std::uint32_t calibration_type;
    std::uint32_t reserved[5];
};

}

extern "C" {

int APS5_VABI sceVrTrackerCpuProcess(const VrTrackerCpuProcessParam* param);
int APS5_VABI sceVrTrackerGetResult(const VrTrackerGetResultParam* param, VrTrackerResult* result);
int APS5_VABI sceVrTrackerGetTime(std::uint64_t* time);
int APS5_VABI sceVrTrackerGpuSubmit(const VrTrackerGpuSubmitParam* param);
int APS5_VABI sceVrTrackerGpuWait(const VrTrackerGpuWaitParam* param);
int APS5_VABI sceVrTrackerInit(const VrTrackerInitParam* param);
int APS5_VABI sceVrTrackerCpuPopMarker();
int APS5_VABI sceVrTrackerCpuPushMarker();
int APS5_VABI sceVrTrackerGetLiveCaptureId();
int APS5_VABI sceVrTrackerGetPlayAreaWarningInfo(void* info);
int APS5_VABI sceVrTrackerGpuWaitAndCpuProcess();
int APS5_VABI sceVrTrackerNotifyEndOfCpuProcess(const void* param);
int APS5_VABI sceVrTrackerQueryMemory(const void* param, void* result);
int APS5_VABI sceVrTrackerRecalibrate(const void* param);
int APS5_VABI sceVrTrackerRegisterDevice(int deviceType, int handle);
int APS5_VABI sceVrTrackerRegisterDevice2(int deviceType, int handle);
int APS5_VABI sceVrTrackerRegisterDeviceInternal(int deviceType, int handle, int unk0, int);
int APS5_VABI sceVrTrackerResetAll();
int APS5_VABI sceVrTrackerResetOrientationRelative(int deviceType, int handle);
int APS5_VABI sceVrTrackerSaveInternalBuffers();
int APS5_VABI sceVrTrackerSetDeviceRejection();
int APS5_VABI sceVrTrackerSetDurationUntilStatusNotTracking(int deviceType, std::uint32_t);
int APS5_VABI sceVrTrackerSetExtendedMode();
int APS5_VABI sceVrTrackerSetLEDBrightness();
int APS5_VABI sceVrTrackerSetRestingMode();
int APS5_VABI sceVrTrackerStartLiveCapture();
int APS5_VABI sceVrTrackerStopLiveCapture();
int APS5_VABI sceVrTrackerTerm();
int APS5_VABI sceVrTrackerUnregisterDevice(int handle);
int APS5_VABI sceVrTrackerUpdateMotionSensorData(const void* param);

}
