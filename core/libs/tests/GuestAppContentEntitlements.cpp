#include "prx/libc/include/Api.hpp"
#include "prx/libSceAppContent/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FileStream.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <source_location>
#include <string>
#include <cstdio>
#include <system_error>
#include <utility>
#include "ProcessParametersFixture.hpp"
#include "prx/libSceNpEntitlementAccess/EntitlementsIni.hpp"

static constexpr int ErrorParameter = static_cast<int>(0x80D90002);
static constexpr int ErrorBusy = static_cast<int>(0x80D90003);
static constexpr int ErrorNotFound = static_cast<int>(0x80D90005);
static constexpr int ErrorDrmNoEntitlement = static_cast<int>(0x80D90007);

static void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "%s:%u: AppContent assertion failed\n", location.file_name(), location.line());
        std::abort();
    }
}

static void SetEntitlements(const std::filesystem::path& file) {
#ifdef _WIN32
    Require(_wputenv_s(L"ANYPS5_ENTITLEMENTS", file.wstring().c_str()) == 0);
#else
    Require(::setenv("ANYPS5_ENTITLEMENTS", file.c_str(), 1) == 0);
#endif
}

struct EntitlementsTestGuard {
    std::filesystem::path root;
#ifdef _WIN32
    bool hadEnv{false};
    std::wstring prevEnv;
#else
    bool hadEnv{false};
    std::string prevEnv;
#endif

    explicit EntitlementsTestGuard(std::filesystem::path dir) : root(std::move(dir)) {
#ifdef _WIN32
        if (const auto* existing = ::_wgetenv(L"ANYPS5_ENTITLEMENTS")) {
            hadEnv = true;
            prevEnv = existing;
        }
#else
        if (const auto* existing = std::getenv("ANYPS5_ENTITLEMENTS")) {
            hadEnv = true;
            prevEnv = existing;
        }
#endif
    }

    ~EntitlementsTestGuard() {
#ifdef _WIN32
        if (hadEnv) {
            ::_wputenv_s(L"ANYPS5_ENTITLEMENTS", prevEnv.c_str());
        } else {
            ::_wputenv_s(L"ANYPS5_ENTITLEMENTS", L"");
        }
#else
        if (hadEnv) {
            ::setenv("ANYPS5_ENTITLEMENTS", prevEnv.c_str(), 1);
        } else {
            ::unsetenv("ANYPS5_ENTITLEMENTS");
        }
#endif
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};

static void WriteBinaryFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    Require(static_cast<bool>(out));
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
    Require(static_cast<bool>(out));
}

static void CheckEntitlementsIni() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_entitlements_test_" + std::to_string(stamp));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    Require(std::filesystem::create_directories(root));
    EntitlementsTestGuard guard(root);

    const auto emptyLabelFile = root / "empty_label.ini";
    WriteBinaryFile(emptyLabelFile, "=addcont0\n");
    SetEntitlements(emptyLabelFile);
    bool threwEmptyLabel = false;
    try {
        (void)AnyPS5::Entitlements::ParseEntries();
    } catch (const std::runtime_error&) {
        threwEmptyLabel = true;
    }
    Require(threwEmptyLabel);

    const auto dirPath = root / "dir_target";
    Require(std::filesystem::create_directories(dirPath));
    SetEntitlements(dirPath);
    bool threwDirectory = false;
    try {
        (void)AnyPS5::Entitlements::ParseEntries();
    } catch (const std::runtime_error&) {
        threwDirectory = true;
    }
    Require(threwDirectory);

    const auto bomFile = root / "bom.ini";
    WriteBinaryFile(bomFile, "\xEF\xBB\xBF" "DLC0000000000001=addcont0\n");
    SetEntitlements(bomFile);
    const auto entries = AnyPS5::Entitlements::ParseEntries();
    Require(entries.size() == 1);
    Require(entries[0].label == "DLC0000000000001");
    Require(entries[0].target == "addcont0");

#ifndef _WIN32
    Require(::setenv("ANYPS5_ENTITLEMENTS", "", 1) == 0);
    const auto defaultPath = AnyPS5::Entitlements::EntitlementsPath();
    const auto emptyEnvEntries = AnyPS5::Entitlements::ParseEntries();
    if (!std::filesystem::exists(defaultPath, ec)) {
        Require(emptyEnvEntries.empty());
    }
#endif
}

static void RequireConfigResolved(const std::source_location location = std::source_location::current()) {
    try {
        uint32_t count = 0;
        Require(sceAppContentGetAddcontInfoList(0, nullptr, 0, &count) == 0);
        Require(count == 3);
    } catch (const std::filesystem::filesystem_error& error) {
        std::fprintf(stderr, "Equivalent add-on aliases failed to resolve at %s:%u: %s\n", location.file_name(), location.line(), error.what());
        std::abort();
    }
}

static void RequireCollisionContents() {
    auto* stream = fopen_nid_postfix("/addcont0/sample.txt", "rb");
    Require(stream != nullptr);
    constexpr char expected[] = "collision-content";
    std::array<char, sizeof(expected)> contents{};
    Require(fread_nid_postfix(contents.data(), 1, contents.size(), stream) == sizeof(expected) - 1);
    Require(std::memcmp(contents.data(), expected, sizeof(expected) - 1) == 0);
    Require(fclose_nid_postfix(stream) == 0);
}

static int RunAliasCollision() {
    const auto original = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-appcontent-alias-collision-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directories(root));
    std::filesystem::current_path(root);
    Require(std::filesystem::create_directories("addcont0"));
    {
        std::ofstream sample("addcont0/sample.txt");
        sample << "collision-content";
        Require(static_cast<bool>(sample));
    }
    const auto iniFile = root / "anyps5-entitlements.ini";
    {
        std::ofstream ini(iniFile);
        ini << "COLLIDEADDON0001 = /addcont0\n";
        ini << "COLLIDEADDON0002 = /addcont0/\n";
        ini << "COLLIDEADDON0003 = /ADDCONT0/\n";
        Require(static_cast<bool>(ini));
    }
    SetEntitlements(iniFile);

    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    std::uint32_t sdkVersion = 0x01500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    AppContentInitParam init{};
    AppContentBootParam boot{};
    Require(sceAppContentInitialize(&init, &boot) == 0);
    RequireConfigResolved();

    NpUnifiedEntitlementLabel firstLabel{};
    NpUnifiedEntitlementLabel secondLabel{};
    NpUnifiedEntitlementLabel thirdLabel{};
    std::memcpy(firstLabel.data, "COLLIDEADDON0001", 16);
    std::memcpy(secondLabel.data, "COLLIDEADDON0002", 16);
    std::memcpy(thirdLabel.data, "COLLIDEADDON0003", 16);
    AppContentMountPoint firstMount{};
    Require(sceAppContentAddcontMount(0, &firstLabel, &firstMount) == 0);
    Require(std::strcmp(firstMount.data, "/addcont0") == 0);
    RequireCollisionContents();

    AppContentMountPoint secondMount{};
    std::memset(&secondMount, 0x5a, sizeof(secondMount));
    const auto untouchedSecondMount = secondMount;
    Require(sceAppContentAddcontMount(0, &secondLabel, &secondMount) == ErrorBusy);
    Require(std::memcmp(&secondMount, &untouchedSecondMount, sizeof(secondMount)) == 0);
    RequireCollisionContents();

    AppContentMountPoint thirdMount{};
    std::memset(&thirdMount, 0x5a, sizeof(thirdMount));
    const auto untouchedThirdMount = thirdMount;
    Require(sceAppContentAddcontMount(0, &thirdLabel, &thirdMount) == ErrorBusy);
    Require(std::memcmp(&thirdMount, &untouchedThirdMount, sizeof(thirdMount)) == 0);
    RequireCollisionContents();

    Require(sceAppContentAddcontUnmount(&firstMount) == 0);
    Require(access_nid_postfix("/addcont0/sample.txt", 0) == -1);
    Require(sceAppContentAddcontMount(0, &thirdLabel, &thirdMount) == 0);
    Require(std::strcmp(thirdMount.data, "/ADDCONT0/") == 0);
    RequireCollisionContents();
    Require(sceAppContentAddcontUnmount(&thirdMount) == 0);
    Require(access_nid_postfix("/addcont0/sample.txt", 0) == -1);
    Require(sceAppContentAddcontMount(0, &secondLabel, &secondMount) == 0);
    Require(std::strcmp(secondMount.data, "/addcont0/") == 0);
    RequireCollisionContents();
    Require(sceAppContentAddcontUnmount(&secondMount) == 0);
    Require(access_nid_postfix("/addcont0/sample.txt", 0) == -1);
    std::filesystem::current_path(original);
    std::filesystem::remove_all(root);
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "alias-collision") == 0) return RunAliasCollision();
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

    hitNum = 0x5a5a5a5au;
    errno = EBUSY;
    Require(sceAppContentGetAddcontInfoList(0, reinterpret_cast<AppContentAddcontInfo*>(std::uintptr_t{0x10}), 1, &hitNum) == ErrorParameter);
    Require(hitNum == 0x5a5a5a5au && errno == EBUSY);
    Require(sceAppContentGetAddcontInfoList(0, nullptr, 0, reinterpret_cast<std::uint32_t*>(std::uintptr_t{0x10})) == ErrorParameter);
    Require(errno == EBUSY);

    NpUnifiedEntitlementLabel label0{};
    std::memcpy(label0.data, "ADDON00000000001", 16);
    AppContentAddcontInfo info{};
    Require(sceAppContentGetAddcontInfo(0, &label0, &info) == 0);
    Require(std::strcmp(info.entitlement_label.data, "ADDON00000000001") == 0);
    Require(info.status == 4);
    errno = EBUSY;
    Require(sceAppContentGetAddcontInfo(0, &label0, reinterpret_cast<AppContentAddcontInfo*>(std::uintptr_t{0x10})) == ErrorParameter);
    Require(errno == EBUSY);

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

    errno = EBUSY;
    Require(sceAppContentAddcontMount(0, &label0, reinterpret_cast<AppContentMountPoint*>(std::uintptr_t{0x10})) == ErrorParameter);
    Require(errno == EBUSY);
    Require(access_nid_postfix("/addcont0/sample.txt", 0) == -1);

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

    CheckEntitlementsIni();
    return 0;
}
