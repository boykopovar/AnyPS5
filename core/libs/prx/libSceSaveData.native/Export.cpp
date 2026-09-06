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

int sceSaveDataCommit(const SaveDataCommitParam* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataCreateTransactionResource(uint32_t size) {
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDelete(const SaveDataDelete* del) {
 (void)del;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDeleteTransactionResource(int32_t resource) {
 (void)resource;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataDirNameSearch(const SaveDataDirNameSearchCond* cond, SaveDataDirNameSearchResult* result) {
 (void)cond;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataGetEventResult(const void* event_param, SaveDataEvent* event) {
 (void)event_param;
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataGetMountInfo(const SaveDataMountPoint* mount_point, SaveDataMountInfo* info) {
 (void)mount_point;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataGetParam(const SaveDataMountPoint* mount_point, uint32_t param_type, void* param_buf, size_t param_buf_size, size_t* got_size) {
 (void)mount_point;
 (void)param_type;
 (void)param_buf;
 (void)param_buf_size;
 (void)got_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataGetSaveDataMemory2(SaveDataMemoryGet2* get_param) {
 (void)get_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataInitialize3(const void* init) {
 (void)init;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataLoadIcon(const SaveDataMountPoint* mount_point, SaveDataIcon* icon) {
 (void)mount_point;
 (void)icon;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataMount3(const SaveDataMount3* mount, SaveDataMountResult* mount_result) {
 (void)mount;
 (void)mount_result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataPrepare(const SaveDataMountPoint* mount_point, const SaveDataPrepareParam* param) {
 (void)mount_point;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataSaveIcon(const SaveDataMountPoint* mount_point, const SaveDataIcon* icon) {
 (void)mount_point;
 (void)icon;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataSaveIconByPath(const SaveDataMountPoint* mount_point, const char* path) {
 (void)mount_point;
 (void)path;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataSetParam(const SaveDataMountPoint* mount_point, uint32_t param_type, const void* param_buf, size_t param_buf_size) {
 (void)mount_point;
 (void)param_type;
 (void)param_buf;
 (void)param_buf_size;
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

int sceSaveDataTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataTransferringMount(const SaveDataTransferringMount* mount, SaveDataMountResult* mount_result) {
 (void)mount;
 (void)mount_result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceSaveDataUmount2(uint32_t mode, const SaveDataMountPoint* mount_point) {
 (void)mode;
 (void)mount_point;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
