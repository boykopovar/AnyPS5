#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "GuestSaveDataFixture.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" {
int APS5_VABI sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond*, SaveDataDirNameSearchResult*);
int APS5_VABI sceSaveDataMount3(const SaveDataMount3*, SaveDataMountResult*);
int APS5_VABI sceSaveDataUmount2(std::uint32_t, const SaveDataMountPoint*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

const std::array names{std::string(31, 'A'), std::string(31, 'B')};

class SearchFixture {
public:
    SearchFixture() {
        for (const auto& name : names) std::filesystem::create_directories(directory.Path() / "_sd" / name);
        buffer.fill('!');
        cond.user_id = 1;
        result.dir_names = reinterpret_cast<SceSaveDataDirName*>(buffer.data());
        result.dir_names_num = 2;
        RequireEqual(sceSaveDataDirNameSearch(&cond, &result), 0, "search");
    }

    const char* Record(int index) const {
        return buffer.data() + index * 32;
    }

    const SaveDataWorkingDirectory directory;
    alignas(SceSaveDataDirName) std::array<char, 80> buffer;
    SaveDataDirNameSearchCond cond{};
    SaveDataDirNameSearchResult result{};
};

const Case searchCount{"DirNameSearch_TwoMaxLengthNames_ReturnsBoth", [] {
    const SearchFixture fixture;
    RequireEqual(fixture.result.hit_num, 2u, "hits");
    RequireEqual(fixture.result.set_num, 2u, "set entries");
}};

const Case searchRecords{"DirNameSearch_TwoMaxLengthNames_WritesTerminatedRecordsWithoutOverrun", [] {
    const SearchFixture fixture;
    for (std::size_t i = 64; i < fixture.buffer.size(); ++i) {
        RequireEqual(fixture.buffer[i], '!', "byte " + std::to_string(i) + " past two 32-byte records");
    }
    std::array<bool, 2> found{};
    for (int i = 0; i < 2; ++i) {
        const char* record = fixture.Record(i);
        RequireEqual(record[31], '\0', "record " + std::to_string(i) + " terminates within 32 bytes");
        const bool first = std::strcmp(record, names[0].c_str()) == 0;
        Require(first || std::strcmp(record, names[1].c_str()) == 0, "record " + std::to_string(i) + " is one of the saves");
        const int index = first ? 0 : 1;
        Require(!found[index], "record " + std::to_string(i) + " is not a duplicate");
        found[index] = true;
    }
}};

const Case searchMountable{"DirNameSearch_ReturnedRecords_AreMountable", [] {
    const SearchFixture fixture;
    for (int i = 0; i < 2; ++i) {
        SaveDataMount3 mount{};
        mount.user_id = fixture.cond.user_id;
        mount.dir_name = reinterpret_cast<const SceSaveDataDirName*>(fixture.Record(i));
        mount.mount_mode = 1;
        SaveDataMountResult mounted{};
        RequireEqual(sceSaveDataMount3(&mount, &mounted), 0, "mount record " + std::to_string(i));
        RequireEqual(sceSaveDataUmount2(0, &mounted.mount_point), 0, "unmount record " + std::to_string(i));
    }
}};

} // namespace
