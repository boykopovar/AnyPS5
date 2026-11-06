#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVdecShevc/include/InventoryExports.hpp"
#include "prx/libSceVdecShevc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceSdecCreateSwHevc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecDestroySwHevc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecExecuteSwHevc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecGetFrameSwHevc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceSdecGetVersionSwHevc(std::uint32_t* version) {
    if (version != nullptr) *version = 0x13590001u;
}

int APS5_VABI sceSdecQueryMemorySwHevc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecResetSwHevc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceVdecShevcUnknown_79qqGAVnWfA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("79qqGAVnWfA", libSceVdecShevcUnknown_79qqGAVnWfA_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_PTO17yTeoSk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PTO17yTeoSk", libSceVdecShevcUnknown_PTO17yTeoSk_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_hZXstu74B4I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hZXstu74B4I", libSceVdecShevcUnknown_hZXstu74B4I_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_lLaAHswK1zQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lLaAHswK1zQ", libSceVdecShevcUnknown_lLaAHswK1zQ_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_wvq_plus_GPpswaw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wvq+GPpswaw", libSceVdecShevcUnknown_wvq_plus_GPpswaw_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_xbRElcEVmdM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xbRElcEVmdM", libSceVdecShevcUnknown_xbRElcEVmdM_nid_no_patch);

}
