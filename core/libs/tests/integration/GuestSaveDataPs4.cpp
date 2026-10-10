#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "GuestSaveDataFixture.hpp"

#include <Testing/Test.hpp>

#include <cstring>
#include <filesystem>
#include <stdexcept>

extern "C" int APS5_VABI sceSaveDataTransferringMountPs4(const SaveDataTransferringMount*, SaveDataMountResult*);
extern "C" int APS5_VABI sceSaveDataDirNameSearchPs4(const SaveDataDirNameSearchCond*, SaveDataDirNameSearchResult*);
extern "C" int APS5_VABI sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond*, SaveDataDirNameSearchResult*);

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int SaveDataErrorNotFound = -2137063416;

class Ps5SaveRoot {
public:
    Ps5SaveRoot() {
        std::filesystem::create_directories(directory.Path() / "_sd" / "kept");
    }

    const SaveDataWorkingDirectory directory;
};

class SearchBuffers {
public:
    SearchBuffers() {
        std::memset(names, 0x5A, sizeof(names));
        cond.user_id = 1;
        result.hit_num = 7;
        result.set_num = 7;
        result.dir_names = names;
        result.dir_names_num = 4;
    }

    SceSaveDataDirName names[4]{};
    SaveDataDirNameSearchCond cond{};
    SaveDataDirNameSearchResult result{};
};

const Case transferringMount{"TransferringMountPs4_AnyTitle_FailsNotFoundWithoutMountPoint", [] {
    const Ps5SaveRoot root;
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
    RequireEqual(sceSaveDataTransferringMountPs4(&mount, &result), SaveDataErrorNotFound, "TransferringMountPs4 returns NOT_FOUND");
    RequireEqual(result.mount_point.data[0], '\0', "TransferringMountPs4 reports no mount point");
}};

const Case ps5Search{"DirNameSearch_Ps5Save_FindsIt", [] {
    const Ps5SaveRoot root;
    SearchBuffers search;
    RequireEqual(sceSaveDataDirNameSearch(&search.cond, &search.result), 0, "the PS5 search succeeds");
    RequireEqual(search.result.hit_num, 1u, "PS5 hits");
    RequireEqual(search.result.set_num, 1u, "PS5 set entries");
}};

const Case ps4Search{"DirNameSearchPs4_Ps5SaveOnly_ReportsNoHitsAndLeavesNames", [] {
    const Ps5SaveRoot root;
    SearchBuffers search;
    RequireEqual(sceSaveDataDirNameSearchPs4(&search.cond, &search.result), 0, "DirNameSearchPs4 succeeds");
    RequireEqual(search.result.hit_num, 0u, "DirNameSearchPs4 writes zero hits");
    RequireEqual(search.result.set_num, 0u, "DirNameSearchPs4 writes zero set entries");
    RequireEqual(static_cast<unsigned char>(search.names[0].data[0]), 0x5A, "DirNameSearchPs4 leaves the name buffer untouched");
}};

const Case ps4SearchNull{"DirNameSearchPs4_NullCondOrResult_Throws", [] {
    const Ps5SaveRoot root;
    SearchBuffers search;
    RequireThrows<std::runtime_error>([&] { sceSaveDataDirNameSearchPs4(nullptr, &search.result); }, "null cond");
    RequireThrows<std::runtime_error>([&] { sceSaveDataDirNameSearchPs4(&search.cond, nullptr); }, "null result");
}};

} // namespace
