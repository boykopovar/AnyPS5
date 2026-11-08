#include "prx/libc/include/Api.hpp"
#include "prx/libSceAppContent/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include "ProcessParametersFixture.hpp"

static constexpr int ErrorParameter = static_cast<int>(0x80D90002);
static constexpr int ErrorBusy = static_cast<int>(0x80D90003);
static constexpr int ErrorNotFound = static_cast<int>(0x80D90005);
static constexpr int ErrorDrmNoEntitlement = static_cast<int>(0x80D90007);

static void Require(bool value) { if (!value) std::abort(); }

static void SetEntitlements(const std::filesystem::path& file) {
#ifdef _WIN32
    Require(_wputenv_s(L"ANYPS5_ENTITLEMENTS", file.wstring().c_str()) == 0);
#else
    Require(::setenv("ANYPS5_ENTITLEMENTS", file.c_str(), 1) == 0);
#endif
}

int main() {
    const auto original = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-appcontent-entitlements-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directories(root));
    std::filesystem::current_path(root);

    Require(std::filesystem::create_directories("addcont0"));
    {
        std::ofstream sample("addcont0/sample.txt");
        sample << "content0";
        Require(static_cast<bool>(sample));
    }
    Require(std::filesystem::create_directories("custom_dlc1"));
    {
        std::ofstream sample("custom_dlc1/sample1.txt");
        sample << "content1";
        Require(static_cast<bool>(sample));
    }

    const auto iniFile = root / "anyps5-entitlements.ini";
    {
        std::ofstream ini(iniFile);
        ini << "ADDON00000000001\n";
        ini << "ADDON00000000002 = /custom_dlc1\n";
        ini << "ADDONNODIR000001 = /dlc_missing\n";
        Require(static_cast<bool>(ini));
    }
    SetEntitlements(iniFile);

    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    std::uint32_t sdkVersion = 0x01500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));

    AppContentInitParam init{};
    AppContentBootParam boot{};
    boot.attr = 0xffffffffu;
    Require(sceAppContentInitialize(&init, &boot) == 0);

    uint32_t count = 0;
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 0, &count) == 0);
    Require(count == 3);

    AppContentAddcontInfo list[3]{};
    uint32_t hitNum = 0;
    Require(sceAppContentGetAddcontInfoList(0, list, 1, &hitNum) == 0);
    Require(hitNum == 1);
    Require(std::strcmp(list[0].entitlement_label.data, "ADDON00000000001") == 0);
    Require(list[0].status == 4);

    hitNum = 0;
    Require(sceAppContentGetAddcontInfoList(0, list, 3, &hitNum) == 0);
    Require(hitNum == 3);
    Require(std::strcmp(list[0].entitlement_label.data, "ADDON00000000001") == 0);
    Require(list[0].status == 4);
    Require(std::strcmp(list[1].entitlement_label.data, "ADDON00000000002") == 0);
    Require(list[1].status == 4);
    Require(std::strcmp(list[2].entitlement_label.data, "ADDONNODIR000001") == 0);
    Require(list[2].status == 4);

    NpUnifiedEntitlementLabel label0{};
    std::memcpy(label0.data, "ADDON00000000001", 16);
    AppContentAddcontInfo info{};
    Require(sceAppContentGetAddcontInfo(0, &label0, &info) == 0);
    Require(std::strcmp(info.entitlement_label.data, "ADDON00000000001") == 0);
    Require(info.status == 4);

    NpUnifiedEntitlementLabel label1{};
    std::memcpy(label1.data, "ADDON00000000002", 16);
    Require(sceAppContentGetAddcontInfo(0, &label1, &info) == 0);
    Require(std::strcmp(info.entitlement_label.data, "ADDON00000000002") == 0);
    Require(info.status == 4);

    NpUnifiedEntitlementLabel labelNoDir{};
    std::memcpy(labelNoDir.data, "ADDONNODIR000001", 16);
    Require(sceAppContentGetAddcontInfo(0, &labelNoDir, &info) == 0);
    Require(std::strcmp(info.entitlement_label.data, "ADDONNODIR000001") == 0);
    Require(info.status == 4);

    NpUnifiedEntitlementLabel unlisted{};
    std::memcpy(unlisted.data, "UNLISTED00000001", 16);
    Require(sceAppContentGetAddcontInfo(0, &unlisted, &info) == ErrorDrmNoEntitlement);

    Require(access_nid_postfix("/addcont0/sample.txt", 0) == -1);
    Require(access_nid_postfix("/custom_dlc1/sample1.txt", 0) == -1);

    AppContentMountPoint mountPointNoDir{};
    Require(sceAppContentAddcontMount(0, &labelNoDir, &mountPointNoDir) == ErrorNotFound);

    AppContentMountPoint mountPoint0{};
    Require(sceAppContentAddcontMount(0, &label0, &mountPoint0) == 0);
    Require(std::strcmp(mountPoint0.data, "/addcont0") == 0);
    Require(access_nid_postfix("/addcont0/sample.txt", 0) == 0);

    AppContentMountPoint busyMount{};
    Require(sceAppContentAddcontMount(0, &label0, &busyMount) == ErrorBusy);

    AppContentMountPoint mountPoint1{};
    Require(sceAppContentAddcontMount(0, &label1, &mountPoint1) == 0);
    Require(std::strcmp(mountPoint1.data, "/custom_dlc1") == 0);
    Require(access_nid_postfix("/custom_dlc1/sample1.txt", 0) == 0);

    Require(sceAppContentAddcontMount(0, &unlisted, &busyMount) == ErrorNotFound);

    Require(sceAppContentAddcontUnmount(&mountPoint0) == 0);
    Require(access_nid_postfix("/addcont0/sample.txt", 0) == -1);
    Require(sceAppContentAddcontUnmount(&mountPoint0) == ErrorNotFound);

    Require(sceAppContentAddcontUnmount(&mountPoint1) == 0);
    Require(access_nid_postfix("/custom_dlc1/sample1.txt", 0) == -1);
    Require(sceAppContentAddcontUnmount(&mountPoint1) == ErrorNotFound);

    Require(sceAppContentGetAddcontInfoByEntitlementId("UP0000-PPSA00000_00-ADDCONT000000001", &info) == ErrorDrmNoEntitlement);
    uint32_t iroHits = 0x5a5a5a5a;
    Require(sceAppContentGetAddcontInfoListByIroTag(1, nullptr, 0, &iroHits) == 0);
    Require(iroHits == 0);

    std::filesystem::current_path(original);
    std::filesystem::remove_all(root);
    return 0;
}
