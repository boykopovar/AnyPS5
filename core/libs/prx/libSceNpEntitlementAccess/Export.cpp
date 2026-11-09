#include "prx/libSceNpEntitlementAccess/include/Api.hpp"
#include "prx/libSceNpEntitlementAccess/EntitlementsIni.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

static constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER = static_cast<int>(0x80558003);
static constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_NOT_FOUND = static_cast<int>(0x80558007);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr uint32_t SKU_FLAG_FULL = 3;
static constexpr std::int32_t USER_ID_INVALID = -1;

namespace {

const std::vector<NpEntitlementAccessAddcontEntitlementInfo>& OwnedAddons() {
    static const auto owned = [] {
        std::vector<NpEntitlementAccessAddcontEntitlementInfo> addons;
        for (const auto& entry : AnyPS5::Entitlements::ParseEntries()) {
            NpEntitlementAccessAddcontEntitlementInfo info{};
            std::memcpy(info.entitlement_label.data, entry.label.data(), entry.label.size());
            info.package_type = 3;
            info.download_status = 4;
            addons.push_back(info);
        }
        return addons;
    }();
    return owned;
}

std::string PrintableLabel(const std::string& label) {
    std::string printable;
    for (const unsigned char byte : label) {
        if (byte >= 0x20 && byte < 0x7f && byte != '\\') {
            printable += static_cast<char>(byte);
            continue;
        }
        char escaped[5];
        std::snprintf(escaped, sizeof(escaped), "\\x%02x", byte);
        printable += escaped;
    }
    return printable;
}

std::string EntitlementsName() {
    if (const auto* configured = AnyPS5::Entitlements::ConfiguredEntitlements(); configured != nullptr && configured[0] != 0) return AnyPS5::Entitlements::EntitlementsPath().string();
    return "anyps5-entitlements.ini";
}

void ReportUnlisted(const std::string& label) {
    constexpr std::size_t reportLimit = 64;
    static std::mutex reportMutex;
    static std::set<std::string> reported;
    static bool capped = false;
    std::lock_guard lock(reportMutex);
    if (reported.contains(label)) return;
    if (reported.size() < reportLimit) {
        reported.insert(label);
        std::fprintf(stderr, "[entitlements] the title asked for the add-on %s, which %s does not list, so it is reported as not installed\n", PrintableLabel(label).c_str(), EntitlementsName().c_str());
    } else if (!capped) {
        capped = true;
        std::fprintf(stderr, "[entitlements] the title asked for more than %zu add-ons that %s does not list; further ones are reported as not installed without a line\n", reportLimit, EntitlementsName().c_str());
    }
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
    ReportUnlisted(std::string(entitlement_label->data, strnlen(entitlement_label->data, sizeof(entitlement_label->data))));
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
    constexpr int errorParameter = static_cast<int>(0x817D0002);
    constexpr int errorAlreadyInitialized = static_cast<int>(0x817D0003);
    static std::mutex mutex;
    static bool initialized = false;
    std::lock_guard lock(mutex);
    if (initialized) return errorAlreadyInitialized;
    if (init_param == nullptr || boot_param == nullptr) return errorParameter;
    if (!std::all_of(std::begin(init_param->reserved), std::end(init_param->reserved), [](char value) { return value == 0; })) return errorParameter;
    if (!std::all_of(std::begin(boot_param->reserved), std::end(boot_param->reserved), [](char value) { return value == 0; })) return errorParameter;
    initialized = true;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessAbortRequest(std::int64_t requestId) {
    (void)requestId;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessDeleteRequest(std::int64_t requestId) {
    (void)requestId;
    return 0;
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

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfoIndividual(std::int32_t user_id, uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, NpEntitlementAccessAddcontEntitlementInfo* info) {
    if (user_id == USER_ID_INVALID) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
    return sceNpEntitlementAccessGetAddcontEntitlementInfo(service_label, entitlement_label, info);
}

int APS5_VABI sceNpEntitlementAccessPollConsumableEntitlementInfo(int64_t requestId, int32_t* result, void* info) {
    (void)requestId;
    (void)result;
    (void)info;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestConsumableEntitlementInfo(
    int32_t userId, uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, int64_t* requestId) {
    (void)userId;
    (void)serviceLabel;
    (void)entitlementLabel;
    (void)requestId;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestConsumeEntitlement(
    int32_t userId, uint32_t serviceLabel, const void* entitlementLabel, const void* transactionId, int32_t useCount,
    int64_t* requestId) {
    (void)userId;
    (void)serviceLabel;
    (void)entitlementLabel;
    (void)transactionId;
    (void)useCount;
    (void)requestId;
    return SCE_NP_ERROR_SIGNED_OUT;
}

}
