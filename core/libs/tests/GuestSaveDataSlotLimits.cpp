#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t MaxMountSlots = 16;
constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate = 4;
constexpr std::uint32_t MountModeCreate2 = 32;

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorMountFull = -2137063412;
constexpr int SaveDataErrorInvalidLoginUser = -2137063407;

int g_failures = 0;

void Check(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", message.c_str());
    ++g_failures;
}

bool ThrowsOnNonEmptyTitleId() {
    SaveDataMountResult result{};
    SaveDataMountPs4 mount{};
    mount.user_id = 0x10000000;
    SceSaveDataDirName dir{};
    std::memcpy(dir.data, "titledir", sizeof("titledir"));
    mount.dir_name = &dir;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;
    char titleId[] = "CUSA00001";
    mount.title_id = titleId;
    try {
        static_cast<void>(sceSaveDataMount(&mount, &result));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool ThrowsOnUnknownMountMode(std::uint32_t mode) {
    SaveDataMountResult result{};
    SaveDataMountPs4 mount{};
    mount.user_id = 0x10000000;
    SceSaveDataDirName dir{};
    std::memcpy(dir.data, "modedir", sizeof("modedir"));
    mount.dir_name = &dir;
    mount.mount_mode = mode;
    mount.title_id = nullptr;
    try {
        static_cast<void>(sceSaveDataMount(&mount, &result));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void CheckInitializationLifecycle() {
    Check(sceSaveDataTerminate() == SaveDataErrorNotInitialized, "terminate before init returns NOT_INITIALIZED");
    Check(sceSaveDataCreateTransactionResource(100) == SaveDataErrorNotInitialized, "create transaction resource before init returns NOT_INITIALIZED");

    SaveDataMountResult result{};
    SaveDataMount2 uninitMount2{};
    uninitMount2.user_id = 0x10000000;
    SceSaveDataDirName dir{};
    std::memcpy(dir.data, "uninitdir", sizeof("uninitdir"));
    uninitMount2.dir_name = &dir;
    uninitMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    Check(sceSaveDataMount2(&uninitMount2, &result) == SaveDataErrorNotInitialized, "mount2 before init returns NOT_INITIALIZED");

    SaveDataMemorySetup2 uninitSetup{};
    uninitSetup.user_id = 0x10000000;
    uninitSetup.memory_size = 0x1000;
    SaveDataMemorySetupResult setupResult{};
    Check(sceSaveDataSetupSaveDataMemory2(&uninitSetup, &setupResult) == SaveDataErrorNotInitialized, "setup memory2 before init returns NOT_INITIALIZED");

    Check(sceSaveDataInitialize(nullptr) == SaveDataOk, "first initialize succeeds");
    Check(sceSaveDataInitialize(nullptr) == SaveDataOk, "double initialize increments reference");

    Check(sceSaveDataTerminate() == SaveDataOk, "first terminate decrements reference");
    Check(sceSaveDataTerminate() == SaveDataOk, "second terminate reaches zero");
    Check(sceSaveDataTerminate() == SaveDataErrorNotInitialized, "third terminate returns NOT_INITIALIZED");

    Check(sceSaveDataInitialize(nullptr) == SaveDataOk, "re-initialize after complete termination succeeds");
}

void CheckMountParameterBoundaries() {
    SaveDataMountResult result{};
    Check(sceSaveDataMount(nullptr, &result) == SaveDataErrorParameter, "mount ps4 null mount returns PARAMETER");
    SaveDataMountPs4 nullMount{};
    Check(sceSaveDataMount(&nullMount, nullptr) == SaveDataErrorParameter, "mount ps4 null result returns PARAMETER");
    Check(sceSaveDataMount(&nullMount, &result) == SaveDataErrorParameter, "mount ps4 null dir_name returns PARAMETER");

    Check(sceSaveDataMount2(nullptr, &result) == SaveDataErrorParameter, "mount2 null mount returns PARAMETER");
    SaveDataMount2 nullMount2{};
    Check(sceSaveDataMount2(&nullMount2, nullptr) == SaveDataErrorParameter, "mount2 null result returns PARAMETER");

    Check(sceSaveDataMount3(nullptr, &result) == SaveDataErrorParameter, "mount3 null mount returns PARAMETER");
    SaveDataMount3 nullMount3{};
    Check(sceSaveDataMount3(&nullMount3, nullptr) == SaveDataErrorParameter, "mount3 null result returns PARAMETER");
    Check(sceSaveDataMount3(&nullMount3, &result) == SaveDataErrorParameter, "mount3 null dir_name returns PARAMETER");

    Check(ThrowsOnNonEmptyTitleId(), "mount with non-null title_id throws not implemented");

    Check(ThrowsOnUnknownMountMode(0), "mount with mode 0 throws unknown mount mode");
    Check(ThrowsOnUnknownMountMode(0x80000000u), "mount with unsupported flag throws unknown mount mode");

    SaveDataMount2 invalidFlagsMount2{};
    invalidFlagsMount2.user_id = 0x10000000;
    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "modezerodir", sizeof("modezerodir"));
    invalidFlagsMount2.dir_name = &validDir;
    invalidFlagsMount2.mount_mode = 0;
    Check(sceSaveDataMount2(&invalidFlagsMount2, &result) == SaveDataErrorParameter, "mount2 with mode 0 returns PARAMETER");

    invalidFlagsMount2.mount_mode = MountModeCreate;
    invalidFlagsMount2.blocks = 1;
    Check(sceSaveDataMount2(&invalidFlagsMount2, &result) == SaveDataErrorParameter, "mount2 create with blocks below minimum returns PARAMETER");

    invalidFlagsMount2.blocks = 96;
    invalidFlagsMount2.user_id = 0;
    Check(sceSaveDataMount2(&invalidFlagsMount2, &result) == SaveDataErrorInvalidLoginUser, "mount2 user_id 0 returns INVALID_LOGIN_USER");
    invalidFlagsMount2.user_id = -1;
    Check(sceSaveDataMount2(&invalidFlagsMount2, &result) == SaveDataErrorInvalidLoginUser, "mount2 user_id -1 returns INVALID_LOGIN_USER");

    SceSaveDataDirName emptyDir{};
    emptyDir.data[0] = '\0';
    SaveDataMountPs4 emptyDirMount{};
    emptyDirMount.user_id = 0x10000000;
    emptyDirMount.dir_name = &emptyDir;
    emptyDirMount.mount_mode = MountModeRdWr | MountModeCreate2;
    emptyDirMount.title_id = nullptr;
    Check(sceSaveDataMount(&emptyDirMount, &result) == SaveDataErrorParameter, "mount with empty dir name returns PARAMETER");

    SceSaveDataDirName traversalDir{};
    std::memcpy(traversalDir.data, "../escape", sizeof("../escape"));
    SaveDataMountPs4 traversalMount{};
    traversalMount.user_id = 0x10000000;
    traversalMount.dir_name = &traversalDir;
    traversalMount.mount_mode = MountModeRdWr | MountModeCreate2;
    traversalMount.title_id = nullptr;
    Check(sceSaveDataMount(&traversalMount, &result) == SaveDataErrorParameter, "mount with relative traversal returns PARAMETER");
}

void CheckInvalidSlotIndexRejection() {
    SaveDataMountPoint invalidSlotNeg1{};
    std::memcpy(invalidSlotNeg1.data, "/savedata-1", sizeof("/savedata-1"));
    SaveDataMountPoint invalidSlot16{};
    std::memcpy(invalidSlot16.data, "/savedata16", sizeof("/savedata16"));
    SaveDataMountPoint invalidSlot99{};
    std::memcpy(invalidSlot99.data, "/savedata99", sizeof("/savedata99"));
    SaveDataMountPoint invalidSlotNonNumeric{};
    std::memcpy(invalidSlotNonNumeric.data, "/savedataabc", sizeof("/savedataabc"));
    SaveDataMountPoint emptyMountPoint{};
    emptyMountPoint.data[0] = '\0';

    const std::array<SaveDataMountPoint, 5> invalidPoints = {
        invalidSlotNeg1,
        invalidSlot16,
        invalidSlot99,
        invalidSlotNonNumeric,
        emptyMountPoint
    };

    SaveDataMountInfo info{};
    SaveDataIcon icon{};
    char iconBuffer[64]{};
    icon.buf = iconBuffer;
    icon.buf_size = sizeof(iconBuffer);
    char paramBuf[64]{};
    std::size_t gotParamSize = 0;

    for (const auto& point : invalidPoints) {
        Check(sceSaveDataUmount(&point) == SaveDataErrorNotMounted, "umount invalid slot index returns NOT_MOUNTED");
        Check(sceSaveDataUmount2(0, &point) == SaveDataErrorNotMounted, "umount2 invalid slot index returns NOT_MOUNTED");
        Check(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted, "getMountInfo invalid slot index returns NOT_MOUNTED");
        Check(sceSaveDataLoadIcon(&point, &icon) == SaveDataErrorNotMounted, "loadIcon invalid slot index returns NOT_MOUNTED");
        Check(sceSaveDataSaveIcon(&point, &icon) == SaveDataErrorNotMounted, "saveIcon invalid slot index returns NOT_MOUNTED");
        Check(sceSaveDataGetParam(&point, 1, paramBuf, sizeof(paramBuf), &gotParamSize) == SaveDataErrorNotMounted, "getParam invalid slot index returns NOT_MOUNTED");
        Check(sceSaveDataSetParam(&point, 1, paramBuf, sizeof(paramBuf)) == SaveDataErrorNotMounted, "setParam invalid slot index returns NOT_MOUNTED");
    }

    Check(sceSaveDataUmount(nullptr) == SaveDataErrorParameter, "umount null point returns PARAMETER");
    Check(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter, "umount2 null point returns PARAMETER");
    Check(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter, "getMountInfo null point returns PARAMETER");
    Check(sceSaveDataLoadIcon(nullptr, &icon) == SaveDataErrorParameter, "loadIcon null point returns PARAMETER");
    Check(sceSaveDataSaveIcon(nullptr, &icon) == SaveDataErrorParameter, "saveIcon null point returns PARAMETER");

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'z', sizeof(unterminatedPoint.data));
    Check(sceSaveDataUmount(&unterminatedPoint) == SaveDataErrorParameter, "umount unterminated point returns PARAMETER");
    Check(sceSaveDataUmount2(0, &unterminatedPoint) == SaveDataErrorParameter, "umount2 unterminated point returns PARAMETER");
}

void CheckSlotLimitsAndRecycling() {
    std::array<SceSaveDataDirName, MaxMountSlots> dirNames{};
    std::array<SaveDataMountPs4, MaxMountSlots> mounts{};
    std::array<SaveDataMountResult, MaxMountSlots> results{};

    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        std::snprintf(dirNames[i].data, sizeof(dirNames[i].data), "slotdir%02zu", i);
        mounts[i].user_id = 0x10000000;
        mounts[i].dir_name = &dirNames[i];
        mounts[i].mount_mode = MountModeRdWr | MountModeCreate2;
        mounts[i].title_id = nullptr;
        mounts[i].blocks = 96;

        Check(sceSaveDataMount(&mounts[i], &results[i]) == SaveDataOk, "mount slot " + std::to_string(i) + " succeeds");
        const std::string expectedPoint = "/savedata" + std::to_string(i);
        Check(std::strcmp(results[i].mount_point.data, expectedPoint.c_str()) == 0, "slot " + std::to_string(i) + " gets expected mount point " + expectedPoint);
    }

    Check(sceSaveDataTerminate() == SaveDataErrorBusy, "terminate returns BUSY while slots are mounted");

    SceSaveDataDirName overflowDir{};
    std::memcpy(overflowDir.data, "overflowdir", sizeof("overflowdir"));
    SaveDataMountPs4 overflowMountPs4{};
    overflowMountPs4.user_id = 0x10000000;
    overflowMountPs4.dir_name = &overflowDir;
    overflowMountPs4.mount_mode = MountModeRdWr | MountModeCreate2;
    overflowMountPs4.title_id = nullptr;
    SaveDataMountResult overflowResult{};
    Check(sceSaveDataMount(&overflowMountPs4, &overflowResult) == SaveDataErrorMountFull, "sceSaveDataMount beyond 16 slots returns MOUNT_FULL");

    SaveDataMount3 overflowMount3{};
    overflowMount3.user_id = 0x10000000;
    overflowMount3.dir_name = &overflowDir;
    overflowMount3.mount_mode = MountModeRdWr | MountModeCreate2;
    Check(sceSaveDataMount3(&overflowMount3, &overflowResult) == SaveDataErrorMountFull, "sceSaveDataMount3 beyond 16 slots returns MOUNT_FULL");

    SaveDataMount2 overflowMount2{};
    overflowMount2.user_id = 0x10000000;
    overflowMount2.dir_name = &overflowDir;
    overflowMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    overflowMount2.blocks = 96;
    Check(sceSaveDataMount2(&overflowMount2, &overflowResult) == SaveDataErrorMountFull, "sceSaveDataMount2 beyond 16 slots returns MOUNT_FULL");

    Check(sceSaveDataUmount(&results[0].mount_point) == SaveDataOk, "unmount slot 0 succeeds");
    Check(sceSaveDataUmount(&results[6].mount_point) == SaveDataOk, "unmount slot 6 succeeds");
    Check(sceSaveDataUmount(&results[15].mount_point) == SaveDataOk, "unmount slot 15 succeeds");

    Check(sceSaveDataUmount(&results[0].mount_point) == SaveDataErrorNotMounted, "unmounting already unmounted slot 0 returns NOT_MOUNTED");
    Check(sceSaveDataUmount(&results[6].mount_point) == SaveDataErrorNotMounted, "unmounting already unmounted slot 6 returns NOT_MOUNTED");
    Check(sceSaveDataUmount(&results[15].mount_point) == SaveDataErrorNotMounted, "unmounting already unmounted slot 15 returns NOT_MOUNTED");

    SceSaveDataDirName reclaimDir0{};
    std::memcpy(reclaimDir0.data, "reclaimdir0", sizeof("reclaimdir0"));
    SaveDataMountPs4 reclaimMount0{};
    reclaimMount0.user_id = 0x10000000;
    reclaimMount0.dir_name = &reclaimDir0;
    reclaimMount0.mount_mode = MountModeRdWr | MountModeCreate2;
    reclaimMount0.title_id = nullptr;
    SaveDataMountResult reclaimResult0{};
    Check(sceSaveDataMount(&reclaimMount0, &reclaimResult0) == SaveDataOk, "mount reclaims lowest free slot 0");
    Check(std::strcmp(reclaimResult0.mount_point.data, "/savedata0") == 0, "reclaimed slot gets /savedata0");

    SceSaveDataDirName reclaimDir6{};
    std::memcpy(reclaimDir6.data, "reclaimdir6", sizeof("reclaimdir6"));
    SaveDataMountPs4 reclaimMount6{};
    reclaimMount6.user_id = 0x10000000;
    reclaimMount6.dir_name = &reclaimDir6;
    reclaimMount6.mount_mode = MountModeRdWr | MountModeCreate2;
    reclaimMount6.title_id = nullptr;
    SaveDataMountResult reclaimResult6{};
    Check(sceSaveDataMount(&reclaimMount6, &reclaimResult6) == SaveDataOk, "mount reclaims slot 6");
    Check(std::strcmp(reclaimResult6.mount_point.data, "/savedata6") == 0, "reclaimed slot gets /savedata6");

    SceSaveDataDirName reclaimDir15{};
    std::memcpy(reclaimDir15.data, "reclaimdir15", sizeof("reclaimdir15"));
    SaveDataMountPs4 reclaimMount15{};
    reclaimMount15.user_id = 0x10000000;
    reclaimMount15.dir_name = &reclaimDir15;
    reclaimMount15.mount_mode = MountModeRdWr | MountModeCreate2;
    reclaimMount15.title_id = nullptr;
    SaveDataMountResult reclaimResult15{};
    Check(sceSaveDataMount(&reclaimMount15, &reclaimResult15) == SaveDataOk, "mount reclaims slot 15");
    Check(std::strcmp(reclaimResult15.mount_point.data, "/savedata15") == 0, "reclaimed slot gets /savedata15");

    Check(sceSaveDataMount(&overflowMountPs4, &overflowResult) == SaveDataErrorMountFull, "mount returns MOUNT_FULL after all slots re-filled");

    results[0] = reclaimResult0;
    results[6] = reclaimResult6;
    results[15] = reclaimResult15;
    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        Check(sceSaveDataUmount(&results[i].mount_point) == SaveDataOk, "unmount slot " + std::to_string(i) + " succeeds");
    }

    Check(sceSaveDataTerminate() == SaveDataOk, "terminate succeeds when all slots unmounted");

    SaveDataMount2 postTerminateMount{};
    postTerminateMount.user_id = 0x10000000;
    postTerminateMount.dir_name = &reclaimDir0;
    postTerminateMount.mount_mode = MountModeRdWr | MountModeCreate2;
    Check(sceSaveDataMount2(&postTerminateMount, &overflowResult) == SaveDataErrorNotInitialized, "mount2 after terminate returns NOT_INITIALIZED");
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-limits-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    CheckInitializationLifecycle();
    CheckMountParameterBoundaries();
    CheckInvalidSlotIndexRejection();
    CheckSlotLimitsAndRecycling();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);

    if (g_failures != 0) {
        std::fprintf(stderr, "GuestSaveDataSlotLimits failed with %d errors\n", g_failures);
        return 1;
    }
    std::printf("GuestSaveDataSlotLimits passed all checks\n");
    return 0;
}
