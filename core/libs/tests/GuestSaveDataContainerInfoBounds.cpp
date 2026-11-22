#include "prx/libSceSaveData/include/Api.hpp"
#include "prx/libSceSaveData.native/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

namespace {

constexpr std::uint32_t MountReadWrite = 2;
constexpr std::uint32_t MountCreate2 = 32;

constexpr int SaveDataOk = 0;
constexpr int SaveDataErrorParameter = -2137063424;
constexpr int SaveDataErrorNotMounted = -2137063420;

constexpr std::uint32_t ParamTypeAll = 0;
constexpr std::uint32_t ParamTypeTitle = 1;
constexpr std::uint32_t ParamTypeSubTitle = 2;
constexpr std::uint32_t ParamTypeDetail = 3;
constexpr std::uint32_t ParamTypeUserParam = 4;
constexpr std::uint32_t ParamTypeMtime = 5;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUninitializedBounds() {
    SaveDataMountPoint point{};
    std::memcpy(point.data, "uninit_cont", sizeof("uninit_cont"));
    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&point, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&point, nullptr) == SaveDataErrorParameter);

    char buf[1024]{};
    std::size_t gotSize = 0;
    Require(sceSaveDataGetParam(nullptr, ParamTypeTitle, buf, sizeof(buf), &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&point, ParamTypeTitle, nullptr, sizeof(buf), &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&point, ParamTypeTitle, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted);
    Require(sceSaveDataSetParam(nullptr, ParamTypeTitle, buf, sizeof(buf)) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&point, ParamTypeTitle, nullptr, sizeof(buf)) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&point, ParamTypeTitle, buf, sizeof(buf)) == SaveDataErrorNotMounted);
}

void TestUnmountedNullAndParameterBounds() {
    SaveDataMountPoint unterminated{};
    std::memset(unterminated.data, 'u', sizeof(unterminated.data));
    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(&unterminated, &info) == SaveDataErrorParameter);

    char buf[1024]{};
    std::size_t gotSize = 0;
    Require(sceSaveDataGetParam(&unterminated, ParamTypeTitle, buf, sizeof(buf), &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&unterminated, ParamTypeTitle, buf, sizeof(buf)) == SaveDataErrorParameter);

    SaveDataMountPoint validUnmounted{};
    std::memcpy(validUnmounted.data, "absent_slot", sizeof("absent_slot"));
    Require(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&validUnmounted, nullptr) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&validUnmounted, &info) == SaveDataErrorNotMounted);

    Require(sceSaveDataGetParam(nullptr, ParamTypeTitle, buf, sizeof(buf), &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&validUnmounted, ParamTypeTitle, nullptr, sizeof(buf), &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&validUnmounted, ParamTypeTitle, buf, sizeof(buf), &gotSize) == SaveDataErrorNotMounted);

    Require(sceSaveDataSetParam(nullptr, ParamTypeTitle, buf, sizeof(buf)) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&validUnmounted, ParamTypeTitle, nullptr, sizeof(buf)) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&validUnmounted, ParamTypeTitle, buf, sizeof(buf)) == SaveDataErrorNotMounted);
}

void TestMountedContainerInfoBounds() {
    SceSaveDataDirName dirName{};
    std::memcpy(dirName.data, "info_bounds", sizeof("info_bounds"));

    SaveDataMount3 mount{};
    mount.dir_name = &dirName;
    mount.mount_mode = MountReadWrite | MountCreate2;

    SaveDataMountResult result{};
    Require(sceSaveDataMount3(&mount, &result) == SaveDataOk);
    Require(result.mount_status == 1u);

    const SaveDataMountPoint mountPoint = result.mount_point;

    SaveDataMountInfo info{};
    Require(sceSaveDataGetMountInfo(nullptr, &info) == SaveDataErrorParameter);
    Require(sceSaveDataGetMountInfo(&mountPoint, nullptr) == SaveDataErrorParameter);

    SaveDataMountPoint unterminated{};
    std::memset(unterminated.data, 'z', sizeof(unterminated.data));
    Require(sceSaveDataGetMountInfo(&unterminated, &info) == SaveDataErrorParameter);

    std::memset(&info, 0xFF, sizeof(info));
    Require(sceSaveDataGetMountInfo(&mountPoint, &info) == SaveDataOk);
    Require(info.blocks >= 48);
    Require(info.free_blocks <= info.blocks);

    char buf[2048]{};
    std::size_t gotSize = 0;

    for (std::uint32_t invalidType : {6u, 7u, 100u, 0xFFFFFFFFu}) {
        Require(sceSaveDataGetParam(&mountPoint, invalidType, buf, sizeof(buf), &gotSize) == SaveDataErrorParameter);
        Require(sceSaveDataSetParam(&mountPoint, invalidType, buf, sizeof(buf)) == SaveDataErrorParameter);
    }

    SaveDataParam allParam{};
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeAll, &allParam, sizeof(SaveDataParam) - 1, &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeAll, &allParam, 0, &gotSize) == SaveDataErrorParameter);

    char titleBuf[128]{};
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeTitle, titleBuf, 127, &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeTitle, titleBuf, 0, &gotSize) == SaveDataErrorParameter);

    char subTitleBuf[128]{};
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeSubTitle, subTitleBuf, 127, &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeSubTitle, subTitleBuf, 0, &gotSize) == SaveDataErrorParameter);

    char detailBuf[1024]{};
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeDetail, detailBuf, 1023, &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeDetail, detailBuf, 0, &gotSize) == SaveDataErrorParameter);

    std::uint32_t userParam = 0;
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeUserParam, &userParam, 3, &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeUserParam, &userParam, 0, &gotSize) == SaveDataErrorParameter);

    std::int64_t mtime = 0;
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeMtime, &mtime, 7, &gotSize) == SaveDataErrorParameter);
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeMtime, &mtime, 0, &gotSize) == SaveDataErrorParameter);

    Require(sceSaveDataGetParam(&mountPoint, ParamTypeUserParam, &userParam, sizeof(userParam), nullptr) == SaveDataOk);
    gotSize = 0;
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeUserParam, &userParam, sizeof(userParam), &gotSize) == SaveDataOk);
    Require(gotSize == sizeof(userParam));

    for (std::uint32_t type : {ParamTypeAll, ParamTypeTitle, ParamTypeSubTitle, ParamTypeDetail, ParamTypeUserParam, ParamTypeMtime}) {
        Require(sceSaveDataSetParam(&mountPoint, type, buf, 0) == SaveDataErrorParameter);
    }

    Require(sceSaveDataSetParam(&mountPoint, ParamTypeAll, &allParam, sizeof(SaveDataParam) - 1) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&mountPoint, ParamTypeUserParam, &userParam, 3) == SaveDataErrorParameter);
    Require(sceSaveDataSetParam(&mountPoint, ParamTypeMtime, &mtime, 7) == SaveDataErrorParameter);

    const char expectedTitle[] = "Antigravity Save Title";
    Require(sceSaveDataSetParam(&mountPoint, ParamTypeTitle, expectedTitle, sizeof(expectedTitle)) == SaveDataOk);
    std::memset(titleBuf, 0, sizeof(titleBuf));
    gotSize = 0;
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeTitle, titleBuf, sizeof(titleBuf), &gotSize) == SaveDataOk);
    Require(gotSize == 128);
    Require(std::strcmp(titleBuf, expectedTitle) == 0);

    const char expectedSubTitle[] = "Container Info SubTitle";
    Require(sceSaveDataSetParam(&mountPoint, ParamTypeSubTitle, expectedSubTitle, sizeof(expectedSubTitle)) == SaveDataOk);
    std::memset(subTitleBuf, 0, sizeof(subTitleBuf));
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeSubTitle, subTitleBuf, sizeof(subTitleBuf), nullptr) == SaveDataOk);
    Require(std::strcmp(subTitleBuf, expectedSubTitle) == 0);

    const char expectedDetail[] = "Detailed container info bounds verification.";
    Require(sceSaveDataSetParam(&mountPoint, ParamTypeDetail, expectedDetail, sizeof(expectedDetail)) == SaveDataOk);
    std::memset(detailBuf, 0, sizeof(detailBuf));
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeDetail, detailBuf, sizeof(detailBuf), nullptr) == SaveDataOk);
    Require(std::strcmp(detailBuf, expectedDetail) == 0);

    const std::uint32_t expectedUserParam = 0xA1B2C3D4u;
    Require(sceSaveDataSetParam(&mountPoint, ParamTypeUserParam, &expectedUserParam, sizeof(expectedUserParam)) == SaveDataOk);
    userParam = 0;
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeUserParam, &userParam, sizeof(userParam), &gotSize) == SaveDataOk);
    Require(gotSize == sizeof(userParam));
    Require(userParam == expectedUserParam);

    const std::int64_t expectedMtime = 1700000000LL;
    Require(sceSaveDataSetParam(&mountPoint, ParamTypeMtime, &expectedMtime, sizeof(expectedMtime)) == SaveDataOk);
    mtime = 0;
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeMtime, &mtime, sizeof(mtime), &gotSize) == SaveDataOk);
    Require(gotSize == sizeof(mtime));
    Require(mtime == expectedMtime);

    std::memset(&allParam, 0, sizeof(allParam));
    gotSize = 0;
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeAll, &allParam, sizeof(allParam), &gotSize) == SaveDataOk);
    Require(gotSize == sizeof(allParam));
    Require(std::strcmp(allParam.title, expectedTitle) == 0);
    Require(std::strcmp(allParam.sub_title, expectedSubTitle) == 0);
    Require(std::strcmp(allParam.detail, expectedDetail) == 0);
    Require(allParam.user_param == expectedUserParam);
    Require(allParam.mtime == expectedMtime);

    Require(sceSaveDataUmount2(0, &mountPoint) == SaveDataOk);

    Require(sceSaveDataGetMountInfo(&mountPoint, &info) == SaveDataErrorNotMounted);
    Require(sceSaveDataGetParam(&mountPoint, ParamTypeTitle, titleBuf, sizeof(titleBuf), nullptr) == SaveDataErrorNotMounted);
    Require(sceSaveDataSetParam(&mountPoint, ParamTypeTitle, expectedTitle, sizeof(expectedTitle)) == SaveDataErrorNotMounted);
}

void TestMultipleContainersIsolation() {
    SceSaveDataDirName dirA{};
    std::memcpy(dirA.data, "info_cont_a", sizeof("info_cont_a"));
    SaveDataMount3 mountA{};
    mountA.dir_name = &dirA;
    mountA.mount_mode = MountReadWrite | MountCreate2;

    SaveDataMountResult resultA{};
    Require(sceSaveDataMount3(&mountA, &resultA) == SaveDataOk);

    SceSaveDataDirName dirB{};
    std::memcpy(dirB.data, "info_cont_b", sizeof("info_cont_b"));
    SaveDataMount3 mountB{};
    mountB.dir_name = &dirB;
    mountB.mount_mode = MountReadWrite | MountCreate2;

    SaveDataMountResult resultB{};
    Require(sceSaveDataMount3(&mountB, &resultB) == SaveDataOk);

    const char titleA[] = "Container Alpha";
    const char titleB[] = "Container Beta";
    const std::uint32_t userParamA = 0x11111111u;
    const std::uint32_t userParamB = 0x22222222u;

    Require(sceSaveDataSetParam(&resultA.mount_point, ParamTypeTitle, titleA, sizeof(titleA)) == SaveDataOk);
    Require(sceSaveDataSetParam(&resultB.mount_point, ParamTypeTitle, titleB, sizeof(titleB)) == SaveDataOk);
    Require(sceSaveDataSetParam(&resultA.mount_point, ParamTypeUserParam, &userParamA, sizeof(userParamA)) == SaveDataOk);
    Require(sceSaveDataSetParam(&resultB.mount_point, ParamTypeUserParam, &userParamB, sizeof(userParamB)) == SaveDataOk);

    char readTitleA[128]{};
    char readTitleB[128]{};
    std::uint32_t readUserParamA = 0;
    std::uint32_t readUserParamB = 0;

    Require(sceSaveDataGetParam(&resultA.mount_point, ParamTypeTitle, readTitleA, sizeof(readTitleA), nullptr) == SaveDataOk);
    Require(sceSaveDataGetParam(&resultB.mount_point, ParamTypeTitle, readTitleB, sizeof(readTitleB), nullptr) == SaveDataOk);
    Require(sceSaveDataGetParam(&resultA.mount_point, ParamTypeUserParam, &readUserParamA, sizeof(readUserParamA), nullptr) == SaveDataOk);
    Require(sceSaveDataGetParam(&resultB.mount_point, ParamTypeUserParam, &readUserParamB, sizeof(readUserParamB), nullptr) == SaveDataOk);

    Require(std::strcmp(readTitleA, titleA) == 0);
    Require(std::strcmp(readTitleB, titleB) == 0);
    Require(readUserParamA == userParamA);
    Require(readUserParamB == userParamB);

    SaveDataMountInfo infoA{};
    SaveDataMountInfo infoB{};
    Require(sceSaveDataGetMountInfo(&resultA.mount_point, &infoA) == SaveDataOk);
    Require(sceSaveDataGetMountInfo(&resultB.mount_point, &infoB) == SaveDataOk);
    Require(infoA.blocks >= 48);
    Require(infoB.blocks >= 48);

    Require(sceSaveDataUmount2(0, &resultA.mount_point) == SaveDataOk);

    Require(sceSaveDataGetMountInfo(&resultA.mount_point, &infoA) == SaveDataErrorNotMounted);
    Require(sceSaveDataGetParam(&resultA.mount_point, ParamTypeTitle, readTitleA, sizeof(readTitleA), nullptr) == SaveDataErrorNotMounted);

    std::memset(readTitleB, 0, sizeof(readTitleB));
    readUserParamB = 0;
    Require(sceSaveDataGetMountInfo(&resultB.mount_point, &infoB) == SaveDataOk);
    Require(sceSaveDataGetParam(&resultB.mount_point, ParamTypeTitle, readTitleB, sizeof(readTitleB), nullptr) == SaveDataOk);
    Require(sceSaveDataGetParam(&resultB.mount_point, ParamTypeUserParam, &readUserParamB, sizeof(readUserParamB), nullptr) == SaveDataOk);
    Require(std::strcmp(readTitleB, titleB) == 0);
    Require(readUserParamB == userParamB);

    Require(sceSaveDataUmount2(0, &resultB.mount_point) == SaveDataOk);
    Require(sceSaveDataGetMountInfo(&resultB.mount_point, &infoB) == SaveDataErrorNotMounted);
    Require(sceSaveDataGetParam(&resultB.mount_point, ParamTypeTitle, readTitleB, sizeof(readTitleB), nullptr) == SaveDataErrorNotMounted);
}

} // namespace

int main() {
    const auto sandbox = std::filesystem::temp_directory_path() / ("anyps5-savedata-info-bounds-" + std::to_string(std::random_device{}()));
    const auto work = sandbox / "work";
    std::filesystem::create_directories(work);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(work);

    TestUninitializedBounds();
    Require(sceSaveDataInitialize3(nullptr) == SaveDataOk);
    TestUnmountedNullAndParameterBounds();
    TestMountedContainerInfoBounds();
    TestMultipleContainersIsolation();
    Require(sceSaveDataTerminate() == SaveDataOk);
    TestUninitializedBounds();

    std::filesystem::current_path(previous);
    std::error_code error;
    std::filesystem::remove_all(sandbox, error);
    return 0;
}
