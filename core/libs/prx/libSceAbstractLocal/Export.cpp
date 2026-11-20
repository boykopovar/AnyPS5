#include "prx/libSceAbstractLocal/include/Exports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAbstractLocal/include/Firmware1340Exports.hpp"

namespace {

constexpr int SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED = static_cast<int>(0x809E0005);
constexpr int SCE_ABSTRACT_STORAGE_ERROR_809E0002 = static_cast<int>(0x809E0002);

}

extern "C" {


int APS5_VABI _ZN15AbstractStorage11LocalFolder10NewContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder12RemoveFolderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder12readExternalESt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder13RemoveContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder13writeExternalESt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder4OpenENS_8ItemTypeE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder5CloseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder6RemoveEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder8NextItemEPSt10shared_ptrINS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolder9NewFolderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_6FolderEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolderC1ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolderC2ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolderD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolderD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage11LocalFolderD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent12readExternalESt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent13ParseMetadataEPN23sceMetadataReaderWriter8MetadataES3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent13writeExternalESt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent4OpenEit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent4ReadEPvlPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent5CloseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent5FlushEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent5WriteEPKvlPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent6RemoveEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent6RenameERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent8SeekByteEliPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContent8SeekTimeElPl() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage12LocalContentC1ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContentC2ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContentD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContentD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalContentD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalService13SearchStorageEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalService18append_storagelistERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalService19isSandBoxRandomWordERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalService20GetSandBoxRandomWordEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalService23create_storage_instanceERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalService4StopEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalService5StartEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalServiceC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalServiceC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalServiceD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalServiceD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalServiceD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage11DeserializeESt10shared_ptrINS_7ContentEEPS1_INS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage12readExternalESt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage13GetRootFolderEPSt10shared_ptrINS_6FolderEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage13writeExternalESt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage14GetStorageSizeEPlS1_S1_() {
    return SCE_ABSTRACT_STORAGE_ERROR_809E0002;
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage15CreateRecursiveERKSbIcSt11char_traitsIcENS_8StlAllocIcEEENS_8ItemTypeEPSt10shared_ptrINS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage15RemoveRecursiveERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage5ChModERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEt() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage7GetItemERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage7IsExistERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorage9SerializeESt10shared_ptrINS_7ContentEES1_INS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorageC1ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorageC2ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorageD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorageD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage12LocalStorageD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK15AbstractStorage11LocalFolder10GetServiceEv() {
    return 0;
}

int APS5_VABI _ZNK15AbstractStorage11LocalFolder13GetCapabilityEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK15AbstractStorage11LocalFolder7GetStatEPNS_4StatE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK15AbstractStorage12LocalContent10GetServiceEv() {
    return 0;
}

int APS5_VABI _ZNK15AbstractStorage12LocalContent13GetCapabilityEv() {
    return 0x37C5A;
}

int APS5_VABI _ZNK15AbstractStorage12LocalContent7GetStatEPNS_4StatE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK15AbstractStorage12LocalService14GetServiceTypeEv() {
    return 0;
}

int APS5_VABI _ZNK15AbstractStorage12LocalStorage10GetServiceEv() {
    return 0;
}

int APS5_VABI _ZNK15AbstractStorage12LocalStorage13GetCapabilityEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK15AbstractStorage12LocalStorage7GetStatEPNS_4StatE() {
    return SCE_ABSTRACT_STORAGE_ERROR_809E0002;
}

}

extern "C" {

int APS5_VABI libSceAbstractLocalUnknown_33cJ1JkYdJc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("33cJ1JkYdJc", libSceAbstractLocalUnknown_33cJ1JkYdJc_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_5wBIe_minus_gNyM8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5wBIe-gNyM8", libSceAbstractLocalUnknown_5wBIe_minus_gNyM8_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_83niX2DjTKU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("83niX2DjTKU", libSceAbstractLocalUnknown_83niX2DjTKU_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_9gXVjWBg6Vc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9gXVjWBg6Vc", libSceAbstractLocalUnknown_9gXVjWBg6Vc_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_ASP58628nNk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ASP58628nNk", libSceAbstractLocalUnknown_ASP58628nNk_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_CsrB2d5fqXc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CsrB2d5fqXc", libSceAbstractLocalUnknown_CsrB2d5fqXc_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_Fo_minus_j2TW1I4A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Fo-j2TW1I4A", libSceAbstractLocalUnknown_Fo_minus_j2TW1I4A_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_N0PK9QVE7IM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("N0PK9QVE7IM", libSceAbstractLocalUnknown_N0PK9QVE7IM_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_N9d7h1oUwxw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("N9d7h1oUwxw", libSceAbstractLocalUnknown_N9d7h1oUwxw_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_NnpFbd_plus_hP9M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NnpFbd+hP9M", libSceAbstractLocalUnknown_NnpFbd_plus_hP9M_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_RubXT69uja8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RubXT69uja8", libSceAbstractLocalUnknown_RubXT69uja8_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_RySoEh5cuv8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RySoEh5cuv8", libSceAbstractLocalUnknown_RySoEh5cuv8_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_SCjSJz8k_plus__minus_8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SCjSJz8k+-8", libSceAbstractLocalUnknown_SCjSJz8k_plus__minus_8_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_XmaVjxFnCYc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XmaVjxFnCYc", libSceAbstractLocalUnknown_XmaVjxFnCYc_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_Yleiq77uqnA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Yleiq77uqnA", libSceAbstractLocalUnknown_Yleiq77uqnA_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_Z9GeusgsR_minus_k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Z9GeusgsR-k", libSceAbstractLocalUnknown_Z9GeusgsR_minus_k_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_ZAGiwYfmZ1Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZAGiwYfmZ1Y", libSceAbstractLocalUnknown_ZAGiwYfmZ1Y_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_ZtNI0tJp8l4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZtNI0tJp8l4", libSceAbstractLocalUnknown_ZtNI0tJp8l4_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_b6YnvaVuz98_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("b6YnvaVuz98", libSceAbstractLocalUnknown_b6YnvaVuz98_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_ej8XyLEsij8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ej8XyLEsij8", libSceAbstractLocalUnknown_ej8XyLEsij8_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_fOagu5wDPoA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fOagu5wDPoA", libSceAbstractLocalUnknown_fOagu5wDPoA_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_gGZnUQvO3lg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gGZnUQvO3lg", libSceAbstractLocalUnknown_gGZnUQvO3lg_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_kvhfgcFt_plus_6A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("kvhfgcFt+6A", libSceAbstractLocalUnknown_kvhfgcFt_plus_6A_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_lCH1FbQPEMc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lCH1FbQPEMc", libSceAbstractLocalUnknown_lCH1FbQPEMc_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_pdxlj4_plus_QrPI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pdxlj4+QrPI", libSceAbstractLocalUnknown_pdxlj4_plus_QrPI_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_rVur_minus_G1kDzs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rVur-G1kDzs", libSceAbstractLocalUnknown_rVur_minus_G1kDzs_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_rrZB1wgZwAg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rrZB1wgZwAg", libSceAbstractLocalUnknown_rrZB1wgZwAg_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_wA7KkfIrQdg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wA7KkfIrQdg", libSceAbstractLocalUnknown_wA7KkfIrQdg_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_wylE5x1ccoY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wylE5x1ccoY", libSceAbstractLocalUnknown_wylE5x1ccoY_nid_no_patch);

int APS5_VABI libSceAbstractLocalUnknown_yQwF9DsUl30_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yQwF9DsUl30", libSceAbstractLocalUnknown_yQwF9DsUl30_nid_no_patch);

}
