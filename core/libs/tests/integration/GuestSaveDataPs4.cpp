#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceSaveDataTransferringMountPs4(const SaveDataTransferringMount*, SaveDataMountResult*);
extern "C" int APS5_VABI sceSaveDataDirNameSearchPs4(const SaveDataDirNameSearchCond*, SaveDataDirNameSearchResult*);
extern "C" int APS5_VABI sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond*, SaveDataDirNameSearchResult*);

namespace {

constexpr int SaveDataErrorNotFound = -2137063416;

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata ps4 check failed: %s\n", what.c_str());
    ++failures;
}

template <typename TFunction>
bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestTransferringMountPs4() {
    SceSaveDataTitleId title{};
    std::strcpy(title.data, "CUSA00001");
    SceSaveDataDirName dir{};
    std::strcpy(dir.data, "kept");
    SaveDataTransferringMount mount{};
    mount.user_id = 1;
    mount.title_id = &title;
    mount.dir_name = &dir;
    SaveDataMountResult result{};
    std::memset(&result, 0xAA, sizeof(result));
    Check(sceSaveDataTransferringMountPs4(&mount, &result) == SaveDataErrorNotFound, "TransferringMountPs4 returns NOT_FOUND");
    Check(result.mount_point.data[0] == '\0', "TransferringMountPs4 reports no mount point");
}

void TestDirNameSearchPs4() {
    SceSaveDataDirName names[4]{};
    std::memset(names, 0x5A, sizeof(names));
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    SaveDataDirNameSearchResult result{};
    result.hit_num = 7;
    result.set_num = 7;
    result.dir_names = names;
    result.dir_names_num = 4;

    Check(sceSaveDataDirNameSearch(&cond, &result) == 0 && result.hit_num == 1 && result.set_num == 1, "the PS5 search finds the PS5 save");

    std::memset(names, 0x5A, sizeof(names));
    result.hit_num = 7;
    result.set_num = 7;
    Check(sceSaveDataDirNameSearchPs4(&cond, &result) == 0, "DirNameSearchPs4 succeeds");
    Check(result.hit_num == 0, "DirNameSearchPs4 writes zero hits");
    Check(result.set_num == 0, "DirNameSearchPs4 writes zero set entries");
    Check(static_cast<unsigned char>(names[0].data[0]) == 0x5A, "DirNameSearchPs4 leaves the name buffer untouched");

    Check(ThrowsRuntimeError([&] { sceSaveDataDirNameSearchPs4(nullptr, &result); }), "null cond throws");
    Check(ThrowsRuntimeError([&] { sceSaveDataDirNameSearchPs4(&cond, nullptr); }), "null result throws");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-ps4-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root / "_sd" / "kept");
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);

    TestTransferringMountPs4();
    TestDirNameSearchPs4();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (failures != 0) return 1;
    std::printf("savedata ps4 tests passed\n");
    return 0;
}
