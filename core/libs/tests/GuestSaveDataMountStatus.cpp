#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <system_error>

extern "C" {
int APS5_VABI sceSaveDataInitialize3(const void*);
int APS5_VABI sceSaveDataBackup(const SaveDataBackup*);
int APS5_VABI sceSaveDataGetEventResult(const void*, SaveDataEvent*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
int APS5_VABI sceSaveDataGetMountInfo(const SaveDataMountPoint*, SaveDataMountInfo*);
int APS5_VABI sceSaveDataGetParam(const SaveDataMountPoint*, std::uint32_t, void*, std::size_t, std::size_t*);
int APS5_VABI sceSaveDataSetParam(const SaveDataMountPoint*, std::uint32_t, const void*, std::size_t);
int APS5_VABI sceSaveDataLoadIcon(const SaveDataMountPoint*, SaveDataIcon*);
int APS5_VABI sceSaveDataSaveIcon(const SaveDataMountPoint*, const SaveDataIcon*);
int APS5_VABI sceSaveDataSaveIconByPath(const SaveDataMountPoint*, const char*);
}

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;
constexpr std::uint32_t MountCreate2 = 32;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotFound = -2137063416;

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
    Check(sceSaveDataGetEventResult(nullptr, nullptr) == SaveDataErrorParameter, "getEventResult null event is PARAMETER");
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
    Check(sceSaveDataLoadIcon(point, &icon) == 0, "loadIcon on mounted save succeeds");
    Check(sceSaveDataSaveIcon(nullptr, &icon) == SaveDataErrorParameter, "saveIcon null point is PARAMETER");
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
    std::filesystem::create_directory_symlink("identity", std::filesystem::path("_sd") / "linked", error);
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
        std::filesystem::remove(std::filesystem::path("_sd") / "linked", error);
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

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-mount-status-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    CheckUnmountedCalls();
    Check(sceSaveDataInitialize3(nullptr) == 0, "SaveData initializes");
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 1u, "CREATE2 of a missing save");
    MountAndCheck("fresh", MountReadWrite | MountCreate2, 0u, "CREATE2 of an existing save");
    MountAndCheck("made", MountReadWrite | MountCreate, 1u, "CREATE of a missing save");
    MountAndCheck("made", MountReadWrite, 0u, "open of an existing save");
    CheckFailedMountKeepsResult();
    CheckEquivalentMounts();
    CheckInvalidNamesAreRejected();
    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata mount status tests passed\n");
    return 0;
}
