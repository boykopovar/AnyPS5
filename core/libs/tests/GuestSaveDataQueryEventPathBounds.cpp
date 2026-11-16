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
constexpr int SaveDataErrorNotInitialized = -2137063423;

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Function>
void RequireThrowsUnimplemented(Function function, std::string_view expectedName) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(expectedName) != std::string_view::npos);
        Require(std::string_view(error.what()).find("not implemented") != std::string_view::npos);
        return;
    }
    std::abort();
}

void TestQueryEventAndPathDebtStubs() {
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataGetAppLaunchedUser()); }, "sceSaveDataGetAppLaunchedUser");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataGetBoundPsnAccountCount()); }, "sceSaveDataGetBoundPsnAccountCount");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataGetEventInfo()); }, "sceSaveDataGetEventInfo");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataSetEventInfo()); }, "sceSaveDataSetEventInfo");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataGetSaveDataRootDir()); }, "sceSaveDataGetSaveDataRootDir");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataGetSaveDataRootPath()); }, "sceSaveDataGetSaveDataRootPath");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataGetSaveDataRootUsbPath()); }, "sceSaveDataGetSaveDataRootUsbPath");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataGetUpdatedDataCount()); }, "sceSaveDataGetUpdatedDataCount");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataSetSaveDataLibraryUser()); }, "sceSaveDataSetSaveDataLibraryUser");
    RequireThrowsUnimplemented([] { static_cast<void>(sceSaveDataIsMounted()); }, "sceSaveDataIsMounted");
    RequireThrowsUnimplemented([] {
        std::uint32_t count = 0;
        static_cast<void>(sceSaveDataGetMountedSaveDataCount(0, &count));
    }, "sceSaveDataGetMountedSaveDataCount");
}

void TestActiveSessionIsolation() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "query_path_save", sizeof("query_path_save"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    TestQueryEventAndPathDebtStubs();

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    SaveDataDelete delReq{};
    delReq.dir_name = &dirName;
    Require(sceSaveDataDelete(&delReq) == SaveDataOk);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-query-path-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    TestQueryEventAndPathDebtStubs();

    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);

    TestActiveSessionIsolation();

    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    TestQueryEventAndPathDebtStubs();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
