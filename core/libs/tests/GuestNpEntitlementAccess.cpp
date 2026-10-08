#include "SceTypes.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceNpEntitlementAccessGetEntitlementKey(std::uint32_t, const NpUnifiedEntitlementLabel*, NpEntitlementAccessEntitlementKey*);

static constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER = static_cast<int>(0x80558003);
static constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_NOT_FOUND = static_cast<int>(0x80558007);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "NpEntitlementAccess check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static NpUnifiedEntitlementLabel Label(const char* text) {
    NpUnifiedEntitlementLabel label{};
    std::strncpy(label.data, text, sizeof(label.data) - 1);
    return label;
}

static bool KeyUntouched(const NpEntitlementAccessEntitlementKey& key) {
    for (const auto byte : key.data) {
        if (byte != 0xA5) return false;
    }
    return true;
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-entitlement-key-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto ini = root / "entitlements.ini";
    {
        std::ofstream file(ini);
        file << "OWNEDADDON000001\n";
    }
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_ENTITLEMENTS", ini.string().c_str()) == 0);
#else
    Require(setenv("ANYPS5_ENTITLEMENTS", ini.string().c_str(), 1) == 0);
#endif

    const auto owned = Label("OWNEDADDON000001");
    const auto missing = Label("MISSINGADDON0001");
    NpEntitlementAccessEntitlementKey key{};
    std::memset(key.data, 0xA5, sizeof(key.data));

    Require(sceNpEntitlementAccessGetEntitlementKey(0, nullptr, &key) == SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER);
    Require(sceNpEntitlementAccessGetEntitlementKey(0, &missing, nullptr) == SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER);
    Require(sceNpEntitlementAccessGetEntitlementKey(0, &missing, &key) == SCE_NP_ENTITLEMENT_ACCESS_ERROR_NOT_FOUND);
    Require(KeyUntouched(key));

    bool threw = false;
    try {
        sceNpEntitlementAccessGetEntitlementKey(0, &owned, &key);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(KeyUntouched(key));

    std::filesystem::remove_all(root);
    std::puts("NpEntitlementAccess tests passed");
    return 0;
}
