#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

namespace {

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotInitialized = -2137063423;
constexpr int SaveDataErrorNotMounted = -2137063420;

constexpr std::uint32_t ParamTypeAll = 0;
constexpr std::uint32_t ParamTypeTitle = 1;
constexpr std::uint32_t ParamTypeSubTitle = 2;
constexpr std::uint32_t ParamTypeDetail = 3;
constexpr std::uint32_t ParamTypeUserParam = 4;
constexpr std::uint32_t ParamTypeMtime = 5;

constexpr std::uint32_t MountModeRdWr = 2;
constexpr std::uint32_t MountModeCreate2 = 32;

#define Require(expr) \
    do { \
        if (!(expr)) { \
            std::fprintf(stderr, "%s:%d: Require failed: %s\n", __FILE__, __LINE__, #expr); \
            std::abort(); \
        } \
    } while (0)

void TestParamBoundsAndRoundtrip() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "param_bounds_save", sizeof("param_bounds_save"));

    SaveDataMount3 mount{};
    mount.user_id = 1;
    mount.dir_name = &dirName;
    mount.mount_mode = MountModeRdWr | MountModeCreate2;

    SaveDataMountResult mountResult{};
    Require(sceSaveDataMount3(&mount, &mountResult) == SaveDataOk);

    char buf[512]{};
    std::size_t gotSize = 0;

    for (const std::uint32_t invalidType : {6u, 7u, 10u, 100u, 0xFFFFFFFFu}) {
        Require(sceSaveDataGetParam(&mountResult.mount_point, invalidType, buf, sizeof(buf), &gotSize) == SaveDataErrorParameter);
        Require(sceSaveDataSetParam(&mountResult.mount_point, invalidType, buf, sizeof(buf)) == SaveDataErrorParameter);
    }

    std::uint32_t userParam = 0x12345678u;
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeUserParam, &userParam, 0) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeUserParam, &userParam, sizeof(userParam) - 1) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeUserParam, buf, sizeof(userParam) - 1, &gotSize) == SaveDataErrorParameter);

    std::int64_t mtimeVal = 1700000000LL;
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeMtime, &mtimeVal, sizeof(mtimeVal) - 1) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeMtime, buf, sizeof(mtimeVal) - 1, &gotSize) == SaveDataErrorParameter);

    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeAll, buf, sizeof(SaveDataParam) - 1) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeAll, buf, sizeof(SaveDataParam) - 1, &gotSize) == SaveDataErrorParameter);

    const char titleText[] = "Sample Save Title";
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeTitle, titleText, sizeof(titleText)) == SaveDataOk);

    char readTitle[128]{};
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeTitle, readTitle, sizeof(readTitle), &gotSize) == SaveDataOk);
    Require(gotSize == 128);
    Require(std::strcmp(readTitle, titleText) == 0);

    const char subTitleText[] = "Sample Sub Title";
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeSubTitle, subTitleText, sizeof(subTitleText)) == SaveDataOk);
    char readSubTitle[128]{};
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeSubTitle, readSubTitle, sizeof(readSubTitle), &gotSize) == SaveDataOk);
    Require(std::strcmp(readSubTitle, subTitleText) == 0);

    const char detailText[] = "Detailed progress summary in save container.";
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeDetail, detailText, sizeof(detailText)) == SaveDataOk);
    char readDetail[1024]{};
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeDetail, readDetail, sizeof(readDetail), &gotSize) == SaveDataOk);
    Require(std::strcmp(readDetail, detailText) == 0);

    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeUserParam, &userParam, sizeof(userParam)) == SaveDataOk);
    std::uint32_t readUserParam = 0;
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeUserParam, &readUserParam, sizeof(readUserParam), &gotSize) == SaveDataOk);
    Require(gotSize == sizeof(userParam));
    Require(readUserParam == userParam);

    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeMtime, &mtimeVal, sizeof(mtimeVal)) == SaveDataOk);
    std::int64_t readMtime = 0;
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeMtime, &readMtime, sizeof(readMtime), &gotSize) == SaveDataOk);
    Require(gotSize == sizeof(mtimeVal));
    Require(readMtime == mtimeVal);

    SaveDataParam allParam{};
    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeAll, &allParam, sizeof(allParam), &gotSize) == SaveDataOk);
    Require(gotSize == sizeof(SaveDataParam));
    Require(std::strcmp(allParam.title, titleText) == 0);
    Require(std::strcmp(allParam.sub_title, subTitleText) == 0);
    Require(allParam.user_param == userParam);
    Require(allParam.mtime == mtimeVal);

    Require(sceSaveDataUmount2(0, &mountResult.mount_point) == SaveDataOk);

    Require(sceSaveDataGetParam(&mountResult.mount_point, ParamTypeTitle, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted);
    Require(sceSaveDataSetParam(&mountResult.mount_point, ParamTypeTitle, buf, sizeof(buf)) == SaveDataErrorNotMounted);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-param-icon-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(sandbox);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(sandbox);

    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestParamBoundsAndRoundtrip();
    Require(sceSaveDataTerminate() == SaveDataOk);
    Require(sceSaveDataTerminate() == SaveDataErrorNotInitialized);

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
