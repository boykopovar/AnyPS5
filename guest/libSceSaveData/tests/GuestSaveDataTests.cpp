#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotMounted = -2137063420;

struct SceSaveDataDirName {
    char data[32];
};

struct SaveDataMountPs4 {
    std::int32_t user_id;
    std::int32_t pad;
    const void* title_id;
    const SceSaveDataDirName* dir_name;
    const void* fingerprint;
    std::uint64_t blocks;
    std::uint32_t mount_mode;
    std::uint8_t reserved[32];
};

struct SaveDataMountPoint {
    char data[16];
};

struct SaveDataMountResult {
    SaveDataMountPoint mount_point;
    std::uint32_t mount_status;
    std::uint32_t reserved;
};

struct SaveDataMountInfo {
    std::uint64_t blocks;
    std::uint64_t free_blocks;
};

extern "C" {
int sceSaveDataMount(const SaveDataMountPs4* mount, SaveDataMountResult* mountResult);
int sceSaveDataUmount(const SaveDataMountPoint* mountPoint);
int sceSaveDataGetMountInfo(const SaveDataMountPoint* mountPoint, SaveDataMountInfo* info);
}

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", what.c_str());
    ++failures;
}

void CheckSaveDataMountBounds() {
    SaveDataMountResult result{};
    SaveDataMountPs4 mount{};

    Check(sceSaveDataMount(nullptr, &result) == SaveDataErrorParameter, "mount null mount returns PARAMETER");
    Check(sceSaveDataMount(&mount, nullptr) == SaveDataErrorParameter, "mount null result returns PARAMETER");
    Check(sceSaveDataMount(nullptr, nullptr) == SaveDataErrorParameter, "mount all null returns PARAMETER");

    mount.dir_name = nullptr;
    Check(sceSaveDataMount(&mount, &result) == SaveDataErrorParameter, "mount null dir_name returns PARAMETER");

    SceSaveDataDirName invalidDir{};
    invalidDir.data[0] = '\0';
    mount.dir_name = &invalidDir;
    Check(sceSaveDataMount(&mount, &result) == SaveDataErrorParameter, "mount empty dir_name returns PARAMETER");

    std::memcpy(invalidDir.data, "../escape", sizeof("../escape"));
    Check(sceSaveDataMount(&mount, &result) == SaveDataErrorParameter, "mount traversal dir_name returns PARAMETER");

    std::memcpy(invalidDir.data, "valid_name", sizeof("valid_name"));
    mount.mount_mode = 0;
    Check(sceSaveDataMount(&mount, &result) == SaveDataErrorParameter, "mount zero mode returns PARAMETER");
}

void CheckSaveDataUmountBounds() {
    Check(sceSaveDataUmount(nullptr) == SaveDataErrorParameter, "umount null point returns PARAMETER");

    SaveDataMountPoint invalidPoint{};
    Check(sceSaveDataUmount(&invalidPoint) == SaveDataErrorNotMounted, "umount zeroed point returns NOT_MOUNTED");

    std::memcpy(invalidPoint.data, "invalid_pt", sizeof("invalid_pt"));
    Check(sceSaveDataUmount(&invalidPoint) == SaveDataErrorNotMounted, "umount unrecognized point returns NOT_MOUNTED");

    std::memcpy(invalidPoint.data, "/savedata99", sizeof("/savedata99"));
    Check(sceSaveDataUmount(&invalidPoint) == SaveDataErrorNotMounted, "umount unmounted slot returns NOT_MOUNTED");
}

void CheckSaveDataGetMountInfoBounds() {
    SaveDataMountInfo info{};
    SaveDataMountPoint point{};
    std::memcpy(point.data, "dummy_pt", sizeof("dummy_pt"));

    Check(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter, "getMountInfo null point returns PARAMETER");
    Check(sceSaveDataGetMountInfo(&point, nullptr) == SaveDataErrorParameter, "getMountInfo null info returns PARAMETER");
    Check(sceSaveDataGetMountInfo(nullptr, nullptr) == SaveDataErrorParameter, "getMountInfo all null returns PARAMETER");

    SaveDataMountPoint zeroPoint{};
    Check(sceSaveDataGetMountInfo(&zeroPoint, &info) == SaveDataErrorNotMounted, "getMountInfo zeroed point returns NOT_MOUNTED");

    Check(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted, "getMountInfo unmounted point returns NOT_MOUNTED");

    SaveDataMountPoint invalidSlot{};
    std::memcpy(invalidSlot.data, "/savedata16", sizeof("/savedata16"));
    Check(sceSaveDataGetMountInfo(&invalidSlot, &info) == SaveDataErrorNotMounted, "getMountInfo invalid slot returns NOT_MOUNTED");
}

}

int main() {
    CheckSaveDataMountBounds();
    CheckSaveDataUmountBounds();
    CheckSaveDataGetMountInfoBounds();

    if (failures != 0) {
        return 1;
    }
    std::printf("all savedata negative bounds tests passed\n");
    return 0;
}
