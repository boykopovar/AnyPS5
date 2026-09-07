#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceSaveDataBackup(const SaveDataBackup* backup) {
 (void)backup;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataGetEventResult(const void* event_param, SaveDataEvent* event) {
 (void)event_param;
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* get_param) {
 (void)get_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2* set_param) {
 (void)set_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2* setup_param, SaveDataMemorySetupResult* result) {
 (void)setup_param;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataSyncSaveDataMemory(const void* sync_param) {
 (void)sync_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataTransferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
 (void)mount;
 (void)mount_result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
