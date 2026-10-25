#include "prx/libSceNpEntitlementAccess/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

static constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER = static_cast<int>(0x80558003);
static constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_NOT_FOUND = static_cast<int>(0x80558007);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr uint32_t SKU_FLAG_FULL = 3;

namespace {

#ifdef _WIN32
const wchar_t* ConfiguredEntitlements() { return _wgetenv(L"ANYPS5_ENTITLEMENTS"); }
#else
const char* ConfiguredEntitlements() { return std::getenv("ANYPS5_ENTITLEMENTS"); }
#endif

std::filesystem::path EntitlementsPath() {
    if (const auto* configured = ConfiguredEntitlements(); configured != nullptr && configured[0] != 0) return configured;
#ifdef _WIN32
    wchar_t module[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    if (length == 0 || length == MAX_PATH) throw std::runtime_error("NpEntitlementAccess: cannot locate the executable");
    return std::filesystem::path(module).parent_path() / "anyps5-entitlements.ini";
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    std::error_code error;
    std::filesystem::path path;
    if (_NSGetExecutablePath(executable.data(), &size) == 0) path = std::filesystem::canonical(executable.data(), error);
    if (path.empty()) throw std::runtime_error("NpEntitlementAccess: cannot locate the executable");
    return path.parent_path() / "anyps5-entitlements.ini";
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path() / "anyps5-entitlements.ini";
#endif
}

const std::vector<NpEntitlementAccessAddcontEntitlementInfo>& OwnedAddons() {
    static const auto owned = [] {
        std::vector<NpEntitlementAccessAddcontEntitlementInfo> addons;
        const auto path = EntitlementsPath();
        std::ifstream file(path);
        if (!file) {
            if (ConfiguredEntitlements() != nullptr || std::filesystem::exists(path)) throw std::runtime_error("NpEntitlementAccess: cannot read " + path.string());
            return addons;
        }
        std::string line;
        while (std::getline(file, line)) {
            line.erase(std::min(line.find_first_of("#;"), line.size()));
            const auto first = line.find_first_not_of(" \t\r");
            if (first == std::string::npos) continue;
            const auto label = line.substr(first, line.find_last_not_of(" \t\r") + 1 - first);
            NpEntitlementAccessAddcontEntitlementInfo info{};
            if (label.size() >= sizeof(info.entitlement_label.data)) throw std::runtime_error("NpEntitlementAccess: entitlement label '" + label + "' in " + path.string() + " is longer than 16 characters");
            std::memcpy(info.entitlement_label.data, label.data(), label.size());
            info.package_type = 3;
            info.download_status = 4;
            addons.push_back(info);
        }
        return addons;
    }();
    return owned;
}

}

extern "C" {

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfo(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, NpEntitlementAccessAddcontEntitlementInfo* info) {
    (void)service_label;
    if (!entitlement_label || !info) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
    for (const auto& addon : OwnedAddons()) {
        if (std::strncmp(addon.entitlement_label.data, entitlement_label->data, sizeof(entitlement_label->data)) == 0) {
            *info = addon;
            return 0;
        }
    }
    return SCE_NP_ENTITLEMENT_ACCESS_ERROR_NOT_FOUND;
}

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfoList(uint32_t service_label, NpEntitlementAccessAddcontEntitlementInfo* list, uint32_t list_num, uint32_t* hit_num) {
    (void)service_label;
    if (!hit_num || (!list && list_num != 0)) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
    const auto& owned = OwnedAddons();
    *hit_num = static_cast<uint32_t>(owned.size());
    std::copy_n(owned.begin(), std::min<std::size_t>(list_num, owned.size()), list);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessGetSkuFlag(uint32_t* sku_flag) {
    if (!sku_flag) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
    *sku_flag = SKU_FLAG_FULL;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessGetEntitlementKey(
    uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel,
    NpEntitlementAccessEntitlementKey* key) {
    (void)serviceLabel;
    constexpr int errorParameter = static_cast<int>(0x817D0002);
    constexpr int errorNoEntitlement = static_cast<int>(0x817D0007);
    if (!entitlementLabel || !key) return errorParameter;
    return errorNoEntitlement;
}

int APS5_VABI sceNpEntitlementAccessInitialize(const NpEntitlementAccessInitParam* init_param, NpEntitlementAccessBootParam* boot_param) {
    (void)init_param;
    (void)boot_param;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessAbortRequest(std::int64_t requestId) {
    throw std::logic_error("sceNpEntitlementAccessAbortRequest: asynchronous request tracking is unimplemented");
}

int APS5_VABI sceNpEntitlementAccessDeleteRequest(std::int64_t requestId) {
    throw std::logic_error("sceNpEntitlementAccessDeleteRequest: asynchronous request tracking is unimplemented");
}

int APS5_VABI sceNpEntitlementAccessGenerateTransactionId(NpEntitlementAccessTransactionId* transactionId) {
 if (!transactionId) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessPollConsumeEntitlement(std::int64_t requestId, std::int32_t* result, std::int32_t* countResult) {
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestConsumeUnifiedEntitlement(std::int32_t userId, std::uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, const NpEntitlementAccessTransactionId* transactionId, std::int32_t count, std::int64_t* requestId) {
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestConsumeServiceEntitlement(std::int32_t userId, std::uint32_t serviceLabel, const NpServiceEntitlementLabel* entitlementLabel, const NpEntitlementAccessTransactionId* transactionId, std::int32_t count, std::int64_t* requestId) {
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfo(std::int64_t requestId, std::int32_t* result, NpEntitlementAccessServiceInfo* info) {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessPollUnifiedEntitlementInfo(std::int64_t requestId, std::int32_t* result, NpEntitlementAccessUnifiedInfo* info) {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfo(std::int32_t userId, std::uint32_t serviceLabel, const NpServiceEntitlementLabel* entitlementLabel, std::int64_t* requestId) {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestUnifiedEntitlementInfo(std::int32_t userId, std::uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, std::int64_t* requestId) {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessGetPftFlag(uint32_t* flag) {
    constexpr int errorParameter = static_cast<int>(0x817D0002);
    if (!flag) return errorParameter;
    *flag = 0;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessGetGameTrialsFlag(uint32_t* flag) {
    constexpr int errorParameter = static_cast<int>(0x817D0002);
    if (!flag) return errorParameter;
    *flag = 0;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfoList(std::int64_t requestId, std::int32_t* result, NpEntitlementAccessServiceInfo* infos, std::uint32_t capacity, std::int32_t* firstResult, std::int32_t* secondResult, std::int32_t* thirdResult) {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessPollUnifiedEntitlementInfoList(std::int64_t requestId, std::int32_t* result, NpEntitlementAccessUnifiedInfo* infos, std::uint32_t capacity, std::int32_t* firstResult, std::int32_t* secondResult, std::int32_t* thirdResult) {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfoList(std::int32_t userId, std::uint32_t serviceLabel, const NpServiceEntitlementLabel* entitlementLabels, std::uint32_t labelCount, const NpEntitlementAccessListParameter* parameter, std::int64_t* requestId) {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestUnifiedEntitlementInfoList(std::int32_t userId, std::uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabels, std::uint32_t labelCount, const NpEntitlementAccessListParameter* parameter, std::int64_t* requestId) {
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessPollConsumableEntitlementInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessRequestConsumableEntitlementInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessRequestConsumeEntitlement() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
