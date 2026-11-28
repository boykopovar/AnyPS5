#include "prx/libSceAgcDriver/State/include/Status.hpp"
#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"
#include "prx/libSceAgcDriver/Resource/include/Lifecycle.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int Unavailable = static_cast<int>(0x8A6C9018);
static void Require(bool value) { if (!value) std::abort(); }

struct ResourceDescriptor {
    const void* memory;
    std::size_t size;
    const char* name;
    std::uint32_t type;
    std::uint32_t reserved;
    std::uint64_t userData;
};

static_assert(sizeof(ResourceDescriptor) == 40);

static void CheckRegistrationLifecycle() {
    constexpr std::uint32_t InvalidOutput = 0x80020065u;
    constexpr std::uint32_t RegistrationUnavailable = 0x8A6C9018u;
    constexpr std::uint64_t Sentinel = 0xa5b6c7d8e9fa1021ull;
    for (unsigned int iteration = 0; iteration != 2; ++iteration) {
        errno = 71;
        Require(sceAgcDriverInitResourceRegistration() == RegistrationUnavailable && errno == 71);
        std::array<std::uint64_t, 3> output{Sentinel, Sentinel, Sentinel};
        Require(sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(&output[1]) == RegistrationUnavailable);
        Require(output[0] == Sentinel && output[1] == 0 && output[2] == Sentinel && errno == 71);
        Require(sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(nullptr) == RegistrationUnavailable && errno == 71);
    }
    errno = 71;
    Require(sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(reinterpret_cast<std::uint64_t*>(0x10)) == InvalidOutput);
    Require(errno == 71);
    std::array<unsigned char, 10> unaligned{};
    unaligned.fill(0xa5);
    Require(sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(reinterpret_cast<std::uint64_t*>(unaligned.data() + 1)) == RegistrationUnavailable);
    Require(unaligned.front() == 0xa5 && unaligned.back() == 0xa5 && errno == 71);
    for (std::size_t index = 1; index != 9; ++index) Require(unaligned[index] == 0);
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    const std::size_t pageSize = static_cast<std::size_t>(size);
    void* page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    std::memcpy(page, &Sentinel, sizeof(Sentinel));
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, pageSize, PROT_READ) == 0);
#endif
    errno = 71;
    Require(sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(static_cast<std::uint64_t*>(page)) == InvalidOutput);
    std::uint64_t unchanged = 0;
    std::memcpy(&unchanged, page, sizeof(unchanged));
    Require(unchanged == Sentinel && errno == 71);
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, pageSize) == 0);
#endif
}

int main() {
    std::uint32_t owner = 7u;
    std::uint32_t resource = 9u;
    const std::uint32_t memory[4]{};
    Require(sceAgcDriverRegisterDefaultOwner(0u) == 0);
    Require(sceAgcDriverRegisterDefaultOwner(owner) == 0);
    Require(sceAgcDriverRegisterOwner(&owner, "owner") == Unavailable);
    Require(sceAgcDriverRegisterResource(&resource, owner, memory, sizeof(memory), "resource", 1u, 2u) == Unavailable);
    Require(sceAgcDriverRegisterGdsResource(&resource, owner, 0u, 64u, "gds", 1u, 2u) == Unavailable);
    Require(owner == 7u && resource == 9u);
    std::array<std::uint32_t, 4> bulkMemory{0x11223344u, 0x55667788u, 0x99aabbccu, 0xddeeff00u};
    const auto originalBulkMemory = bulkMemory;
    std::array<ResourceDescriptor, 2> descriptors{{
        {bulkMemory.data(), sizeof(bulkMemory), "default", 3u, 0u, 0u},
        {bulkMemory.data(), sizeof(bulkMemory), "second", 5u, 0u, 23u},
    }};
    const auto originalDescriptors = descriptors;
    std::uint32_t bulkHandle = 0xffffffffu;
    Require(sceAgcDriverRegisterMultipleResources(&bulkHandle, 0u, descriptors.data(), 1u) == Unavailable);
    Require(bulkHandle == 0xffffffffu);
    bulkHandle = 17u;
    Require(sceAgcDriverRegisterMultipleResources(&bulkHandle, owner, descriptors.data(), 2u) == Unavailable);
    Require(sceAgcDriverRegisterMultipleResources(&bulkHandle, owner, nullptr, 0u) == Unavailable);
    Require(bulkHandle == 17u && owner == 7u);
    Require(std::memcmp(descriptors.data(), originalDescriptors.data(), sizeof(descriptors)) == 0);
    Require(bulkMemory == originalBulkMemory);
    Require(sceAgcDriverGetDefaultOwner(&owner) == Unavailable && owner == 7u);
    std::uint32_t maxNameLength = 8u;
    Require(sceAgcDriverGetResourceRegistrationMaxNameLength(&maxNameLength) == Unavailable && maxNameLength == 8u);
    Require(sceAgcDriverGetResourceRegistrationMaxNameLength(nullptr) == Unavailable);
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
    Require(sceAgcDriverUnregisterOwnerAndResources(owner) == Unavailable);
    Require(sceAgcDriverUnregisterOwnerAndResources(0u) == Unavailable);
    Require(sceAgcDriverUnregisterResource(resource) == Unavailable);
    Require(sceAgcDriverUnregisterResource(0u) == Unavailable);
    Require(std::string_view(name) == "unchanged");
    constexpr int InvalidStream = static_cast<int>(0x8A6C0033);
    constexpr int NullStream = static_cast<int>(0x8A6C0035);
    constexpr int StreamNotRegistered = static_cast<int>(0x8A6C003A);
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(0u));
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(1u));
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(2u));
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(31u));
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(32u));
    for (const std::uint32_t id : {0u, 32u, 0x101u, 0xffffffffu}) {
        Require(sceAgcDriverRegisterWorkloadStream(id, memory) == InvalidStream);
        Require(sceAgcDriverRegisterWorkloadStream(id, nullptr) == InvalidStream);
        Require(sceAgcDriverUnregisterWorkloadStream(id) == InvalidStream);
        Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(id));
    }
    Require(sceAgcDriverRegisterWorkloadStream(2u, nullptr) == NullStream);
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(2u));
    Require(sceAgcDriverUnregisterWorkloadStream(2u) == StreamNotRegistered);
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(2u));
    Require(sceAgcDriverRegisterWorkloadStream(1u, memory) == 0);
    Require(AgcDriverWorkloadStreamRegistered_nid_postfix(1u));
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(2u));
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(31u));
    Require(sceAgcDriverRegisterWorkloadStream(1u, memory) == InvalidStream);
    Require(sceAgcDriverRegisterWorkloadStream(1u, nullptr) == InvalidStream);
    Require(AgcDriverWorkloadStreamRegistered_nid_postfix(1u));
    Require(sceAgcDriverRegisterWorkloadStream(31u, reinterpret_cast<const char*>(memory) + 1) == 0);
    Require(AgcDriverWorkloadStreamRegistered_nid_postfix(1u));
    Require(AgcDriverWorkloadStreamRegistered_nid_postfix(31u));
    Require(sceAgcDriverUnregisterWorkloadStream(1u) == 0);
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(1u));
    Require(AgcDriverWorkloadStreamRegistered_nid_postfix(31u));
    Require(sceAgcDriverUnregisterWorkloadStream(1u) == StreamNotRegistered);
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(1u));
    Require(sceAgcDriverRegisterWorkloadStream(1u, memory) == 0);
    Require(AgcDriverWorkloadStreamRegistered_nid_postfix(1u));
    Require(sceAgcDriverUnregisterWorkloadStream(1u) == 0);
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(1u));
    Require(sceAgcDriverUnregisterWorkloadStream(31u) == 0);
    Require(!AgcDriverWorkloadStreamRegistered_nid_postfix(31u));
    Require(!sceAgcDriverIsCaptureInProgress());
    Require(!sceAgcDriverIsTraceInProgress());
    Require(!sceAgcDriverIsSubmitValidationEnabled());
    constexpr int CaptureUnavailable = static_cast<int>(0x8A6C1000);
    Require(sceAgcDriverRequestCaptureStart(nullptr) == CaptureUnavailable);
    Require(sceAgcDriverRequestCaptureStart("capture.trace") == CaptureUnavailable);
    Require(sceAgcDriverRequestCaptureStop() == CaptureUnavailable);
    Require(sceAgcDriverTriggerCapture() == CaptureUnavailable);
    Require(sceAgcDriverRegisterOwnerForSystem() == Unavailable);
    Require(sceAgcDriverInitResourceRegistrationInterface() == Unavailable);
    Require(sceAgcDriverSetResourceRegistrationInfo() == 0);
    CheckRegistrationLifecycle();
}
