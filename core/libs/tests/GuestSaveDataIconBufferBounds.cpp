#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorNotFound = -2137063416;

constexpr std::uint32_t MountReadOnly = 1;
constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUnmountedAndNullParameterBounds() {
    SaveDataMountPoint unmountedPoint{};
    std::memcpy(unmountedPoint.data, "unmounted_slot", sizeof("unmounted_slot"));

    char iconData[128]{};
    SaveDataIcon validIcon{};
    validIcon.buf = iconData;
    validIcon.buf_size = sizeof(iconData);
    validIcon.data_size = sizeof(iconData);

    Require(sceSaveDataLoadIcon(&unmountedPoint, &validIcon) == SaveDataErrorNotMounted);
    Require(sceSaveDataSaveIcon(&unmountedPoint, &validIcon) == SaveDataErrorNotMounted);

    Require(sceSaveDataLoadIcon(nullptr, &validIcon) == SaveDataErrorParameter);
    Require(sceSaveDataLoadIcon(&unmountedPoint, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIcon(nullptr, &validIcon) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIcon(&unmountedPoint, nullptr) == SaveDataErrorParameter);

    SaveDataIcon nullBufIcon{};
    nullBufIcon.buf = nullptr;
    nullBufIcon.buf_size = 128;
    nullBufIcon.data_size = 128;
    Require(sceSaveDataLoadIcon(&unmountedPoint, &nullBufIcon) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIcon(&unmountedPoint, &nullBufIcon) == SaveDataErrorParameter);

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'x', sizeof(unterminatedPoint.data));
    Require(sceSaveDataLoadIcon(&unterminatedPoint, &validIcon) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIcon(&unterminatedPoint, &validIcon) == SaveDataErrorParameter);

    Require(sceSaveDataSaveIconByPath(nullptr, "sce_sys/icon0.png") == SaveDataErrorParameter);
    Require(sceSaveDataSaveIconByPath(&unterminatedPoint, "sce_sys/icon0.png") == SaveDataErrorParameter);
    Require(sceSaveDataSaveIconByPath(&unmountedPoint, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataSaveIconByPath(&unmountedPoint, "") == SaveDataErrorParameter);
    Require(sceSaveDataSaveIconByPath(&unmountedPoint, "sce_sys/icon0.png") == SaveDataErrorNotMounted);
}

void TestIconLifecycleAndBufferSizeBounds() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "icon_bounds", sizeof("icon_bounds"));

    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadWrite | MountCreate2;
    mount.blocks = 96;
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == 0);
    Require(mountResult.mount_status == 1u);

    char loadBuf[256]{};
    SaveDataIcon loadIcon{};
    loadIcon.buf = loadBuf;
    loadIcon.buf_size = sizeof(loadBuf);
    loadIcon.data_size = 0;
    Require(sceSaveDataLoadIcon(&mountResult.mount_point, &loadIcon) == SaveDataErrorNotFound);

    std::array<std::uint8_t, 64> pngPayload{};
    pngPayload[0] = 0x89;
    pngPayload[1] = 'P';
    pngPayload[2] = 'N';
    pngPayload[3] = 'G';
    pngPayload[4] = 0x0D;
    pngPayload[5] = 0x0A;
    pngPayload[6] = 0x1A;
    pngPayload[7] = 0x0A;
    for (std::size_t i = 8; i < pngPayload.size(); ++i) {
        pngPayload[i] = static_cast<std::uint8_t>(i * 17);
    }

    SaveDataIcon saveIcon{};
    saveIcon.buf = pngPayload.data();
    saveIcon.buf_size = pngPayload.size();
    saveIcon.data_size = pngPayload.size();
    Require(sceSaveDataSaveIcon(&mountResult.mount_point, &saveIcon) == 0);

    std::array<std::uint8_t, 128> receiveBuf{};
    receiveBuf.fill(0xEE);
    loadIcon.buf = receiveBuf.data();
    loadIcon.buf_size = receiveBuf.size();
    loadIcon.data_size = 0;
    Require(sceSaveDataLoadIcon(&mountResult.mount_point, &loadIcon) == 0);
    Require(loadIcon.data_size == pngPayload.size());
    Require(std::memcmp(receiveBuf.data(), pngPayload.data(), pngPayload.size()) == 0);
    for (std::size_t i = pngPayload.size(); i < receiveBuf.size(); ++i) {
        Require(receiveBuf[i] == 0xEE);
    }

    receiveBuf.fill(0xAA);
    loadIcon.buf = receiveBuf.data();
    loadIcon.buf_size = 16;
    loadIcon.data_size = 0;
    Require(sceSaveDataLoadIcon(&mountResult.mount_point, &loadIcon) == 0);
    Require(loadIcon.data_size == pngPayload.size());
    Require(std::memcmp(receiveBuf.data(), pngPayload.data(), 16) == 0);
    for (std::size_t i = 16; i < receiveBuf.size(); ++i) {
        Require(receiveBuf[i] == 0xAA);
    }

    receiveBuf.fill(0xBB);
    loadIcon.buf = receiveBuf.data();
    loadIcon.buf_size = 0;
    loadIcon.data_size = 0;
    Require(sceSaveDataLoadIcon(&mountResult.mount_point, &loadIcon) == 0);
    Require(loadIcon.data_size == pngPayload.size());
    for (std::size_t i = 0; i < receiveBuf.size(); ++i) {
        Require(receiveBuf[i] == 0xBB);
    }

    Require(sceSaveDataSaveIconByPath(&mountResult.mount_point, "sce_sys/icon0.png") == SaveDataErrorNotFound);

    const auto sourceIcon = ResolvePath_nid_no_patch("/app0/source-icon.png");
    std::filesystem::create_directories(sourceIcon.parent_path());
    std::ofstream sourceStream(sourceIcon, std::ios::binary | std::ios::trunc);
    sourceStream.write(reinterpret_cast<const char*>(pngPayload.data()), pngPayload.size());
    sourceStream.close();
    Require(sceSaveDataSaveIconByPath(&mountResult.mount_point, "/app0/source-icon.png") == 0);

    loadIcon.data_size = 0;
    Require(sceSaveDataLoadIcon(&mountResult.mount_point, &loadIcon) == 0);
    Require(loadIcon.data_size == pngPayload.size());

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == 0);
    Require(sceSaveDataLoadIcon(&mountResult.mount_point, &loadIcon) == SaveDataErrorNotMounted);
    Require(sceSaveDataSaveIcon(&mountResult.mount_point, &saveIcon) == SaveDataErrorNotMounted);
}

void TestTitleIdValidationWithBackupIcon() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "backup_icon_dir", sizeof("backup_icon_dir"));

    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadWrite | MountCreate2;
    mount.blocks = 96;
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == 0);

    char dummyIconBuf[64]{};
    SaveDataIcon dummyIcon{};
    dummyIcon.buf = dummyIconBuf;
    dummyIcon.buf_size = sizeof(dummyIconBuf);
    dummyIcon.data_size = sizeof(dummyIconBuf);

    SceSaveDataTitleId invalidTitleId{};
    std::memcpy(invalidTitleId.data, "CUSA88888", sizeof("CUSA88888"));

    SaveDataCheckBackupData check{};
    check.dir_name = &dirName;
    check.icon = &dummyIcon;
    check.title_id = &invalidTitleId;
    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    Require(sceSaveDataUmountWithBackup(&mountResult.mount_point) == 0);

    Require(sceSaveDataCheckBackupData(&check) == SaveDataErrorNotFound);

    check.title_id = nullptr;
    Require(sceSaveDataCheckBackupData(&check) == 0);

    SceSaveDataTitleId matchingTitleId{};
    std::memcpy(matchingTitleId.data, "PPSA00000", sizeof("PPSA00000"));
    check.title_id = &matchingTitleId;
    Require(sceSaveDataCheckBackupData(&check) == 0);
}

void TestReadOnlyMountIconSave() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "rdonly_icon", sizeof("rdonly_icon"));

    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadOnly | MountCreate2;
    mount.blocks = 96;
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == 0);

    std::array<std::uint8_t, 32> iconBytes{};
    iconBytes.fill(0x77);
    SaveDataIcon icon{};
    icon.buf = iconBytes.data();
    icon.buf_size = iconBytes.size();
    icon.data_size = iconBytes.size();

    Require(sceSaveDataSaveIcon(&mountResult.mount_point, &icon) == 0);

    std::array<std::uint8_t, 64> readBytes{};
    readBytes.fill(0);
    SaveDataIcon readIcon{};
    readIcon.buf = readBytes.data();
    readIcon.buf_size = readBytes.size();
    Require(sceSaveDataLoadIcon(&mountResult.mount_point, &readIcon) == 0);
    Require(readIcon.data_size == iconBytes.size());
    Require(std::memcmp(readBytes.data(), iconBytes.data(), iconBytes.size()) == 0);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == 0);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-icon-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    const auto paramJson = ResolvePath_nid_no_patch("/app0/sce_sys/param.json");
    std::filesystem::create_directories(paramJson.parent_path());
    std::ofstream paramStream(paramJson, std::ios::binary | std::ios::trunc);
    paramStream << "{\"titleId\":\"PPSA00000\",\"localizedParameters\":{\"en-US\":{\"titleName\":\"Example\"}}}";
    paramStream.close();

    Require(sceSaveDataInitialize3(nullptr) == 0);

    TestUnmountedAndNullParameterBounds();
    TestIconLifecycleAndBufferSizeBounds();
    TestTitleIdValidationWithBackupIcon();
    TestReadOnlyMountIconSave();

    Require(sceSaveDataTerminate() == 0);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
