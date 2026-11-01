#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/SaveDataHostSpace.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNoSpaceFs = -2137063414;
constexpr std::uint64_t BlockBytes = 65536;

int failures = 0;

std::uint64_t fakeAvailable = 0;
int fakeCalls = 0;
bool fakeFails = false;

std::uint64_t FakeFreeSpace(const std::string&) {
    ++fakeCalls;
    if (fakeFails) throw std::runtime_error("fake volume query failed");
    return fakeAvailable;
}

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "savedata host space check failed: %s\n", what.c_str());
    ++failures;
}

void SetFreeSpaceVariable(const char* value) {
#ifdef _WIN32
    _putenv_s("ANYPS5_FREE_SPACE", value != nullptr ? value : "");
#else
    if (value == nullptr) {
        unsetenv("ANYPS5_FREE_SPACE");
    } else {
        setenv("ANYPS5_FREE_SPACE", value, 1);
    }
#endif
}

int Mount(const char* name, std::uint32_t mode, std::uint64_t blocks, SaveDataMountResult* result) {
    SceSaveDataDirName dirName{};
    std::snprintf(dirName.data, sizeof(dirName.data), "%s", name);
    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = mode;
    mount.blocks = blocks;
    return sceSaveDataMount3(&mount, result);
}

void CheckValidValuesMount() {
    const char* values[] = {nullptr, "fixed", "host"};
    int index = 0;
    for (const char* value : values) {
        SetFreeSpaceVariable(value);
        const std::string name = "HS_VALID_" + std::to_string(index++);
        SaveDataMountResult result{};
        const int code = Mount(name.c_str(), MountReadWrite | MountCreate2, 96, &result);
        Check(code == 0, "a valid ANYPS5_FREE_SPACE value lets a save be created (" + name + ")");
        if (code == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the created save unmounts (" + name + ")");
    }
    SetFreeSpaceVariable(nullptr);
}

void CheckInvalidValueStopsMount() {
    SetFreeSpaceVariable("bogus");
    SaveDataMountResult result{};
    std::string message;
    try {
        Mount("HS_BOGUS", MountReadWrite | MountCreate2, 96, &result);
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    Check(message.find("ANYPS5_FREE_SPACE") != std::string::npos, "an invalid ANYPS5_FREE_SPACE throws an error naming the variable");
    Check(!std::filesystem::exists("_sd/HS_BOGUS"), "an invalid ANYPS5_FREE_SPACE stops the mount before a save is created");

    message.clear();
    try {
        Mount("HS_BOGUS", MountReadWrite, 0, &result);
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    Check(message.find("ANYPS5_FREE_SPACE") != std::string::npos, "an open mount with an invalid ANYPS5_FREE_SPACE also throws");
    SetFreeSpaceVariable(nullptr);
}

#ifndef _WIN32
void CheckEmptyValueStopsMount() {
    SetFreeSpaceVariable("");
    SaveDataMountResult result{};
    std::string message;
    try {
        Mount("HS_EMPTY", MountReadWrite | MountCreate2, 96, &result);
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    Check(message.find("ANYPS5_FREE_SPACE") != std::string::npos, "an empty ANYPS5_FREE_SPACE throws an error naming the variable");
    Check(!std::filesystem::exists("_sd/HS_EMPTY"), "an empty ANYPS5_FREE_SPACE stops the mount before a save is created");
    SetFreeSpaceVariable(nullptr);
}
#endif

void CheckHostRoom() {
    SetFreeSpaceVariable("host");
    SetSaveDataFreeSpaceSourceForTest_nid_no_patch(FakeFreeSpace);
    SaveDataMountResult result{};

    fakeAvailable = 0;
    fakeCalls = 0;
    result = SaveDataMountResult{};
    int code = Mount("HS_FULL", MountReadWrite | MountCreate2, 96, &result);
    Check(code == SaveDataErrorNoSpaceFs, "a save the full volume cannot hold is NO_SPACE_FS");
    Check(result.required_blocks == 96, "a full volume reports every requested block as required");
    Check(!std::filesystem::exists("_sd/HS_FULL"), "a refused save creates no directory");
    Check(fakeCalls == 1, "the volume is queried once per create");

    fakeAvailable = 96 * BlockBytes - 1;
    result = SaveDataMountResult{};
    code = Mount("HS_TINY", MountReadWrite | MountCreate2, 96, &result);
    Check(code == SaveDataErrorNoSpaceFs && result.required_blocks == 1, "one byte short reports one block required");
    Check(!std::filesystem::exists("_sd/HS_TINY"), "a one-byte-short save creates no directory");

    fakeAvailable = 96 * BlockBytes;
    code = Mount("HS_EXACT", MountReadWrite | MountCreate2, 96, &result);
    Check(code == 0, "a volume with exactly the requested room creates the save");
    if (code == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the exact-room save unmounts");

    fakeAvailable = UINT64_MAX;
    code = Mount("HS_HUGE", MountReadWrite | MountCreate2, 0, &result);
    Check(code == 0, "a volume with unbounded room creates a default-size save");
    if (code == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the huge-room save unmounts");

    fakeAvailable = 16384 * BlockBytes - 1;
    result = SaveDataMountResult{};
    code = Mount("HS_DEFAULT", MountReadWrite | MountCreate2, 0, &result);
    Check(code == SaveDataErrorNoSpaceFs && result.required_blocks == 1, "a default-size save needs the full 16384 blocks");

    fakeFails = true;
    code = Mount("HS_ERROR", MountReadWrite | MountCreate2, 96, &result);
    Check(code == 0, "a failed volume query keeps today's behaviour and creates the save");
    if (code == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the save created after a failed query unmounts");
    fakeFails = false;

    fakeAvailable = UINT64_MAX;
    code = Mount("HS_OPEN", MountReadWrite | MountCreate2, 96, &result);
    Check(code == 0, "the save to reopen is created while the volume has room");
    if (code == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the save to reopen unmounts");
    fakeAvailable = 0;
    code = Mount("HS_OPEN", MountReadWrite, 0, &result);
    Check(code == 0, "a full volume still opens an existing save");
    if (code == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the opened save unmounts");

    SetFreeSpaceVariable(nullptr);
    fakeAvailable = 0;
    fakeCalls = 0;
    code = Mount("HS_FIXED", MountReadWrite | MountCreate2, 96, &result);
    Check(code == 0, "the default fixed mode creates a save on a full volume");
    Check(fakeCalls == 0, "the default fixed mode never queries the volume");
    if (code == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the fixed-mode save unmounts");

    SetSaveDataFreeSpaceSourceForTest_nid_no_patch(nullptr);
}

std::uint64_t MountInfoFreeBlocks(const char* name, std::uint64_t* blocks) {
    SceSaveDataDirName dirName{};
    std::snprintf(dirName.data, sizeof(dirName.data), "%s", name);
    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadWrite;
    SaveDataMountResult result{};
    Check(sceSaveDataMount3(&mount, &result) == 0, std::string("open for mount info: ") + name);
    SaveDataMountInfo info{};
    Check(sceSaveDataGetMountInfo(&result.mount_point, &info) == 0, std::string("mount info of ") + name);
    Check(sceSaveDataUmount2(0, &result.mount_point) == 0, std::string("unmount after mount info: ") + name);
    *blocks = info.blocks;
    return info.free_blocks;
}

std::uint64_t SearchFreeBlocks(const char* pattern, std::uint64_t* blocks) {
    SceSaveDataDirName patternName{};
    std::snprintf(patternName.data, sizeof(patternName.data), "%s", pattern);
    SaveDataDirNameSearchCond cond{};
    cond.user_id = 1;
    cond.dir_name = &patternName;
    cond.key = 5;
    cond.order = 0;
    SceSaveDataDirName names[1]{};
    SaveDataSearchInfo infos[1]{};
    SaveDataDirNameSearchResult result{};
    result.dir_names = names;
    result.dir_names_num = 1;
    result.infos = infos;
    Check(sceSaveDataDirNameSearch(&cond, &result) == 0, "free-blocks search succeeds");
    Check(result.set_num == 1, "free-blocks search finds the save");
    *blocks = infos[0].blocks;
    return infos[0].free_blocks;
}

void CheckHostFreeBlocks() {
    SetFreeSpaceVariable("host");
    SetSaveDataFreeSpaceSourceForTest_nid_no_patch(FakeFreeSpace);
    SaveDataMountResult result{};
    fakeAvailable = UINT64_MAX;
    const int created = Mount("HS_INFO", MountReadWrite | MountCreate2, 200, &result);
    Check(created == 0, "a save with 200 blocks is created");
    if (created == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the new save unmounts");
    {
        std::ofstream data("_sd/HS_INFO/data.bin", std::ios::binary);
        const std::string bytes(40000, 'x');
        data.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    std::uint64_t blocks = 0;
    fakeAvailable = UINT64_MAX;
    Check(MountInfoFreeBlocks("HS_INFO", &blocks) == 199 && blocks == 200, "an unbounded volume reports the save's own free blocks");
    fakeAvailable = 10 * BlockBytes + 5;
    Check(MountInfoFreeBlocks("HS_INFO", &blocks) == 10 && blocks == 200, "mount info is bounded by the blocks the volume has");
    Check(SearchFreeBlocks("HS_INF%", &blocks) == 10 && blocks == 200, "the search reports the same bound");
    fakeAvailable = 0;
    Check(MountInfoFreeBlocks("HS_INFO", &blocks) == 0, "a full volume reports no free blocks");
    fakeFails = true;
    Check(MountInfoFreeBlocks("HS_INFO", &blocks) == 199, "a failed volume query reports the save's own free blocks");
    fakeFails = false;

    SetFreeSpaceVariable(nullptr);
    fakeAvailable = 0;
    fakeCalls = 0;
    Check(MountInfoFreeBlocks("HS_INFO", &blocks) == 199, "the default fixed mode reports the save's own free blocks");
    Check(fakeCalls == 0, "the default fixed mode never bounds free blocks by the volume");
    SetSaveDataFreeSpaceSourceForTest_nid_no_patch(nullptr);
}

void CheckDefaultVolumeQuery() {
    SetFreeSpaceVariable("host");
    SetSaveDataFreeSpaceSourceForTest_nid_no_patch(nullptr);
    SaveDataMountResult result{};
    const int created = Mount("HS_REAL", MountReadWrite | MountCreate2, 1, &result);
    Check(created == 0, "a 1-block save is created with the default volume query");
    if (created == 0) Check(sceSaveDataUmount2(0, &result.mount_point) == 0, "the default-query save unmounts");

    std::uint64_t blocks = 0;
    Check(MountInfoFreeBlocks("HS_REAL", &blocks) > 0, "the default volume query reports room for a 1-block save");
    SetFreeSpaceVariable(nullptr);
}

}  // namespace

int main() {
    const auto previous = std::filesystem::current_path();
    std::mt19937_64 random(std::random_device{}());
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-savedata-host-space-" + std::to_string(random()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    sceSaveDataInitialize3(nullptr);

    CheckValidValuesMount();
    CheckInvalidValueStopsMount();
    CheckHostRoom();
    CheckHostFreeBlocks();
    CheckDefaultVolumeQuery();
#ifndef _WIN32
    CheckEmptyValueStopsMount();
#endif

    sceSaveDataTerminate();
    std::filesystem::current_path(previous);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (failures != 0) {
        std::fprintf(stderr, "%d savedata host space check(s) failed\n", failures);
        return 1;
    }
    std::printf("savedata host space tests passed\n");
    return 0;
}
