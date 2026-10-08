#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

extern "C" {
int APS5_VABI sceAgcDriverRegisterOwner(std::uint32_t*, const char*);
int APS5_VABI sceAgcDriverRegisterResource(std::uint32_t*, std::uint32_t, const void*, std::size_t, const char*, std::uint32_t, std::uint64_t);
int APS5_VABI sceAgcDriverRegisterGdsResource(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t, const char*, std::uint32_t, std::uint64_t);
int APS5_VABI sceAgcDriverUnregisterAllResourcesForOwner(std::uint32_t);
int APS5_VABI sceAgcDriverRegisterWorkloadStream(std::uint32_t, const void*);
int APS5_VABI sceAgcDriverUnregisterWorkloadStream(std::uint32_t);
int APS5_VABI sceAgcDriverGetDefaultOwner(std::uint32_t*);
int APS5_VABI sceAgcDriverGetResourceRegistrationMaxNameLength(std::uint32_t*);
int APS5_VABI sceAgcDriverGetOwnerName(std::uint32_t, const char**);
int APS5_VABI sceAgcDriverGetResourceName(std::uint32_t, const char**);
int APS5_VABI sceAgcDriverGetResourceType(std::uint32_t, std::uint32_t*);
int APS5_VABI sceAgcDriverGetResourceUserData(std::uint32_t, std::uint64_t*);
int APS5_VABI sceAgcDriverSetResourceUserData(std::uint32_t, std::uint64_t);
int APS5_VABI sceAgcDriverGetResourceBaseAddressAndSizeInBytes(std::uint32_t, void**, std::size_t*);
int APS5_VABI sceAgcDriverGetResourceShaderGuid(std::uint32_t, void*);
int APS5_VABI sceAgcDriverFindResourcesPublic(const void*, void*);
bool APS5_VABI sceAgcDriverIsCaptureInProgress(void);
bool APS5_VABI sceAgcDriverIsTraceInProgress(void);
bool APS5_VABI sceAgcDriverIsSubmitValidationEnabled(void);
}

static constexpr int Unavailable = static_cast<int>(0x8A6C9018);
static void Require(bool value) { if (!value) std::abort(); }

int main() {
    std::uint32_t owner = 7u;
    std::uint32_t resource = 9u;
    const std::uint32_t memory[4]{};
    Require(sceAgcDriverRegisterOwner(&owner, "owner") == Unavailable);
    Require(sceAgcDriverRegisterResource(&resource, owner, memory, sizeof(memory), "resource", 1u, 2u) == Unavailable);
    Require(sceAgcDriverRegisterGdsResource(&resource, owner, 0u, 64u, "gds", 1u, 2u) == Unavailable);
    Require(owner == 7u && resource == 9u);
    Require(sceAgcDriverGetDefaultOwner(&owner) == Unavailable && owner == 7u);
    std::uint32_t maxNameLength = 8u;
    Require(sceAgcDriverGetResourceRegistrationMaxNameLength(&maxNameLength) == 0 && maxNameLength == 0xfcu);
    const char* name = "unchanged";
    Require(sceAgcDriverGetOwnerName(owner, &name) == Unavailable);
    Require(sceAgcDriverGetResourceName(resource, &name) == Unavailable);
    std::uint32_t type = 3u;
    Require(sceAgcDriverGetResourceType(resource, &type) == Unavailable && type == 3u);
    std::uint64_t userData = 4u;
    Require(sceAgcDriverGetResourceUserData(resource, &userData) == Unavailable && userData == 4u);
    Require(sceAgcDriverSetResourceUserData(resource, 5u) == Unavailable);
    void* base = nullptr;
    std::size_t size = 6u;
    Require(sceAgcDriverGetResourceBaseAddressAndSizeInBytes(resource, &base, &size) == Unavailable && base == nullptr && size == 6u);
    std::uint8_t guid[16]{};
    Require(sceAgcDriverGetResourceShaderGuid(resource, guid) == Unavailable);
    Require(sceAgcDriverFindResourcesPublic(nullptr, nullptr) == Unavailable);
    Require(sceAgcDriverUnregisterAllResourcesForOwner(owner) == Unavailable);
    Require(std::string_view(name) == "unchanged");
    Require(sceAgcDriverRegisterWorkloadStream(1u, memory) == 0);
    Require(sceAgcDriverUnregisterWorkloadStream(1u) == 0);
    Require(!sceAgcDriverIsCaptureInProgress());
    Require(!sceAgcDriverIsTraceInProgress());
    Require(!sceAgcDriverIsSubmitValidationEnabled());
}
