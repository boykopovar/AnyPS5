#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_PSML_ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x8A810001);
constexpr std::int32_t SCE_PSML_ERROR_INVALID_OBJECT = static_cast<std::int32_t>(0x8A810005);
constexpr std::int32_t SCE_PSML_ERROR_INVALID_POINTER = static_cast<std::int32_t>(0x8A810009);
constexpr std::int32_t SCE_PSML_ERROR_INVALID_VALUE = static_cast<std::int32_t>(0x8A81000D);

constexpr std::uint32_t SHARED_RESOURCES_MAGIC = 0xA9C4u;
constexpr std::uint32_t CONTEXT_MAGIC = 0x9231u;

constexpr std::uint64_t MAIN_MEMORY_BLOCK_SIZE = 0x200000;
constexpr std::uint64_t MAIN_MEMORY_ALIGNMENT = 0x200000;
constexpr std::uint64_t EXTRA_VA_BYTES = 0x600000;
constexpr std::uint32_t WORK_AREA_SIZE = 0x600;

struct MainMemoryRequirements {
    std::uint64_t block_size;
    std::uint64_t alignment;
    std::uint64_t block_count;
};

struct MainMemoryParameters {
    std::uint32_t type;
    std::uint32_t reserved;
};

struct SharedResourcesInitParameters {
    std::uint32_t type;
    std::uint32_t reserved;
    const void* blocks;
    std::uint64_t block_count;
    std::uint64_t virtual_address_start;
};

std::mutex g_mutex;
bool g_initialized = false;
std::uint32_t g_supported_modes = 0x3;
std::int32_t g_shared_ref_count = 0;

std::uint64_t BaseVaBytes(std::uint32_t type) {
    switch (type) {
        case 0: return 0x06200000;
        case 1: return 0x18200000;
        case 2: return 0x12200000;
        default: return 0;
    }
}

bool IsSupportedType(std::uint32_t type) {
    switch (type) {
        case 0: return true;
        case 1: return (g_supported_modes & 0x1u) != 0;
        case 2: return (g_supported_modes & 0x2u) != 0;
        default: return false;
    }
}

std::uint64_t RequiredBlockCount(std::uint32_t type) {
    const std::uint64_t base = BaseVaBytes(type);
    return base == 0 ? 0 : (base + EXTRA_VA_BYTES) / MAIN_MEMORY_BLOCK_SIZE;
}

bool HasKnownObject(const void* object) {
    if (!object) return false;
    const std::uint32_t magic = *static_cast<const std::uint32_t*>(object);
    return magic == SHARED_RESOURCES_MAGIC || magic == CONTEXT_MAGIC;
}

std::int32_t ValidateObject(const void* object) {
    if (!g_initialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
    if (!HasKnownObject(object)) return SCE_PSML_ERROR_INVALID_OBJECT;
    return 0;
}

}

extern "C" {

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement1100(void* requirement, const void* param) {
 (void)requirement;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext1100(void** context, const void* param) {
 (void)context;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1100(void* context, void* commandBuffer, const void* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePsmlMfsrGetSharedResourcesInitRequirement(void* out, const void* params) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
    if (!out || !params) return SCE_PSML_ERROR_INVALID_POINTER;
    const std::uint32_t type = static_cast<const MainMemoryParameters*>(params)->type;
    if (!IsSupportedType(type)) return SCE_PSML_ERROR_INVALID_VALUE;
    auto* requirements = static_cast<MainMemoryRequirements*>(out);
    requirements->block_size = MAIN_MEMORY_BLOCK_SIZE;
    requirements->alignment = MAIN_MEMORY_ALIGNMENT;
    requirements->block_count = RequiredBlockCount(type);
    return 0;
}

int APS5_VABI scePsmlMfsrInit() {
    std::lock_guard lock(g_mutex);
    g_initialized = true;
    g_supported_modes = 0x3;
    g_shared_ref_count = 0;
    return 0;
}

int APS5_VABI scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(const void* object, std::uint32_t* out_size) {
    std::lock_guard lock(g_mutex);
    const std::int32_t status = ValidateObject(object);
    if (status != 0) return status;
    if (!out_size) return SCE_PSML_ERROR_INVALID_POINTER;
    *out_size = WORK_AREA_SIZE;
    return 0;
}

int APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3_2(void* out, std::uint64_t params) {
    (void)params;
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
    if (!out) return SCE_PSML_ERROR_INVALID_POINTER;
    auto* requirements = static_cast<MainMemoryRequirements*>(out);
    requirements->block_size = MAIN_MEMORY_BLOCK_SIZE;
    requirements->alignment = MAIN_MEMORY_ALIGNMENT;
    requirements->block_count = 1;
    return 0;
}

int APS5_VABI scePsmlMfsrSelectConfig(const void* object) {
    std::lock_guard lock(g_mutex);
    return ValidateObject(object);
}

int APS5_VABI scePsmlMfsrGetMipmapBias(const void* object, float* out_progress, std::uint32_t frame_index) {
    (void)frame_index;
    std::lock_guard lock(g_mutex);
    const std::int32_t status = ValidateObject(object);
    if (status != 0) return status;
    if (!out_progress) return SCE_PSML_ERROR_INVALID_POINTER;
    *out_progress = 0.0f;
    return 0;
}

int APS5_VABI scePsmlMfsrRequestCapture(const void* object) {
    std::lock_guard lock(g_mutex);
    return ValidateObject(object);
}

int APS5_VABI scePsmlMfsrReleaseContext(void* context) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
    if (!HasKnownObject(context)) return SCE_PSML_ERROR_INVALID_OBJECT;
    *static_cast<std::uint32_t*>(context) = 0;
    return 0;
}

int APS5_VABI scePsmlMfsrIsCaptureInProgress(const void* object) {
    std::lock_guard lock(g_mutex);
    return ValidateObject(object);
}

int APS5_VABI scePsmlMfsrGetDispatchMfsrPacket900(void* context, std::uint64_t command, std::uint64_t params) {
    std::lock_guard lock(g_mutex);
    const std::int32_t status = ValidateObject(context);
    if (status != 0) return status;
    if (command == 0 || params == 0) return SCE_PSML_ERROR_INVALID_POINTER;
    return 0;
}

int APS5_VABI scePsmlMfsrCreateSharedResources(void* resources, const void* params) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
    if (!resources || !params) return SCE_PSML_ERROR_INVALID_POINTER;
    const auto* parameters = static_cast<const SharedResourcesInitParameters*>(params);
    if (!parameters->blocks) return SCE_PSML_ERROR_INVALID_POINTER;
    if (parameters->reserved != 0 || !IsSupportedType(parameters->type) || parameters->block_count == 0) return SCE_PSML_ERROR_INVALID_VALUE;
    *static_cast<std::uint32_t*>(resources) = SHARED_RESOURCES_MAGIC;
    *reinterpret_cast<void**>(static_cast<std::uint8_t*>(resources) + 0x08) = const_cast<void*>(parameters->blocks);
    *reinterpret_cast<std::uint64_t*>(static_cast<std::uint8_t*>(resources) + 0x18) = parameters->block_count;
    *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(resources) + 0x20) = parameters->type;
    *reinterpret_cast<std::uint64_t*>(static_cast<std::uint8_t*>(resources) + 0x28) = parameters->virtual_address_start;
    ++g_shared_ref_count;
    return 0;
}

int APS5_VABI scePsmlMfsrCreateContext800M3_2(void* context, const void* params) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
    if (!context || !params) return SCE_PSML_ERROR_INVALID_POINTER;
    const void* shared_resources = *reinterpret_cast<void* const*>(static_cast<const std::uint8_t*>(params) + 0x08);
    if (!shared_resources) return SCE_PSML_ERROR_INVALID_POINTER;
    *static_cast<std::uint32_t*>(context) = CONTEXT_MAGIC;
    *reinterpret_cast<void**>(static_cast<std::uint8_t*>(context) + 0x360) = const_cast<void*>(shared_resources);
    *(static_cast<std::uint8_t*>(context) + 0x368) = 0;
    return 0;
}

int APS5_VABI scePsmlMfsrReleaseSharedResources(void* resources) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PSML_ERROR_NOT_INITIALIZED;
    if (!HasKnownObject(resources)) return SCE_PSML_ERROR_INVALID_OBJECT;
    if (g_shared_ref_count > 0) --g_shared_ref_count;
    return 0;
}

APS5_EXPORT("0GAw7SmkwII", scePsmlMfsr2Unknown_0GAw7SmkwII);
int APS5_VABI scePsmlMfsr2Unknown_0GAw7SmkwII(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("1ic5q-kdOsc", scePsmlMfsr2Unknown_1ic5q_MkdOsc);
int APS5_VABI scePsmlMfsr2Unknown_1ic5q_MkdOsc(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("5z2gBlqxJ+0", scePsmlMfsr2Unknown_5z2gBlqxJ_P0);
int APS5_VABI scePsmlMfsr2Unknown_5z2gBlqxJ_P0(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("Kyy1baXgaVU", scePsmlMfsr2Unknown_Kyy1baXgaVU);
int APS5_VABI scePsmlMfsr2Unknown_Kyy1baXgaVU(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("ZLL31lzzxr4", scePsmlMfsr2Unknown_ZLL31lzzxr4);
int APS5_VABI scePsmlMfsr2Unknown_ZLL31lzzxr4(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("gMduXCLYrNg", scePsmlMfsr2Unknown_gMduXCLYrNg);
int APS5_VABI scePsmlMfsr2Unknown_gMduXCLYrNg(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("lrJwpLjKXRc", scePsmlMfsr2Unknown_lrJwpLjKXRc);
int APS5_VABI scePsmlMfsr2Unknown_lrJwpLjKXRc(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("m9JLPc3wOQw", scePsmlMfsr2Unknown_m9JLPc3wOQw);
int APS5_VABI scePsmlMfsr2Unknown_m9JLPc3wOQw(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("o+NM86gEwFE", scePsmlMfsr2Unknown_o_PNM86gEwFE);
int APS5_VABI scePsmlMfsr2Unknown_o_PNM86gEwFE(void) {
    return SCE_PSML_ERROR_NOT_INITIALIZED;
}

}