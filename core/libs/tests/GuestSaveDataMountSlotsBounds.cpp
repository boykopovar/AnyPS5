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
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr int SaveDataErrorNotMounted = -2137063420;
constexpr int SaveDataErrorExists = -2137063417;
constexpr int SaveDataErrorNotFound = -2137063416;
constexpr int SaveDataErrorMountFull = -2137063412;
constexpr int SaveDataErrorInvalidLoginUser = -2137063407;

constexpr std::uint32_t MountModeRdOnly = 1;
constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate = 4;
constexpr std::uint32_t MountModeCreate2 = 32;

constexpr std::size_t MaxMountSlots = 16;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Function>
void RequireThrowsUnknownMode(Function function) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find("unknown mount_mode") != std::string_view::npos);
        return;
    }
    std::abort();
}

void TestPreInitializationBounds() {
    SaveDataMountResult result{};
    Require(sceSaveDataMount2(nullptr, &result) == SaveDataErrorParameter);

    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "preinit_dir", sizeof("preinit_dir"));

    SaveDataMount2 mount2{};
    mount2.user_id = 1;
    mount2.dir_name = &dirName;
    mount2.mount_mode = MountModeRdWr | MountModeCreate2;
    mount2.blocks = 96;
    Require(sceSaveDataMount2(&mount2, &result) == SaveDataErrorNotInitialized);

    SaveDataMountPoint point{};
    std::memcpy(point.data, "/savedata0", sizeof("/savedata0"));
    Require(sceSaveDataUmount2(0, &point) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount(&point) == SaveDataErrorNotMounted);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted);
}

void TestMountParameterValidation() {
    SaveDataMountResult result{};

    Require(sceSaveDataMount3(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount3(nullptr, &result) == SaveDataErrorParameter);

    SaveDataMount3 mount3{};
    Require(sceSaveDataMount3(&mount3, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount3(&mount3, &result) == SaveDataErrorParameter);

    SceSaveDataDirName validDir{};
    std::memcpy(validDir.data, "valid_param_dir", sizeof("valid_param_dir"));
    mount3.dir_name = &validDir;

    mount3.mount_mode = 0;
    RequireThrowsUnknownMode([&] { static_cast<void>(sceSaveDataMount3(&mount3, &result)); });

    mount3.mount_mode = 0x80000000u;
    RequireThrowsUnknownMode([&] { static_cast<void>(sceSaveDataMount3(&mount3, &result)); });

    Require(sceSaveDataMount2(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount2(nullptr, &result) == SaveDataErrorParameter);

    SaveDataMount2 mount2{};
    Require(sceSaveDataMount2(&mount2, nullptr) == SaveDataErrorParameter);

    mount2.user_id = 0;
    mount2.dir_name = &validDir;
    mount2.mount_mode = MountModeRdWr | MountModeCreate2;
    mount2.blocks = 96;
    Require(sceSaveDataMount2(&mount2, &result) == SaveDataErrorInvalidLoginUser);

    mount2.user_id = -1;
    Require(sceSaveDataMount2(&mount2, &result) == SaveDataErrorInvalidLoginUser);

    mount2.user_id = 1;
    mount2.mount_mode = 0;
    Require(sceSaveDataMount2(&mount2, &result) == SaveDataErrorParameter);

    mount2.mount_mode = MountModeRdWr | MountModeCreate;
    mount2.blocks = 95;
    Require(sceSaveDataMount2(&mount2, &result) == SaveDataErrorParameter);

    mount2.blocks = 0;
    Require(sceSaveDataMount2(&mount2, &result) == SaveDataErrorParameter);

    mount2.blocks = 96;
    SceSaveDataDirName underscoreDir{};
    std::memcpy(underscoreDir.data, "has_underscore", sizeof("has_underscore"));
    mount2.dir_name = &underscoreDir;
    Require(sceSaveDataMount2(&mount2, &result) == SaveDataErrorParameter);

    Require(sceSaveDataMount(nullptr, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount(nullptr, &result) == SaveDataErrorParameter);

    SaveDataMountPs4 mountPs4{};
    Require(sceSaveDataMount(&mountPs4, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount(&mountPs4, &result) == SaveDataErrorParameter);

    mountPs4.user_id = 1;
    mountPs4.dir_name = &validDir;
    mountPs4.mount_mode = MountModeRdWr | MountModeCreate2;
    mountPs4.blocks = 96;

    char dummyTitle[16] = "CUSA00001";
    mountPs4.title_id = dummyTitle;
    RequireThrowsUnimplemented([&] { static_cast<void>(sceSaveDataMount(&mountPs4, &result)); }, "sceSaveDataMount of another title's save data");
}

void TestDirectoryPathBoundsAndTraversals() {
    SaveDataMountResult result{};
    SaveDataMount3 mount{};
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SceSaveDataDirName unterminated{};
    std::memset(unterminated.data, 'z', sizeof(unterminated.data));
    mount.dir_name = &unterminated;
    Require(sceSaveDataMount3(&mount, &result) == SaveDataErrorParameter);

    SceSaveDataDirName emptyDir{};
    emptyDir.data[0] = '\0';
    mount.dir_name = &emptyDir;
    Require(sceSaveDataMount3(&mount, &result) == SaveDataErrorParameter);

    for (const char* invalid : {".", "..", "../escape", "..\\escape", "sub/dir", "sub\\dir", "path:colon", "/leadslash", "\\leadbackslash"}) {
        SceSaveDataDirName invalidDir{};
        std::snprintf(invalidDir.data, sizeof(invalidDir.data), "%s", invalid);
        mount.dir_name = &invalidDir;
        Require(sceSaveDataMount3(&mount, &result) == SaveDataErrorParameter);
    }

    SceSaveDataDirName maxLenDir{};
    std::memset(maxLenDir.data, 'a', 31);
    maxLenDir.data[31] = '\0';
    mount.dir_name = &maxLenDir;
    Require(sceSaveDataMount3(&mount, &result) == SaveDataOk);
    Require(result.mount_status == 1u);
    Require(sceSaveDataUmount2(0, &result.mount_point) == SaveDataOk);
}

void TestContainerSlotLimitsAndExhaustion() {
    std::array<SaveDataMountResult, MaxMountSlots> results{};

    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        SceSaveDataDirName dirName{};
        std::snprintf(dirName.data, sizeof(dirName.data), "slotcontainer%02zu", i);

        SaveDataMount3 mount{};
        mount.user_id = 1;
        mount.dir_name = &dirName;
        mount.mount_mode = MountModeRdWr | MountModeCreate2;

        Require(sceSaveDataMount3(&mount, &results[i]) == SaveDataOk);
        Require(results[i].mount_status == 1u);

        char expectedPoint[16]{};
        std::snprintf(expectedPoint, sizeof(expectedPoint), "/savedata%zu", i);
        Require(std::strcmp(results[i].mount_point.data, expectedPoint) == 0);

        SaveDataMountInfo info{};
        Require(sceSaveDataGetMountInfo(&results[i].mount_point, &info) == SaveDataOk);
    }

    SceSaveDataDirName overflowDir{};
    std::memcpy(overflowDir.data, "overflowslot", sizeof("overflowslot"));

    SaveDataMount3 overflowMount3{};
    overflowMount3.user_id = 1;
    overflowMount3.dir_name = &overflowDir;
    overflowMount3.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult overflowResult{};
    Require(sceSaveDataMount3(&overflowMount3, &overflowResult) == SaveDataErrorMountFull);

    SaveDataMount2 overflowMount2{};
    overflowMount2.user_id = 1;
    overflowMount2.dir_name = &overflowDir;
    overflowMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    overflowMount2.blocks = 96;
    Require(sceSaveDataMount2(&overflowMount2, &overflowResult) == SaveDataErrorMountFull);

    SaveDataMountPs4 overflowMountPs4{};
    overflowMountPs4.user_id = 1;
    overflowMountPs4.dir_name = &overflowDir;
    overflowMountPs4.mount_mode = MountModeRdWr | MountModeCreate2;
    Require(sceSaveDataMount(&overflowMountPs4, &overflowResult) == SaveDataErrorMountFull);

    SceSaveDataDirName busyDir{};
    std::memcpy(busyDir.data, "slotcontainer00", sizeof("slotcontainer00"));
    SaveDataMount3 busyMount{};
    busyMount.user_id = 1;
    busyMount.dir_name = &busyDir;
    busyMount.mount_mode = MountModeRdWr | MountModeCreate2;
    Require(sceSaveDataMount3(&busyMount, &overflowResult) == SaveDataErrorBusy);

    Require(sceSaveDataTerminate() == SaveDataErrorBusy);

    constexpr std::size_t SlotToFree = 7;
    Require(sceSaveDataUmount2(0, &results[SlotToFree].mount_point) == SaveDataOk);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&results[SlotToFree].mount_point, &info) == SaveDataErrorNotMounted);

    SceSaveDataDirName replacementDir{};
    std::memcpy(replacementDir.data, "replaceslot07", sizeof("replaceslot07"));
    SaveDataMount3 replacementMount{};
    replacementMount.user_id = 1;
    replacementMount.dir_name = &replacementDir;
    replacementMount.mount_mode = MountModeRdWr | MountModeCreate2;
    SaveDataMountResult replacementResult{};
    Require(sceSaveDataMount3(&replacementMount, &replacementResult) == SaveDataOk);
    Require(std::strcmp(replacementResult.mount_point.data, "/savedata7") == 0);

    Require(sceSaveDataMount3(&overflowMount3, &overflowResult) == SaveDataErrorMountFull);

    constexpr std::size_t AnotherSlotToFree = 3;
    Require(sceSaveDataUmount(&results[AnotherSlotToFree].mount_point) == SaveDataOk);
    Require(sceSaveDataGetMountInfo(&results[AnotherSlotToFree].mount_point, &info) == SaveDataErrorNotMounted);

    SceSaveDataDirName replacementDir2{};
    std::memcpy(replacementDir2.data, "replaceslot03", sizeof("replaceslot03"));
    SaveDataMount2 replacementMount2{};
    replacementMount2.user_id = 1;
    replacementMount2.dir_name = &replacementDir2;
    replacementMount2.mount_mode = MountModeRdWr | MountModeCreate2;
    replacementMount2.blocks = 96;
    SaveDataMountResult replacementResult2{};
    Require(sceSaveDataMount2(&replacementMount2, &replacementResult2) == SaveDataOk);
    Require(std::strcmp(replacementResult2.mount_point.data, "/savedata3") == 0);

    Require(sceSaveDataUmount2(0, &replacementResult.mount_point) == SaveDataOk);
    Require(sceSaveDataUmount2(0, &replacementResult2.mount_point) == SaveDataOk);
    for (std::size_t i = 0; i < MaxMountSlots; ++i) {
        if (i != SlotToFree && i != AnotherSlotToFree) {
            Require(sceSaveDataUmount2(0, &results[i].mount_point) == SaveDataOk);
        }
    }
}

void TestUnmountBoundsAndErrors() {
    Require(sceSaveDataUmount2(0, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataUmount(nullptr) == SaveDataErrorParameter);

    SaveDataMountPoint unterminatedPoint{};
    std::memset(unterminatedPoint.data, 'x', sizeof(unterminatedPoint.data));
    Require(sceSaveDataUmount2(0, &unterminatedPoint) == SaveDataErrorParameter);
    Require(sceSaveDataUmount(&unterminatedPoint) == SaveDataErrorParameter);

    SaveDataMountPoint invalidPoint{};
    std::memcpy(invalidPoint.data, "/savedata99", sizeof("/savedata99"));
    Require(sceSaveDataUmount2(0, &invalidPoint) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount(&invalidPoint) == SaveDataErrorNotMounted);

    std::memcpy(invalidPoint.data, "unmountedslot", sizeof("unmountedslot"));
    Require(sceSaveDataUmount2(0, &invalidPoint) == SaveDataErrorNotMounted);
    Require(sceSaveDataUmount(&invalidPoint) == SaveDataErrorNotMounted);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-mount-slots-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestPreInitializationBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestMountParameterValidation();
    TestDirectoryPathBoundsAndTraversals();
    TestContainerSlotLimitsAndExhaustion();
    TestUnmountBoundsAndErrors();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
