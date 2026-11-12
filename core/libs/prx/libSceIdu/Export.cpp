#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceIdu/include/InventoryExports.hpp"
#include "prx/libSceIdu/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_IDU_ERROR_INVALID_ARGUMENT = static_cast<int>(0xFFFFFF99u);
constexpr int SCE_IDU_ERROR_NOT_INITIALIZED = static_cast<int>(0xFFFFFF9Au);

std::uint32_t ReadWord(const void* source, const char* function) {
    std::uint32_t value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value))) NotImplemented_nid_no_patch(function);
    return value;
}

bool IsEmptyString(const char* text, const char* function) {
    char first = 0;
    if (!GuestCopyIn_nid_no_patch(&first, text, sizeof(first))) NotImplemented_nid_no_patch(function);
    return first == 0;
}

} // namespace

extern "C" {

int APS5_VABI sceIduGetEnv() {
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduInit(const void* param) {
    if (param == nullptr) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    if (ReadWord(param, __func__) != 8) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIduInstallPkg(const void* first, const void* second) {
    if (first == nullptr || second == nullptr) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduIsAppInstalled() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceIduIsRifInstalled(const char* name) {
    if (name == nullptr || IsEmptyString(name, __func__)) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduIsStaffMode() {
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduReboot() {
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduTerm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceIduGetDiscTitleInfo(void* info, std::uint32_t count) {
    if ((info == nullptr) == (count != 0)) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduGetFsStats(void* stats) {
    if (stats == nullptr || ReadWord(stats, __func__) != 0x18) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduGetHardwareId(void* id) {
    if (id == nullptr) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduGetPkgInstallStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceIduGetTitleList(void* list, std::int32_t count) {
    if ((list == nullptr) == (count != 0)) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduInstallRif(const char* name, const void* data, std::uint32_t size) {
    if (name == nullptr || IsEmptyString(name, __func__)) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    if (data == nullptr || size == 0) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduNotificationHide(const char* id) {
    if (id == nullptr || IsEmptyString(id, __func__)) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceIduNotificationShow() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceIduStartTitle(const void* title, std::int32_t count, const void* items, const void* param) {
    if (title == nullptr) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    if ((count > 0) != (items != nullptr) || param == nullptr) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    if (ReadWord(param, __func__) != 0xC) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}

int APS5_VABI libSceIduUnknown_3XKI6v75VEo_nid_no_patch() {
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("3XKI6v75VEo", libSceIduUnknown_3XKI6v75VEo_nid_no_patch);

int APS5_VABI libSceIduUnknown_J9y431a1uM0_nid_no_patch() {
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("J9y431a1uM0", libSceIduUnknown_J9y431a1uM0_nid_no_patch);

int APS5_VABI libSceIduUnknown_tmeq5nZRqIs_nid_no_patch() {
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("tmeq5nZRqIs", libSceIduUnknown_tmeq5nZRqIs_nid_no_patch);

int APS5_VABI libSceIduUnknown_uoHZCJW8Cdk_nid_no_patch(void* out) {
    if (out == nullptr) return SCE_IDU_ERROR_INVALID_ARGUMENT;
    return SCE_IDU_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("uoHZCJW8Cdk", libSceIduUnknown_uoHZCJW8Cdk_nid_no_patch);

}
