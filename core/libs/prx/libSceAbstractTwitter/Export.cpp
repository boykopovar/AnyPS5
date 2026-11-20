#include "prx/libSceAbstractTwitter/include/Exports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAbstractTwitter/include/Firmware1340Exports.hpp"

namespace {

constexpr int SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED = static_cast<int>(0x809E0005);

}

extern "C" {


int APS5_VABI _ZN15AbstractStorage10TwitterAPI11SendContentEPKvl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage10TwitterAPIC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage10TwitterAPID1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder10NewContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder12RemoveFolderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder13RemoveContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder4OpenENS_8ItemTypeE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder5CloseEv() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder6RemoveEb() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder8NextItemEPSt10shared_ptrINS_4ItemEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder9NewFolderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_6FolderEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolderC1ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolderC2ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolderD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolderD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolderD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent11SetMetadataEPN23sceMetadataReaderWriter8MetadataE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent12readExternalESt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent13ParseMetadataEPN23sceMetadataReaderWriter8MetadataES3_() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent13writeExternalESt10shared_ptrINS_7ContentEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent4OpenEit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent4ReadEPvlPl() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent5CloseEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent5FlushEv() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent5WriteEPKvlPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent6RemoveEv() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent6RenameERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent8SeekByteEliPl() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContent8SeekTimeElPl() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterContentC1ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContentC2ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEb() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContentD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContentD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterContentD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterService13SearchStorageEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterService23create_storage_instanceERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterService4StopEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterService5StartEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterServiceC1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterServiceC2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterServiceD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterServiceD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterServiceD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage10GetContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_7ContentEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage11DeserializeESt10shared_ptrINS_7ContentEEPS1_INS_4ItemEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage13GetRootFolderEPSt10shared_ptrINS_6FolderEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage14GetStorageSizeEPlS1_S1_() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage15CreateRecursiveERKSbIcSt11char_traitsIcENS_8StlAllocIcEEENS_8ItemTypeEPSt10shared_ptrINS_4ItemEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage15RemoveRecursiveERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage7GetItemERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage7IsExistERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    return 0;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage9GetForderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_6FolderEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage9SerializeESt10shared_ptrINS_7ContentEES1_INS_4ItemEE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorageC1ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorageC2ERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorageD0Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorageD1Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorageD2Ev() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK15AbstractStorage13TwitterFolder10GetServiceEv() {
    return 3;
}

int APS5_VABI _ZNK15AbstractStorage13TwitterFolder13GetCapabilityEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK15AbstractStorage13TwitterFolder7GetStatEPNS_4StatE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZNK15AbstractStorage14TwitterContent10GetServiceEv() {
    return 3;
}

int APS5_VABI _ZNK15AbstractStorage14TwitterContent13GetCapabilityEv() {
    return 0x6C0A;
}

int APS5_VABI _ZNK15AbstractStorage14TwitterContent7GetStatEPNS_4StatE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZNK15AbstractStorage14TwitterService14GetServiceTypeEv() {
    return 3;
}

int APS5_VABI _ZNK15AbstractStorage14TwitterStorage10GetServiceEv() {
    return 3;
}

int APS5_VABI _ZNK15AbstractStorage14TwitterStorage13GetCapabilityEv() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK15AbstractStorage14TwitterStorage7GetStatEPNS_4StatE() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

}

extern "C" {

int APS5_VABI libSceAbstractTwitterUnknown__minus_F2Zy1e7FOY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-F2Zy1e7FOY", libSceAbstractTwitterUnknown__minus_F2Zy1e7FOY_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown__minus_eYRuWdVXik_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-eYRuWdVXik", libSceAbstractTwitterUnknown__minus_eYRuWdVXik_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_Ahg8qnhYlGk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Ahg8qnhYlGk", libSceAbstractTwitterUnknown_Ahg8qnhYlGk_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_CkKHcUqNv3k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CkKHcUqNv3k", libSceAbstractTwitterUnknown_CkKHcUqNv3k_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_EVhTLAOKj1w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("EVhTLAOKj1w", libSceAbstractTwitterUnknown_EVhTLAOKj1w_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_FGVLIQ_minus_SMcM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("FGVLIQ-SMcM", libSceAbstractTwitterUnknown_FGVLIQ_minus_SMcM_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_IdnWRFLM0vU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("IdnWRFLM0vU", libSceAbstractTwitterUnknown_IdnWRFLM0vU_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_MNUWDqzVtto_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MNUWDqzVtto", libSceAbstractTwitterUnknown_MNUWDqzVtto_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_NJhy7o_plus_CmlQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NJhy7o+CmlQ", libSceAbstractTwitterUnknown_NJhy7o_plus_CmlQ_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_PlZbHxbI3vU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PlZbHxbI3vU", libSceAbstractTwitterUnknown_PlZbHxbI3vU_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_RVG4i_plus_u_plus_7ZM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RVG4i+u+7ZM", libSceAbstractTwitterUnknown_RVG4i_plus_u_plus_7ZM_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_TQR0NQgtmO0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TQR0NQgtmO0", libSceAbstractTwitterUnknown_TQR0NQgtmO0_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_X9INiGFZd9U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("X9INiGFZd9U", libSceAbstractTwitterUnknown_X9INiGFZd9U_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_Yd0hJhW5Q84_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Yd0hJhW5Q84", libSceAbstractTwitterUnknown_Yd0hJhW5Q84_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_a_plus_PZyGBdVBc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("a+PZyGBdVBc", libSceAbstractTwitterUnknown_a_plus_PZyGBdVBc_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_aPWdr85yNks_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aPWdr85yNks", libSceAbstractTwitterUnknown_aPWdr85yNks_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_cI_minus_KT_minus_zsH7Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cI-KT-zsH7Q", libSceAbstractTwitterUnknown_cI_minus_KT_minus_zsH7Q_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_ciUyXfF69w4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ciUyXfF69w4", libSceAbstractTwitterUnknown_ciUyXfF69w4_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_f6Bas3Wsj0Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("f6Bas3Wsj0Y", libSceAbstractTwitterUnknown_f6Bas3Wsj0Y_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_lbL1i2qNNGs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lbL1i2qNNGs", libSceAbstractTwitterUnknown_lbL1i2qNNGs_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_rOc_plus_J0eLLqo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rOc+J0eLLqo", libSceAbstractTwitterUnknown_rOc_plus_J0eLLqo_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_tYmE1_plus_szuLc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tYmE1+szuLc", libSceAbstractTwitterUnknown_tYmE1_plus_szuLc_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_vU9dwLMvxIw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("vU9dwLMvxIw", libSceAbstractTwitterUnknown_vU9dwLMvxIw_nid_no_patch);

int APS5_VABI libSceAbstractTwitterUnknown_wzNc5Qv0uJc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wzNc5Qv0uJc", libSceAbstractTwitterUnknown_wzNc5Qv0uJc_nid_no_patch);

}
