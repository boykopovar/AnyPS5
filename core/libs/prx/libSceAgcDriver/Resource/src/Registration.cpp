#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"

#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE = static_cast<int>(0x8A6C9018);
static constexpr uint32_t RESOURCE_REGISTRATION_MAX_NAME_LENGTH = 0xfc;
static constexpr int SCE_AGC_ERROR_WORKLOAD_STREAM_INVALID_ID = static_cast<int>(0x8A6C0033);
static constexpr int SCE_AGC_ERROR_WORKLOAD_STREAM_NULL = static_cast<int>(0x8A6C0035);
static constexpr int SCE_AGC_ERROR_WORKLOAD_STREAM_NOT_REGISTERED = static_cast<int>(0x8A6C003A);
static constexpr uint32_t WORKLOAD_STREAM_MIN_ID = 1;
static constexpr uint32_t WORKLOAD_STREAM_MAX_ID = 31;

static std::mutex g_workloadStreamLock;
static uint32_t g_workloadStreams = 0;

extern "C" {

bool AgcDriverWorkloadStreamRegistered_nid_postfix(uint32_t streamId) {
    if (streamId < WORKLOAD_STREAM_MIN_ID || streamId > WORKLOAD_STREAM_MAX_ID) return false;
    std::lock_guard lock(g_workloadStreamLock);
    return (g_workloadStreams & (1u << streamId)) != 0;
}

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
 if (stream_id < WORKLOAD_STREAM_MIN_ID || stream_id > WORKLOAD_STREAM_MAX_ID) return SCE_AGC_ERROR_WORKLOAD_STREAM_INVALID_ID;
 std::lock_guard lock(g_workloadStreamLock);
 if ((g_workloadStreams & (1u << stream_id)) != 0) return SCE_AGC_ERROR_WORKLOAD_STREAM_INVALID_ID;
 if (stream == nullptr) return SCE_AGC_ERROR_WORKLOAD_STREAM_NULL;
 g_workloadStreams |= 1u << stream_id;
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
    if (stream_id < WORKLOAD_STREAM_MIN_ID || stream_id > WORKLOAD_STREAM_MAX_ID) return SCE_AGC_ERROR_WORKLOAD_STREAM_INVALID_ID;
    std::lock_guard lock(g_workloadStreamLock);
    if ((g_workloadStreams & (1u << stream_id)) == 0) return SCE_AGC_ERROR_WORKLOAD_STREAM_NOT_REGISTERED;
    g_workloadStreams &= ~(1u << stream_id);
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

int APS5_VABI sceAgcDriverRegisterOwnerForSystem() {
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverInitResourceRegistrationInterface() {
    return SCE_AGC_ERROR_RESOURCE_REGISTRATION_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetResourceRegistrationInfo() {
    return 0;
}

}
