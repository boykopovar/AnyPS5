#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Resource registration feeds GPU debugging tools, none of which are attached; the game treats this code as benign.
static constexpr int SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE = static_cast<int>(0x8A6C9018);

extern "C" {

int APS5_VABI sceAgcDriverRegisterOwner(uint32_t* owner_handle, const char* name) {
    (void)owner_handle;
    (void)name;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverRegisterResource(uint32_t* resource_handle, uint32_t owner_handle, const void* memory, size_t size, const char* name, uint32_t type, uint64_t user_data) {
    (void)resource_handle;
    (void)owner_handle;
    (void)memory;
    (void)size;
    (void)name;
    (void)type;
    (void)user_data;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverRegisterWorkloadStream(uint32_t stream_id, const void* stream) {
 (void)stream_id;
 (void)stream;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverUnregisterOwnerAndResources(uint32_t owner_handle) {
    (void)owner_handle;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverUnregisterResource(uint32_t resource_handle) {
    (void)resource_handle;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

}
