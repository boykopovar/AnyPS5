#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI _ZN3sce16CommonDialogUtil12getSelfAppIdEv();
int APS5_VABI _ZN3sce16CommonDialogUtil6Client11closeModuleEv(void* self);
int APS5_VABI _ZN3sce16CommonDialogUtil6Client11updateStateEv(void* self);
int APS5_VABI _ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv(void* self);
int APS5_VABI _ZN3sce16CommonDialogUtil6ClientD0Ev();
int APS5_VABI _ZN3sce16CommonDialogUtil6ClientD1Ev();
int APS5_VABI _ZN3sce16CommonDialogUtil6ClientD2Ev();
int APS5_VABI _ZNK3sce16CommonDialogUtil6Client10isCloseReqEv(const void* self);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm(const void* self, void* data, std::uint64_t size);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Client14getClientStateEv(const void* self);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv(const void* self);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Client8getAppIdEv(const void* self);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Client8isFinishEv(const void* self);
int APS5_VABI _ZNK3sce16CommonDialogUtil6Client9getResultEv(const void* self);
int APS5_VABI libSceCommonDialogUnknown_Zaa6fhiKQDE_nid_no_patch(const void* self);
int APS5_VABI libSceCommonDialogUnknown_SDpCfY9uB0g_nid_no_patch(const void* self);
int APS5_VABI libSceCommonDialogUnknown_mVRnPerBcK0_nid_no_patch(const void* self);
int APS5_VABI libSceCommonDialogUnknown_yxjgDvqUbGQ_nid_no_patch(const void* self);
int APS5_VABI libSceCommonDialogUnknown_aUS4PgJye98_nid_no_patch(const void* self);
int APS5_VABI libSceCommonDialogUnknown_D_minus_V35OhFeIM_nid_no_patch();
int APS5_VABI libSceCommonDialogUnknown_p9TTq4bLdFU_nid_no_patch();
int APS5_VABI libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch(const void* param);

}
