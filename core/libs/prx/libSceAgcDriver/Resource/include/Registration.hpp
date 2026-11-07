#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_RESOURCE_INCLUDE_REGISTRATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_RESOURCE_INCLUDE_REGISTRATION_HPP

#include <cstdint>

extern "C" bool AgcDriverWorkloadStreamRegistered_nid_postfix(std::uint32_t streamId);

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAgcDriverRegisterOwner(uint32_t* ownerHandle, const char* name);
int APS5_VABI sceAgcDriverRegisterResource(uint32_t* resourceHandle, uint32_t ownerHandle, const void* memory, size_t size, const char* name, uint32_t type, uint64_t userData);
int APS5_VABI sceAgcDriverRegisterMultipleResources(uint32_t* resourceHandle, uint32_t ownerHandle, const void* resources, uint32_t resourceCount);
int APS5_VABI sceAgcDriverRegisterWorkloadStream(uint32_t streamId, const void* stream);
int APS5_VABI sceAgcDriverUnregisterOwnerAndResources(uint32_t ownerHandle);
int APS5_VABI sceAgcDriverUnregisterResource(uint32_t resourceHandle);
int APS5_VABI sceAgcDriverRegisterGdsResource(uint32_t* resourceHandle, uint32_t ownerHandle, uint32_t gdsOffset, uint32_t gdsSize, const char* name, uint32_t type, uint64_t userData);
int APS5_VABI sceAgcDriverUnregisterAllResourcesForOwner(uint32_t ownerHandle);
int APS5_VABI sceAgcDriverUnregisterWorkloadStream(uint32_t streamId);
int APS5_VABI sceAgcDriverGetDefaultOwner(uint32_t* ownerHandle);
int APS5_VABI sceAgcDriverGetResourceRegistrationMaxNameLength(uint32_t* maxLength);
int APS5_VABI sceAgcDriverGetOwnerName(uint32_t ownerHandle, const char** name);
int APS5_VABI sceAgcDriverGetResourceName(uint32_t resourceHandle, const char** name);
int APS5_VABI sceAgcDriverGetResourceType(uint32_t resourceHandle, uint32_t* type);
int APS5_VABI sceAgcDriverGetResourceUserData(uint32_t resourceHandle, uint64_t* userData);
int APS5_VABI sceAgcDriverSetResourceUserData(uint32_t resourceHandle, uint64_t userData);
int APS5_VABI sceAgcDriverGetResourceBaseAddressAndSizeInBytes(uint32_t resourceHandle, void** memory, size_t* size);
int APS5_VABI sceAgcDriverGetResourceShaderGuid(uint32_t resourceHandle, void* guid);
int APS5_VABI sceAgcDriverFindResourcesPublic(const void* callback, void* userData);
int APS5_VABI sceAgcDriverRegisterOwnerForSystem();
int APS5_VABI sceAgcDriverInitResourceRegistrationInterface();
int APS5_VABI sceAgcDriverSetResourceRegistrationInfo();

}

#endif
