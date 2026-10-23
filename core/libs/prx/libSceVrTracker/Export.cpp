#include <atomic>
#include <mutex>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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

static_assert(sizeof(VrTrackerMemoryBlock) == 16);
static_assert(sizeof(VrTrackerInitConfig) == 32);
static_assert(sizeof(VrTrackerInitParam) == 144);
static_assert(offsetof(VrTrackerInitParam, config) == 32);
static_assert(offsetof(VrTrackerInitParam, memory) == 64);
static_assert(offsetof(VrTrackerInitParam, option112) == 112);
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

static_assert(sizeof(VrTrackerCpuProcessParam) == 32);
static_assert(offsetof(VrTrackerCpuProcessParam, mode) == 4);
static_assert(offsetof(VrTrackerCpuProcessParam, argument) == 8);
static_assert(sizeof(VrTrackerGpuWaitParam) == 32);
static_assert(sizeof(VrTrackerGetResultParam) == 56);
static_assert(offsetof(VrTrackerGetResultParam, time) == 16);
static_assert(offsetof(VrTrackerGetResultParam, statusMode) == 24);
static_assert(offsetof(VrTrackerGetResultParam, outputValue) == 36);
static_assert(offsetof(VrTrackerGetResultParam, reserved1) == 44);

namespace {

constexpr int NotInit = static_cast<int>(0x81260801u);
constexpr int AlreadyInitialized = static_cast<int>(0x81260802u);
constexpr int DeviceNotRegistered = static_cast<int>(0x81260803u);
constexpr int DeviceAlreadyRegistered = static_cast<int>(0x81260804u);
constexpr int ArgumentInvalid = static_cast<int>(0x81260806u);
constexpr int NotExecuteGpuSubmit = static_cast<int>(0x81260815u);

constexpr std::uint32_t MemoryAlignment = 0x10000;
constexpr std::uint32_t BaseOnionSize = 0x400000;
constexpr std::uint32_t GarlicSize = 0x3000000;
constexpr std::uint32_t WorkSize = 0x1000000;
constexpr std::uint32_t CalibrationManual = 0;
constexpr std::uint32_t CalibrationAuto = 1;

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
static_assert(sizeof(QueryMemoryParam) == 0x40);

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

bool ValidCalibration(const CalibrationSettings& settings) {
    return settings.pad_position <= CalibrationAuto && settings.hmd_position <= CalibrationManual &&
        settings.move_position <= CalibrationAuto && settings.gun_position <= CalibrationAuto;
}

std::uint32_t OnionSize(const CalibrationSettings& settings) {
    return settings.move_position == CalibrationAuto ? BaseOnionSize * 2 : BaseOnionSize;
}

constexpr std::int32_t ComputePipes = 7;
constexpr std::int32_t QueuesPerPipe = 8;

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
static_assert(sizeof(InitParam) == 0x80);

std::atomic<bool> g_initialized{false};

constexpr std::uint32_t DeviceHmd = 0;
constexpr std::uint32_t DeviceGun = 3;

std::mutex g_devicesMutex;
std::int32_t g_deviceHandles[DeviceGun + 1] = {-1, -1, -1, -1};

struct RecalibrateParam {
    std::uint32_t size;
    std::uint32_t device_type;
    std::uint32_t calibration_type;
    std::uint32_t reserved[5];
};

} // namespace

extern "C" {

int APS5_VABI sceVrTrackerRegisterDeviceInternal(int device_type, int handle, int unk0, int);

int APS5_VABI sceVrTrackerCpuProcess(const VrTrackerCpuProcessParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerGetResult(const VrTrackerGetResultParam* param, VrTrackerResult* result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerGetTime(std::uint64_t* time) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerGpuSubmit(const VrTrackerGpuSubmitParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerGpuWait(const VrTrackerGpuWaitParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerInit(const VrTrackerInitParam* param) {
    if (g_initialized.load()) return AlreadyInitialized;
    if (param && param->size == sizeof(VrTrackerInitParam)) NotImplemented_nid_no_patch(__func__);
    const auto* init = reinterpret_cast<const InitParam*>(param);
    if (!init || init->size != sizeof(InitParam) || !init->direct_memory_garlic ||
        init->direct_memory_garlic_alignment != MemoryAlignment || init->direct_memory_garlic_size != GarlicSize ||
        !init->direct_memory_onion || init->direct_memory_onion_alignment != MemoryAlignment ||
        init->direct_memory_onion_size != OnionSize(init->calibration_settings) || !init->work_memory ||
        init->work_memory_alignment != MemoryAlignment || init->work_memory_size != WorkSize ||
        init->gpu_pipe_id >= ComputePipes || init->gpu_queue_id >= QueuesPerPipe ||
        !ValidCalibration(init->calibration_settings)) {
        return ArgumentInvalid;
    }
    bool expected = false;
    return g_initialized.compare_exchange_strong(expected, true) ? 0 : AlreadyInitialized;
}
int APS5_VABI sceVrTrackerCpuPopMarker() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerCpuPushMarker() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerGetLiveCaptureId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerGetPlayAreaWarningInfo(void* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerGpuWaitAndCpuProcess() {
    return g_initialized.load() ? NotExecuteGpuSubmit : NotInit;
}
int APS5_VABI sceVrTrackerNotifyEndOfCpuProcess(const void* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerQueryMemory(const void* param, void* result) {
    const auto* query = static_cast<const QueryMemoryParam*>(param);
    auto* memory = static_cast<QueryMemoryResult*>(result);
    if (!query || !memory || query->size != sizeof(QueryMemoryParam) || (query->profile != 0 && query->profile != 100) ||
        !ValidCalibration(query->calibration_settings)) {
        return ArgumentInvalid;
    }
    memory->direct_memory_onion_size = OnionSize(query->calibration_settings);
    memory->direct_memory_onion_alignment = MemoryAlignment;
    memory->direct_memory_garlic_size = GarlicSize;
    memory->direct_memory_garlic_alignment = MemoryAlignment;
    memory->work_memory_size = WorkSize;
    memory->work_memory_alignment = MemoryAlignment;
    return 0;
}
int APS5_VABI sceVrTrackerRecalibrate(const void* param) {
    if (!g_initialized.load()) return NotInit;
    const auto* recalibrate = static_cast<const RecalibrateParam*>(param);
    if (!recalibrate || recalibrate->size != sizeof(RecalibrateParam) || recalibrate->device_type > DeviceGun) return ArgumentInvalid;
    if (recalibrate->device_type == DeviceHmd) return DeviceNotRegistered;
    std::lock_guard lock(g_devicesMutex);
    return g_deviceHandles[recalibrate->device_type] == -1 ? DeviceNotRegistered : 0;
}
int APS5_VABI sceVrTrackerRegisterDevice(int device_type, int handle) {
    return sceVrTrackerRegisterDeviceInternal(device_type, handle, -1, 0);
}
int APS5_VABI sceVrTrackerRegisterDevice2(int device_type, int handle) {
    return sceVrTrackerRegisterDeviceInternal(device_type, handle, -1, 1);
}
int APS5_VABI sceVrTrackerRegisterDeviceInternal(int device_type, int handle, int unk0, int) {
    if (!g_initialized.load()) return NotInit;
    const auto type = static_cast<std::uint32_t>(device_type);
    if (type > DeviceGun || unk0 > 4) return ArgumentInvalid;
    std::lock_guard lock(g_devicesMutex);
    if (g_deviceHandles[type] != -1) return DeviceAlreadyRegistered;
    g_deviceHandles[type] = handle;
    return 0;
}
int APS5_VABI sceVrTrackerResetAll() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerResetOrientationRelative(int device_type, int handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerSaveInternalBuffers() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerSetDeviceRejection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerSetDurationUntilStatusNotTracking(int device_type, std::uint32_t) {
    if (!g_initialized.load()) return NotInit;
    return static_cast<std::uint32_t>(device_type) > DeviceGun ? ArgumentInvalid : 0;
}
int APS5_VABI sceVrTrackerSetExtendedMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerSetLEDBrightness() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerSetRestingMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerStartLiveCapture() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerStopLiveCapture() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceVrTrackerTerm() {
    bool expected = true;
    return g_initialized.compare_exchange_strong(expected, false) ? 0 : NotInit;
}
int APS5_VABI sceVrTrackerUnregisterDevice(int handle) {
    if (!g_initialized.load()) return NotInit;
    if (handle < 0) return ArgumentInvalid;
    std::lock_guard lock(g_devicesMutex);
    for (auto& registered : g_deviceHandles) {
        if (registered == handle) {
            registered = -1;
            return 0;
        }
    }
    return ArgumentInvalid;
}
int APS5_VABI sceVrTrackerUpdateMotionSensorData(const void* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
