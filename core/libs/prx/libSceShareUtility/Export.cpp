#include "prx/libSceShareUtility/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceShareUtilityAdvanceFromFileWithContentIdList() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceFromFileWithTitleIdList() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceGetServiceInfo() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceInitialize(std::uint64_t argument1, std::uint64_t argument2) {
    if (argument1 != 2 || argument2 < 0x20000) return static_cast<int>(0x81170003u);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceQueryServiceInfo() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceQueryServiceInfoA() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceResetPlayingContent() {
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityAdvanceSetPlayingContent(std::int64_t argument1) {
    if (argument1 < 0) return static_cast<int>(0x81170003u);
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityAdvanceSetUploadContentData(const void* argument1, std::uint64_t argument2) {
    if (argument1 == nullptr || argument2 == 0) return static_cast<int>(0x81170003u);
    if (argument2 > 0x1fff) return static_cast<int>(0x81170004u);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceTerminate() {
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityAdvanceValidateContents() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenu(std::int64_t argument1, std::uint32_t argument2) {
    if (argument2 < 1 || argument2 > 4 || argument1 < 0) return static_cast<int>(0x81170003u);
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuBroadcast() {
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuDefault() {
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuForShareContent(const void* argument1, std::uint64_t argument2) {
    if (argument1 == nullptr || argument2 == 0) return static_cast<int>(0x81170003u);
    if (argument2 >= 0x1000) return static_cast<int>(0x81170004u);
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityInitialize(std::uint64_t argument1, std::uint64_t argument2) {
    if (argument1 != 1 || argument2 < 0x20000) return static_cast<int>(0x81170003u);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityInitializeEx2(std::uint64_t argument1, std::uint64_t argument2) {
    if (argument1 != 1 || argument2 < 0x20000) return static_cast<int>(0x81170003u);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenu(std::int64_t argument1, std::uint32_t argument2) {
    if (argument2 < 1 || argument2 > 4 || argument1 < 0) return static_cast<int>(0x81170003u);
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityOpenShareMenuBroadcast() {
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityOpenShareMenuDefault() {
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityOpenShareMenuForShareContent(const void* argument1, std::uint64_t argument2) {
    if (argument1 == nullptr || argument2 == 0) return static_cast<int>(0x81170003u);
    if (argument2 >= 0x1000) return static_cast<int>(0x81170004u);
    return static_cast<int>(0x81170102u);
}

int APS5_VABI sceShareUtilityTerminate() {
    return static_cast<int>(0x81170102u);
}

}
