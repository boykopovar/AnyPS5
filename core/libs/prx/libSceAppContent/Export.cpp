#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceAppContentAddcontMount(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, AppContentMountPoint* mount_point) {
 (void)service_label;
 (void)entitlement_label;
 (void)mount_point;
 return 0;
}

int sceAppContentAddcontUnmount(const AppContentMountPoint* mount_point) {
 (void)mount_point;
 return 0;
}

int sceAppContentAppParamGetInt(uint32_t param_id, int32_t* value) {
 (void)param_id;
 (void)value;
 return 0;
}

int sceAppContentDownloadDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
 (void)mount_point;
 (void)available_space_kb;
 return 0;
}

int sceAppContentInitialize(const AppContentInitParam* init_param, AppContentBootParam* boot_param) {
 (void)init_param;
 (void)boot_param;
 return 0;
}

int sceAppContentTemporaryDataFormat(const AppContentMountPoint* mount_point) {
 (void)mount_point;
 return 0;
}

int sceAppContentTemporaryDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
 (void)mount_point;
 (void)available_space_kb;
 return 0;
}

int sceAppContentTemporaryDataMount2(uint32_t option, AppContentMountPoint* mount_point) {
 (void)option;
 (void)mount_point;
 return 0;
}

}
