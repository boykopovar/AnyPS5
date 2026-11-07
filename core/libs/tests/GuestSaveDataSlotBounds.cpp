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

    SaveDataMount2 uninitMount2{};
    uninitMount2.user_id = 0x10000000;
    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "uninit", sizeof("uninit"));
    uninitMount2.dir_name = &validDir;
    uninitMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    Check(sceSaveDataMount2(&uninitMount2, &result) == SaveDataErrorNotInitialized, "mount2 uninitialized");
}

void CheckInvalidCharactersAndTraversal() {
    const std::vector<const char*> invalidVectors = {
        "",
        ".",
        "..",
        "/",
        "\\",
        ":",
        "../slot",
        "../../slot",
        "..\\slot",
        "..\\..\\slot",
        "slot/..",
        "slot\\..",
        "slot/../outside",
        "slot\\..\\outside",
        "../",
        "..\\",
        "./",
        ".\\",
        "/slot",
        "\\slot",
        ":slot",
        "slot:0",
        "nested/slot",
        "nested\\slot",
        "c:slot",
        "c:\\slot"
    };

    for (const char* invalid : invalidVectors) {
        SceSaveDataDirName dirName{};
        std::snprintf(dirName.data, sizeof(dirName.data), "%s", invalid);

        SaveDataMount3 mount3Req{};
        mount3Req.dir_name = &dirName;
        mount3Req.mount_mode = MountModeRdWr | MountModeCreate2;
        SaveDataMountResult result3{};
        std::memset(&result3, 0xEE, sizeof(result3));
        const SaveDataMountResult beforeResult3 = result3;
        const int rcMount3 = sceSaveDataMount3(&mount3Req, &result3);
        Check(rcMount3 == SaveDataErrorParameter, std::string("mount3 rejected invalid name: ") + invalid);
        Check(std::memcmp(&result3, &beforeResult3, sizeof(result3)) == 0, std::string("mount3 preserves result on invalid name: ") + invalid);

        SaveDataDelete delReq{};
        delReq.dir_name = &dirName;
        Check(sceSaveDataDelete(&delReq) == SaveDataErrorParameter, std::string("delete rejected invalid name: ") + invalid);

        SaveDataBackup backupReq{};
        backupReq.user_id = 0x10000000;
        backupReq.dir_name = &dirName;
        Check(sceSaveDataBackup(&backupReq) == SaveDataErrorParameter, std::string("backup rejected invalid name: ") + invalid);
    }

    char unterminatedBuffer[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminatedBuffer, 'z', sizeof(unterminatedBuffer));
    SaveDataMount3 unterminatedMount{};
    unterminatedMount.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminatedBuffer);
    unterminatedMount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult unterminatedResult{};
    std::memset(&unterminatedResult, 0xDD, sizeof(unterminatedResult));
    const SaveDataMountResult beforeUnterminated = unterminatedResult;
    Check(sceSaveDataMount3(&unterminatedMount, &unterminatedResult) == SaveDataErrorParameter, "mount3 rejects unterminated dir_name");
    Check(std::memcmp(&unterminatedResult, &beforeUnterminated, sizeof(unterminatedResult)) == 0, "mount3 preserves result on unterminated dir_name");

    SaveDataDelete unterminatedDel{};
    unterminatedDel.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminatedBuffer);
    Check(sceSaveDataDelete(&unterminatedDel) == SaveDataErrorParameter, "delete rejects unterminated dir_name");

    SaveDataBackup unterminatedBackup{};
    unterminatedBackup.user_id = 0x10000000;
    unterminatedBackup.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminatedBuffer);
    Check(sceSaveDataBackup(&unterminatedBackup) == SaveDataErrorParameter, "backup rejects unterminated dir_name");

    SceSaveDataDirName underscoreName{};
    std::snprintf(underscoreName.data, sizeof(underscoreName.data), "slot_with_underscore");
    SaveDataMount2 underMount{};
    underMount.user_id = 0x10000000;
    underMount.dir_name = &underscoreName;
    underMount.mount_mode = MountModeRdWr | MountModeCreate;
    underMount.blocks = 96;
    SaveDataMountResult underResult{};
    Check(sceSaveDataMount2(&underMount, &underResult) == SaveDataErrorParameter, "mount2 CREATE rejects name containing underscore");
}

void CheckSlotBoundsAndCapacityExhaustion() {
    std::array<SceSaveDataDirName, MaxMountSlots> names{};
    std::array<SaveDataMountResult, MaxMountSlots> results{};
    std::array<bool, MaxMountSlots> active{};
    active.fill(false);

    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        std::snprintf(names[i].data, sizeof(names[i].data), "slot%02zu", i);
        SaveDataMount3 mount{};
        mount.dir_name = &names[i];
        mount.mount_mode = MountModeRdWr | MountModeCreate2;
        const int rc = sceSaveDataMount3(&mount, &results[i]);
        Check(rc == SaveDataOk, "mount slot " + std::to_string(i) + " succeeds");
        if (rc == SaveDataOk) {
            active[i] = true;
            const std::string expectedPoint = "/savedata" + std::to_string(i);
            Check(std::strcmp(results[i].mount_point.data, expectedPoint.c_str()) == 0,
                  "mount point matches slot index: " + expectedPoint);
        }
    }

    SceSaveDataDirName overflow3Name{};
    std::snprintf(overflow3Name.data, sizeof(overflow3Name.data), "overflow3");
    SaveDataMount3 overflow3{};
    overflow3.dir_name = &overflow3Name;
    overflow3.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult overflow3Result{};
    std::memset(&overflow3Result, 0xAA, sizeof(overflow3Result));
    const SaveDataMountResult beforeOverflow3 = overflow3Result;
    const int rcOverflow3 = sceSaveDataMount3(&overflow3, &overflow3Result);
    Check(rcOverflow3 == SaveDataErrorMountFull, "17th mount3 returns SAVE_DATA_ERROR_MOUNT_FULL");
    Check(std::memcmp(&overflow3Result, &beforeOverflow3, sizeof(overflow3Result)) == 0,
          "overflow mount3 preserves result struct");

    SceSaveDataDirName overflow2Name{};
    std::snprintf(overflow2Name.data, sizeof(overflow2Name.data), "overflow2");
    SaveDataMount2 overflow2{};
    overflow2.user_id = 0x10000000;
    overflow2.dir_name = &overflow2Name;
    overflow2.mount_mode = MountModeRdWr | MountModeCreate2;
    overflow2.blocks = 96;
    SaveDataMountResult overflow2Result{};
    const int rcOverflow2 = sceSaveDataMount2(&overflow2, &overflow2Result);
    Check(rcOverflow2 == SaveDataErrorMountFull, "mount2 returns SAVE_DATA_ERROR_MOUNT_FULL when capacity reached");

    SceSaveDataDirName overflowPs4Name{};
    std::snprintf(overflowPs4Name.data, sizeof(overflowPs4Name.data), "overflowps4");
    SaveDataMountPs4 overflowPs4{};
    overflowPs4.user_id = 0x10000000;
    overflowPs4.dir_name = &overflowPs4Name;
    overflowPs4.mount_mode = MountModeRdWr | MountModeCreate2;
    overflowPs4.blocks = 96;
    SaveDataMountResult overflowPs4Result{};
    const int rcOverflowPs4 = sceSaveDataMount(&overflowPs4, &overflowPs4Result);
    Check(rcOverflowPs4 == SaveDataErrorMountFull, "mount ps4 returns SAVE_DATA_ERROR_MOUNT_FULL when capacity reached");

    constexpr std::size_t freedSlotIndex = 4;
    Check(sceSaveDataUmount2(0, &results[freedSlotIndex].mount_point) == SaveDataOk, "unmount single slot succeeds");
    active[freedSlotIndex] = false;

    SceSaveDataDirName reclaimed4Name{};
    std::snprintf(reclaimed4Name.data, sizeof(reclaimed4Name.data), "reclaimed04");
    SaveDataMount3 reclaimed4{};
    reclaimed4.dir_name = &reclaimed4Name;
    reclaimed4.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult reclaimed4Result{};
    const int rcReclaimed4 = sceSaveDataMount3(&reclaimed4, &reclaimed4Result);
    Check(rcReclaimed4 == SaveDataOk, "mounting into freed slot succeeds");
    Check(std::strcmp(reclaimed4Result.mount_point.data, "/savedata4") == 0, "reclaimed slot receives /savedata4");
    if (rcReclaimed4 == SaveDataOk) {
        results[freedSlotIndex] = reclaimed4Result;
        active[freedSlotIndex] = true;
    }

    Check(sceSaveDataMount3(&overflow3, &overflow3Result) == SaveDataErrorMountFull,
          "capacity is full again after filling single freed slot");

    constexpr std::size_t multiFree1 = 2;
    constexpr std::size_t multiFree2 = 8;
    constexpr std::size_t multiFree3 = 14;
    Check(sceSaveDataUmount2(0, &results[multiFree1].mount_point) == SaveDataOk, "unmount slot 2 succeeds");
    Check(sceSaveDataUmount2(0, &results[multiFree2].mount_point) == SaveDataOk, "unmount slot 8 succeeds");
    Check(sceSaveDataUmount2(0, &results[multiFree3].mount_point) == SaveDataOk, "unmount slot 14 succeeds");
    active[multiFree1] = false;
    active[multiFree2] = false;
    active[multiFree3] = false;

    SceSaveDataDirName multiName1{};
    std::snprintf(multiName1.data, sizeof(multiName1.data), "reclaimed02");
    SaveDataMount3 multiMount1{};
    multiMount1.dir_name = &multiName1;
    multiMount1.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult multiResult1{};
    const int rcMulti1 = sceSaveDataMount3(&multiMount1, &multiResult1);
    Check(rcMulti1 == SaveDataOk, "mounting into first multi-freed slot succeeds");
    Check(std::strcmp(multiResult1.mount_point.data, "/savedata2") == 0, "first multi-freed slot receives /savedata2");
    if (rcMulti1 == SaveDataOk) {
        results[multiFree1] = multiResult1;
        active[multiFree1] = true;
    }

    SceSaveDataDirName multiName2{};
    std::snprintf(multiName2.data, sizeof(multiName2.data), "reclaimed08");
    SaveDataMount3 multiMount2{};
    multiMount2.dir_name = &multiName2;
    multiMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult multiResult2{};
    const int rcMulti2 = sceSaveDataMount3(&multiMount2, &multiResult2);
    Check(rcMulti2 == SaveDataOk, "mounting into second multi-freed slot succeeds");
    Check(std::strcmp(multiResult2.mount_point.data, "/savedata8") == 0, "second multi-freed slot receives /savedata8");
    if (rcMulti2 == SaveDataOk) {
        results[multiFree2] = multiResult2;
        active[multiFree2] = true;
    }

    SceSaveDataDirName multiName3{};
    std::snprintf(multiName3.data, sizeof(multiName3.data), "reclaimed14");
    SaveDataMount3 multiMount3{};
    multiMount3.dir_name = &multiName3;
    multiMount3.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult multiResult3{};
    const int rcMulti3 = sceSaveDataMount3(&multiMount3, &multiResult3);
    Check(rcMulti3 == SaveDataOk, "mounting into third multi-freed slot succeeds");
    Check(std::strcmp(multiResult3.mount_point.data, "/savedata14") == 0, "third multi-freed slot receives /savedata14");
    if (rcMulti3 == SaveDataOk) {
        results[multiFree3] = multiResult3;
        active[multiFree3] = true;
    }

    Check(sceSaveDataMount3(&overflow3, &overflow3Result) == SaveDataErrorMountFull,
          "capacity is full again after filling multiple freed slots");

    Check(sceSaveDataTerminate() == SaveDataErrorBusy, "terminate returns SAVE_DATA_ERROR_BUSY while slots are mounted");

    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        if (active[i]) {
            Check(sceSaveDataUmount2(0, &results[i].mount_point) == SaveDataOk, "unmount active slot " + std::to_string(i));
            active[i] = false;
        }
    }

    Check(sceSaveDataUmount2(0, &results[0].mount_point) == SaveDataErrorNotMounted,
          "unmounting already-unmounted container returns SAVE_DATA_ERROR_NOT_MOUNTED");
    Check(sceSaveDataUmount(&results[0].mount_point) == SaveDataErrorNotMounted,
          "sceSaveDataUmount on already-unmounted container returns SAVE_DATA_ERROR_NOT_MOUNTED");

    std::array<SceSaveDataDirName, MaxMountSlots> cycleNames{};
    std::array<SaveDataMountResult, MaxMountSlots> cycleResults{};
    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        std::snprintf(cycleNames[i].data, sizeof(cycleNames[i].data), "cycle%02zu", i);
        SaveDataMount3 mount{};
        mount.dir_name = &cycleNames[i];
        mount.mount_mode = MountModeRdWr | MountModeCreate2;
        Check(sceSaveDataMount3(&mount, &cycleResults[i]) == SaveDataOk, "re-cycle mount slot " + std::to_string(i));
    }

    Check(sceSaveDataMount3(&overflow3, &overflow3Result) == SaveDataErrorMountFull,
          "overflow after complete re-cycle returns SAVE_DATA_ERROR_MOUNT_FULL");

    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        Check(sceSaveDataUmount2(0, &cycleResults[i].mount_point) == SaveDataOk, "re-cycle unmount slot " + std::to_string(i));
    }
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-slot-bounds-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    CheckNullAndUninitialized();

    Check(sceSaveDataInitialize3(nullptr) == SaveDataOk, "SaveData initializes cleanly");

    CheckInvalidCharactersAndTraversal();
    CheckSlotBoundsAndCapacityExhaustion();

    Check(sceSaveDataTerminate() == SaveDataOk, "SaveData terminates cleanly with all slots unmounted");

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
        std::fprintf(stderr, "GuestSaveDataSlotBounds failed with %d errors\n", g_failures);
        return 1;
    }
    std::puts("GuestSaveDataSlotBounds passed all checks");
    return 0;
}
