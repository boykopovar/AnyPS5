#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Resource registration feeds GPU debugging tools, none of which are attached; the game treats this code as benign.
static constexpr int SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE = static_cast<int>(0x8A6C9018);
static constexpr uint32_t RESOURCE_REGISTRATION_MAX_NAME_LENGTH = 0xfc;

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

int APS5_VABI sceAgcDriverRegisterMultipleResources(uint32_t* resourceHandle, uint32_t ownerHandle, const void* resources, uint32_t resourceCount) {
    (void)resourceHandle;
    (void)ownerHandle;
    (void)resources;
    (void)resourceCount;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverRegisterWorkloadStream(uint32_t stream_id, const void* stream) {
 (void)stream_id;
 (void)stream;
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

int APS5_VABI sceAgcDriverRegisterGdsResource(uint32_t* resource_handle, uint32_t owner_handle, uint32_t gds_offset, uint32_t gds_size, const char* name, uint32_t type, uint64_t user_data) {
    (void)resource_handle;
    (void)owner_handle;
    (void)gds_offset;
    (void)gds_size;
    (void)name;
    (void)type;
    (void)user_data;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverUnregisterAllResourcesForOwner(uint32_t owner_handle) {
    (void)owner_handle;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverUnregisterWorkloadStream(uint32_t stream_id) {
    (void)stream_id;
    return 0;
}

int APS5_VABI sceAgcDriverGetDefaultOwner(uint32_t* owner_handle) {
    (void)owner_handle;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetResourceRegistrationMaxNameLength(uint32_t* max_length) {
    if (max_length == nullptr) APS5_INVALID_ARG_EX;
    *max_length = RESOURCE_REGISTRATION_MAX_NAME_LENGTH;
    return 0;
}

int APS5_VABI sceAgcDriverGetOwnerName(uint32_t owner_handle, const char** name) {
    (void)owner_handle;
    (void)name;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetResourceName(uint32_t resource_handle, const char** name) {
    (void)resource_handle;
    (void)name;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetResourceType(uint32_t resource_handle, uint32_t* type) {
    (void)resource_handle;
    (void)type;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetResourceUserData(uint32_t resource_handle, uint64_t* user_data) {
    (void)resource_handle;
    (void)user_data;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetResourceUserData(uint32_t resource_handle, uint64_t user_data) {
    (void)resource_handle;
    (void)user_data;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetResourceBaseAddressAndSizeInBytes(uint32_t resource_handle, void** memory, size_t* size) {
    (void)resource_handle;
    (void)memory;
    (void)size;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetResourceShaderGuid(uint32_t resource_handle, void* guid) {
    (void)resource_handle;
    (void)guid;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverFindResourcesPublic(const void* callback, void* user_data) {
    (void)callback;
    (void)user_data;
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

}
