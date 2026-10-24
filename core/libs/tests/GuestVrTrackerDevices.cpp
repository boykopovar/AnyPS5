#include "prx/libSceVrTracker/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int NotInit = static_cast<int>(0x81260801u);
constexpr int AlreadyInitialized = static_cast<int>(0x81260802u);
constexpr int DeviceNotRegistered = static_cast<int>(0x81260803u);
constexpr int DeviceAlreadyRegistered = static_cast<int>(0x81260804u);
constexpr int ArgumentInvalid = static_cast<int>(0x81260806u);
constexpr int NotExecuteGpuSubmit = static_cast<int>(0x81260815u);
constexpr std::uint32_t Untouched = 0xa5a5a5a5u;
constexpr int Hmd = 0;
constexpr int Pad = 1;
constexpr int Move = 2;
constexpr int Gun = 3;

struct QueryResult {
    std::uint32_t words[16];
};

std::uint8_t g_onion[16];
std::uint8_t g_garlic[16];
std::uint8_t g_work[16];

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
void RequireThrows(TFunction function) {
    try { function(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

QueryResult FreshResult() {
    QueryResult result;
    for (auto& word : result.words) word = Untouched;
    return result;
}

VrTrackerLegacy::InitParam ValidInit() {
    VrTrackerLegacy::InitParam init{};
    init.size = sizeof(init);
    init.direct_memory_onion_size = 0x400000;
    init.direct_memory_onion_alignment = 0x10000;
    init.direct_memory_garlic_size = 0x3000000;
    init.direct_memory_garlic_alignment = 0x10000;
    init.work_memory_size = 0x1000000;
    init.work_memory_alignment = 0x10000;
    init.direct_memory_onion = g_onion;
    init.direct_memory_garlic = g_garlic;
    init.work_memory = g_work;
    return init;
}

int Recalibrate(std::uint32_t deviceType, std::uint32_t size = sizeof(VrTrackerLegacy::RecalibrateParam)) {
    VrTrackerLegacy::RecalibrateParam param{};
    param.size = size;
    param.device_type = deviceType;
    return sceVrTrackerRecalibrate(&param);
}

void CheckQueryMemory() {
    VrTrackerLegacy::QueryMemoryParam query{sizeof(VrTrackerLegacy::QueryMemoryParam)};
    auto result = FreshResult();
    Require(sceVrTrackerQueryMemory(&query, &result) == 0);
    Require(result.words[0] == Untouched);
    Require(result.words[1] == 0x400000 && result.words[2] == 0x10000);
    Require(result.words[3] == 0x3000000 && result.words[4] == 0x10000);
    Require(result.words[5] == 0x1000000 && result.words[6] == 0x10000);
    Require(result.words[7] == Untouched);

    query.calibration_settings.move_position = 1;
    query.profile = 100;
    Require(sceVrTrackerQueryMemory(&query, &result) == 0 && result.words[1] == 0x800000);

    auto rejected = FreshResult();
    VrTrackerLegacy::QueryMemoryParam bad = query;
    bad.profile = 1;
    Require(sceVrTrackerQueryMemory(&bad, &rejected) == ArgumentInvalid);
    bad = query;
    bad.calibration_settings.hmd_position = 1;
    Require(sceVrTrackerQueryMemory(&bad, &rejected) == ArgumentInvalid);
    bad = query;
    bad.calibration_settings.pad_position = 2;
    Require(sceVrTrackerQueryMemory(&bad, &rejected) == ArgumentInvalid);
    bad = query;
    bad.size = sizeof(VrTrackerLegacy::QueryMemoryParam) - 1;
    Require(sceVrTrackerQueryMemory(&bad, &rejected) == ArgumentInvalid);
    Require(sceVrTrackerQueryMemory(nullptr, &rejected) == ArgumentInvalid);
    Require(sceVrTrackerQueryMemory(&query, nullptr) == ArgumentInvalid);
    Require(rejected.words[1] == Untouched);
}

void CheckBeforeInit() {
    Require(sceVrTrackerTerm() == NotInit);
    Require(sceVrTrackerRegisterDevice(Pad, 5) == NotInit);
    Require(sceVrTrackerUnregisterDevice(5) == NotInit);
    Require(Recalibrate(Pad) == NotInit);
    Require(sceVrTrackerSetDurationUntilStatusNotTracking(Pad, 10) == NotInit);
    Require(sceVrTrackerGpuWaitAndCpuProcess() == NotInit);
}

void CheckInit() {
    auto bad = ValidInit();
    bad.size = sizeof(VrTrackerLegacy::InitParam) - 8;
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&bad)) == ArgumentInvalid);
    bad.size = 144;
    RequireThrows([&] { sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&bad)); });
    bad = ValidInit();
    bad.calibration_settings.move_position = 1;
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&bad)) == ArgumentInvalid);
    bad.direct_memory_onion_size = 0x800000;
    bad.gpu_pipe_id = 7;
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&bad)) == ArgumentInvalid);
    bad.gpu_pipe_id = 0;
    bad.gpu_queue_id = 8;
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&bad)) == ArgumentInvalid);
    bad.gpu_queue_id = 0;
    bad.work_memory = nullptr;
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&bad)) == ArgumentInvalid);
    bad.work_memory = g_work;
    bad.direct_memory_garlic_alignment = 0x1000;
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&bad)) == ArgumentInvalid);
    Require(sceVrTrackerInit(nullptr) == ArgumentInvalid);
    Require(sceVrTrackerTerm() == NotInit);

    const auto init = ValidInit();
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&init)) == 0);
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&init)) == AlreadyInitialized);
    Require(sceVrTrackerInit(nullptr) == AlreadyInitialized);
    Require(sceVrTrackerGpuWaitAndCpuProcess() == NotExecuteGpuSubmit);
}

void CheckDevices() {
    Require(sceVrTrackerRegisterDevice(Pad, 5) == 0);
    Require(sceVrTrackerRegisterDevice(Pad, 6) == DeviceAlreadyRegistered);
    Require(sceVrTrackerRegisterDevice2(Move, 7) == 0);
    Require(sceVrTrackerRegisterDeviceInternal(Gun, 8, 5, 0) == ArgumentInvalid);
    Require(sceVrTrackerRegisterDevice(4, 8) == ArgumentInvalid);
    Require(sceVrTrackerRegisterDevice(-1, 8) == ArgumentInvalid);
    Require(sceVrTrackerRegisterDevice(Hmd, 9) == 0);

    Require(Recalibrate(Pad) == 0);
    Require(Recalibrate(Move) == 0);
    Require(Recalibrate(Hmd) == DeviceNotRegistered);
    Require(Recalibrate(Gun) == DeviceNotRegistered);
    Require(Recalibrate(4) == ArgumentInvalid);
    Require(Recalibrate(Pad, sizeof(VrTrackerLegacy::RecalibrateParam) + 4) == ArgumentInvalid);
    Require(sceVrTrackerRecalibrate(nullptr) == ArgumentInvalid);

    Require(sceVrTrackerSetDurationUntilStatusNotTracking(Gun, 10) == 0);
    Require(sceVrTrackerSetDurationUntilStatusNotTracking(4, 10) == ArgumentInvalid);

    Require(sceVrTrackerUnregisterDevice(5) == 0);
    Require(sceVrTrackerUnregisterDevice(5) == ArgumentInvalid);
    Require(sceVrTrackerUnregisterDevice(-1) == ArgumentInvalid);
    Require(Recalibrate(Pad) == DeviceNotRegistered);
    Require(sceVrTrackerRegisterDevice(Pad, 6) == 0);
    Require(sceVrTrackerUnregisterDevice(9) == 0);
}

void CheckTerm() {
    Require(sceVrTrackerTerm() == 0);
    Require(sceVrTrackerTerm() == NotInit);
    Require(sceVrTrackerRegisterDevice(Gun, 8) == NotInit);
    const auto init = ValidInit();
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&init)) == 0);
    Require(sceVrTrackerRegisterDevice(Move, 11) == DeviceAlreadyRegistered);
    Require(sceVrTrackerTerm() == 0);
}

} // namespace

int main() {
    CheckQueryMemory();
    CheckBeforeInit();
    CheckInit();
    CheckDevices();
    CheckTerm();
}
