#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceVrTrackerQueryMemory(const void*, void*);
int APS5_VABI sceVrTrackerInit(const void*);
int APS5_VABI sceVrTrackerTerm();
int APS5_VABI sceVrTrackerRegisterDevice(int, int);
int APS5_VABI sceVrTrackerRegisterDevice2(int, int);
int APS5_VABI sceVrTrackerRegisterDeviceInternal(int, int, int, int);
int APS5_VABI sceVrTrackerUnregisterDevice(int);
int APS5_VABI sceVrTrackerRecalibrate(const void*);
int APS5_VABI sceVrTrackerSetDurationUntilStatusNotTracking(int, std::uint32_t);
int APS5_VABI sceVrTrackerGpuWaitAndCpuProcess();
}

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

struct Calibration {
    std::uint32_t hmd, pad, move, gun;
    std::uint32_t reserved[4];
};

struct QueryParam {
    std::uint32_t size = sizeof(QueryParam);
    std::uint32_t profile = 0;
    std::uint32_t reserved[6]{};
    Calibration calibration{};
};

struct QueryResult {
    std::uint32_t words[16];
};

struct InitParam {
    std::uint32_t size = sizeof(InitParam);
    std::uint32_t profile = 0;
    std::uint32_t executionMode = 0;
    std::int32_t priorities[4]{};
    std::int32_t reserved = 0;
    std::uint64_t cpuMask = 0;
    Calibration calibration{};
    void* onion = nullptr;
    std::uint32_t onionSize = 0x400000;
    std::uint32_t onionAlignment = 0x10000;
    void* garlic = nullptr;
    std::uint32_t garlicSize = 0x3000000;
    std::uint32_t garlicAlignment = 0x10000;
    void* work = nullptr;
    std::uint32_t workSize = 0x1000000;
    std::uint32_t workAlignment = 0x10000;
    std::int32_t pipe = 0;
    std::int32_t queue = 0;
};

struct RecalibrateParam {
    std::uint32_t size = sizeof(RecalibrateParam);
    std::uint32_t deviceType = 0;
    std::uint32_t calibrationType = 0;
    std::uint32_t reserved[5]{};
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

InitParam ValidInit() {
    InitParam init;
    init.onion = g_onion;
    init.garlic = g_garlic;
    init.work = g_work;
    return init;
}

int Recalibrate(std::uint32_t deviceType, std::uint32_t size = sizeof(RecalibrateParam)) {
    RecalibrateParam param;
    param.size = size;
    param.deviceType = deviceType;
    return sceVrTrackerRecalibrate(&param);
}

void CheckQueryMemory() {
    QueryParam query;
    auto result = FreshResult();
    Require(sceVrTrackerQueryMemory(&query, &result) == 0);
    Require(result.words[0] == Untouched);
    Require(result.words[1] == 0x400000 && result.words[2] == 0x10000);
    Require(result.words[3] == 0x3000000 && result.words[4] == 0x10000);
    Require(result.words[5] == 0x1000000 && result.words[6] == 0x10000);
    Require(result.words[7] == Untouched);

    query.calibration.move = 1;
    query.profile = 100;
    Require(sceVrTrackerQueryMemory(&query, &result) == 0 && result.words[1] == 0x800000);

    auto rejected = FreshResult();
    QueryParam bad = query;
    bad.profile = 1;
    Require(sceVrTrackerQueryMemory(&bad, &rejected) == ArgumentInvalid);
    bad = query;
    bad.calibration.hmd = 1;
    Require(sceVrTrackerQueryMemory(&bad, &rejected) == ArgumentInvalid);
    bad = query;
    bad.calibration.pad = 2;
    Require(sceVrTrackerQueryMemory(&bad, &rejected) == ArgumentInvalid);
    bad = query;
    bad.size = sizeof(QueryParam) - 1;
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
    bad.size = sizeof(InitParam) - 8;
    Require(sceVrTrackerInit(&bad) == ArgumentInvalid);
    bad.size = 144;
    RequireThrows([&] { sceVrTrackerInit(&bad); });
    bad = ValidInit();
    bad.calibration.move = 1;
    Require(sceVrTrackerInit(&bad) == ArgumentInvalid);
    bad.onionSize = 0x800000;
    bad.pipe = 7;
    Require(sceVrTrackerInit(&bad) == ArgumentInvalid);
    bad.pipe = 0;
    bad.queue = 8;
    Require(sceVrTrackerInit(&bad) == ArgumentInvalid);
    bad.queue = 0;
    bad.work = nullptr;
    Require(sceVrTrackerInit(&bad) == ArgumentInvalid);
    bad.work = g_work;
    bad.garlicAlignment = 0x1000;
    Require(sceVrTrackerInit(&bad) == ArgumentInvalid);
    Require(sceVrTrackerInit(nullptr) == ArgumentInvalid);
    Require(sceVrTrackerTerm() == NotInit);

    const auto init = ValidInit();
    Require(sceVrTrackerInit(&init) == 0);
    Require(sceVrTrackerInit(&init) == AlreadyInitialized);
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
    Require(Recalibrate(Pad, sizeof(RecalibrateParam) + 4) == ArgumentInvalid);
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
    Require(sceVrTrackerInit(&init) == 0);
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
