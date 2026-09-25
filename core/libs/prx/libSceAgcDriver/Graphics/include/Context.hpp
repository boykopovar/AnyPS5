#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CONTEXT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_CONTEXT_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

class TextureDetiler;
class GpuColorTransfer;
class BufferPool;
class TextureCache;
class RenderCache;
class DrawQueue;
class GraphicsPipelineCache;
class Recorder;
class DescriptorCache;
class SamplerCache;

inline void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("AGC graphics: " + reason);
}

inline void Check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string("AGC graphics: ") + operation + ": Vulkan result " + std::to_string(result));
}

inline void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("AGC graphics: ") + reason);
}

struct Context {
    VkDevice device;
    VkPhysicalDevice physical;
    VkQueue queue;
    VkCommandPool pool;
    PFN_vkGetDeviceProcAddr deviceProc;
    PFN_vkGetPhysicalDeviceFormatProperties formatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties imageFormatProperties;
    VkPhysicalDeviceMemoryProperties memory;
    VkPhysicalDeviceLimits limits;
    bool tessellationShader = false;
    bool meshShader = false;
    VkPhysicalDeviceMeshShaderPropertiesEXT meshLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    bool depthClipControl = false;
    bool depthRangeUnrestricted = false;
    bool bufferDeviceAddress = false;
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    bool fragmentShaderBarycentric = false;
    bool samplerAnisotropy = false;
    bool textureCompressionBC = false;
    TextureDetiler* detiler = nullptr;
    GpuColorTransfer* colorTransfer = nullptr;
    mutable std::shared_ptr<BufferPool> bufferPool;
    TextureCache* textureCache = nullptr;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    bool depthClamp = false;
    // Nonzero when VK_EXT_external_memory_host is enabled: the required host pointer alignment.
    VkDeviceSize hostImportAlignment = 0;
    RenderCache* renderCache = nullptr;
    DrawQueue* drawQueue = nullptr;
    GraphicsPipelineCache* graphicsPipelines = nullptr;
    // Batches GPU work across guest commands (see Recorder); null before the device finished setup.
    Recorder* recorder = nullptr;
    // Per-device caches of the immutable descriptor objects a ShaderResources build needs (set
    // layouts, descriptor pools, samplers); null (tests) means every build makes and destroys its own.
    DescriptorCache* descriptorCache = nullptr;
    SamplerCache* samplerCache = nullptr;

    template<typename TFunction>
    TFunction Function(const char* name) const {
        Require(deviceProc != nullptr, "missing Vulkan device function resolver");
        const auto function = reinterpret_cast<TFunction>(deviceProc(device, name));
        if (function == nullptr) throw std::runtime_error(std::string("AGC graphics: missing Vulkan function: ") + name);
        return function;
    }

    std::uint32_t MemoryType(std::uint32_t mask, VkMemoryPropertyFlags flags) const {
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
            if ((mask & (1u << i)) != 0 && (memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
        }
        throw std::runtime_error("AGC graphics: required Vulkan memory type is unavailable");
    }
};

}

#endif
