#include "prx/libSceAgcDriver/Execution/include/ShaderDeviceProfile.hpp"
#include "BdaAbi.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>

namespace AgcDriver {
namespace {

template<typename TFeatures>
const TFeatures* findFeatures(const void* chain, VkStructureType type) {
    auto* current = static_cast<const VkBaseInStructure*>(chain);
    while (current != nullptr) {
        if (current->sType == type) return reinterpret_cast<const TFeatures*>(current);
        current = current->pNext;
    }
    return nullptr;
}

}

ShaderDeviceProfile::ShaderDeviceProfile(const ShaderRecompiler::SpirvTarget& target, const VkDeviceCreateInfo& deviceInfo, const VkPhysicalDeviceLimits& limits) : target(target), limits(limits), capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end()) {
    using Graphics::Require;
    Require(deviceInfo.pEnabledFeatures != nullptr, "shader device profile requires enabled core features");
    const auto& core = *deviceInfo.pEnabledFeatures;
    const auto* robustness = findFeatures<VkPhysicalDeviceRobustness2FeaturesEXT>(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT);
    nullDescriptors = robustness != nullptr && robustness->nullDescriptor == VK_TRUE;
    const auto* bda = findFeatures<VkPhysicalDeviceBufferDeviceAddressFeatures>(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
    const auto* bytes = findFeatures<VkPhysicalDevice8BitStorageFeatures>(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES);
    const auto* indexing = findFeatures<VkPhysicalDeviceDescriptorIndexingFeatures>(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);
    const auto* barycentric = findFeatures<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR);
    const auto* mesh = findFeatures<VkPhysicalDeviceMeshShaderFeaturesEXT>(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT);
    const auto* clock = findFeatures<VkPhysicalDeviceShaderClockFeaturesKHR>(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR);
    const auto* maintenance8 = findFeatures<VkPhysicalDeviceMaintenance8FeaturesKHR>(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_8_FEATURES_KHR);
    Require(target.bdaAbiVersion == ShaderRecompiler::BdaAbi::Version, "shader device profile has an incompatible BDA ABI");
    Require(bda != nullptr && bda->bufferDeviceAddress == VK_TRUE && core.shaderInt64 == VK_TRUE, "shader runtime requires enabled bufferDeviceAddress and shaderInt64");
    Require(bytes != nullptr && bytes->storageBuffer8BitAccess == VK_TRUE, "shader runtime requires enabled storageBuffer8BitAccess");
    Require(core.vertexPipelineStoresAndAtomics == VK_TRUE && core.fragmentStoresAndAtomics == VK_TRUE, "shader runtime requires enabled graphics stores and atomics");
    Require(limits.maxPushConstantsSize >= ShaderRecompiler::RuntimeAbi::PushConstantDwords * sizeof(std::uint32_t) && limits.maxBoundDescriptorSets > ShaderRecompiler::RuntimeAbi::DescriptorSet, "shader runtime ABI exceeds device limits");
    Require(limits.maxStorageBufferRange >= sizeof(ShaderRecompiler::RuntimeAbi::ShaderData), "shader runtime ShaderData exceeds device limits");
    std::ranges::sort(capabilities);
    capabilities.erase(std::unique(capabilities.begin(), capabilities.end()), capabilities.end());
    const auto hasCapability = [&](spv::Capability capability) { return std::ranges::binary_search(capabilities, static_cast<std::uint32_t>(capability)); };
    Require(hasCapability(spv::CapabilityShader) && hasCapability(spv::CapabilityInt64) && hasCapability(spv::CapabilityPhysicalStorageBufferAddresses) && hasCapability(spv::CapabilityStorageBuffer8BitAccess), "shader device profile is missing runtime capabilities");
    const auto checkFeature = [&](spv::Capability capability, bool enabled, const char* name) {
        Require(!hasCapability(capability) || enabled, std::string("shader device profile advertises a disabled feature: ") + name);
    };
    checkFeature(spv::CapabilityFloat64, core.shaderFloat64 == VK_TRUE, "shaderFloat64");
    checkFeature(spv::CapabilityTessellation, core.tessellationShader == VK_TRUE, "tessellationShader");
    checkFeature(spv::CapabilityImageGatherExtended, core.shaderImageGatherExtended == VK_TRUE, "shaderImageGatherExtended");
    checkFeature(spv::CapabilityMinLod, core.shaderResourceMinLod == VK_TRUE, "shaderResourceMinLod");
    checkFeature(spv::CapabilityStorageImageMultisample, core.shaderStorageImageMultisample == VK_TRUE, "shaderStorageImageMultisample");
    checkFeature(spv::CapabilityStorageImageReadWithoutFormat, core.shaderStorageImageReadWithoutFormat == VK_TRUE, "shaderStorageImageReadWithoutFormat");
    checkFeature(spv::CapabilityStorageImageWriteWithoutFormat, core.shaderStorageImageWriteWithoutFormat == VK_TRUE, "shaderStorageImageWriteWithoutFormat");
    checkFeature(spv::CapabilitySampledImageArrayDynamicIndexing, core.shaderSampledImageArrayDynamicIndexing == VK_TRUE, "shaderSampledImageArrayDynamicIndexing");
    checkFeature(spv::CapabilityStorageImageArrayDynamicIndexing, core.shaderStorageImageArrayDynamicIndexing == VK_TRUE, "shaderStorageImageArrayDynamicIndexing");
    const bool sampledNonUniform = indexing != nullptr && indexing->shaderSampledImageArrayNonUniformIndexing == VK_TRUE;
    const bool storageNonUniform = indexing != nullptr && indexing->shaderStorageImageArrayNonUniformIndexing == VK_TRUE;
    checkFeature(spv::CapabilitySampledImageArrayNonUniformIndexing, sampledNonUniform, "shaderSampledImageArrayNonUniformIndexing");
    checkFeature(spv::CapabilityStorageImageArrayNonUniformIndexing, storageNonUniform, "shaderStorageImageArrayNonUniformIndexing");
    checkFeature(spv::CapabilityShaderNonUniform, sampledNonUniform || storageNonUniform, "descriptor indexing");
    const bool barycentricEnabled = barycentric != nullptr && barycentric->fragmentShaderBarycentric == VK_TRUE;
    const bool meshEnabled = mesh != nullptr && mesh->meshShader == VK_TRUE;
    checkFeature(spv::CapabilityFragmentBarycentricKHR, barycentricEnabled, "fragmentShaderBarycentric");
    checkFeature(spv::CapabilityMeshShadingEXT, meshEnabled, "meshShader");
    checkFeature(spv::CapabilityShaderClockKHR, clock != nullptr && clock->shaderSubgroupClock == VK_TRUE && clock->shaderDeviceClock == VK_TRUE, "shader clock");
    Require(!target.fragmentShaderBarycentricEnabled || barycentricEnabled, "shader device profile exposes disabled barycentrics");
    Require(!target.mesh || meshEnabled, "shader device profile exposes disabled mesh shaders");
    Require(!target.nonConstantImageOffsets || (maintenance8 != nullptr && maintenance8->maintenance8 == VK_TRUE), "shader device profile exposes disabled maintenance8");
    Require(!target.tessellation || core.tessellationShader == VK_TRUE, "shader device profile exposes disabled tessellation");
    for (const auto extension : target.supportedExtensions) extensions.emplace_back(extension);
    std::ranges::sort(extensions);
    extensions.erase(std::unique(extensions.begin(), extensions.end()), extensions.end());
    for (const auto& extension : extensions) extensionViews.emplace_back(extension);
    Require(std::ranges::find(extensionViews, "SPV_KHR_physical_storage_buffer") != extensionViews.end() && std::ranges::find(extensionViews, "SPV_KHR_8bit_storage") != extensionViews.end(), "shader device profile is missing runtime extensions");
    this->target.supportedCapabilities = capabilities;
    this->target.supportedExtensions = extensionViews;
}

}
