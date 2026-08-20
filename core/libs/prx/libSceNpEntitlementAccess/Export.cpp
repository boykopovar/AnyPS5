#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNpEntitlementAccessGetAddcontEntitlementInfo(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, NpEntitlementAccessAddcontEntitlementInfo* info) {
 (void)service_label;
 (void)entitlement_label;
 (void)info;
 return 0;
}

int sceNpEntitlementAccessGetAddcontEntitlementInfoList(uint32_t service_label, NpEntitlementAccessAddcontEntitlementInfo* list, uint32_t list_num, uint32_t* hit_num) {
 (void)service_label;
 (void)list;
 (void)list_num;
 (void)hit_num;
 return 0;
}

int sceNpEntitlementAccessGetSkuFlag(uint32_t* sku_flag) {
 (void)sku_flag;
 return 0;
}

int sceNpEntitlementAccessInitialize(const NpEntitlementAccessInitParam* init_param, NpEntitlementAccessBootParam* boot_param) {
 (void)init_param;
 (void)boot_param;
 return 0;
}

}
