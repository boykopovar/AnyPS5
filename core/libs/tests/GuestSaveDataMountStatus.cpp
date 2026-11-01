#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <set>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
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
constexpr int SaveDataErrorInternal = -2137063413;
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
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "fresh", sizeof("fresh"));
    SaveDataCheckBackupData check{};
    check.dir_name = &dirName;
    Check(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotInitialized, "checkBackupData before initialize is NOT_INITIALIZED");
    SaveDataRestoreBackupData restore{};
    restore.dir_name = &dirName;
    Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotInitialized, "restoreBackupData before initialize is NOT_INITIALIZED");
    Check(sceSaveDataUmountWithBackup(&point) == SaveDataErrorNotInitialized, "umountWithBackup before initialize is NOT_INITIALIZED");
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

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    Check(static_cast<bool>(file), "writes " + path.string());
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool MountSave(SceSaveDataDirName* dirName, std::uint32_t mode, SaveDataMountResult* result) {
    SaveDataMount3 mount{};
    mount.user_id = 7;
    mount.dir_name = dirName;
    mount.mount_mode = mode;
    return sceSaveDataMount3(&mount, result) == 0;
}

std::map<std::string, std::string> Snapshot(const std::filesystem::path& save) {
    std::map<std::string, std::string> files;
    for (const char* side : {".param", ".blocks"}) {
        std::filesystem::path path = save;
        path += side;
        if (std::filesystem::exists(path)) files[side] = ReadText(path);
    }
    if (!std::filesystem::is_directory(save)) return files;
    files["/"] = "";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(save)) {
        const std::string relative = std::filesystem::relative(entry.path(), save).generic_string();
        if (entry.is_directory()) {
            files[relative + "/"] = "";
        } else {
            files[relative] = ReadText(entry.path());
        }
    }
    return files;
}

int UmountWithBackup(const std::string& name, const SaveDataMountPoint* point) {
    Check(sceSaveDataUmountWithBackup(point) == 0, name + " unmounts with a backup");
    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, name + "'s backup posts an event");
    return event.error_code;
}

int BackUp(SceSaveDataDirName* dirName, const std::string& data, const char* title) {
    const std::string name = dirName->data;
    SaveDataMountResult mounted{};
    if (!MountSave(dirName, MountReadWrite | MountCreate2, &mounted)) {
        Check(false, name + " mounts to be backed up");
        return SaveDataErrorNotMounted;
    }
    WriteText(std::filesystem::path("_sd") / name / "data.bin", data);
    Check(sceSaveDataSetParam(&mounted.mount_point, 1, title, std::strlen(title) + 1) == 0, name + " takes the title " + title);
    return UmountWithBackup(name, &mounted.mount_point);
}

int RemountWithBackup(SceSaveDataDirName* dirName) {
    const std::string name = dirName->data;
    SaveDataMountResult mounted{};
    if (!MountSave(dirName, MountReadWrite, &mounted)) {
        Check(false, name + " mounts to be backed up again");
        return SaveDataErrorNotMounted;
    }
    return UmountWithBackup(name, &mounted.mount_point);
}

void CheckBackups() {
    const auto paramJson = ResolvePath_nid_no_patch("/app0/sce_sys/param.json");
    std::filesystem::create_directories(paramJson.parent_path());
    WriteText(paramJson, "{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Example\"}}}");
    SceSaveDataDirName kept{};
    std::memcpy(kept.data, "kept", sizeof("kept"));
    SceSaveDataDirName fresh{};
    std::memcpy(fresh.data, "fresh", sizeof("fresh"));
    SaveDataCheckBackupData check{};
    SaveDataRestoreBackupData restore{};
    Check(sceSaveDataCheckBackupData(nullptr) == SaveDataErrorParameter, "checkBackupData null block is PARAMETER");
    Check(sceSaveDataCheckBackupData(&check) == SaveDataErrorParameter, "checkBackupData null dir_name is PARAMETER");
    Check(sceSaveDataRestoreBackupData(nullptr) == SaveDataErrorParameter, "restoreBackupData null block is PARAMETER");
    Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorParameter, "restoreBackupData null dir_name is PARAMETER");
    Check(sceSaveDataUmountWithBackup(nullptr) == SaveDataErrorParameter, "umountWithBackup null point is PARAMETER");
    check.dir_name = &fresh;
    restore.dir_name = &fresh;
    Check(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound, "checkBackupData without a backup is NOT_FOUND");
    Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound, "restoreBackupData without a backup is NOT_FOUND");

    SaveDataMountResult mounted{};
    Check(MountSave(&kept, MountReadWrite | MountCreate2, &mounted), "the backed-up save mounts");
    const auto data = std::filesystem::path("_sd") / "kept" / "data.bin";
    WriteText(data, "first");
    const char backedUpTitle[] = "Backed Up";
    Check(sceSaveDataSetParam(&mounted.mount_point, 1, backedUpTitle, sizeof(backedUpTitle)) == 0, "the backed-up save takes a title");
    check.dir_name = &kept;
    restore.dir_name = &kept;
    Check(sceSaveDataCheckBackupData(&check) == SaveDataErrorBusy, "checkBackupData of a mounted save is BUSY");
    Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorBusy, "restoreBackupData of a mounted save is BUSY");
    Check(sceSaveDataUmountWithBackup(&mounted.mount_point) == 0, "umountWithBackup succeeds");
    SaveDataMountInfo info{};
    Check(sceSaveDataGetMountInfo(&mounted.mount_point, &info) == SaveDataErrorNotMounted, "umountWithBackup unmounts the save");

    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0, "umountWithBackup posts an event");
    Check(event.type == 1, "the event is UMOUNT_BACKUP, got type " + std::to_string(event.type));
    Check(event.error_code == 0, "the backup succeeded, error " + std::to_string(event.error_code));
    Check(event.user_id == 7, "the event names the mounting user");
    Check(std::strncmp(event.title_id.data, "PPSA00000", sizeof(event.title_id.data)) == 0, "the event names the running title");
    Check(std::strncmp(event.dir_name.data, "kept", sizeof(event.dir_name.data)) == 0, "the event names the save");

    SaveDataParam param{};
    check.param = &param;
    Check(sceSaveDataCheckBackupData(&check) == 0, "checkBackupData finds the backup");
    Check(std::strcmp(param.title, backedUpTitle) == 0, "checkBackupData fills param from the backup");
    SceSaveDataTitleId running{};
    std::memcpy(running.data, "PPSA00000", sizeof("PPSA00000"));
    check.title_id = &running;
    Check(sceSaveDataCheckBackupData(&check) == 0, "checkBackupData with the running title's id finds the backup");

    WriteText(data, "second");
    WriteText(std::filesystem::path("_sd") / "kept" / "added.bin", "added");
    Check(MountSave(&kept, MountReadWrite, &mounted), "the save mounts again after the backup");
    const char changedTitle[] = "Changed";
    Check(sceSaveDataSetParam(&mounted.mount_point, 1, changedTitle, sizeof(changedTitle)) == 0, "the title changes after the backup");
    Check(sceSaveDataUmount2(0, &mounted.mount_point) == 0, "the changed save unmounts");
    Check(sceSaveDataRestoreBackupData(&restore) == 0, "restoreBackupData succeeds");
    Check(ReadText(data) == "first", "restoreBackupData puts back a file changed after the backup");
    Check(!std::filesystem::exists(std::filesystem::path("_sd") / "kept" / "added.bin"), "restoreBackupData removes a file added after the backup");
    char title[128]{};
    Check(MountSave(&kept, MountReadWrite, &mounted), "the restored save mounts");
    Check(sceSaveDataGetParam(&mounted.mount_point, 1, title, sizeof(title), nullptr) == 0 && std::strcmp(title, backedUpTitle) == 0, "restoreBackupData puts back the backed-up title");
    Check(sceSaveDataUmount2(0, &mounted.mount_point) == 0, "the restored save unmounts");
    SaveDataDelete del{};
    del.dir_name = &kept;
    Check(sceSaveDataDelete(&del) == 0, "the backed-up save is deleted");
    Check(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound, "deleting a save deletes its backup");
}

const char* failingStep = nullptr;
int failingCall = 0;
int stepCalls = 0;

bool FailingStep(const char* step, const char*) {
    if (failingStep == nullptr || std::strcmp(step, failingStep) != 0) return true;
    return ++stepCalls != failingCall;
}

void FailStep(const char* step, int call) {
    failingStep = step;
    failingCall = call;
    stepCalls = 0;
    SaveDataSetFileStepHook_nid_no_patch(FailingStep);
}

void ClearFailingStep() {
    SaveDataSetFileStepHook_nid_no_patch(nullptr);
    failingStep = nullptr;
}

struct FailedStep {
    const char* step;
    int call;
};

bool LeavesCopies(const std::string& name) {
    for (const char* root : {"_sd_backup_tmp", "_sd_backup_old", "_sd_restore_tmp", "_sd_restore_old"}) {
        if (std::filesystem::exists(std::filesystem::path(root) / name)) return true;
    }
    return false;
}

template <typename Damage>
void CheckCorruptBackup(const std::string& label, Damage damage) {
    SceSaveDataDirName name{};
    std::memcpy(name.data, "corrupt", sizeof("corrupt"));
    Check(BackUp(&name, "backed up", "Corrupt") == 0, label + ": the save is backed up");
    WriteText(std::filesystem::path("_sd") / "corrupt" / "data.bin", "live");
    damage(std::filesystem::path("_sd_backup") / "corrupt");
    const auto live = std::filesystem::path("_sd") / "corrupt";
    const auto before = Snapshot(live);
    SaveDataCheckBackupData check{};
    check.dir_name = &name;
    Check(sceSaveDataCheckBackupData(&check) == SaveDataErrorInternal, label + ": checkBackupData without param is INTERNAL");
    SaveDataParam param;
    std::memset(&param, 0xA5, sizeof(param));
    const SaveDataParam untouched = param;
    check.param = &param;
    Check(sceSaveDataCheckBackupData(&check) == SaveDataErrorInternal, label + ": checkBackupData with param is INTERNAL");
    Check(std::memcmp(&param, &untouched, sizeof(param)) == 0, label + ": checkBackupData leaves param as it was");
    SaveDataRestoreBackupData restore{};
    restore.dir_name = &name;
    Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorInternal, label + ": restoreBackupData is INTERNAL");
    Check(Snapshot(live) == before, label + ": restoreBackupData leaves the save byte for byte");
}

void CheckCorruptBackups() {
    CheckCorruptBackup("a backup without its .param", [](const std::filesystem::path& entry) { std::filesystem::remove(entry / "save.param"); });
    CheckCorruptBackup("an empty backup directory", [](const std::filesystem::path& entry) {
        std::filesystem::remove_all(entry);
        std::filesystem::create_directory(entry);
    });
    CheckCorruptBackup("a backup with a short .param", [](const std::filesystem::path& entry) {
        const std::string bytes = ReadText(entry / "save.param");
        WriteText(entry / "save.param", bytes.substr(0, bytes.size() - 1));
    });
    CheckCorruptBackup("a backup with a long .param", [](const std::filesystem::path& entry) { WriteText(entry / "save.param", ReadText(entry / "save.param") + "x"); });
}

void CheckStagedRestore() {
    SceSaveDataDirName name{};
    std::memcpy(name.data, "staged", sizeof("staged"));
    const auto live = std::filesystem::path("_sd") / "staged";
    std::filesystem::create_directories(live / "sub");
    WriteText(live / "sub" / "deep.bin", "deep");
    WriteText(std::filesystem::path("_sd") / "staged.blocks", "96");
    Check(BackUp(&name, "backed up", "Staged") == 0, "the staged save is backed up");
    const auto backup = Snapshot(std::filesystem::path("_sd_backup") / "staged" / "save");
    WriteText(live / "data.bin", "live");
    WriteText(live / "added.bin", "added");
    WriteText(std::filesystem::path("_sd") / "staged.blocks", "128");
    const auto before = Snapshot(live);
    SaveDataRestoreBackupData restore{};
    restore.dir_name = &name;
    const FailedStep failures[] = {{"restore.copy", 1}, {"restore.copy", 3}, {"restore.copy", 6}, {"restore.aside", 1}, {"restore.aside", 2}, {"restore.place", 1}, {"restore.place", 3}};
    for (const FailedStep& failure : failures) {
        const std::string what = std::string("a restore whose ") + failure.step + " step " + std::to_string(failure.call) + " fails";
        FailStep(failure.step, failure.call);
        Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorInternal, what + " is INTERNAL");
        ClearFailingStep();
        Check(Snapshot(live) == before, what + " leaves the save as it was");
        Check(!LeavesCopies("staged"), what + " leaves no staged or old copy");
    }
    Check(sceSaveDataRestoreBackupData(&restore) == 0, "a restore with no failing step succeeds");
    Check(Snapshot(live) == backup, "the restored save matches its backup byte for byte");
    Check(!LeavesCopies("staged"), "a restore leaves no staged or old copy");
    std::filesystem::remove_all(live);
    Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorInternal, "a restore into a missing save directory is INTERNAL");
    SaveDataCheckBackupData check{};
    check.dir_name = &name;
    Check(sceSaveDataCheckBackupData(&check) == 0, "the backup outlives a restore into a missing save directory");
}

void CheckBackupSwap() {
    SceSaveDataDirName name{};
    std::memcpy(name.data, "rotated", sizeof("rotated"));
    const auto backup = std::filesystem::path("_sd_backup") / "rotated" / "save";
    SaveDataParam param{};
    SaveDataCheckBackupData check{};
    check.dir_name = &name;
    check.param = &param;
    const char* titles[] = {"First", "Second", "Third"};
    for (int i = 0; i < 3; ++i) {
        const std::string what = "backup " + std::to_string(i + 1) + " of three";
        const std::string data = "rotation " + std::to_string(i + 1);
        Check(BackUp(&name, data, titles[i]) == 0, what + " succeeds");
        Check(ReadText(backup / "data.bin") == data, what + " holds the save it was made from");
        Check(sceSaveDataCheckBackupData(&check) == 0 && std::strcmp(param.title, titles[i]) == 0, what + " replaces the last one's param");
        Check(!LeavesCopies("rotated"), what + " leaves no staged or old copy");
    }
    const auto previous = Snapshot(backup);
    const FailedStep failures[] = {{"backup.aside", 1}, {"backup.place", 1}};
    for (const FailedStep& failure : failures) {
        const std::string what = std::string("a backup whose ") + failure.step + " step fails";
        FailStep(failure.step, failure.call);
        const int error = BackUp(&name, "unsaved", "Unsaved");
        ClearFailingStep();
        Check(error == SaveDataErrorInternal, what + " gives the event error INTERNAL, got " + std::to_string(error));
        Check(Snapshot(backup) == previous, what + " keeps the previous backup byte for byte");
        param = {};
        Check(sceSaveDataCheckBackupData(&check) == 0 && std::strcmp(param.title, "Third") == 0, what + " leaves the previous backup usable");
        Check(!LeavesCopies("rotated"), what + " leaves no staged or old copy");
    }
    SaveDataRestoreBackupData restore{};
    restore.dir_name = &name;
    Check(sceSaveDataRestoreBackupData(&restore) == 0, "the backup kept by the failed backups restores");
    Check(ReadText(std::filesystem::path("_sd") / "rotated" / "data.bin") == "rotation 3", "the restore puts back the third backup");
}

#ifdef _WIN32
bool Junction(const std::filesystem::path& link, const std::filesystem::path& target) {
    std::error_code error;
    if (!std::filesystem::create_directory(link, error)) return false;
    const auto printName = std::filesystem::absolute(target).make_preferred().wstring();
    const auto substituteName = L"\\??\\" + printName;
    struct MountPoint {
        ULONG tag;
        USHORT dataLength;
        USHORT reserved;
        USHORT substituteOffset;
        USHORT substituteLength;
        USHORT printOffset;
        USHORT printLength;
        wchar_t names[2048];
    } data{};
    if (substituteName.size() + printName.size() + 2 > std::size(data.names)) return false;
    data.tag = IO_REPARSE_TAG_MOUNT_POINT;
    data.substituteLength = static_cast<USHORT>(substituteName.size() * sizeof(wchar_t));
    data.printOffset = static_cast<USHORT>((substituteName.size() + 1) * sizeof(wchar_t));
    data.printLength = static_cast<USHORT>(printName.size() * sizeof(wchar_t));
    data.dataLength = static_cast<USHORT>(8 + data.printOffset + data.printLength + sizeof(wchar_t));
    std::memcpy(data.names, substituteName.c_str(), data.substituteLength + sizeof(wchar_t));
    std::memcpy(reinterpret_cast<char*>(data.names) + data.printOffset, printName.c_str(), data.printLength + sizeof(wchar_t));
    const HANDLE handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    bool set = false;
    if (handle != INVALID_HANDLE_VALUE) {
        DWORD returned = 0;
        set = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &data, 8 + data.dataLength, nullptr, 0, &returned, nullptr) != 0;
        CloseHandle(handle);
    }
    if (!set) RemoveDirectoryW(link.c_str());
    return set;
}
#endif

bool LinkDirectory(const std::filesystem::path& link, const std::filesystem::path& target) {
#ifdef _WIN32
    return Junction(link, target);
#else
    std::error_code error;
    std::filesystem::create_directory_symlink(std::filesystem::absolute(target), link, error);
    return !error;
#endif
}

bool LinkFile(const std::filesystem::path& link, const std::filesystem::path& target) {
#ifdef _WIN32
    return CreateSymbolicLinkW(link.c_str(), std::filesystem::absolute(target).c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
#else
    std::error_code error;
    std::filesystem::create_symlink(std::filesystem::absolute(target), link, error);
    return !error;
#endif
}

void Unlink(const std::filesystem::path& link) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(link.c_str());
    const bool removed = attributes != INVALID_FILE_ATTRIBUTES && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? RemoveDirectoryW(link.c_str()) != 0 : DeleteFileW(link.c_str()) != 0);
#else
    std::error_code error;
    const bool removed = std::filesystem::remove(link, error);
#endif
    Check(removed, "the link " + link.string() + " is removed");
}

void CheckLinkedPaths() {
    const auto outside = std::filesystem::absolute("outside");
    std::filesystem::create_directories(outside / "sentinel");
    WriteText(outside / "sentinel" / "keep.bin", "sentinel");
    std::filesystem::create_directories(outside / "old" / "escape");
    WriteText(outside / "old" / "escape" / "keep.bin", "sentinel");
    const auto sentinel = Snapshot(outside / "sentinel");
    const auto oldSentinel = Snapshot(outside / "old" / "escape");
    SceSaveDataDirName name{};
    std::memcpy(name.data, "escape", sizeof("escape"));
    Check(BackUp(&name, "inside", "Inside") == 0, "the escape save is backed up");
    const auto backup = std::filesystem::path("_sd_backup") / "escape" / "save";
    const auto kept = Snapshot(backup);
    const auto live = std::filesystem::path("_sd") / "escape";
    SaveDataRestoreBackupData restore{};
    restore.dir_name = &name;
    const auto survives = [&](const std::string& what) {
        Check(Snapshot(outside / "sentinel") == sentinel && Snapshot(outside / "old" / "escape") == oldSentinel, what + ": the outside directory and its files survive");
        Check(Snapshot(backup) == kept, what + ": the backup is kept");
    };
    const auto restoreRefused = [&](const std::string& what) {
        Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorInternal, what + ": restoreBackupData is INTERNAL");
        survives(what);
    };
    const auto backupRefused = [&](const std::string& what) {
        Check(RemountWithBackup(&name) == SaveDataErrorInternal, what + ": the backup gives the event error INTERNAL");
        survives(what);
    };

    std::filesystem::remove_all(live);
    if (LinkDirectory(live, outside / "sentinel")) {
        restoreRefused("a save directory that links outside");
        backupRefused("a save directory that links outside");
        Unlink(live);
    } else {
        std::fprintf(stderr, "skipped the linked save directory: the link could not be created\n");
    }
    std::filesystem::create_directories(live);
    WriteText(live / "data.bin", "inside");

    if (LinkFile(live / "link.bin", outside / "sentinel" / "keep.bin")) {
        restoreRefused("a save with a file that links outside");
        backupRefused("a save with a file that links outside");
        Unlink(live / "link.bin");
    } else {
        std::fprintf(stderr, "skipped the linked file in a save: the link could not be created\n");
    }
    const auto inside = Snapshot(live);

    for (const char* root : {"_sd_backup_old", "_sd_restore_old"}) {
        std::error_code error;
        std::filesystem::remove_all(root, error);
        const std::string what = std::string("a ") + root + " root that links outside";
        if (!LinkDirectory(root, outside / "old")) {
            std::fprintf(stderr, "skipped %s: the link could not be created\n", what.c_str());
            continue;
        }
        if (std::strcmp(root, "_sd_backup_old") == 0) {
            backupRefused(what);
        } else {
            restoreRefused(what);
        }
        Unlink(root);
    }

    Check(Snapshot(live) == inside, "the refused calls leave the save as it was");
    Check(sceSaveDataRestoreBackupData(&restore) == 0, "the save restores once its paths are plain again");
    Check(ReadText(live / "data.bin") == "inside", "the restore puts back the kept backup");
}

void CheckBackupGuestErrors() {
    SceSaveDataDirName name{};
    std::memcpy(name.data, "errors", sizeof("errors"));
    SaveDataParam param{};
    SaveDataCheckBackupData check{};
    check.dir_name = &name;
    check.param = &param;
    Check(BackUp(&name, "first", "First") == 0, "the first backup of a save succeeds");
    Check(BackUp(&name, "second", "Second") == 0, "a second sceSaveDataUmountWithBackup succeeds");
    Check(sceSaveDataCheckBackupData(&check) == 0 && std::strcmp(param.title, "Second") == 0, "the second backup replaces the first one's param");
    Check(ReadText(std::filesystem::path("_sd_backup") / "errors" / "save" / "data.bin") == "second", "the second backup replaces the first one's files");

    SaveDataMountResult mounted{};
    Check(MountSave(&name, MountReadWrite, &mounted), "the save mounts for a backup whose copy fails");
    FailStep("backup.copy", 1);
    Check(sceSaveDataUmountWithBackup(&mounted.mount_point) == 0, "umountWithBackup returns 0 when the backup copy fails");
    ClearFailingStep();
    SaveDataEvent event{};
    Check(sceSaveDataGetEventResult(nullptr, &event) == 0 && event.error_code == SaveDataErrorInternal, "a failed backup copy gives the event error INTERNAL, got " + std::to_string(event.error_code));
    SaveDataMountInfo info{};
    Check(sceSaveDataGetMountInfo(&mounted.mount_point, &info) == SaveDataErrorNotMounted, "a failed backup copy still unmounts the save");
    param = {};
    Check(sceSaveDataCheckBackupData(&check) == 0 && std::strcmp(param.title, "Second") == 0, "a failed backup copy keeps the previous backup");

    SceSaveDataTitleId other{};
    std::memcpy(other.data, "PPSA99999", sizeof("PPSA99999"));
    check.title_id = &other;
    Check(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound, "checkBackupData with another title's id is NOT_FOUND");
    SaveDataRestoreBackupData restore{};
    restore.dir_name = &name;
    restore.title_id = &other;
    Check(sceSaveDataRestoreBackupData(&restore) == SaveDataErrorNotFound, "restoreBackupData with another title's id is NOT_FOUND");

    for (const char* invalid : {"..", "a/b"}) {
        const std::string label = std::string("\"") + invalid + "\"";
        SceSaveDataDirName bad{};
        std::memcpy(bad.data, invalid, std::strlen(invalid) + 1);
        SaveDataCheckBackupData badCheck{};
        badCheck.dir_name = &bad;
        Check(sceSaveDataCheckBackupData(&badCheck) == SaveDataErrorParameter, "checkBackupData of " + label + " is PARAMETER");
        SaveDataRestoreBackupData badRestore{};
        badRestore.dir_name = &bad;
        Check(sceSaveDataRestoreBackupData(&badRestore) == SaveDataErrorParameter, "restoreBackupData of " + label + " is PARAMETER");
        SaveDataMount3 mount{};
        mount.dir_name = &bad;
        mount.mount_mode = MountReadWrite | MountCreate2;
        SaveDataMountResult result{};
        Check(sceSaveDataMount3(&mount, &result) == SaveDataErrorParameter, "a save named " + label + " cannot be mounted to be backed up");
        SaveDataMountPoint point{};
        std::memcpy(point.data, invalid, std::strlen(invalid) + 1);
        Check(sceSaveDataUmountWithBackup(&point) == SaveDataErrorNotMounted, "umountWithBackup of the mount point " + label + " is NOT_MOUNTED");
    }
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

void CheckHostMountFailures() {
    const auto previous = std::filesystem::current_path();
    const auto root = previous / "mount-host-failures";
    std::filesystem::create_directories(root / "previous-alias");
    std::filesystem::current_path(root);
    const auto writeFixture = [](const char* path, const char* text) {
        auto* file = std::fopen(path, "wb");
        Check(file != nullptr, std::string("open fixture ") + path);
        if (file == nullptr) return;
        const bool written = std::fputs(text, file) >= 0;
        const bool closed = std::fclose(file) == 0;
        Check(written && closed, std::string("write fixture ") + path);
    };
    const auto checkFailure = [&](const char* name, std::uint32_t mode, std::uint64_t blocks, const std::string& label) {
        SceSaveDataDirName directory{};
        std::snprintf(directory.data, sizeof(directory.data), "%s", name);
        SaveDataMount3 request{};
        request.dir_name = &directory;
        request.mount_mode = mode;
        request.blocks = blocks;
        SaveDataMountResult result;
        std::memset(&result, 0xCD, sizeof(result));
        const auto before = result;
        const auto alias = std::filesystem::absolute("previous-alias").string();
        AddPathAlias_nid_no_patch("/savedata0", alias.c_str());
        const auto resolvedBefore = ResolvePath_nid_no_patch("/savedata0/probe");
        try {
            Check(sceSaveDataMount3(&request, &result) == static_cast<int>(0x809F000B), label + " returns INTERNAL");
        } catch (const std::exception& error) {
            Check(false, label + " throws: " + error.what());
        }
        Check(std::memcmp(&result, &before, sizeof(result)) == 0, label + " preserves the mount result");
        Check(ResolvePath_nid_no_patch("/savedata0/probe") == resolvedBefore, label + " preserves the path alias");
        SaveDataMountPoint point{};
        std::memcpy(point.data, "/savedata0", sizeof("/savedata0"));
        SaveDataMountInfo info{};
        const auto mounted = sceSaveDataGetMountInfo(&point, &info);
        Check(mounted == SaveDataErrorNotMounted, label + " consumes no mount slot");
        if (mounted == 0) sceSaveDataUmount2(0, &point);
        RemovePathAlias_nid_no_patch("/savedata0");
    };
    writeFixture("_sd", "not a directory");
    for (const auto mode : {MountReadWrite | MountCreate, MountReadWrite | MountCreate2}) {
        checkFailure("blocked", mode, 96, "save-root file with mode " + std::to_string(mode));
        Check(std::filesystem::is_regular_file("_sd"), "a failed mount preserves the blocking file");
    }
    Check(std::filesystem::remove("_sd"), "remove the blocked save-root fixture");
    std::filesystem::create_directories("_sd/metadata.blocks");
    writeFixture("_sd/metadata.blocks/keep", "preserve this directory");
    checkFailure("metadata", MountReadWrite | MountCreate2, 96, "size metadata path is a directory");
    Check(!std::filesystem::exists("_sd/metadata"), "failed size persistence removes the newly created empty save");
    Check(std::filesystem::is_regular_file("_sd/metadata.blocks/keep"), "failed size persistence preserves the blocking directory");
    std::filesystem::remove_all("_sd/metadata.blocks");
    MountAndCheck("metadata", MountReadWrite | MountCreate2, 1u, "mount after repairing host storage");
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
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
    CheckHostMountFailures();
    CheckMountCapacityAndLifecycle();
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 1u, "CREATE2 of a missing save");
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 0u, "CREATE2 of an existing save");
    MountAndCheck("made", MountReadWrite | MountCreate, 1u, "CREATE of a missing save");
    MountAndCheck("made", MountReadWrite, 0u, "open of an existing save");
    CheckFailedMountKeepsResult();
    CheckEquivalentMounts();
    CheckInvalidNamesAreRejected();
    CheckBackups();
    CheckCorruptBackups();
    CheckStagedRestore();
    CheckBackupSwap();
    CheckLinkedPaths();
    CheckBackupGuestErrors();
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
