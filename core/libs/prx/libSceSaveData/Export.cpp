#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceSaveDataBackup(const SaveDataBackup* backup) {
 (void)backup;
 return 0;
}

int sceSaveDataDelete(const SaveDataDelete* del) {
 (void)del;
 return 0;
}

int sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
 (void)cond;
 (void)result;
 return 0;
}

int sceSaveDataGetEventResult(const void* event_param, SaveDataEvent* event) {
 (void)event_param;
 (void)event;
 return 0;
}

int sceSaveDataGetMountInfo(const SaveDataMountPoint* mount_point, SaveDataMountInfo* info) {
 (void)mount_point;
 (void)info;
 return 0;
}

int sceSaveDataGetParam(const SaveDataMountPoint* mount_point, uint32_t param_type, void* param_buf, size_t param_buf_size, size_t* got_size) {
 (void)mount_point;
 (void)param_type;
 (void)param_buf;
 (void)param_buf_size;
 (void)got_size;
 return 0;
}

int sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* get_param) {
 (void)get_param;
 return 0;
}

int sceSaveDataInitialize3(const void* init) {
 (void)init;
 return 0;
}

int sceSaveDataLoadIcon(const SaveDataMountPoint* mount_point, SaveDataIcon* icon) {
 (void)mount_point;
 (void)icon;
 return 0;
}

int sceSaveDataSaveIcon(const SaveDataMountPoint* mount_point, const SaveDataIcon* icon) {
 (void)mount_point;
 (void)icon;
 return 0;
}

int sceSaveDataSetParam(const SaveDataMountPoint* mount_point, uint32_t param_type, const void* param_buf, size_t param_buf_size) {
 (void)mount_point;
 (void)param_type;
 (void)param_buf;
 (void)param_buf_size;
 return 0;
}

int sceSaveDataSetSaveDataMemory2(const SaveDataMemorySet2* set_param) {
 (void)set_param;
 return 0;
}

int sceSaveDataSetupSaveDataMemory2(const SaveDataMemorySetup2* setup_param, SaveDataMemorySetupResult* result) {
 (void)setup_param;
 (void)result;
 return 0;
}

int sceSaveDataSyncSaveDataMemory(const void* sync_param) {
 (void)sync_param;
 return 0;
}

int sceSaveDataTerminate(void) {
 return 0;
}

int sceSaveDataTransferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
 (void)mount;
 (void)mount_result;
 return 0;
}

}
