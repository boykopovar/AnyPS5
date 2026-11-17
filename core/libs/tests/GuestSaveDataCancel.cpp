#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <random>
#include <string>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorBusy = -2137063421;
constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;

bool ReportsSuccess(std::uint32_t argument) {
    const int result = sceSaveDataCancel(argument);
    if (result != 0) {
        std::fprintf(stderr, "sceSaveDataCancel(%#x) after initialization did not report success: %#x\n", argument, static_cast<unsigned>(result));
        return false;
    }
    return true;
}

void TestActiveMountTerminationBusyCheck() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-cancel-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    if (sceSaveDataInitialize3(nullptr) != SaveDataOk) {
        std::fprintf(stderr, "SaveData did not initialize in busy check\n");
        std::abort();
    }

    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "term_busy_dir", sizeof("term_busy_dir"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    if (sceSaveDataMount3(&mount, &mountResult) != SaveDataOk) {
        std::fprintf(stderr, "sceSaveDataMount3 failed\n");
        std::abort();
    }

    if (sceSaveDataTerminate() != SaveDataErrorBusy) {
        std::fprintf(stderr, "sceSaveDataTerminate with active mount did not return busy\n");
        std::abort();
    }

    if (sceSaveDataUmount2(0, &mountResult.mount_point) != SaveDataOk) {
        std::fprintf(stderr, "sceSaveDataUmount2 failed\n");
        std::abort();
    }

    SaveDataDelete delReq{};
    delReq.dir_name = &dirName;
    sceSaveDataDelete(&delReq);

    if (sceSaveDataTerminate() != SaveDataOk) {
        std::fprintf(stderr, "sceSaveDataTerminate after unmount failed\n");
        std::abort();
    }

    if (sceSaveDataTerminate() != SaveDataErrorNotInitialized) {
        std::fprintf(stderr, "sceSaveDataTerminate extra call did not return not initialized\n");
        std::abort();
    }

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
}

} // namespace

int main() {
    for (const std::uint32_t argument : {0u, 1u, 2u, 0xffffffffu}) {
        if (sceSaveDataCancel(argument) != SaveDataErrorNotInitialized) {
            std::fprintf(stderr, "sceSaveDataCancel(%#x) before initialization did not report not initialized\n", argument);
            return 1;
        }
    }
    if (sceSaveDataInitialize3(nullptr) != 0) {
        std::fprintf(stderr, "SaveData did not initialize\n");
        return 1;
    }
    if (!ReportsSuccess(0)) return 1;

    std::uint32_t dummyConfig = 0xaabbccdd;
    if (sceSaveDataInitialize2(&dummyConfig) != 0) {
        std::fprintf(stderr, "SaveData initialize2 with config failed\n");
        return 1;
    }

    if (sceSaveDataInitialize3(nullptr) != 0) {
        std::fprintf(stderr, "SaveData did not initialize nested session\n");
        return 1;
    }
    if (!ReportsSuccess(1)) return 1;

    if (sceSaveDataTerminate() != 0) {
        std::fprintf(stderr, "SaveData nested session did not terminate\n");
        return 1;
    }
    if (sceSaveDataTerminate() != 0) {
        std::fprintf(stderr, "SaveData second session did not terminate\n");
        return 1;
    }
    if (!ReportsSuccess(2)) return 1;

    if (sceSaveDataTerminate() != 0) {
        std::fprintf(stderr, "SaveData did not terminate\n");
        return 1;
    }
    for (const std::uint32_t argument : {0u, 1u, 2u, 0x1000u, 0xffffffffu}) {
        if (sceSaveDataCancel(argument) != SaveDataErrorNotInitialized) {
            std::fprintf(stderr, "sceSaveDataCancel(%#x) after final termination did not report not initialized\n", argument);
            return 1;
        }
    }

    TestActiveMountTerminationBusyCheck();

    return 0;
}
