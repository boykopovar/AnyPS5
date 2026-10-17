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
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
}

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate = 4;
constexpr std::uint32_t MountCreate2 = 32;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotFound = -2137063416;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata mount status check failed: %s\n", what.c_str());
    ++failures;
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
