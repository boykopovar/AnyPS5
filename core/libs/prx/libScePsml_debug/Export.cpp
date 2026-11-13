#include "prx/libScePsml_debug/include/Api.hpp"
#include "prx/libScePsml_debug/include/LaterExports.hpp"
#include "prx/libkernel/libkernel.hpp"
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <mutex>
#include <utility>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_PSML_ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x8A810001);
constexpr std::int32_t SCE_PSML_ERROR_INVALID_ARGUMENT = static_cast<std::int32_t>(0x8A810005);
constexpr std::int32_t SCE_PSML_ERROR_NULL_OUTPUT = static_cast<std::int32_t>(0x8A810009);
constexpr std::int32_t SCE_PSML_ERROR_UNSUPPORTED_TYPE = static_cast<std::int32_t>(0x8A81000D);
constexpr std::int32_t SCE_PSML_ERROR_UNSUPPORTED_CONSOLE = static_cast<std::int32_t>(0x8A810012);
constexpr std::int32_t SCE_PSML_ERROR_UNSUPPORTED_RESOLUTION = static_cast<std::int32_t>(0x8A810013);
constexpr std::int32_t SCE_PSML_ERROR_SHARED_RESOURCES_BUSY = static_cast<std::int32_t>(0x8A810015);
constexpr std::int32_t SCE_PSML_ERROR_SYNC_SUBMIT = static_cast<std::int32_t>(0x8A810016);
constexpr std::int32_t Mfsr2ErrorNotInitialized = static_cast<std::int32_t>(0x8A8A0001);
constexpr std::int32_t Mfsr2ErrorUnsupportedConsole = static_cast<std::int32_t>(0x8A8A0012);
constexpr std::uint64_t MfsrBlockBytes = 0x200000;
constexpr std::uint64_t MfsrBlockCount = 4;
constexpr std::uint32_t MfsrDispatchPacketDwords = 0x600;
constexpr std::uint64_t MfsrSharedBlockCount = 52;
constexpr std::pair<std::uint32_t, std::uint32_t> Mfsr4KDimensions{3840, 2160};

static_assert(sizeof(Mfsr2SharedResourcesParam) == 32);
static_assert(offsetof(Mfsr2SharedResourcesParam, blocks) == 8);
static_assert(offsetof(Mfsr2SharedResourcesParam, blockCount) == 16);
static_assert(offsetof(Mfsr2SharedResourcesParam, width) == 24);
static_assert(offsetof(Mfsr2SharedResourcesParam, reserved1) == 28);

static_assert(sizeof(Mfsr2ContextParam) == 48);
static_assert(offsetof(Mfsr2ContextParam, sharedResources) == 8);
static_assert(offsetof(Mfsr2ContextParam, blocks) == 16);
static_assert(offsetof(Mfsr2ContextParam, blockCount) == 24);
static_assert(offsetof(Mfsr2ContextParam, outputWidth) == 40);
static_assert(offsetof(Mfsr2ContextParam, inputColorSpace) == 44);
static_assert(sizeof(MfsrCommandBuffer) == 16);
static_assert(offsetof(MfsrCommandBuffer, sizeInDwords) == 8);
static_assert(sizeof(MfsrTextureDescriptor) == 32);
static_assert(sizeof(Mfsr2DispatchParam) == 112);
static_assert(offsetof(Mfsr2DispatchParam, preExposure) == 48);
static_assert(offsetof(Mfsr2DispatchParam, renderX) == 52);
static_assert(offsetof(Mfsr2DispatchParam, jitter) == 68);
static_assert(offsetof(Mfsr2DispatchParam, depthRange) == 92);
static_assert(offsetof(Mfsr2DispatchParam, resetHistory) == 100);
static_assert(offsetof(Mfsr2DispatchParam, optionFlags) == 104);
static_assert(offsetof(Mfsr2DispatchParam, sharpness) == 108);

static_assert(sizeof(MfsrRequirement) == 0x18);
static_assert(sizeof(MfsrMemoryParam) == 0x18);
static_assert(offsetof(MfsrMemoryParam, blocks) == 0x08);
static_assert(offsetof(MfsrMemoryParam, blockCount) == 0x10);
static_assert(sizeof(MfsrSharedResourcesParam) == 0x20);
static_assert(offsetof(MfsrContextParam, blocks) == 0x08);
static_assert(offsetof(MfsrContextParam, blockCount) == 0x10);
static_assert(offsetof(MfsrContextParam, inputWidth) == 0x18);
static_assert(offsetof(MfsrContextParam, descriptor0) == 0x20);
static_assert(offsetof(MfsrContextParam, descriptor1) == 0x28);
static_assert(offsetof(MfsrContextParam, sharedResources) == 0x38);

enum class MfsrObjectKind { SharedResources, Context };

struct MfsrObject {
 MfsrObjectKind kind;
 void* sharedResources;
 std::uint32_t inputWidth;
 std::uint32_t inputHeight;
};

std::atomic<bool> g_mfsrInitialized{false};
std::atomic<bool> g_mfsrLowLatencySubmit{false};
std::atomic<bool> g_mfsrSyncSubmit{false};
std::atomic<std::uint8_t> g_mfsrSpecularAA{0};
std::atomic<std::uint32_t> g_mfsrFeatureFlags{0};
std::mutex g_mfsrMutex;
std::unordered_map<void*, MfsrObject*> g_mfsrObjects;

void RequireMfsr(bool condition, const char* function, const char* what) {
 if (!condition) throw std::runtime_error(std::string(function) + ": " + what);
}

bool CopyIn(void* dst, const void* src, std::size_t size) {
 const int saved = errno;
 const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
 errno = saved;
 return ok;
}

bool CopyOut(void* dst, const void* src, std::size_t size) {
 const int saved = errno;
 const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
 errno = saved;
 return ok;
}

void RequireBlocks(const MfsrBlock* blocks, std::uint64_t blockCount, std::uint64_t requiredCount, const char* function) {
 RequireMfsr(blockCount >= requiredCount, function, "fewer memory blocks than the requirement");
 RequireMfsr(blocks != nullptr, function, "null memory block list");
 std::vector<MfsrBlock> localBlocks(static_cast<std::size_t>(requiredCount));
 RequireMfsr(CopyIn(localBlocks.data(), blocks, localBlocks.size() * sizeof(MfsrBlock)), function, "unreadable memory block list");
 for (std::uint64_t i = 0; i < requiredCount; ++i) {
  RequireMfsr(localBlocks[i].bytes >= MfsrBlockBytes && localBlocks[i].address % MfsrBlockBytes == 0, function, "memory block smaller or less aligned than the requirement");
 }
}

void* CreateMfsrContext(void* sharedResources, const char* function, std::uint32_t inputWidth, std::uint32_t inputHeight) {
 std::lock_guard lock(g_mfsrMutex);
 if (sharedResources != nullptr) {
  const auto found = g_mfsrObjects.find(sharedResources);
  RequireMfsr(found != g_mfsrObjects.end() && found->second->kind == MfsrObjectKind::SharedResources, function, "shared resources handle was not created by this library");
 }
 auto* object = new MfsrObject{MfsrObjectKind::Context, sharedResources, inputWidth, inputHeight};
 g_mfsrObjects.emplace(object, object);
 return object;
}

bool FindMfsrObject(void* handle, MfsrObject* out) {
 std::lock_guard lock(g_mfsrMutex);
 const auto found = g_mfsrObjects.find(handle);
 if (found == g_mfsrObjects.end()) return false;
 *out = *found->second;
 return true;
}

std::pair<std::uint32_t, std::uint32_t> MfsrInputDimensions(const MfsrContextParam* param) {
 if (param->inputWidth != 0) return {param->inputWidth, param->inputHeight};
 if (param->descriptor0 == nullptr) return {0, 0};
 std::uint16_t width = 0, height = 0;
 std::memcpy(&width, static_cast<const std::uint8_t*>(param->descriptor0) + 0x20, sizeof(width));
 std::memcpy(&height, static_cast<const std::uint8_t*>(param->descriptor0) + 0x22, sizeof(height));
 return {width, height};
}

std::int32_t ContextBufferRequirement(MfsrRequirement* requirement, const void* param, const char* function) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 RequireMfsr(requirement != nullptr && param != nullptr, function, "null requirement or parameter");
 *requirement = {MfsrBlockBytes, MfsrBlockBytes, MfsrBlockCount};
 return 0;
}

std::int32_t CreateContextFromParam(void** context, const MfsrContextParam* param, bool useSharedResources, const char* function) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 RequireMfsr(context != nullptr && param != nullptr, function, "null handle or parameter");
 RequireBlocks(param->blocks, param->blockCount, MfsrBlockCount, function);
 RequireMfsr(!useSharedResources || param->sharedResources != nullptr, function, "null shared resources handle");
 const auto [inputWidth, inputHeight] = MfsrInputDimensions(param);
 *context = CreateMfsrContext(useSharedResources ? param->sharedResources : nullptr, function, inputWidth, inputHeight);
 return 0;
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
 if (sceKernelIsTrinityMode() == 0) return SCE_PSML_ERROR_UNSUPPORTED_CONSOLE;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetSharedResourcesInitRequirement(MfsrRequirement* requirement, const MfsrSharedResourcesParam* param) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 RequireMfsr(requirement != nullptr && param != nullptr, __func__, "null requirement or parameter");
 MfsrSharedResourcesParam localParam{};
 RequireMfsr(CopyIn(&localParam, param, sizeof(localParam)), __func__, "unreadable parameter");
 if (localParam.type != 0) {
  const bool gated = (localParam.type == 1 && (g_mfsrFeatureFlags.load() & 1u) != 0) || (localParam.type == 2 && (g_mfsrFeatureFlags.load() & 2u) != 0);
  if (!gated) return SCE_PSML_ERROR_UNSUPPORTED_TYPE;
  NotImplemented_nid_no_patch(__func__);
 }
 const MfsrRequirement localReq{MfsrBlockBytes, MfsrBlockBytes, MfsrSharedBlockCount};
 RequireMfsr(CopyOut(requirement, &localReq, sizeof(localReq)), __func__, "unwritable requirement");
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrCreateSharedResources(void** sharedResources, const MfsrSharedResourcesParam* param) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 std::lock_guard lock(g_mfsrMutex);
 for (const auto& [handle, object] : g_mfsrObjects) {
  if (object->kind == MfsrObjectKind::SharedResources) return SCE_PSML_ERROR_SHARED_RESOURCES_BUSY;
 }
 RequireMfsr(sharedResources != nullptr && param != nullptr, __func__, "null handle or parameter");
 MfsrSharedResourcesParam localParam{};
 RequireMfsr(CopyIn(&localParam, param, sizeof(localParam)), __func__, "unreadable parameter");
 if (localParam.type != 0) return SCE_PSML_ERROR_UNSUPPORTED_TYPE;
 RequireBlocks(localParam.blocks, localParam.blockCount, MfsrSharedBlockCount, __func__);
 auto* object = new MfsrObject{MfsrObjectKind::SharedResources, nullptr, 0, 0};
 void* outHandle = object;
 if (!CopyOut(sharedResources, &outHandle, sizeof(outHandle))) {
  delete object;
  RequireMfsr(false, __func__, "unwritable handle");
 }
 g_mfsrObjects.emplace(object, object);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrReleaseSharedResources(void* sharedResources) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 ReleaseMfsrObject(sharedResources, MfsrObjectKind::SharedResources, __func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3_2(MfsrRequirement* requirement, const MfsrContextParam* param) {
 return ContextBufferRequirement(requirement, param, __func__);
}

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement(MfsrRequirement* requirement, const void* param) {
 return ContextBufferRequirement(requirement, param, __func__);
}

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3(MfsrRequirement* requirement, const void* param) {
 return ContextBufferRequirement(requirement, param, __func__);
}

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirementFrom2023July(MfsrRequirement* requirement, const void* param) {
 return ContextBufferRequirement(requirement, param, __func__);
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext800M3_2(void** context, const MfsrContextParam* param) {
 return CreateContextFromParam(context, param, true, __func__);
}

std::int32_t APS5_VABI scePsmlMfsrCreateContextFrom2023July(void** context, const MfsrContextParam* param) {
 return CreateContextFromParam(context, param, false, __func__);
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext(void** context, const MfsrMemoryParam* param) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 RequireMfsr(context != nullptr && param != nullptr, __func__, "null handle or parameter");
 MfsrMemoryParam localParam{};
 RequireMfsr(CopyIn(&localParam, param, sizeof(localParam)), __func__, "unreadable parameter");
 RequireBlocks(localParam.blocks, localParam.blockCount, MfsrBlockCount, __func__);
 RequireMfsr(localParam.type <= 2, __func__, "unknown context type");
 if (localParam.type != 0) NotImplemented_nid_no_patch(__func__);
 void* created = CreateMfsrContext(nullptr, __func__, Mfsr4KDimensions.first, Mfsr4KDimensions.second);
 if (!CopyOut(context, &created, sizeof(created))) {
  ReleaseMfsrObject(created, MfsrObjectKind::Context, __func__);
  RequireMfsr(false, __func__, "unwritable handle");
 }
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrReleaseContext(void* context) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 ReleaseMfsrObject(context, MfsrObjectKind::Context, __func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext1300(void** context, const void* param) {
 if (param != nullptr) {
  const auto* bytes = static_cast<const std::uint8_t*>(param);
  std::uint32_t type;
  std::uint16_t width, height;
  std::memcpy(&type, bytes, sizeof(type));
  std::memcpy(&width, bytes + 0x18, sizeof(width));
  std::memcpy(&height, bytes + 0x1a, sizeof(height));
  if (type == 0 && (width > Mfsr4KDimensions.first || height > Mfsr4KDimensions.second)) return SCE_PSML_ERROR_UNSUPPORTED_RESOLUTION;
 }
 (void)context;
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

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(void* context, std::uint32_t* packetSizeInDwords) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 MfsrObject object;
 if (!FindMfsrObject(context, &object)) return SCE_PSML_ERROR_INVALID_ARGUMENT;
 if (packetSizeInDwords == nullptr) return SCE_PSML_ERROR_NULL_OUTPUT;
 *packetSizeInDwords = MfsrDispatchPacketDwords;
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrSelectConfig(void* config) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 MfsrObject object;
 if (!FindMfsrObject(config, &object)) return SCE_PSML_ERROR_INVALID_ARGUMENT;
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetMipmapBias(void* context, float* bias, std::uint32_t dimension) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 MfsrObject object;
 if (!FindMfsrObject(context, &object) || object.kind != MfsrObjectKind::Context) return SCE_PSML_ERROR_INVALID_ARGUMENT;
 if (bias == nullptr) return SCE_PSML_ERROR_NULL_OUTPUT;
 if (object.inputWidth == 0) return SCE_PSML_ERROR_INVALID_ARGUMENT;
 *bias = std::log2f(static_cast<float>(dimension) / static_cast<float>(object.inputWidth)) - 1.0f;
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrRequestCapture(void* context) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 MfsrObject object;
 if (!FindMfsrObject(context, &object)) return SCE_PSML_ERROR_INVALID_ARGUMENT;
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrIsCaptureInProgress(void* context) {
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 MfsrObject object;
 if (!FindMfsrObject(context, &object)) return SCE_PSML_ERROR_INVALID_ARGUMENT;
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1000(void* context, void* commandBuffer, const void* param) {
 (void)param;
 if (!g_mfsrInitialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
 MfsrObject object;
 if (!FindMfsrObject(context, &object)) return SCE_PSML_ERROR_INVALID_ARGUMENT;
 if (commandBuffer == nullptr) return SCE_PSML_ERROR_NULL_OUTPUT;
 if (!g_mfsrLowLatencySubmit.load() && g_mfsrSyncSubmit.load()) return SCE_PSML_ERROR_SYNC_SUBMIT;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket900(void* context, void* commandBuffer, const void* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 if (g_mfsrInitialized) NotImplemented_nid_no_patch(__func__);
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsr2ReleaseSharedResources(void* sharedResources) {
 (void)sharedResources;
 return Mfsr2ErrorNotInitialized;
}

std::int32_t APS5_VABI scePsmlMfsr2GetSharedResourcesInitRequirement(MfsrRequirement* requirement, const Mfsr2SharedResourcesParam* param) {
 (void)requirement;
 (void)param;
 return Mfsr2ErrorNotInitialized;
}

std::int32_t APS5_VABI scePsmlMfsr2GetContextInitRequirement(MfsrRequirement* requirement, const Mfsr2ContextParam* param) {
 (void)requirement;
 (void)param;
 return Mfsr2ErrorNotInitialized;
}

std::int32_t APS5_VABI scePsmlMfsr2ReleaseContext(void* context) {
 (void)context;
 return Mfsr2ErrorNotInitialized;
}

std::int32_t APS5_VABI scePsmlMfsr2GetDispatchPacketsSizeInDwords(void* context, std::uint32_t* packetSizeInDwords) {
 (void)context;
 (void)packetSizeInDwords;
 return Mfsr2ErrorNotInitialized;
}

std::int32_t APS5_VABI scePsmlMfsr2CreateSharedResources(void** sharedResources, const Mfsr2SharedResourcesParam* param) {
 (void)sharedResources;
 (void)param;
 return Mfsr2ErrorNotInitialized;
}

std::int32_t APS5_VABI scePsmlMfsr2GetDispatchPackets(void* context, const MfsrCommandBuffer* commandBuffer, const Mfsr2DispatchParam* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 return Mfsr2ErrorNotInitialized;
}

std::int32_t APS5_VABI scePsmlMfsr2CreateContext(void** context, const Mfsr2ContextParam* param) {
 (void)context;
 (void)param;
 return Mfsr2ErrorNotInitialized;
}

std::int32_t APS5_VABI scePsmlMfsr2Init() {
 if (sceKernelIsTrinityMode() == 0) return Mfsr2ErrorUnsupportedConsole;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePsmlMfsrEnableSpecularAA(std::uint32_t) {
 g_mfsrSpecularAA = 1;
 return 0;
}

int APS5_VABI scePsmlMfsrEnableQuality8K(std::uint32_t) {
 g_mfsrFeatureFlags |= 1u;
 return 0;
}

int APS5_VABI scePsmlMfsrEnableLightWeight(std::uint32_t) {
 g_mfsrFeatureFlags |= 2u;
 return 0;
}

}

extern "C" {

int APS5_VABI scePsmlMfsrCreateContext800M3() {
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePsmlMfsrGetDispatchMfsrPacket() {
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePsmlMfsrGetDispatchMfsrPacketFrom2023July() {
 NotImplemented_nid_no_patch(__func__);
}

}
