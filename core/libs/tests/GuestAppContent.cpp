#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

extern "C" {
int APS5_VABI sceAppContentInitialize(const AppContentInitParam*, AppContentBootParam*);
int APS5_VABI sceAppContentAddcontMount(uint32_t, const NpUnifiedEntitlementLabel*, AppContentMountPoint*);
int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint*);
int APS5_VABI sceAppContentTemporaryDataMount2(uint32_t, AppContentMountPoint*);
int APS5_VABI sceAppContentTemporaryDataUnmount(const AppContentMountPoint*);
int APS5_VABI sceAppContentTemporaryDataFormat(const AppContentMountPoint*);
int APS5_VABI sceAppContentTemporaryDataGetAvailableSpaceKb(const AppContentMountPoint*, size_t*);
int APS5_VABI access_nid_postfix(const char*, int);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceAppContentGetAddcontInfo(uint32_t, const NpUnifiedEntitlementLabel*, void*);
int APS5_VABI sceAppContentGetAddcontInfoList(uint32_t, void*, uint32_t, uint32_t*);
int APS5_VABI sceAppContentDownloadDataGetAvailableSpaceKb(const AppContentMountPoint*, size_t*);
int APS5_VABI sceAppContentAppParamGetInt(uint32_t, int32_t*);
}

static constexpr int ErrorParameter = static_cast<int>(0x80D90002);
static constexpr int ErrorNotFound = static_cast<int>(0x80D90005);
static constexpr int ErrorDrmNoEntitlement = static_cast<int>(0x80D90007);
static constexpr int ErrorBusy = static_cast<int>(0x80D90003);
static constexpr int ErrorNotMounted = static_cast<int>(0x80D90004);
static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("appcontent-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory));
    std::filesystem::current_path(directory);
    std::filesystem::create_directories("app0/sce_sys");
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"downloadDataSize":0,)"
              R"("userDefinedParam1":23566,"userDefinedParam2":-7,"userDefinedParam3":0,"userDefinedParam4":2147483647})";
        Require(static_cast<bool>(param));
    }
    AppContentMountPoint download{};
    std::memcpy(download.data, "/download0", 11);
    size_t availableKb = 12345;
    Require(sceAppContentDownloadDataGetAvailableSpaceKb(&download, &availableKb) == 0);
    Require(availableKb == 0);
    Require(sceAppContentDownloadDataGetAvailableSpaceKb(&download, nullptr) == ErrorParameter);

    const int32_t expectedParams[] = {3, 23566, -7, 0, 2147483647};
    for (uint32_t id = 0; id < 5; ++id) {
        int32_t value = 0x5a5a5a5a;
        Require(sceAppContentAppParamGetInt(id, &value) == 0);
        Require(value == expectedParams[id]);
    }
    int32_t paramValue = 0x5a5a5a5a;
    Require(sceAppContentAppParamGetInt(5, &paramValue) == ErrorParameter);
    Require(paramValue == 0x5a5a5a5a);
    Require(sceAppContentAppParamGetInt(0, nullptr) == ErrorParameter);

    NpUnifiedEntitlementLabel label{};
    std::memcpy(&label, "ADDCONT000000001", 16);
    AppContentMountPoint mountPoint{};
    std::memset(&mountPoint, 0x5a, sizeof(mountPoint));
    const AppContentMountPoint untouched = mountPoint;
    Require(sceAppContentAddcontMount(0, &label, &mountPoint) == ErrorNotFound);
    Require(std::memcmp(&mountPoint, &untouched, sizeof(mountPoint)) == 0);
    Require(sceAppContentAddcontMount(0, nullptr, &mountPoint) == ErrorParameter);
    Require(sceAppContentAddcontMount(0, &label, nullptr) == ErrorParameter);
    std::memcpy(mountPoint.data, "/addcont0", 10);
    Require(sceAppContentAddcontUnmount(&mountPoint) == ErrorNotFound);
    Require(sceAppContentAddcontUnmount(nullptr) == ErrorParameter);

    std::filesystem::create_directory(directory / "temp0");
    { std::ofstream file(directory / "temp0" / "old.txt"); file << "previous run"; }
    AppContentBootParam boot{};
    Require(sceAppContentInitialize(nullptr, &boot) == 0);
    Require(access_nid_postfix("/temp0/old.txt", 0) == -1);
    Require(sceAppContentTemporaryDataUnmount(nullptr) == ErrorParameter);
    Require(sceAppContentTemporaryDataUnmount(&mountPoint) == ErrorNotMounted);
    Require(sceAppContentTemporaryDataMount2(0, nullptr) == ErrorParameter);
    std::memset(&mountPoint, 0x5a, sizeof(mountPoint));
    Require(sceAppContentTemporaryDataMount2(2, &mountPoint) == ErrorParameter);
    Require(std::memcmp(&mountPoint, &untouched, sizeof(mountPoint)) == 0);
    Require(sceAppContentTemporaryDataMount2(0, &mountPoint) == 0);
    Require(std::strcmp(mountPoint.data, "/temp0") == 0);
    const auto backing = ResolvePath_nid_no_patch("/temp0");
    { std::ofstream file(backing / "keep.txt"); file << "retained"; }
    { std::ofstream file(directory / "temp01"); file << "unrelated"; }
    AppContentMountPoint second = untouched;
    Require(sceAppContentTemporaryDataMount2(1, &second) == ErrorBusy);
    Require(std::memcmp(&second, &untouched, sizeof(second)) == 0);
    Require(std::filesystem::exists(backing / "keep.txt"));
    Require(sceAppContentTemporaryDataUnmount(&second) == ErrorNotMounted);
    Require(access_nid_postfix("/temp0/keep.txt", 0) == 0);
    size_t available = 0;
    Require(sceAppContentTemporaryDataGetAvailableSpaceKb(&mountPoint, &available) == 0 && available > 0);
    Require(sceAppContentTemporaryDataUnmount(&mountPoint) == 0);
    Require(std::filesystem::exists(backing / "keep.txt"));
    Require(access_nid_postfix("/temp0/keep.txt", 0) == -1 && *__error_nid_postfix() == 2);
    Require(access_nid_postfix("/temp0", 0) == -1 && *__error_nid_postfix() == 2);
    Require(access_nid_postfix("/temp01", 0) == 0);
    Require(sceAppContentTemporaryDataUnmount(&mountPoint) == ErrorNotMounted);
    available = 123;
    Require(sceAppContentTemporaryDataGetAvailableSpaceKb(&mountPoint, &available) == ErrorNotMounted && available == 123);
    Require(sceAppContentTemporaryDataFormat(&mountPoint) == ErrorNotMounted);
    Require(sceAppContentTemporaryDataMount2(0, &mountPoint) == 0);
    Require(access_nid_postfix("/temp0/keep.txt", 0) == 0);
    Require(sceAppContentTemporaryDataFormat(nullptr) == ErrorParameter);
    Require(sceAppContentTemporaryDataFormat(&mountPoint) == 0);
    Require(std::filesystem::is_empty(backing));
    { std::ofstream file(backing / "remove.txt"); file << "format"; }
    Require(sceAppContentTemporaryDataUnmount(&mountPoint) == 0);
    Require(sceAppContentTemporaryDataMount2(1, &mountPoint) == 0);
    Require(std::filesystem::is_empty(backing));
    Require(sceAppContentTemporaryDataUnmount(&mountPoint) == 0);
    unsigned char info[24];
    std::memset(info, 0x5a, sizeof(info));
    unsigned char untouchedInfo[24];
    std::memcpy(untouchedInfo, info, sizeof(info));
    Require(sceAppContentGetAddcontInfo(0, &label, info) == ErrorDrmNoEntitlement);
    Require(std::memcmp(info, untouchedInfo, sizeof(info)) == 0);
    Require(sceAppContentGetAddcontInfo(0, nullptr, info) == ErrorParameter);
    Require(sceAppContentGetAddcontInfo(0, &label, nullptr) == ErrorParameter);

    uint32_t hitNum = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 0, &hitNum) == 0);
    Require(hitNum == 0);
    hitNum = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoList(0, info, 0, &hitNum) == 0);
    Require(hitNum == 0);
    hitNum = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 1, &hitNum) == 0);
    Require(hitNum == 0);
    hitNum = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoList(0, info, 1, &hitNum) == 0);
    Require(hitNum == 0);
    Require(std::memcmp(info, untouchedInfo, sizeof(info)) == 0);
    Require(sceAppContentGetAddcontInfoList(0, info, 1, nullptr) == 0);
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 0, nullptr) == ErrorParameter);
    Require(sceAppContentGetAddcontInfoList(0, info, 0, nullptr) == ErrorParameter);
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 1, nullptr) == ErrorParameter);
    std::filesystem::current_path(original);
    std::filesystem::remove_all(directory);
}
