#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorAlreadyInitialized = -2137063419;
constexpr int SaveDataErrorMountFull = -2137063412;
constexpr int SaveDataErrorInvalidLoginUser = -2137063407;
constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "assertion failed: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireThrows(TCall call, const std::string& expectedMessage) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == expectedMessage, "unexpected error message");
        return;
    }
    Require(false, "call returned instead of reporting that it is not implemented");
}

void TestUnimplementedQueryExports() {
    RequireThrows([] { static_cast<void>(sceSaveDataGetSaveDataCount()); }, "sceSaveDataGetSaveDataCount not implemented");
    RequireThrows([] { static_cast<void>(sceSaveDataGetAllSize()); }, "sceSaveDataGetAllSize not implemented");
    RequireThrows([] { static_cast<void>(sceSaveDataGetAllSizeProspero()); }, "sceSaveDataGetAllSizeProspero not implemented");
    RequireThrows([] { static_cast<void>(sceSaveDataGetMountedSaveDataCount(0, nullptr)); }, "sceSaveDataGetMountedSaveDataCount not implemented");
    std::uint32_t count = 0;
    RequireThrows([&] { static_cast<void>(sceSaveDataGetMountedSaveDataCount(0, &count)); }, "sceSaveDataGetMountedSaveDataCount not implemented");
    RequireThrows([&] { static_cast<void>(sceSaveDataGetMountedSaveDataCount(1, &count)); }, "sceSaveDataGetMountedSaveDataCount not implemented");
    RequireThrows([&] { static_cast<void>(sceSaveDataGetMountedSaveDataCount(-1, &count)); }, "sceSaveDataGetMountedSaveDataCount not implemented");
}

void TestQueryNullAndParameterBounds() {
    SaveDataDirNameSearchResult searchResult{};
    SaveDataDirNameSearchCond searchCond{};
    Require(sceSaveDataDirNameSearch(nullptr, nullptr) == SaveDataErrorParameter, "dirNameSearch null null");
    Require(sceSaveDataDirNameSearch(&searchCond, nullptr) == SaveDataErrorParameter, "dirNameSearch cond null result");
    Require(sceSaveDataDirNameSearch(nullptr, &searchResult) == SaveDataErrorParameter, "dirNameSearch null cond result");

    Require(sceSaveDataDirNameSearch2(nullptr, nullptr) == SaveDataErrorParameter, "dirNameSearch2 null null");
    Require(sceSaveDataDirNameSearch2(&searchCond, nullptr) == SaveDataErrorParameter, "dirNameSearch2 cond null result");
    Require(sceSaveDataDirNameSearch2(nullptr, &searchResult) == SaveDataErrorParameter, "dirNameSearch2 null cond result");

    Require(sceSaveDataDirNameSearchPs4(nullptr, nullptr) == SaveDataErrorParameter, "dirNameSearchPs4 null null");
    Require(sceSaveDataDirNameSearchPs4(&searchCond, nullptr) == SaveDataErrorParameter, "dirNameSearchPs4 cond null result");
    Require(sceSaveDataDirNameSearchPs4(nullptr, &searchResult) == SaveDataErrorParameter, "dirNameSearchPs4 null cond result");

    SaveDataDirNameSearchCond invalidKeyCond{};
    invalidKeyCond.key = 4;
    Require(sceSaveDataDirNameSearch(&invalidKeyCond, &searchResult) == SaveDataErrorParameter, "dirNameSearch invalid key 4");
    invalidKeyCond.key = 6;
    Require(sceSaveDataDirNameSearch(&invalidKeyCond, &searchResult) == SaveDataErrorParameter, "dirNameSearch invalid key 6");
    invalidKeyCond.key = 0x80000000u;
    Require(sceSaveDataDirNameSearch(&invalidKeyCond, &searchResult) == SaveDataErrorParameter, "dirNameSearch invalid key high");

    SaveDataDirNameSearchCond invalidOrderCond{};
    invalidOrderCond.order = 2;
    Require(sceSaveDataDirNameSearch(&invalidOrderCond, &searchResult) == SaveDataErrorParameter, "dirNameSearch invalid order 2");
    invalidOrderCond.order = 0xffffffffu;
    Require(sceSaveDataDirNameSearch(&invalidOrderCond, &searchResult) == SaveDataErrorParameter, "dirNameSearch invalid order max");

    SceSaveDataDirName unterminatedDir{};
    std::memset(unterminatedDir.data, 'x', sizeof(unterminatedDir.data));
    SaveDataDirNameSearchCond unterminatedCond{};
    unterminatedCond.dir_name = &unterminatedDir;
    Require(sceSaveDataDirNameSearch(&unterminatedCond, &searchResult) == SaveDataErrorParameter, "dirNameSearch unterminated dir");
    Require(sceSaveDataDirNameSearch2(&unterminatedCond, &searchResult) == SaveDataErrorParameter, "dirNameSearch2 unterminated dir");
    Require(sceSaveDataDirNameSearchPs4(&unterminatedCond, &searchResult) == SaveDataErrorParameter, "dirNameSearchPs4 unterminated dir");
}

void TestQueryOutputPointerValidation() {
    SaveDataMountPoint point{};
    std::memcpy(point.data, "test_point", sizeof("test_point"));
    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(nullptr, nullptr) == SaveDataErrorParameter, "getMountInfo null null");
    Require(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter, "getMountInfo null point");
    Require(sceSaveDataGetMountInfo(&point, nullptr) == SaveDataErrorParameter, "getMountInfo null info");

    std::size_t gotSize = 0;
    char paramBuf[64]{};
    Require(sceSaveDataGetParam(nullptr, 0, nullptr, 0, nullptr) == SaveDataErrorParameter, "getParam null null");
    Require(sceSaveDataGetParam(&point, 0, nullptr, sizeof(paramBuf), &gotSize) == SaveDataErrorParameter, "getParam null buf");
    Require(sceSaveDataGetParam(&point, 0, paramBuf, sizeof(paramBuf), nullptr) == SaveDataErrorParameter, "getParam null gotSize");

    SaveDataIcon icon{};
    Require(sceSaveDataLoadIcon(nullptr, nullptr) == SaveDataErrorParameter, "loadIcon null null");
    Require(sceSaveDataLoadIcon(nullptr, &icon) == SaveDataErrorParameter, "loadIcon null point");
    Require(sceSaveDataLoadIcon(&point, nullptr) == SaveDataErrorParameter, "loadIcon null icon");
}

void TestUserIdValidation() {
    SaveDataMount2 mount2Param{};
    SaveDataMountResult mountResult{};
    mount2Param.user_id = 0;
    Require(sceSaveDataMount2(&mount2Param, &mountResult) == SaveDataErrorInvalidLoginUser, "mount2 user_id 0 rejected");
    mount2Param.user_id = -1;
    Require(sceSaveDataMount2(&mount2Param, &mountResult) == SaveDataErrorInvalidLoginUser, "mount2 user_id -1 rejected");
    mount2Param.user_id = 1;
    mount2Param.mount_mode = 0;
    Require(sceSaveDataMount2(&mount2Param, &mountResult) == SaveDataErrorParameter, "mount2 mode 0 rejected");
}

void TestContainerSlotLimits() {
    std::array<SaveDataMountResult, 16> mountedResults{};
    std::array<SceSaveDataDirName, 16> dirNames{};
    for (std::size_t i = 0; i < 16; ++i) {
        std::snprintf(dirNames[i].data, sizeof(dirNames[i].data), "slot_query_%zu", i);
        SaveDataMount3 mount{};
        mount.user_id = 1;
        mount.dir_name = &dirNames[i];
        mount.mount_mode = MountReadWrite | MountCreate2;
        mount.blocks = 96;
        Require(sceSaveDataMount3(&mount, &mountedResults[i]) == SaveDataOk, "mount within 16 slot capacity succeeds");
    }

    SceSaveDataDirName overflowDir{};
    std::snprintf(overflowDir.data, sizeof(overflowDir.data), "slot_query_overflow");
    SaveDataMount3 overflowMount{};
    overflowMount.user_id = 1;
    overflowMount.dir_name = &overflowDir;
    overflowMount.mount_mode = MountReadWrite | MountCreate2;
    overflowMount.blocks = 96;
    SaveDataMountResult overflowResult{};
    Require(sceSaveDataMount3(&overflowMount, &overflowResult) == SaveDataErrorMountFull, "17th mount exceeds 16 slot capacity");

    for (std::size_t i = 0; i < 16; ++i) {
        Require(sceSaveDataUmount2(0, &mountedResults[i].mount_point) == SaveDataOk, "unmount restores slot capacity");
    }
}

void TestLifecycleBounds() {
    Require(sceSaveDataInitialize(nullptr) == SaveDataErrorAlreadyInitialized, "double initialize rejected");
}

} // namespace

int main() {
    SaveDataMount2 preInitMount{};
    SaveDataMountResult preInitResult{};
    preInitMount.user_id = 1;
    Require(sceSaveDataMount2(&preInitMount, &preInitResult) == SaveDataErrorNotInitialized, "mount2 before init returns NOT_INITIALIZED");
    TestUnimplementedQueryExports();

    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk, "initialize3 succeeds");

    TestUnimplementedQueryExports();
    TestLifecycleBounds();
    TestUserIdValidation();
    TestQueryNullAndParameterBounds();
    TestQueryOutputPointerValidation();
    TestContainerSlotLimits();

    Require(sceSaveDataTerminate() == SaveDataOk, "terminate succeeds");

    Require(sceSaveDataMount2(&preInitMount, &preInitResult) == SaveDataErrorNotInitialized, "mount2 after terminate returns NOT_INITIALIZED");
    TestUnimplementedQueryExports();
    return 0;
}
