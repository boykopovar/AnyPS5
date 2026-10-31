#include "prx/libSceNpEntitlementAccess/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

static void Require(bool value) { if (!value) std::abort(); }

static void SetEntitlements(const std::filesystem::path& file) {
#ifdef _WIN32
    Require(_wputenv_s(L"ANYPS5_ENTITLEMENTS", file.wstring().c_str()) == 0);
#else
    Require(::setenv("ANYPS5_ENTITLEMENTS", file.c_str(), 1) == 0);
#endif
}

int main(int argc, char** argv) {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5_guest_entitlements-" + std::to_string(std::random_device{}()));
    const auto directory = root / std::filesystem::path(u8"dlc-a\u00e7\u00e3o-\u65e5\u672c");
    const auto file = directory / "anyps5-entitlements.ini";
    if (argc == 2 && std::string_view(argv[1]) == "missing") {
        std::uint32_t skuFlag = 0;
    Require(sceNpEntitlementAccessGetSkuFlag(nullptr) == static_cast<int>(0x80558003));
    Require(sceNpEntitlementAccessGetSkuFlag(&skuFlag) == 0 && skuFlag == 3);
    std::uint32_t pftFlag = 1;
    Require(sceNpEntitlementAccessGetPftFlag(nullptr) == static_cast<int>(0x817D0002));
    Require(sceNpEntitlementAccessGetPftFlag(&pftFlag) == 0 && pftFlag == 0);
    std::uint32_t trialFlag = 1;
    Require(sceNpEntitlementAccessGetGameTrialsFlag(nullptr) == static_cast<int>(0x817D0002));
    Require(sceNpEntitlementAccessGetGameTrialsFlag(&trialFlag) == 0 && trialFlag == 0);
    Require(sceNpEntitlementAccessGenerateTransactionId(nullptr) == static_cast<int>(0x80558003));
    SetEntitlements(file);
        std::uint32_t hits = 0;
        bool rejected = false;
        try {
            sceNpEntitlementAccessGetAddcontEntitlementInfoList(0, nullptr, 0, &hits);

        } catch (const std::runtime_error& error) {
            rejected = std::string_view(error.what()).starts_with("NpEntitlementAccess: cannot read ");
        }
        Require(rejected);
        std::puts("Guest entitlements rejected a missing configured file");
        return 0;
    }
    std::filesystem::create_directories(directory);
    {
        std::ofstream ini(file, std::ios::binary);
        ini << "# owned add-ons\nFIRSTADDON0000\n  SECONDADDON ; second\n";
        Require(static_cast<bool>(ini));
    }
    SetEntitlements(file);
    NpEntitlementAccessAddcontEntitlementInfo list[3]{};
    std::uint32_t hits = 0;
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoList(0, list, 3, &hits) == 0);
    Require(hits == 2);
    Require(std::strcmp(list[0].entitlement_label.data, "FIRSTADDON0000") == 0);
    Require(std::strcmp(list[1].entitlement_label.data, "SECONDADDON") == 0);
    NpUnifiedEntitlementLabel label{};
    std::memcpy(label.data, "SECONDADDON", 11);
    NpEntitlementAccessAddcontEntitlementInfo info{};
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfo(0, &label, &info) == 0);
    Require(std::strcmp(info.entitlement_label.data, "SECONDADDON") == 0 && info.package_type == 3 && info.download_status == 4);
    for (std::int32_t userId : {1, 0x10000000, std::numeric_limits<std::int32_t>::min()}) {
        NpEntitlementAccessAddcontEntitlementInfo individual{};
        Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(userId, 0, &label, &individual) == 0);
        Require(std::memcmp(&individual, &info, sizeof(info)) == 0);
    }
    NpEntitlementAccessAddcontEntitlementInfo untouched{};
    std::memset(&untouched, 0x5a, sizeof(untouched));
    const auto original = untouched;
    NpUnifiedEntitlementLabel unlisted{};
    std::memcpy(unlisted.data, "UNLISTEDADDON", 13);
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(1, 0, &unlisted, &untouched) == static_cast<int>(0x80558007));
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(-1, 0, &label, &untouched) == static_cast<int>(0x80558003));
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(-1, 0, nullptr, nullptr) == static_cast<int>(0x80558003));
    Require(std::memcmp(&untouched, &original, sizeof(original)) == 0);
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(1, 0, nullptr, &untouched) == static_cast<int>(0x80558003));
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(1, 0, &label, nullptr) == static_cast<int>(0x80558003));
    NpUnifiedEntitlementLabel empty{};
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(1, 0, &empty, &untouched) == static_cast<int>(0x80558007));
    NpUnifiedEntitlementLabel unterminated{};
    std::memset(unterminated.data, 'A', sizeof(unterminated.data));
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(1, 0, &unterminated, &untouched) == static_cast<int>(0x80558007));
    Require(std::memcmp(&untouched, &original, sizeof(original)) == 0);
    auto padded = label;
    std::memset(padded.padding, 0x5a, sizeof(padded.padding));
    NpEntitlementAccessAddcontEntitlementInfo paddedInfo{};
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(1, 0, &padded, &paddedInfo) == 0);
    Require(std::memcmp(&paddedInfo, &info, sizeof(info)) == 0);
    NpEntitlementAccessEntitlementKey key{};
    Require(sceNpEntitlementAccessGetEntitlementKey(0, nullptr, &key) == static_cast<int>(0x817D0002));
    Require(sceNpEntitlementAccessGetEntitlementKey(0, &label, nullptr) == static_cast<int>(0x817D0002));
    Require(sceNpEntitlementAccessGetEntitlementKey(0, &label, &key) == static_cast<int>(0x817D0007));
    for (std::int64_t requestId : {std::int64_t{0}, std::int64_t{1}, std::int64_t{-7}, std::numeric_limits<std::int64_t>::max()}) {
        Require(sceNpEntitlementAccessAbortRequest(requestId) == 0);
        Require(sceNpEntitlementAccessDeleteRequest(requestId) == 0);
        Require(sceNpEntitlementAccessDeleteRequest(requestId) == 0);
    }
    std::filesystem::remove_all(root);
    std::puts("Guest entitlements passed");
    return 0;
}
