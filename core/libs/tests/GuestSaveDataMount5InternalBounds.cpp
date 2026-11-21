#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <source_location>
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

constexpr std::uint32_t MountModeRdOnly = 1;
constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate = 4;
constexpr std::uint32_t MountModeCreate2 = 32;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

template <typename Function>
void RequireThrowsUnimplemented(Function function, std::string_view expectedName, const std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        if (std::string_view(error.what()).find(expectedName) == std::string_view::npos ||
            std::string_view(error.what()).find("not implemented") == std::string_view::npos) {
            std::fprintf(stderr, "RequireThrowsUnimplemented failed at line %u\n", static_cast<unsigned>(location.line()));
            std::fflush(stderr);
            std::abort();
        }
        return;
    }
    std::fprintf(stderr, "RequireThrowsUnimplemented did not throw at line %u\n", static_cast<unsigned>(location.line()));
    std::fflush(stderr);
    std::abort();
}

void TestUnimplementedMountVariants() {
    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataMount5());
    }, "sceSaveDataMount5");

    RequireThrowsUnimplemented([] {
        static_cast<void>(sceSaveDataMountInternal());
    }, "sceSaveDataMountInternal");
}

void TestMount3ParameterBounds() {
    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(nullptr, &mountResult) == SaveDataErrorParameter);

    SaveDataMount3 nullMount{};
    Require(sceSaveDataMount3(&nullMount, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataMount3(&nullMount, &mountResult) == SaveDataErrorParameter);

    SceSaveDataDirName emptyDir{};
    nullMount.dir_name = &emptyDir;
    nullMount.mount_mode = MountModeRdWr | MountModeCreate2;
    Require(sceSaveDataMount3(&nullMount, &mountResult) == SaveDataErrorParameter);

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'm', sizeof(unterminatedDir.data));
    nullMount.dir_name = &unterminatedDir;
    Require(sceSaveDataMount3(&nullMount, &mountResult) == SaveDataErrorParameter);

    for (const char* invalidName : {".", "..", "../escape", "..\\escape", "bad/dir", "bad\\dir", "bad:colon"}) {
        SceSaveDataDirName badDir{};
        std::snprintf(badDir.data, sizeof(badDir.data), "%s", invalidName);
        nullMount.dir_name = &badDir;
        Require(sceSaveDataMount3(&nullMount, &mountResult) == SaveDataErrorParameter);
    }
}

void TestMount3LifecycleAndCompatibility() {
    SceSaveDataDirName absentDir{};
    std::memcpy(absentDir.data, "absent_dir_check", sizeof("absent_dir_check"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &absentDir;
    mount.mount_mode = MountModeRdWr;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataErrorNotFound);

    mount.mount_mode = MountModeRdOnly;
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataErrorNotFound);

    SceSaveDataDirName liveDir{};
    std::memcpy(liveDir.data, "compat_save_dir", sizeof("compat_save_dir"));
    mount.dir_name = &liveDir;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);
    Require(mountResult.mount_status == 1);
    Require(std::strlen(mountResult.mount_point.data) > 0);

    SaveDataMountResult busyResult{};
    Require(sceSaveDataMount3(&mount, &busyResult) == SaveDataErrorBusy);

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&mountResult.mount_point, &info) == SaveDataOk);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);
    Require(sceSaveDataGetMountInfo(&mountResult.mount_point, &info) == SaveDataErrorNotMounted);

    mount.mount_mode = MountModeRdWr | MountModeCreate;
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataErrorExists);

    mount.mount_mode = MountModeRdWr;
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);
    Require(mountResult.mount_status == 0);
    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    SaveDataDelete del{};
    del.user_id = 1;
    del.dir_name = &liveDir;
    Require(sceSaveDataDelete(&del) == SaveDataOk);
}

void TestMultiSlotMountIsolation() {
    SceSaveDataDirName dirA{};
    std::memcpy(dirA.data, "MULTI_DIR_A", sizeof("MULTI_DIR_A"));
    SceSaveDataDirName dirB{};
    std::memcpy(dirB.data, "MULTI_DIR_B", sizeof("MULTI_DIR_B"));

    SaveDataMount3 mountA{};
    mountA.user_id = 1;
    mountA.dir_name = &dirA;
    mountA.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult resultA{};
    Require(sceSaveDataMount3(&mountA, &resultA) == SaveDataOk);

    SaveDataMount3 mountB{};
    mountB.user_id = 1;
    mountB.dir_name = &dirB;
    mountB.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult resultB{};
    Require(sceSaveDataMount3(&mountB, &resultB) == SaveDataOk);

    Require(std::strcmp(resultA.mount_point.data, resultB.mount_point.data) != 0);

    Require(sceSaveDataUmount2(0, &resultA.mount_point) == SaveDataOk);
    Require(sceSaveDataUmount2(0, &resultB.mount_point) == SaveDataOk);

    SaveDataDelete delA{};
    delA.user_id = 1;
    delA.dir_name = &dirA;
    Require(sceSaveDataDelete(&delA) == SaveDataOk);

    SaveDataDelete delB{};
    delB.user_id = 1;
    delB.dir_name = &dirB;
    Require(sceSaveDataDelete(&delB) == SaveDataOk);
}

void TestTerminationLifecycle() {
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-mount5-internal-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox / "app0" / "sce_sys");
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
    param << "{\"titleId\":\"PPSA00000\"}\n";
    param.close();

    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);

    TestUnimplementedMountVariants();
    TestMount3ParameterBounds();
    TestMount3LifecycleAndCompatibility();
    TestMultiSlotMountIsolation();
    TestTerminationLifecycle();
    TestUnimplementedMountVariants();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
