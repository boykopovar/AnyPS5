#include "prx/libSceAbstractTwitter/include/Exports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder13RemoveContentERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
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
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage13TwitterFolder9NewFolderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_6FolderEE() {
    NotImplemented_nid_no_patch(__func__);
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
    NotImplemented_nid_no_patch(__func__);
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
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage11DeserializeESt10shared_ptrINS_7ContentEEPS1_INS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage13GetRootFolderEPSt10shared_ptrINS_6FolderEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage14GetStorageSizeEPlS1_S1_() {
    return SCE_ABSTRACT_STORAGE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage15CreateRecursiveERKSbIcSt11char_traitsIcENS_8StlAllocIcEEENS_8ItemTypeEPSt10shared_ptrINS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage15RemoveRecursiveERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage7GetItemERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage7IsExistERKSbIcSt11char_traitsIcENS_8StlAllocIcEEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage9GetForderERKSbIcSt11char_traitsIcENS_8StlAllocIcEEEPSt10shared_ptrINS_6FolderEE() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZN15AbstractStorage14TwitterStorage9SerializeESt10shared_ptrINS_7ContentEES1_INS_4ItemEE() {
    NotImplemented_nid_no_patch(__func__);
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
