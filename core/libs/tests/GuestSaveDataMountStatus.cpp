#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;
constexpr std::uint32_t MountCreate2 = 32;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotFound = -2137063416;
constexpr int SaveDataErrorMountFull = -2137063412;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata mount status check failed: %s\n", what.c_str());
    ++failures;
}

void CheckUnmountedCalls() {
    SaveDataMountPoint point{};
    std::memcpy(point.data, "test", sizeof("test"));
    SaveDataMountInfo info{};
    SaveDataIcon icon{};
    char buf[128]{};
    std::size_t gotSize = 0;
    SaveDataMount3 nullMount{};
    SaveDataMountResult mountResult{};
    Check(sceSaveDataMount3(nullptr, &mountResult) == SaveDataErrorParameter, "mount3 null mount is PARAMETER");
    Check(sceSaveDataMount3(&nullMount, nullptr) == SaveDataErrorParameter, "mount3 null result is PARAMETER");
    Check(sceSaveDataMount3(&nullMount, &mountResult) == SaveDataErrorParameter, "mount3 null dir_name is PARAMETER");
    Check(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter, "umount2 null point is PARAMETER");
    Check(sceSaveDataUmount2(0, &point) == SaveDataErrorNotMounted, "umount2 unmounted point is NOT_MOUNTED");
    Check(sceSaveDataLoadIcon(nullptr, &icon) == SaveDataErrorParameter, "loadIcon null point is PARAMETER");
    Check(sceSaveDataLoadIcon(&point, &icon) == SaveDataErrorNotMounted, "loadIcon unmounted point is NOT_MOUNTED");
    Check(sceSaveDataSaveIcon(nullptr, &icon) == SaveDataErrorParameter, "saveIcon null point is PARAMETER");
    Check(sceSaveDataSaveIcon(&point, &icon) == SaveDataErrorNotMounted, "saveIcon unmounted point is NOT_MOUNTED");
    Check(sceSaveDataSaveIconByPath(&point, "test.png") == 0, "saveIconByPath succeeds");
    Check(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter, "getMountInfo null point is PARAMETER");
    Check(sceSaveDataGetMountInfo(&point, nullptr) == SaveDataErrorParameter, "getMountInfo null info is PARAMETER");
    Check(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted, "getMountInfo unmounted point is NOT_MOUNTED");
    Check(sceSaveDataGetParam(nullptr, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorParameter, "getParam null point is PARAMETER");
    Check(sceSaveDataGetParam(&point, 1, nullptr, sizeof(buf), &gotSize) == SaveDataErrorParameter, "getParam null buf is PARAMETER");
    Check(sceSaveDataGetParam(&point, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted, "getParam unmounted point is NOT_MOUNTED");
    Check(sceSaveDataSetParam(nullptr, 1, buf, sizeof(buf)) == SaveDataErrorParameter, "setParam null point is PARAMETER");
    Check(sceSaveDataSetParam(&point, 1, nullptr, sizeof(buf)) == SaveDataErrorParameter, "setParam null buf is PARAMETER");
    Check(sceSaveDataSetParam(&point, 1, buf, sizeof(buf)) == SaveDataErrorNotMounted, "setParam unmounted point is NOT_MOUNTED");
    Check(sceSaveDataBackup(nullptr) == SaveDataErrorParameter, "backup null backup is PARAMETER");
    SaveDataBackup nullDirBackup{};
    Check(sceSaveDataBackup(&nullDirBackup) == SaveDataErrorParameter, "backup null dir_name is PARAMETER");
    char unterminatedBackup[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminatedBackup, 'a', sizeof(unterminatedBackup));
    SaveDataBackup unterminatedBackupParam{};
    unterminatedBackupParam.user_id = 0x10000000;
    unterminatedBackupParam.dir_name = reinterpret_cast<const SceSaveDataDirName*>(unterminatedBackup);
    Check(sceSaveDataBackup(&unterminatedBackupParam) == SaveDataErrorParameter, "backup unterminated dir_name is PARAMETER");
    Check(sceSaveDataGetEventResult(nullptr, nullptr) == SaveDataErrorParameter, "getEventResult null event is PARAMETER");
    SaveDataEvent emptyEvent{};
    Check(sceSaveDataGetEventResult(nullptr, &emptyEvent) == SaveDataErrorNotFound, "getEventResult on empty queue returns NOT_FOUND");
    Check(sceSaveDataGetEventResult(&emptyEvent, nullptr) == SaveDataErrorParameter, "getEventResult null result pointer is PARAMETER");
    SceSaveDataDirName validDir{};
    std::snprintf(validDir.data, sizeof(validDir.data), "valid_backup");
    SaveDataBackup validBackup{};
    validBackup.user_id = 0x10000000;
    validBackup.dir_name = &validDir;
    Check(sceSaveDataBackup(&validBackup) == 0, "backup valid dir_name succeeds");
    SaveDataEvent backupEvent{};
    Check(sceSaveDataGetEventResult(nullptr, &backupEvent) == 0, "getEventResult retrieves backup event");
    Check(backupEvent.type == 2, "backup event type is 2");
    Check(backupEvent.error_code == 0, "backup event error code is 0");
    Check(backupEvent.user_id == 0x10000000, "backup event user_id matches");
    Check(std::strcmp(backupEvent.dir_name.data, "valid_backup") == 0, "backup event dir_name matches");
    Check(sceSaveDataGetEventResult(nullptr, &emptyEvent) == SaveDataErrorNotFound, "event queue is empty after retrieving backup event");
}

void CheckMountInfoAndParam(const SaveDataMountPoint* point) {
    SaveDataMountInfo info{};
    SaveDataIcon icon{};
    char buf[128]{};
    char smallBuf[64]{};
    std::size_t gotSize = 0;
    SaveDataMountPoint unmounted{};
    std::memcpy(unmounted.data, "unmounted", sizeof("unmounted"));
    const char newTitle[] = "Mounted Title";
    char readTitle[128]{};
    Check(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter, "umount2 null point is PARAMETER");
    Check(sceSaveDataLoadIcon(nullptr, &icon) == SaveDataErrorParameter, "loadIcon null point is PARAMETER");
    Check(sceSaveDataLoadIcon(&unmounted, &icon) == SaveDataErrorNotMounted, "loadIcon unmounted point is NOT_MOUNTED");
    Check(sceSaveDataLoadIcon(&unmounted, nullptr) == SaveDataErrorNotMounted, "loadIcon unmounted point with null icon is NOT_MOUNTED");
    Check(sceSaveDataLoadIcon(point, &icon) == 0, "loadIcon on mounted save succeeds");
    Check(sceSaveDataSaveIcon(nullptr, &icon) == SaveDataErrorParameter, "saveIcon null point is PARAMETER");
    Check(sceSaveDataSaveIcon(&unmounted, &icon) == SaveDataErrorNotMounted, "saveIcon unmounted point is NOT_MOUNTED");
    Check(sceSaveDataSaveIcon(&unmounted, nullptr) == SaveDataErrorNotMounted, "saveIcon unmounted point with null icon is NOT_MOUNTED");
    Check(sceSaveDataSaveIcon(point, &icon) == 0, "saveIcon on mounted save succeeds");
    Check(sceSaveDataSaveIconByPath(point, "icon.png") == 0, "saveIconByPath on mounted save succeeds");
    Check(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter, "getMountInfo null point is PARAMETER");
    Check(sceSaveDataGetMountInfo(point, nullptr) == SaveDataErrorParameter, "getMountInfo null info is PARAMETER");
    Check(sceSaveDataGetMountInfo(&unmounted, &info) == SaveDataErrorNotMounted, "getMountInfo unmounted point is NOT_MOUNTED");
    Check(sceSaveDataGetMountInfo(point, &info) == 0, "getMountInfo on mounted save succeeds");
    Check(sceSaveDataGetParam(nullptr, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorParameter, "getParam null point is PARAMETER");
    Check(sceSaveDataGetParam(point, 1, nullptr, sizeof(buf), &gotSize) == SaveDataErrorParameter, "getParam null buf is PARAMETER");
    Check(sceSaveDataGetParam(point, 1, smallBuf, sizeof(smallBuf), &gotSize) == SaveDataErrorParameter, "getParam small buf is PARAMETER");
    Check(sceSaveDataGetParam(&unmounted, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted, "getParam unmounted point is NOT_MOUNTED");
    Check(sceSaveDataGetParam(point, 1, buf, sizeof(buf), &gotSize) == 0, "getParam on mounted save succeeds");
    Check(gotSize == sizeof(buf), "getParam reports expected size");
    Check(sceSaveDataGetParam(point, 1, buf, sizeof(buf), nullptr) == 0, "getParam with null got_size on mounted save succeeds");
    Check(sceSaveDataSetParam(nullptr, 1, newTitle, sizeof(newTitle)) == SaveDataErrorParameter, "setParam null point is PARAMETER");
    Check(sceSaveDataSetParam(point, 1, nullptr, sizeof(newTitle)) == SaveDataErrorParameter, "setParam null buf is PARAMETER");
    Check(sceSaveDataSetParam(point, 999, newTitle, sizeof(newTitle)) == SaveDataErrorParameter, "setParam invalid type is PARAMETER");
    Check(sceSaveDataSetParam(point, 1, newTitle, 0) == SaveDataErrorParameter, "setParam zero size is PARAMETER");
    Check(sceSaveDataSetParam(point, 4, smallBuf, 1) == SaveDataErrorParameter, "setParam short user param is PARAMETER");
    Check(sceSaveDataSetParam(&unmounted, 1, newTitle, sizeof(newTitle)) == SaveDataErrorNotMounted, "setParam unmounted point is NOT_MOUNTED");
    Check(sceSaveDataSetParam(point, 1, newTitle, sizeof(newTitle)) == 0, "setParam on mounted save succeeds");
    Check(sceSaveDataGetParam(point, 1, readTitle, sizeof(readTitle), &gotSize) == 0, "getParam reads back set title");
    Check(std::strcmp(readTitle, newTitle) == 0, "readTitle matches newTitle");
}

void MountAndCheck(const char* name, std::uint32_t mode, std::uint32_t expectedStatus, const std::string& what) {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, name, std::strlen(name) + 1);
    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = mode;
    SaveDataMountResult result{};
    const int code = sceSaveDataMount3(&mount, &result);
    Check(code == 0, what + ": mounts");
    if (code != 0) return;
    Check(result.mount_status == expectedStatus, what + ": status is " + std::to_string(result.mount_status) + ", expected " + std::to_string(expectedStatus));
    CheckMountInfoAndParam(&result.mount_point);
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, what + ": unmounts");
}

void CheckFailedMountKeepsResult() {
    SceSaveDataDirName held{};
    std::memcpy(held.data, "held", sizeof("held"));
    SaveDataMount3 mount{};
    mount.dir_name = &held;
    mount.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult result{};
    Check(sceSaveDataMount3(&mount, &result) == 0, "first mount of a save");
    const SaveDataMountResult mounted = result;
    Check(sceSaveDataMount3(&mount, &result) == SaveDataErrorBusy, "second mount of a mounted save is BUSY");
    Check(std::memcmp(&result, &mounted, sizeof(result)) == 0, "a BUSY mount leaves the result as it was");
    SceSaveDataDirName absent{};
    std::memcpy(absent.data, "absent", sizeof("absent"));
    mount.dir_name = &absent;
    mount.mount_mode = MountReadWrite;
    Check(sceSaveDataMount3(&mount, &result) == SaveDataErrorNotFound, "open of a missing save is NOT_FOUND");
    Check(std::memcmp(&result, &mounted, sizeof(result)) == 0, "a NOT_FOUND mount leaves the result as it was");
    MountAndCheck("new-create", MountReadWrite | MountCreate, 1u, "CREATE while another save is mounted");
    MountAndCheck("new-create2", MountReadWrite | MountCreate2, 1u, "CREATE2 while another save is mounted");
    Check(std::filesystem::remove(std::filesystem::path("_sd") / "held"), "the mounted backing directory is removed");
    mount.dir_name = &held;
    Check(sceSaveDataMount3(&mount, &result) == SaveDataErrorBusy, "the same name remains BUSY after its backing directory disappears");
    Check(std::memcmp(&result, &mounted, sizeof(result)) == 0, "a missing backing directory leaves the BUSY result unchanged");
    MountAndCheck("made", MountReadWrite, 0u, "open of another save while a mounted backing directory is missing");
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the kept mount point unmounts the save");
}

void CheckEquivalentMounts() {
    SceSaveDataDirName original{};
    std::memcpy(original.data, "identity", sizeof("identity"));
    SaveDataMount3 mount{};
    mount.dir_name = &original;
    mount.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult first{};
    Check(sceSaveDataMount3(&mount, &first) == 0, "original save mounts");

    const auto originalPath = std::filesystem::path("_sd") / "identity";
    std::error_code error;
    const auto casePath = std::filesystem::path("_sd") / "IDENTITY";
    const bool caseAlias = std::filesystem::exists(casePath) && std::filesystem::equivalent(originalPath, casePath, error);
    Check(!error || error == std::errc::no_such_file_or_directory, "case-alias identity check succeeds");
#ifdef _WIN32
    error.clear();
    if (!CreateSymbolicLinkW((std::filesystem::path("_sd") / "linked").c_str(), L"identity",
            SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        error = std::error_code(GetLastError(), std::system_category());
    }
#else
    std::filesystem::create_directory_symlink("identity", std::filesystem::path("_sd") / "linked", error);
#endif
    const bool symlinkAlias = !error;
    Check(caseAlias || symlinkAlias, "a host alias is available to check mount identity");

    const auto checkAlias = [&](const char* name) {
        SceSaveDataDirName alias{};
        std::memcpy(alias.data, name, std::strlen(name) + 1);
        mount.dir_name = &alias;
        mount.mount_mode = MountReadWrite;
        SaveDataMountResult result;
        std::memset(&result, 0xCD, sizeof(result));
        const SaveDataMountResult before = result;
        Check(sceSaveDataMount3(&mount, &result) == SaveDataErrorBusy, std::string(name) + " cannot mount the same directory twice");
        Check(std::memcmp(&result, &before, sizeof(result)) == 0, std::string(name) + " leaves the result unchanged");
    };
    if (caseAlias) checkAlias("IDENTITY");
    if (symlinkAlias) checkAlias("linked");

    Check(sceSaveDataUmount2(0, &first.mount_point) == 0, "original save unmounts");
    if (caseAlias) MountAndCheck("IDENTITY", MountReadWrite, 0u, "case alias after unmount");
    if (symlinkAlias) {
        MountAndCheck("linked", MountReadWrite, 0u, "symlink alias after unmount");
#ifdef _WIN32
        error.clear();
        if (!RemoveDirectoryW((std::filesystem::path("_sd") / "linked").c_str())) error = std::error_code(GetLastError(), std::system_category());
#else
        std::filesystem::remove(std::filesystem::path("_sd") / "linked", error);
#endif
        Check(!error, "symlink alias is removed");
    }
}

std::set<std::filesystem::path> Tree() {
    std::set<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path())) paths.insert(entry.path());
    return paths;
}

void CheckInvalidNameIsRejected(const char* data, std::size_t size, const std::string& label) {
    for (const std::uint32_t mode : {MountReadWrite | MountCreate2, MountReadWrite | MountCreate, MountReadWrite}) {
        SceSaveDataDirName dirName{};
        std::memcpy(dirName.data, data, size);
        SaveDataMount3 mount{};
        mount.dir_name = &dirName;
        mount.mount_mode = mode;
        SaveDataMountResult result;
        std::memset(&result, 0xCD, sizeof(result));
        const SaveDataMountResult before = result;
        const std::string what = label + " with mount mode " + std::to_string(mode);
        Check(sceSaveDataMount3(&mount, &result) == SaveDataErrorParameter, what + " is a parameter error");
        Check(std::memcmp(&result, &before, sizeof(result)) == 0, what + " leaves the result as it was");
    }
}

void CheckInvalidNamesAreRejected() {
    const auto before = Tree();
    for (const char* invalid : {"", ".", "..", "a/b", "a\\b", "c:d"}) {
        CheckInvalidNameIsRejected(invalid, std::strlen(invalid) + 1, std::string("\"") + invalid + "\"");
    }
    char unterminated[sizeof(SceSaveDataDirName::data)];
    std::memset(unterminated, 'a', sizeof(unterminated));
    CheckInvalidNameIsRejected(unterminated, sizeof(unterminated), "an unterminated name");
    Check(Tree() == before, "rejected names create nothing");
    MountAndCheck("after", MountReadWrite | MountCreate2, 1u, "CREATE2 after the rejected names");
}

void CheckMountCapacityAndLifecycle() {
    Check(sceSaveDataAbort() == 0, "abort after init succeeds");
    Check(sceSaveDataClearProgress() == 0, "clearProgress after init succeeds");

    SceSaveDataDirName dirA{};
    std::memcpy(dirA.data, "count-a", sizeof("count-a"));
    SaveDataMount3 mountA{};
    mountA.dir_name = &dirA;
    mountA.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult resultA{};
    Check(sceSaveDataMount3(&mountA, &resultA) == 0, "mountA succeeds");
    SaveDataMountResult duplicate{};
    Check(sceSaveDataMount3(&mountA, &duplicate) == SaveDataErrorBusy, "duplicate mount is busy");

    SceSaveDataDirName dirB{};
    std::memcpy(dirB.data, "count-b", sizeof("count-b"));
    SaveDataMount3 mountB{};
    mountB.dir_name = &dirB;
    mountB.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult resultB{};
    Check(sceSaveDataMount3(&mountB, &resultB) == 0, "mountB succeeds");

    SceSaveDataDirName dirC{};
    std::memcpy(dirC.data, "count-c", sizeof("count-c"));
    SaveDataMount3 mountC{};
    mountC.dir_name = &dirC;
    mountC.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult resultC{};
    Check(sceSaveDataMount3(&mountC, &resultC) == 0, "mountC succeeds");

    Check(sceSaveDataUmount2(0, &resultB.mount_point) == 0, "unmountB succeeds");

    Check(sceSaveDataUmount2(0, &resultA.mount_point) == 0, "unmountA succeeds");

    Check(sceSaveDataUmount2(0, &resultC.mount_point) == 0, "unmountC succeeds");

    std::array<SceSaveDataDirName, 17> names{};
    std::array<SaveDataMountResult, 16> mounted{};
    for (std::size_t index = 0; index < mounted.size(); ++index) {
        std::snprintf(names[index].data, sizeof(names[index].data), "capacity-%zu", index);
        SaveDataMount3 request{};
        request.dir_name = &names[index];
        request.mount_mode = MountReadWrite | MountCreate2;
        Check(sceSaveDataMount3(&request, &mounted[index]) == 0, "mount at capacity index " + std::to_string(index));
    }
    std::memcpy(names.back().data, "capacity-overflow", sizeof("capacity-overflow"));
    SaveDataMount3 overflow{};
    overflow.dir_name = &names.back();
    overflow.mount_mode = MountReadWrite | MountCreate2;
    SaveDataMountResult rejected{};
    Check(sceSaveDataMount3(&overflow, &rejected) == SaveDataErrorMountFull, "mount beyond capacity is rejected");
    for (const auto& mount : mounted) Check(sceSaveDataUmount2(0, &mount.mount_point) == 0, "capacity unmount succeeds");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-mount-status-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    Check(sceSaveDataAbort() == SaveDataErrorNotInitialized, "abort pre-init is NOT_INITIALIZED");
    Check(sceSaveDataClearProgress() == SaveDataErrorNotInitialized, "clearProgress pre-init is NOT_INITIALIZED");
    CheckUnmountedCalls();
    Check(sceSaveDataSyncSaveDataMemory(nullptr) == static_cast<int>(0x809F0000), "null param rejected in memory sync");
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    CheckMountCapacityAndLifecycle();
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 1u, "CREATE2 of a missing save");
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 0u, "CREATE2 of an existing save");
    MountAndCheck("made", MountReadWrite | MountCreate, 1u, "CREATE of a missing save");
    MountAndCheck("made", MountReadWrite, 0u, "open of an existing save");
    CheckFailedMountKeepsResult();
    CheckEquivalentMounts();
    CheckInvalidNamesAreRejected();
    Check(sceSaveDataTerminate() == 0, "SaveData terminates");
    Check(sceSaveDataAbort() == SaveDataErrorNotInitialized, "abort post-terminate returns NOT_INITIALIZED");
    Check(sceSaveDataClearProgress() == SaveDataErrorNotInitialized, "clearProgress post-terminate returns NOT_INITIALIZED");
    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata mount status tests passed\n");
    return 0;
}
