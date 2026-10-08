#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/AppMetadata/include/Addcont.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

static constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER = static_cast<int>(0x80558003);
static constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_NOT_FOUND = static_cast<int>(0x80558007);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr uint32_t SKU_FLAG_FULL = 3;
static constexpr uint32_t PACKAGE_TYPE_PSAC = 2;
static constexpr uint32_t PACKAGE_TYPE_PSAL = 3;
static constexpr uint32_t DOWNLOAD_STATUS_INSTALLED = 4;

namespace {

const std::vector<NpEntitlementAccessAddcontEntitlementInfo>& OwnedAddons() {
    static const auto owned = [] {
        std::vector<NpEntitlementAccessAddcontEntitlementInfo> addons;
        for (const auto& entry : AddcontEntries_nid_no_patch()) {
            NpEntitlementAccessAddcontEntitlementInfo info{};
            std::memcpy(info.entitlement_label.data, entry.label, sizeof(entry.label));
            info.package_type = entry.hasData ? PACKAGE_TYPE_PSAC : PACKAGE_TYPE_PSAL;
            info.download_status = DOWNLOAD_STATUS_INSTALLED;
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

int APS5_VABI sceNpEntitlementAccessInitialize(const NpEntitlementAccessInitParam* init_param, NpEntitlementAccessBootParam* boot_param) {
    (void)init_param;
    (void)boot_param;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessAbortRequest(void) {
 return 0;
}

int APS5_VABI sceNpEntitlementAccessDeleteRequest(void) {
 return 0;
}

int APS5_VABI sceNpEntitlementAccessGenerateTransactionId(void* transaction_id) {
 if (!transaction_id) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessPollConsumeEntitlement(void) {
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpEntitlementAccessRequestConsumeUnifiedEntitlement(void) {
 return SCE_NP_ERROR_SIGNED_OUT;
}


int APS5_VABI sceNpEntitlementAccessRequestConsumeServiceEntitlement(void) {
 return SCE_NP_ERROR_SIGNED_OUT;
}


int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessPollUnifiedEntitlementInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessRequestUnifiedEntitlementInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessGetPftFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfoList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessPollUnifiedEntitlementInfoList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfoList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEntitlementAccessRequestUnifiedEntitlementInfoList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
