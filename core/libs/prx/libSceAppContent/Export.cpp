#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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
 (void)param_id;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAppContentDownloadDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
 (void)mount_point;
 (void)available_space_kb;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAppContentInitialize(const AppContentInitParam* init_param, AppContentBootParam* boot_param) {
 (void)init_param;
 (void)boot_param;
 NotImplemented_nid_no_patch(__func__);
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
