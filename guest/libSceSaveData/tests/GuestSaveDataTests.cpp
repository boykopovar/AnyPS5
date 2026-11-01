#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotMounted = -2137063420;

struct SceSaveDataTitleId {
    char data[10];
    char pad[6];
};

struct SceSaveDataDirName {
    char data[32];
};

struct SaveDataDirNameSearchCond {
    std::int32_t user_id;
    std::int32_t pad;
    const SceSaveDataTitleId* title_id;
    const SceSaveDataDirName* dir_name;
    std::uint32_t key;
    std::uint32_t order;
    std::uint8_t reserved[32];
};

struct SaveDataParam {
    char title[128];
    char sub_title[128];
    char detail[1024];
    std::uint32_t user_param;
    std::int32_t pad;
    std::int64_t mtime;
    std::uint8_t reserved[32];
};

struct SaveDataSearchInfo {
    std::uint64_t blocks;
    std::uint64_t free_blocks;
    std::uint8_t reserved[32];
};

struct SaveDataDirNameSearchResult {
    std::uint32_t hit_num;
    std::int32_t pad;
    SceSaveDataDirName* dir_names;
    std::uint32_t dir_names_num;
    std::uint32_t set_num;
    SaveDataParam* params;
    SaveDataSearchInfo* infos;
    std::uint8_t reserved[12];
    std::int32_t pad2;
};

struct SaveDataMountPoint {
    char data[16];
};

extern "C" {
int sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result);
int sceSaveDataGetParam(const SaveDataMountPoint* mountPoint, std::uint32_t paramType, void* paramBuf, std::size_t paramBufSize, std::size_t* gotSize);
}

int failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", what.c_str());
    ++failures;
}

void CheckSaveDataDirNameSearchBounds() {
    SaveDataDirNameSearchResult result{};
    SaveDataDirNameSearchCond cond{};

    Check(sceSaveDataDirNameSearch(nullptr, &result) == SaveDataErrorParameter, "dirNameSearch null cond returns PARAMETER");
    Check(sceSaveDataDirNameSearch(&cond, nullptr) == SaveDataErrorParameter, "dirNameSearch null result returns PARAMETER");
    Check(sceSaveDataDirNameSearch(nullptr, nullptr) == SaveDataErrorParameter, "dirNameSearch all null returns PARAMETER");

    cond.key = 9999;
    Check(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter, "dirNameSearch invalid key returns PARAMETER");

    cond.key = 0;
    cond.order = 9999;
    Check(sceSaveDataDirNameSearch(&cond, &result) == SaveDataErrorParameter, "dirNameSearch invalid order returns PARAMETER");
}

void CheckSaveDataGetParamBounds() {
    char buf[128]{};
    std::size_t gotSize = 0;
    SaveDataMountPoint point{};
    std::memcpy(point.data, "test_mount", sizeof("test_mount"));

    Check(sceSaveDataGetParam(nullptr, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorParameter, "getParam null point returns PARAMETER");
    Check(sceSaveDataGetParam(&point, 1, nullptr, sizeof(buf), &gotSize) == SaveDataErrorParameter, "getParam null buf returns PARAMETER");
    Check(sceSaveDataGetParam(nullptr, 1, nullptr, sizeof(buf), &gotSize) == SaveDataErrorParameter, "getParam all null returns PARAMETER");

    SaveDataMountPoint zeroPoint{};
    Check(sceSaveDataGetParam(&zeroPoint, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted, "getParam zeroed point returns NOT_MOUNTED");

    Check(sceSaveDataGetParam(&point, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted, "getParam unmounted point returns NOT_MOUNTED");

    SaveDataMountPoint invalidSlot{};
    std::memcpy(invalidSlot.data, "/savedata99", sizeof("/savedata99"));
    Check(sceSaveDataGetParam(&invalidSlot, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted, "getParam invalid slot returns NOT_MOUNTED");

    SaveDataMountPoint overflowSlot{};
    std::memcpy(overflowSlot.data, "invalid_handle", sizeof("invalid_handle"));
    Check(sceSaveDataGetParam(&overflowSlot, 1, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted, "getParam invalid handle returns NOT_MOUNTED");
}

}

int main() {
    CheckSaveDataDirNameSearchBounds();
    CheckSaveDataGetParamBounds();

    if (failures != 0) {
        return 1;
    }
    std::printf("all savedata search and param negative bounds tests passed\n");
    return 0;
}
