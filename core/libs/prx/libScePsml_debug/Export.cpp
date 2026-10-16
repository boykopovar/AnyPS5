#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_PSML_ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x8A810001);
constexpr std::uint64_t MfsrBlockBytes = 0x200000;
constexpr std::uint64_t MfsrBlockCount = 4;

struct MfsrRequirement {
 std::uint64_t blockBytes;
 std::uint64_t alignment;
 std::uint64_t blockCount;
};

struct MfsrBlock {
 std::uint64_t address;
 std::uint64_t bytes;
};

struct MfsrSharedResourcesParam {
 std::uint32_t type;
 std::uint32_t reserved;
 const MfsrBlock* blocks;
 std::uint64_t blockCount;
 std::uint64_t reserved2;
};

struct MfsrContextParam {
 std::uint32_t reserved0[2];
 const MfsrBlock* blocks;
 std::uint64_t blockCount;
 std::uint32_t reserved1[8];
 void* sharedResources;
};

static_assert(sizeof(MfsrRequirement) == 0x18);
static_assert(sizeof(MfsrSharedResourcesParam) == 0x20);
static_assert(offsetof(MfsrContextParam, blocks) == 0x08);
static_assert(offsetof(MfsrContextParam, blockCount) == 0x10);
static_assert(offsetof(MfsrContextParam, sharedResources) == 0x38);

enum class MfsrObjectKind { SharedResources, Context };

struct MfsrObject {
 MfsrObjectKind kind;
 void* sharedResources;
};

std::atomic<bool> g_mfsrInitialized{false};
std::mutex g_mfsrMutex;
std::unordered_map<void*, MfsrObject*> g_mfsrObjects;

void RequireMfsr(bool condition, const char* function, const char* what) {
 if (!condition) throw std::runtime_error(std::string(function) + ": " + what);
}

void RequireBlocks(const MfsrBlock* blocks, std::uint64_t blockCount, const char* function) {
 RequireMfsr(blockCount >= MfsrBlockCount, function, "fewer memory blocks than the requirement");
 RequireMfsr(blocks != nullptr, function, "null memory block list");
 for (std::uint64_t i = 0; i < MfsrBlockCount; ++i) {
  RequireMfsr(blocks[i].bytes >= MfsrBlockBytes && blocks[i].address % MfsrBlockBytes == 0, function, "memory block smaller or less aligned than the requirement");
 }
}

void* CreateMfsrObject(MfsrObjectKind kind, void* sharedResources, const char* function) {
 std::lock_guard lock(g_mfsrMutex);
 if (kind == MfsrObjectKind::Context) {
  const auto found = g_mfsrObjects.find(sharedResources);
  RequireMfsr(found != g_mfsrObjects.end() && found->second->kind == MfsrObjectKind::SharedResources, function, "shared resources handle was not created by this library");
 }
 auto* object = new MfsrObject{kind, sharedResources};
 g_mfsrObjects.emplace(object, object);
 return object;
}

void ReleaseMfsrObject(void* handle, MfsrObjectKind kind, const char* function) {
 std::lock_guard lock(g_mfsrMutex);
 const auto found = g_mfsrObjects.find(handle);
 RequireMfsr(found != g_mfsrObjects.end() && found->second->kind == kind, function, "handle was not created by this library");
 if (kind == MfsrObjectKind::SharedResources) {
  for (const auto& [key, object] : g_mfsrObjects) {
   RequireMfsr(object->sharedResources != handle, function, "shared resources still used by a context");
  }
 }
 delete found->second;
 g_mfsrObjects.erase(found);
}

}

extern "C" {

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement1100(void* requirement, const void* param) {
 (void)requirement;
 (void)param;
 if (g_mfsrInitialized) NotImplemented_nid_no_patch(__func__);
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext1100(void** context, const void* param) {
 (void)context;
 (void)param;
 if (g_mfsrInitialized) NotImplemented_nid_no_patch(__func__);
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1100(void* context, void* commandBuffer, const void* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 if (g_mfsrInitialized) NotImplemented_nid_no_patch(__func__);
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrInit() {
 g_mfsrInitialized = true;
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetSharedResourcesInitRequirement(MfsrRequirement* requirement, const MfsrSharedResourcesParam* param) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 RequireMfsr(requirement != nullptr && param != nullptr, __func__, "null requirement or parameter");
 RequireMfsr(param->type == 0, __func__, ("shared resources type " + std::to_string(param->type) + " is not supported").c_str());
 *requirement = {MfsrBlockBytes, MfsrBlockBytes, MfsrBlockCount};
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrCreateSharedResources(void** sharedResources, const MfsrSharedResourcesParam* param) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 RequireMfsr(sharedResources != nullptr && param != nullptr, __func__, "null handle or parameter");
 RequireMfsr(param->type == 0, __func__, ("shared resources type " + std::to_string(param->type) + " is not supported").c_str());
 RequireBlocks(param->blocks, param->blockCount, __func__);
 *sharedResources = CreateMfsrObject(MfsrObjectKind::SharedResources, nullptr, __func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrReleaseSharedResources(void* sharedResources) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 ReleaseMfsrObject(sharedResources, MfsrObjectKind::SharedResources, __func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3_2(MfsrRequirement* requirement, const MfsrContextParam* param) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 RequireMfsr(requirement != nullptr && param != nullptr, __func__, "null requirement or parameter");
 *requirement = {MfsrBlockBytes, MfsrBlockBytes, MfsrBlockCount};
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext800M3_2(void** context, const MfsrContextParam* param) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 RequireMfsr(context != nullptr && param != nullptr, __func__, "null handle or parameter");
 RequireBlocks(param->blocks, param->blockCount, __func__);
 *context = CreateMfsrObject(MfsrObjectKind::Context, param->sharedResources, __func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrReleaseContext(void* context) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 ReleaseMfsrObject(context, MfsrObjectKind::Context, __func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext1300(void** context, const void* param) {
 (void)context;
 (void)param;
 if (g_mfsrInitialized) NotImplemented_nid_no_patch(__func__);
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1300(void* context, void* commandBuffer, const void* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 if (g_mfsrInitialized) NotImplemented_nid_no_patch(__func__);
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePsmlMfsrGetDispatchMfsrPacketSizeInDwords() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePsmlMfsrSelectConfig() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePsmlMfsrGetMipmapBias() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePsmlMfsrRequestCapture() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePsmlMfsrIsCaptureInProgress() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1000(void* context, void* commandBuffer, const void* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 if (g_mfsrInitialized) NotImplemented_nid_no_patch(__func__);
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket900(void* context, void* commandBuffer, const void* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 if (g_mfsrInitialized) NotImplemented_nid_no_patch(__func__);
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePsmlMfsr2ReleaseSharedResources() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2GetSharedResourcesInitRequirement() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2GetContextInitRequirement() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2ReleaseContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2GetDispatchPacketsSizeInDwords() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2CreateSharedResources() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2GetDispatchPackets() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2CreateContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2Init() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsrEnableSpecularAA() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsrEnableQuality8K() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
