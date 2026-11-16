#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceShareInternal.native/include/InventoryExports.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81960002u);

bool CopyFirstByte(char* destination, const char* source) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, 1);
    errno = savedErrno;
    return copied;
}

} // namespace

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI sceShareEventCallbackProcessed() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareFeaturePermitCompatible(std::int32_t id) {
    if (id == -1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareFeatureProhibitCompatible(std::int32_t id) {
    if (id == -1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareGetContentParam(std::int32_t id, void* argument2, std::size_t size, void* argument4) {
    if (id == -1 || size == 0 || size > 0x1800 || argument2 == nullptr || argument4 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareGetStoredAppStatus(std::int32_t id, std::uint32_t argument2, void* status) {
    if (status == nullptr || id == -1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareInitializeForShellCore() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareInitializeInternal() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareRegisterEventCallback(std::int32_t argument1, std::uint32_t argument2, const void* callback) {
    if (callback == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareSetCurrentStatus(std::int32_t id, std::uint32_t argument2, const void* status) {
    if (status == nullptr || id == -1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareSetUploadEventInfo(std::int32_t argument1, const void* argument2, const void* argument3) {
    if (argument2 == nullptr || argument3 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareTerminateForShellCore() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareTerminateInternal() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareUnregisterEventCallback(const void* callback) {
    if (callback == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceShareInternal_nativeUnknown__plus_LB32s0kCYg_nid_no_patch(std::int32_t id) {
    if (id == -1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+LB32s0kCYg", libSceShareInternal_nativeUnknown__plus_LB32s0kCYg_nid_no_patch);

int APS5_VABI libSceShareInternal_nativeUnknown_0IbPXX_minus_WvQQ_nid_no_patch(std::int32_t argument1, const void* argument2) {
    if (argument2 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0IbPXX-WvQQ", libSceShareInternal_nativeUnknown_0IbPXX_minus_WvQQ_nid_no_patch);

int APS5_VABI libSceShareInternal_nativeUnknown_8E7VGc9qRvY_nid_no_patch(std::int32_t id, void* argument2, void* argument3, void* argument4) {
    if (id == -1 || argument2 == nullptr || argument3 == nullptr || argument4 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8E7VGc9qRvY", libSceShareInternal_nativeUnknown_8E7VGc9qRvY_nid_no_patch);

int APS5_VABI libSceShareInternal_nativeUnknown_9fIEK6i1idg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9fIEK6i1idg", libSceShareInternal_nativeUnknown_9fIEK6i1idg_nid_no_patch);

int APS5_VABI libSceShareInternal_nativeUnknown_9oywh7VdSjM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9oywh7VdSjM", libSceShareInternal_nativeUnknown_9oywh7VdSjM_nid_no_patch);

int APS5_VABI libSceShareInternal_nativeUnknown_IvkDssl52mA_nid_no_patch(std::int32_t argument1, void* argument2) {
    if (argument2 == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("IvkDssl52mA", libSceShareInternal_nativeUnknown_IvkDssl52mA_nid_no_patch);

int APS5_VABI libSceShareInternal_nativeUnknown_zyzD8P5ZKPA_nid_no_patch(const char* name, void* argument2, std::size_t size, void* argument4) {
    if (name == nullptr || argument2 == nullptr || argument4 == nullptr) return ErrorInvalidArgument;
    if (size == 0 || size > 0x10) return ErrorInvalidArgument;
    char first = 0;
    if (!CopyFirstByte(&first, name)) NotImplemented_nid_no_patch(__func__);
    if (first == 0) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zyzD8P5ZKPA", libSceShareInternal_nativeUnknown_zyzD8P5ZKPA_nid_no_patch);

}
