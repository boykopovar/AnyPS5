#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>

static constexpr int SCE_APP_CONTENT_ERROR_PARAMETER = static_cast<int>(0x80D90002);
static constexpr uint32_t APPPARAM_ID_SKU_FLAG = 1;
static constexpr int32_t SKU_FLAG_FULL = 3;

extern "C" {

int APS5_VABI sceAppContentAddcontMount(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, AppContentMountPoint* mount_point) {
 (void)service_label;
 (void)entitlement_label;
 (void)mount_point;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint* mount_point) {
 (void)mount_point;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAppContentAppParamGetInt(uint32_t param_id, int32_t* value) {
    if (!value) return SCE_APP_CONTENT_ERROR_PARAMETER;
    switch (param_id) {
    case APPPARAM_ID_SKU_FLAG:
        *value = SKU_FLAG_FULL;
        return 0;
    case 2: case 3: case 4: case 5:
        *value = 0;
        return 0;
    default:
        return SCE_APP_CONTENT_ERROR_PARAMETER;
    }
}

int APS5_VABI sceAppContentDownloadDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
 (void)mount_point;
 (void)available_space_kb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAppContentInitialize(const AppContentInitParam* init_param, AppContentBootParam* boot_param) {
    (void)init_param;
    if (!boot_param) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::memset(boot_param, 0, sizeof(*boot_param));
    return 0;
}

int APS5_VABI sceAppContentTemporaryDataFormat(const AppContentMountPoint* mount_point) {
 (void)mount_point;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAppContentTemporaryDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
 (void)mount_point;
 (void)available_space_kb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAppContentTemporaryDataMount2(uint32_t option, AppContentMountPoint* mount_point) {
 (void)option;
 (void)mount_point;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
