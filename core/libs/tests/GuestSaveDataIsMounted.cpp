#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotFound = -2137063416;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedState() {
    SaveDataMountPoint point{};
    std::memcpy(point.data, "uninit", sizeof("uninit"));
    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &point) == SaveDataErrorNotMounted);
}

void TestNullAndParameterBounds() {
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(nullptr, &mountResult) == SaveDataErrorParameter);

    SaveDataMount3 nullDirMount{};
    Require(sceSaveDataMount3(&nullDirMount, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount3(&nullDirMount, &mountResult) == SaveDataErrorParameter);

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'a', sizeof(unterminatedDir.data));
    SaveDataMount3 unterminatedMount{};
    unterminatedMount.dir_name = &unterminatedDir;
    unterminatedMount.mount_mode = MountReadWrite | MountCreate2;
    Require(sceSaveDataMount3(&unterminatedMount, &mountResult) == SaveDataErrorParameter);

    Require(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter);

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'x', sizeof(unterminatedPoint.data));
    Require(sceSaveDataUmount2(0, &unterminatedPoint) == SaveDataErrorParameter);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&unterminatedPoint, &info) == SaveDataErrorParameter);

    SaveDataMountPoint validUnmountedPoint{};
    std::memcpy(validUnmountedPoint.data, "slot_none", sizeof("slot_none"));
    Require(sceSaveDataGetMountInfo(&validUnmountedPoint, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&validUnmountedPoint, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &validUnmountedPoint) == SaveDataErrorNotMounted);

    SaveDataDirNameSearchResult searchResult{};
    Require(sceSaveDataDirNameSearch(nullptr, &searchResult) == SaveDataErrorParameter);

    SaveDataDirNameSearchCond searchCond{};
    Require(sceSaveDataDirNameSearch(&searchCond, nullptr) == SaveDataErrorParameter);
}

void TestMountLifecycleAndStateVerification() {
    const auto savePath = std::filesystem::path("_sd") / "mount_audit";
    std::filesystem::remove_all(savePath);

    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "mount_audit", sizeof("mount_audit"));

    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadWrite | MountCreate2;

    SaveDataMountResult result{};
    const int mountStatus = sceSaveDataMount3(&mount, &result);
    Require(mountStatus == 0);
    Require(result.mount_status == 1u);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&result.mount_point, &info) == 0);
    Require(info.blocks >= 48);

    SaveDataMountResult secondResult{};
    Require(sceSaveDataMount3(&mount, &secondResult) == SaveDataErrorBusy);

    SceSaveDataDirName nonExistentDir{};
    std::memcpy(nonExistentDir.data, "absent_mount", sizeof("absent_mount"));
    SaveDataMount3 absentMount{};
    absentMount.dir_name = &nonExistentDir;
    absentMount.mount_mode = MountReadWrite;
    SaveDataMountResult absentResult{};
    Require(sceSaveDataMount3(&absentMount, &absentResult) == SaveDataErrorNotFound);

    Require(sceSaveDataUmount2(0, &result.mount_point) == 0);

    Require(sceSaveDataGetMountInfo(&result.mount_point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount2(0, &result.mount_point) == SaveDataErrorNotMounted);

    std::filesystem::remove_all(savePath);
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-mount-bounds-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    TestUninitializedState();
    Require(sceSaveDataInitialize3(nullptr) == 0);
    TestNullAndParameterBounds();
    TestMountLifecycleAndStateVerification();
    Require(sceSaveDataTerminate() == 0);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    return 0;
}
