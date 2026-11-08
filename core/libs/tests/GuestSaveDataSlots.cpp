#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

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

int g_failures = 0;

void Check(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", message.c_str());
    ++g_failures;
}

void CheckNullAndUninitialized() {
    SaveDataMountResult result{};
    Check(sceSaveDataMount3(nullptr, &result) == SaveDataErrorParameter, "mount3 null mount");
    SaveDataMount3 nullResultMount{};
    Check(sceSaveDataMount3(&nullResultMount, nullptr) == SaveDataErrorParameter, "mount3 null result");
    Check(sceSaveDataMount3(&nullResultMount, &result) == SaveDataErrorParameter, "mount3 null dir_name");

    SaveDataMount2 nullMount2{};
    Check(sceSaveDataMount2(nullptr, &result) == SaveDataErrorParameter, "mount2 null mount");
    Check(sceSaveDataMount2(&nullMount2, nullptr) == SaveDataErrorParameter, "mount2 null result");

    SaveDataMountPs4 nullMountPs4{};
    Check(sceSaveDataMount(nullptr, &result) == SaveDataErrorParameter, "mount ps4 null mount");
    Check(sceSaveDataMount(&nullMountPs4, nullptr) == SaveDataErrorParameter, "mount ps4 null result");
    Check(sceSaveDataMount(&nullMountPs4, &result) == SaveDataErrorParameter, "mount ps4 null dir_name");

    Check(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter, "umount2 null mount point");
    Check(sceSaveDataUmount(nullptr) == SaveDataErrorParameter, "umount null mount point");

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'a', sizeof(unterminatedPoint.data));
    Check(sceSaveDataUmount2(0, &unterminatedPoint) == SaveDataErrorParameter, "umount2 unterminated point");
    Check(sceSaveDataUmount(&unterminatedPoint) == SaveDataErrorParameter, "umount unterminated point");

    SaveDataMountPoint outOfRangePoint{};
    std::memcpy(outOfRangePoint.data, "/savedata16", sizeof("/savedata16"));
    Check(sceSaveDataUmount2(0, &outOfRangePoint) == SaveDataErrorNotMounted, "umount2 slot 16 unmounted");
    Check(sceSaveDataUmount(&outOfRangePoint) == SaveDataErrorNotMounted, "umount slot 16 unmounted");

    SaveDataMountPoint arbitraryPoint{};
    std::memcpy(arbitraryPoint.data, "/savedata99", sizeof("/savedata99"));
    Check(sceSaveDataUmount2(0, &arbitraryPoint) == SaveDataErrorNotMounted, "umount2 slot 99 unmounted");
    Check(sceSaveDataUmount(&arbitraryPoint) == SaveDataErrorNotMounted, "umount slot 99 unmounted");

    Check(sceSaveDataAbort() == SaveDataErrorNotInitialized, "abort before init");
    Check(sceSaveDataClearProgress() == SaveDataErrorNotInitialized, "clearProgress before init");

    SaveDataMount2 uninitMount2{};
    uninitMount2.user_id = 0x10000000;
    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "uninit", sizeof("uninit"));
    uninitMount2.dir_name = &validDir;
    uninitMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    Check(sceSaveDataMount2(&uninitMount2, &result) == SaveDataErrorNotInitialized, "mount2 before init");
}

void CheckDirectoryNameBoundsAndTraversal() {
    const std::vector<std::string> invalidNames = {
        "",
        ".",
        "..",
        "../escape",
        "..\\escape",
        "../../root",
        "dir/../outside",
        "nested/dir",
        "nested\\dir",
        "device:dir",
        "a:b",
        "colon:save",
        "/absolute",
        "\\backslash"
    };

    for (const auto& invalid : invalidNames) {
        SceSaveDataDirName dirName{};
        if (invalid.size() < sizeof(dirName.data)) {
            std::memcpy(dirName.data, invalid.c_str(), invalid.size() + 1);
        } else {
            std::memcpy(dirName.data, invalid.data(), sizeof(dirName.data));
        }

        SaveDataMount3 mount3Req{};
        mount3Req.user_id = 0x10000000;
        mount3Req.dir_name = &dirName;
        mount3Req.mount_mode = MountModeRdWr | MountModeCreate2;
        SaveDataMountResult result3{};
        Check(sceSaveDataMount3(&mount3Req, &result3) == SaveDataErrorParameter,
              "mount3 rejected invalid name: " + invalid);

        SaveDataMount2 mount2Req{};
        mount2Req.user_id = 0x10000000;
        mount2Req.dir_name = &dirName;
        mount2Req.mount_mode = MountModeRdWr | MountModeCreate2;
        SaveDataMountResult result2{};
        Check(sceSaveDataMount2(&mount2Req, &result2) == SaveDataErrorParameter,
              "mount2 rejected invalid name: " + invalid);

        SaveDataMountPs4 mountPs4Req{};
        mountPs4Req.user_id = 0x10000000;
        mountPs4Req.dir_name = &dirName;
        mountPs4Req.mount_mode = MountModeRdWr | MountModeCreate2;
        SaveDataMountResult resultPs4{};
        Check(sceSaveDataMount(&mountPs4Req, &resultPs4) == SaveDataErrorParameter,
              "mount ps4 rejected invalid name: " + invalid);

        SaveDataDelete delReq{};
        delReq.user_id = 0x10000000;
        delReq.dir_name = &dirName;
        Check(sceSaveDataDelete(&delReq) == SaveDataErrorParameter,
              "delete rejected invalid name: " + invalid);

        SaveDataCheckBackupData checkReq{};
        checkReq.user_id = 0x10000000;
        checkReq.dir_name = &dirName;
        Check(sceSaveDataCheckBackupData(&checkReq) == SaveDataErrorParameter,
              "checkBackupData rejected invalid name: " + invalid);

        SaveDataRestoreBackupData restoreReq{};
        restoreReq.user_id = 0x10000000;
        restoreReq.dir_name = &dirName;
        Check(sceSaveDataRestoreBackupData(&restoreReq) == SaveDataErrorParameter,
              "restoreBackupData rejected invalid name: " + invalid);
    }

    char unterminatedBuffer[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminatedBuffer, 'z', sizeof(unterminatedBuffer));
    const auto* unterminatedDir = reinterpret_cast<const SceSaveDataDirName*>(unterminatedBuffer);

    SaveDataMount3 unterminatedMount3{};
    unterminatedMount3.user_id = 0x10000000;
    unterminatedMount3.dir_name = unterminatedDir;
    unterminatedMount3.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult unterminatedResult{};
    Check(sceSaveDataMount3(&unterminatedMount3, &unterminatedResult) == SaveDataErrorParameter,
          "mount3 rejected unterminated name");

    SaveDataMount2 unterminatedMount2{};
    unterminatedMount2.user_id = 0x10000000;
    unterminatedMount2.dir_name = unterminatedDir;
    unterminatedMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    Check(sceSaveDataMount2(&unterminatedMount2, &unterminatedResult) == SaveDataErrorParameter,
          "mount2 rejected unterminated name");

    SaveDataMountPs4 unterminatedMountPs4{};
    unterminatedMountPs4.user_id = 0x10000000;
    unterminatedMountPs4.dir_name = unterminatedDir;
    unterminatedMountPs4.mount_mode = MountModeRdWr | MountModeCreate2;
    Check(sceSaveDataMount(&unterminatedMountPs4, &unterminatedResult) == SaveDataErrorParameter,
          "mount ps4 rejected unterminated name");

    SaveDataDelete unterminatedDelete{};
    unterminatedDelete.user_id = 0x10000000;
    unterminatedDelete.dir_name = unterminatedDir;
    Check(sceSaveDataDelete(&unterminatedDelete) == SaveDataErrorParameter,
          "delete rejected unterminated name");

    SaveDataCheckBackupData unterminatedCheck{};
    unterminatedCheck.user_id = 0x10000000;
    unterminatedCheck.dir_name = unterminatedDir;
    Check(sceSaveDataCheckBackupData(&unterminatedCheck) == SaveDataErrorParameter,
          "checkBackupData rejected unterminated name");

    SaveDataRestoreBackupData unterminatedRestore{};
    unterminatedRestore.user_id = 0x10000000;
    unterminatedRestore.dir_name = unterminatedDir;
    Check(sceSaveDataRestoreBackupData(&unterminatedRestore) == SaveDataErrorParameter,
          "restoreBackupData rejected unterminated name");

    SceSaveDataDirName singleCharDir{};
    std::memcpy(singleCharDir.data, "x", sizeof("x"));
    SaveDataMount3 singleCharMount{};
    singleCharMount.user_id = 0x10000000;
    singleCharMount.dir_name = &singleCharDir;
    singleCharMount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult singleCharResult{};
    Check(sceSaveDataMount3(&singleCharMount, &singleCharResult) == SaveDataOk,
          "mount single character dir name succeeds");
    Check(sceSaveDataUmount2(0, &singleCharResult.mount_point) == SaveDataOk,
          "unmount single character dir name succeeds");

    SceSaveDataDirName maxLenDir{};
    std::memset(maxLenDir.data, 'k', sizeof(maxLenDir.data) - 1);
    maxLenDir.data[sizeof(maxLenDir.data) - 1] = '\0';
    SaveDataMount3 maxLenMount{};
    maxLenMount.user_id = 0x10000000;
    maxLenMount.dir_name = &maxLenDir;
    maxLenMount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult maxLenResult{};
    Check(sceSaveDataMount3(&maxLenMount, &maxLenResult) == SaveDataOk,
          "mount 31-character max length dir name succeeds");
    Check(sceSaveDataUmount2(0, &maxLenResult.mount_point) == SaveDataOk,
          "unmount 31-character max length dir name succeeds");
}

void CheckSlotCapacityAndRecycling() {
    std::array<SceSaveDataDirName, MaxMountSlots> slotNames{};
    std::array<SaveDataMountResult, MaxMountSlots> results{};
    std::array<bool, MaxMountSlots> active{};
    active.fill(false);

    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        std::snprintf(slotNames[i].data, sizeof(slotNames[i].data), "container%02zu", i);
        SaveDataMount3 mount{};
        mount.user_id = 0x10000000;
        mount.dir_name = &slotNames[i];
        mount.mount_mode = MountModeRdWr | MountModeCreate2;
        const int rc = sceSaveDataMount3(&mount, &results[i]);
        Check(rc == SaveDataOk, "mount slot index " + std::to_string(i));
        const std::string expectedPoint = "/savedata" + std::to_string(i);
        Check(std::strcmp(results[i].mount_point.data, expectedPoint.c_str()) == 0,
              "slot " + std::to_string(i) + " receives mount point " + expectedPoint);
        if (rc == SaveDataOk) {
            active[i] = true;
        }
    }

    SceSaveDataDirName overflowName{};
    std::memcpy(overflowName.data, "overflowslot", sizeof("overflowslot"));
    SaveDataMount3 overflowMount3{};
    overflowMount3.user_id = 0x10000000;
    overflowMount3.dir_name = &overflowName;
    overflowMount3.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult overflowResult3{};
    Check(sceSaveDataMount3(&overflowMount3, &overflowResult3) == SaveDataErrorMountFull,
          "mount3 beyond 16 slots returns SAVE_DATA_ERROR_MOUNT_FULL");

    SaveDataMount2 overflowMount2{};
    overflowMount2.user_id = 0x10000000;
    overflowMount2.dir_name = &overflowName;
    overflowMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult overflowResult2{};
    Check(sceSaveDataMount2(&overflowMount2, &overflowResult2) == SaveDataErrorMountFull,
          "mount2 beyond 16 slots returns SAVE_DATA_ERROR_MOUNT_FULL");

    SaveDataMountPs4 overflowMountPs4{};
    overflowMountPs4.user_id = 0x10000000;
    overflowMountPs4.dir_name = &overflowName;
    overflowMountPs4.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult overflowResultPs4{};
    Check(sceSaveDataMount(&overflowMountPs4, &overflowResultPs4) == SaveDataErrorMountFull,
          "mount ps4 beyond 16 slots returns SAVE_DATA_ERROR_MOUNT_FULL");

    Check(sceSaveDataTerminate() == SaveDataErrorBusy,
          "terminate returns SAVE_DATA_ERROR_BUSY while containers are mounted");

    constexpr std::size_t freedSlotA = 4;
    Check(sceSaveDataUmount2(0, &results[freedSlotA].mount_point) == SaveDataOk,
          "unmount slot 4 succeeds");
    active[freedSlotA] = false;

    SceSaveDataDirName reclaimNameA{};
    std::memcpy(reclaimNameA.data, "reclaimed04", sizeof("reclaimed04"));
    SaveDataMount3 reclaimMountA{};
    reclaimMountA.user_id = 0x10000000;
    reclaimMountA.dir_name = &reclaimNameA;
    reclaimMountA.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult reclaimResultA{};
    Check(sceSaveDataMount3(&reclaimMountA, &reclaimResultA) == SaveDataOk,
          "mounting into freed slot 4 succeeds");
    Check(std::strcmp(reclaimResultA.mount_point.data, "/savedata4") == 0,
          "reclaimed slot receives /savedata4");
    results[freedSlotA] = reclaimResultA;
    active[freedSlotA] = true;

    Check(sceSaveDataMount3(&overflowMount3, &overflowResult3) == SaveDataErrorMountFull,
          "capacity is full again after reclaiming slot 4");

    constexpr std::size_t freeIndex0 = 0;
    constexpr std::size_t freeIndex7 = 7;
    constexpr std::size_t freeIndex15 = 15;

    Check(sceSaveDataUmount2(0, &results[freeIndex0].mount_point) == SaveDataOk, "unmount slot 0");
    active[freeIndex0] = false;
    Check(sceSaveDataUmount2(0, &results[freeIndex7].mount_point) == SaveDataOk, "unmount slot 7");
    active[freeIndex7] = false;
    Check(sceSaveDataUmount2(0, &results[freeIndex15].mount_point) == SaveDataOk, "unmount slot 15");
    active[freeIndex15] = false;

    SceSaveDataDirName newName0{};
    std::memcpy(newName0.data, "newslot00", sizeof("newslot00"));
    SaveDataMount3 newMount0{};
    newMount0.user_id = 0x10000000;
    newMount0.dir_name = &newName0;
    newMount0.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult newResult0{};
    Check(sceSaveDataMount3(&newMount0, &newResult0) == SaveDataOk, "mount reclaims lowest free slot 0");
    Check(std::strcmp(newResult0.mount_point.data, "/savedata0") == 0, "first freed slot gets /savedata0");
    results[freeIndex0] = newResult0;
    active[freeIndex0] = true;

    SceSaveDataDirName newName7{};
    std::memcpy(newName7.data, "newslot07", sizeof("newslot07"));
    SaveDataMount3 newMount7{};
    newMount7.user_id = 0x10000000;
    newMount7.dir_name = &newName7;
    newMount7.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult newResult7{};
    Check(sceSaveDataMount3(&newMount7, &newResult7) == SaveDataOk, "mount reclaims next free slot 7");
    Check(std::strcmp(newResult7.mount_point.data, "/savedata7") == 0, "second freed slot gets /savedata7");
    results[freeIndex7] = newResult7;
    active[freeIndex7] = true;

    SceSaveDataDirName newName15{};
    std::memcpy(newName15.data, "newslot15", sizeof("newslot15"));
    SaveDataMount3 newMount15{};
    newMount15.user_id = 0x10000000;
    newMount15.dir_name = &newName15;
    newMount15.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult newResult15{};
    Check(sceSaveDataMount3(&newMount15, &newResult15) == SaveDataOk, "mount reclaims last free slot 15");
    Check(std::strcmp(newResult15.mount_point.data, "/savedata15") == 0, "third freed slot gets /savedata15");
    results[freeIndex15] = newResult15;
    active[freeIndex15] = true;

    Check(sceSaveDataMount3(&overflowMount3, &overflowResult3) == SaveDataErrorMountFull,
          "capacity is exhausted again after filling all slots");

    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        if (active[i]) {
            Check(sceSaveDataUmount2(0, &results[i].mount_point) == SaveDataOk,
                  "unmount active slot " + std::to_string(i));
            active[i] = false;
        }
    }

    Check(sceSaveDataUmount2(0, &results[0].mount_point) == SaveDataErrorNotMounted,
          "unmounting already unmounted container returns SAVE_DATA_ERROR_NOT_MOUNTED");
    Check(sceSaveDataUmount(&results[0].mount_point) == SaveDataErrorNotMounted,
          "sceSaveDataUmount on already unmounted container returns SAVE_DATA_ERROR_NOT_MOUNTED");

    std::array<SceSaveDataDirName, MaxMountSlots> cycleNames{};
    std::array<SaveDataMountResult, MaxMountSlots> cycleResults{};
    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        std::snprintf(cycleNames[i].data, sizeof(cycleNames[i].data), "recycle%02zu", i);
        SaveDataMount3 mount{};
        mount.user_id = 0x10000000;
        mount.dir_name = &cycleNames[i];
        mount.mount_mode = MountModeRdWr | MountModeCreate2;
        Check(sceSaveDataMount3(&mount, &cycleResults[i]) == SaveDataOk,
              "recycle mount slot " + std::to_string(i));
    }

    Check(sceSaveDataMount3(&overflowMount3, &overflowResult3) == SaveDataErrorMountFull,
          "overflow after full recycling returns SAVE_DATA_ERROR_MOUNT_FULL");

    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        Check(sceSaveDataUmount2(0, &cycleResults[i].mount_point) == SaveDataOk,
              "recycle unmount slot " + std::to_string(i));
    }
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("anyps5-savedata-slots-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    CheckNullAndUninitialized();

    Check(sceSaveDataInitialize3(nullptr) == SaveDataOk, "SaveData initializes cleanly");

    CheckDirectoryNameBoundsAndTraversal();
    CheckSlotCapacityAndRecycling();

    Check(sceSaveDataTerminate() == SaveDataOk, "SaveData terminates cleanly when all slots unmounted");

    SaveDataMount2 postUninitMount2{};
    postUninitMount2.user_id = 0x10000000;
    SceSaveDataDirName postDir{};
    std::memcpy(postDir.data, "postuninit", sizeof("postuninit"));
    postUninitMount2.dir_name = &postDir;
    postUninitMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult postResult{};
    Check(sceSaveDataMount2(&postUninitMount2, &postResult) == SaveDataErrorNotInitialized,
          "mount2 after terminate returns NOT_INITIALIZED");

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);

    if (g_failures != 0) {
        std::fprintf(stderr, "GuestSaveDataSlots failed with %d errors\n", g_failures);
        return 1;
    }
    std::puts("GuestSaveDataSlots passed all checks");
    return 0;
}
