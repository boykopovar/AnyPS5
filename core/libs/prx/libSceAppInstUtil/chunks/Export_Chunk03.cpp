extern "C" {
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAppInstUtil/include/InventoryExports.hpp"
#include "prx/libSceAppInstUtil/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"

static std::mutex g_serviceMutex;
static int g_serviceReferences = 0;
static constexpr int ErrorNullArgument = static_cast<int>(0x80A30003);
static constexpr int ErrorNotInitialized = static_cast<int>(0x80A31000);

int APS5_VABI sceAppInstUtilGetNeededStorageSize(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetStorageDestType(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilInstallAddons(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppConvertAppDiscToDownload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetCustomThemeInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppGetInstallAddcontFromDiscStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallCloudGame(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallMediaPlayer(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppInstallRequestAddcontFromDisc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppInstUtilAppSetSavedataUploadStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {





int APS5_VABI libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("2WQuFX8yYXg", libSceAppInstUtilUnknown_2WQuFX8yYXg_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("4vqRxwpIE1M", libSceAppInstUtilUnknown_4vqRxwpIE1M_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_9QIC1kZpSG4_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("9QIC1kZpSG4", libSceAppInstUtilUnknown_9QIC1kZpSG4_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch(const void* argument1, const void* argument2, std::uint64_t argument3, const void* argument4) {
    if (argument1 == nullptr || argument2 == nullptr || argument4 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("K73u2VX0JFo", libSceAppInstUtilUnknown_K73u2VX0JFo_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_LsUZe38RhCs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LsUZe38RhCs", libSceAppInstUtilUnknown_LsUZe38RhCs_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_N6H7LMfZD5Q_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("N6H7LMfZD5Q", libSceAppInstUtilUnknown_N6H7LMfZD5Q_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_NXqdUFpinmo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NXqdUFpinmo", libSceAppInstUtilUnknown_NXqdUFpinmo_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("QMjuGVPtjUY", libSceAppInstUtilUnknown_QMjuGVPtjUY_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch(const void* argument1, const void* argument2, const void* argument3) {
    if (argument1 == nullptr || argument2 == nullptr || argument3 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("R-OUApMZ1d8", libSceAppInstUtilUnknown_R_minus_OUApMZ1d8_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_T9h6l8bFKGE_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("T9h6l8bFKGE", libSceAppInstUtilUnknown_T9h6l8bFKGE_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_X5Q7QxodFcg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("X5Q7QxodFcg", libSceAppInstUtilUnknown_X5Q7QxodFcg_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_XWjOLtdgxa4_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("XWjOLtdgxa4", libSceAppInstUtilUnknown_XWjOLtdgxa4_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Y+ln-TkY+hA", libSceAppInstUtilUnknown_Y_plus_ln_minus_TkY_plus_hA_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch(const void* argument1, std::uint64_t argument2, const void* argument3, const void* argument4) {
    if (argument1 == nullptr || argument3 == nullptr || argument4 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("Zv+26GDSKcQ", libSceAppInstUtilUnknown_Zv_plus_26GDSKcQ_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_ecad2GAw55Y_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("ecad2GAw55Y", libSceAppInstUtilUnknown_ecad2GAw55Y_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_gFlBHjtrMDI_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("gFlBHjtrMDI", libSceAppInstUtilUnknown_gFlBHjtrMDI_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_gP27iU_plus_aZFw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gP27iU+aZFw", libSceAppInstUtilUnknown_gP27iU_plus_aZFw_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_grC16befiso_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("grC16befiso", libSceAppInstUtilUnknown_grC16befiso_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_j80u95nZ_plus_3A_nid_no_patch() {
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("j80u95nZ+3A", libSceAppInstUtilUnknown_j80u95nZ_plus_3A_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_j9qjbjMifhQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("j9qjbjMifhQ", libSceAppInstUtilUnknown_j9qjbjMifhQ_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_pbYNtR2enrY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pbYNtR2enrY", libSceAppInstUtilUnknown_pbYNtR2enrY_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("qn0aziKw-kE", libSceAppInstUtilUnknown_qn0aziKw_minus_kE_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_qxjCWxg8ki4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qxjCWxg8ki4", libSceAppInstUtilUnknown_qxjCWxg8ki4_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_vZ1SWJ0y4mo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("vZ1SWJ0y4mo", libSceAppInstUtilUnknown_vZ1SWJ0y4mo_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_xV7MhF_plus_9AQk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xV7MhF+9AQk", libSceAppInstUtilUnknown_xV7MhF_plus_9AQk_nid_no_patch);

int APS5_VABI libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch(const void* argument1) {
    if (argument1 == nullptr) return ErrorNullArgument;
    if (!ServiceInitialized()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("yYVV-f0zoWY", libSceAppInstUtilUnknown_yYVV_minus_f0zoWY_nid_no_patch);

}
