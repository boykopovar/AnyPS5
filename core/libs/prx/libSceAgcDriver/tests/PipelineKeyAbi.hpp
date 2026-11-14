#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_PIPELINEKEYABI_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_PIPELINEKEYABI_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <array>
#include <cstddef>
#include <cstdint>

struct PipelineKeyAbiRegisters {
    std::array<std::uint64_t, 8> general{};
    std::array<std::array<std::uint64_t, 2>, 10> vector{};
};

struct PipelineKeyAbiObservation {
    PipelineKeyAbiRegisters before;
    PipelineKeyAbiRegisters after;
    VkResult result = VK_ERROR_UNKNOWN;
};

struct PipelineKeyAbiNestedCall {
    PFN_vkGetPipelineKeyKHR function;
    PipelineKeyAbiObservation* observation;
};

static_assert(sizeof(PipelineKeyAbiRegisters) == 224);
static_assert(offsetof(PipelineKeyAbiObservation, after) == 224);
static_assert(offsetof(PipelineKeyAbiObservation, result) == 448);
static_assert(offsetof(VkPipelineBinaryKeyKHR, keySize) == 16);
static_assert(offsetof(VkPipelineBinaryKeyKHR, key) == 20);
static_assert(offsetof(PipelineKeyAbiNestedCall, observation) == 8);

extern "C" void PipelineKeyAbiProbe(PFN_vkGetPipelineKeyKHR function, VkDevice device, const VkPipelineCreateInfoKHR* info, VkPipelineBinaryKeyKHR* key, PipelineKeyAbiObservation* observation, std::uint64_t seed = 0);
extern "C" VKAPI_ATTR VkResult VKAPI_CALL PipelineKeyAbiConforming(VkDevice device, const VkPipelineCreateInfoKHR* info, VkPipelineBinaryKeyKHR* key);
extern "C" VKAPI_ATTR VkResult VKAPI_CALL PipelineKeyAbiClobbering(VkDevice device, const VkPipelineCreateInfoKHR* info, VkPipelineBinaryKeyKHR* key);
extern "C" VKAPI_ATTR VkResult VKAPI_CALL PipelineKeyAbiNested(VkDevice device, const VkPipelineCreateInfoKHR* info, VkPipelineBinaryKeyKHR* key);

unsigned PipelineKeyAbiDifferences(const PipelineKeyAbiObservation& observation);
const char* PipelineKeyAbiPlatform();
int PipelineKeyAbiSynthetic(bool clobbering);

#endif
