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
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }

static std::string CaptureStderr(void (*body)()) {
    std::fflush(stderr);
    std::FILE* file = std::tmpfile();
    Require(file != nullptr);
#ifdef _WIN32
    const int saved = _dup(_fileno(stderr));
    _dup2(_fileno(file), _fileno(stderr));
#else
    const int saved = dup(fileno(stderr));
    dup2(fileno(file), fileno(stderr));
#endif
    body();
    std::fflush(stderr);
#ifdef _WIN32
    _dup2(saved, _fileno(stderr));
    _close(saved);
#else
    dup2(saved, fileno(stderr));
    close(saved);
#endif
    std::rewind(file);
    std::string text;
    char buffer[4096];
    for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
    std::fclose(file);
    return text;
}

static void QueryUnlistedTwiceAndListedOnce() {
    NpEntitlementAccessAddcontEntitlementInfo info{};
    NpUnifiedEntitlementLabel unlisted{};
    std::memcpy(unlisted.data, "UNLISTEDADDON", 13);
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfo(0, &unlisted, &info) == static_cast<int>(0x80558007));
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfo(0, &unlisted, &info) == static_cast<int>(0x80558007));
    NpUnifiedEntitlementLabel listed{};
    std::memcpy(listed.data, "SECONDADDON", 11);
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfo(0, &listed, &info) == 0);
}

static void QueryUnlisted(const char* text, std::size_t length) {
    NpEntitlementAccessAddcontEntitlementInfo info{};
    NpUnifiedEntitlementLabel label{};
    std::memcpy(label.data, text, length);
    Require(sceNpEntitlementAccessGetAddcontEntitlementInfo(0, &label, &info) == static_cast<int>(0x80558007));
}

static void QueryUnprintable() {
    QueryUnlisted("BAD\nNAME\x1b[1m", 13);
    QueryUnlisted("SLASH\\ADDON", 11);
}

static void QueryPastTheLimit() {
    for (int i = 0; i < 70; ++i) {
        char text[17]{};
        std::snprintf(text, sizeof(text), "CAPADDON%02d", i);
        QueryUnlisted(text, std::strlen(text));
    }
    QueryUnlisted("UNLISTEDADDON", 13);
}

static std::size_t Occurrences(const std::string& text, std::string_view needle) {
    std::size_t count = 0;
    for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size())) ++count;
    return count;
}

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
    const auto log = CaptureStderr(QueryUnlistedTwiceAndListedOnce);
    Require(Occurrences(log, "[entitlements] the title asked for the add-on UNLISTEDADDON, which ") == 1);
    Require(log.find(file.filename().string() + " does not list, so it is reported as not installed\n") != std::string::npos);
    Require(Occurrences(log, "[entitlements]") == 1);
    const auto escaped = CaptureStderr(QueryUnprintable);
    Require(escaped.find("the add-on BAD\\x0aNAME\\x1b[1m, which ") != std::string::npos);
    Require(escaped.find("the add-on SLASH\\x5cADDON, which ") != std::string::npos);
    Require(Occurrences(escaped, "\n") == 2 && escaped.find('\x1b') == std::string::npos);
    const auto capped = CaptureStderr(QueryPastTheLimit);
    Require(Occurrences(capped, "the add-on CAPADDON") == 61);
    Require(Occurrences(capped, "[entitlements] the title asked for more than 64 add-ons that ") == 1);
    Require(Occurrences(capped, "[entitlements]") == 62);
    std::filesystem::remove_all(root);
    std::puts("Guest entitlements passed");
    return 0;
}
