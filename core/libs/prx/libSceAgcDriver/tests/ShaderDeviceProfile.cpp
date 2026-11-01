#include "prx/libSceAgcDriver/Execution/include/ShaderDeviceProfile.hpp"
#include "BdaAbi.hpp"
#include "Optimization/BindingAllocator.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

struct ProfileInput {
    std::vector<std::uint32_t> capabilities{spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    std::vector<std::string> extensionStrings{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    std::vector<std::string_view> extensions{extensionStrings[0], extensionStrings[1]};
    VkPhysicalDeviceRobustness2FeaturesEXT robustness{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT, nullptr, VK_FALSE, VK_FALSE, VK_TRUE};
    VkPhysicalDeviceDescriptorIndexingFeatures indexing{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES, &robustness};
    VkPhysicalDevice8BitStorageFeatures bytes{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES, &indexing, VK_TRUE};
    VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, &bytes, VK_TRUE};
    VkPhysicalDeviceFeatures core{};
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    VkPhysicalDeviceLimits limits{};

    ProfileInput() {
        core.shaderInt64 = VK_TRUE;
        core.vertexPipelineStoresAndAtomics = VK_TRUE;
        core.fragmentStoresAndAtomics = VK_TRUE;
        device.pEnabledFeatures = &core;
        device.pNext = &bda;
        limits.maxPushConstantsSize = 128u;
        limits.maxBoundDescriptorSets = 1u;
        limits.maxStorageBufferRange = sizeof(ShaderRecompiler::RuntimeAbi::ShaderData);
    }

    ShaderRecompiler::SpirvTarget Target() const {
        ShaderRecompiler::SpirvTarget target{};
        target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        target.supportedCapabilities = capabilities;
        target.supportedExtensions = extensions;
        return target;
    }
};

template<typename TAction>
void Reject(TAction action, const char* expected) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, "unexpected profile validation error");
        return;
    }
    throw std::runtime_error(std::string("invalid shader device profile was accepted: ") + expected);
}

void CheckProfile() {
    ProfileInput input;
    const AgcDriver::ShaderDeviceProfile profile(input.Target(), input.device, input.limits);
    input.capabilities.clear();
    input.extensionStrings[0] = "changed";
    input.core.shaderInt64 = VK_FALSE;
    input.robustness.nullDescriptor = VK_FALSE;
    input.limits.maxPushConstantsSize = 0u;
    const auto target = profile.Target();
    Require(target.supportedCapabilities.size() == 4u && std::ranges::is_sorted(target.supportedCapabilities), "profile capabilities were not frozen");
    Require(std::ranges::find(target.supportedExtensions, "SPV_KHR_physical_storage_buffer") != target.supportedExtensions.end(), "profile extension storage is borrowed");
    Require(profile.Limits().maxPushConstantsSize == 128u, "profile limits were not frozen");
    Require(profile.NullDescriptors(), "profile null descriptor support was not frozen");
    static_assert(!std::is_copy_constructible_v<AgcDriver::ShaderDeviceProfile> && !std::is_move_constructible_v<AgcDriver::ShaderDeviceProfile>);
    const auto invalid = [](auto mutate, const char* expected) {
        ProfileInput candidate;
        mutate(candidate);
        Reject([&] { AgcDriver::ShaderDeviceProfile rejected(candidate.Target(), candidate.device, candidate.limits); }, expected);
    };
    invalid([](auto& value) { value.device.pEnabledFeatures = nullptr; }, "enabled core features");
    invalid([](auto& value) { value.bda.bufferDeviceAddress = VK_FALSE; }, "bufferDeviceAddress");
    invalid([](auto& value) { value.device.pNext = nullptr; }, "bufferDeviceAddress");
    invalid([](auto& value) { value.core.shaderInt64 = VK_FALSE; }, "shaderInt64");
    invalid([](auto& value) { value.capabilities.push_back(spv::CapabilityGeometry); }, "geometryShader");
    invalid([](auto& value) { value.bytes.storageBuffer8BitAccess = VK_FALSE; }, "storageBuffer8BitAccess");
    ProfileInput withoutNull;
    withoutNull.robustness.nullDescriptor = VK_FALSE;
    Require(!AgcDriver::ShaderDeviceProfile(withoutNull.Target(), withoutNull.device, withoutNull.limits).NullDescriptors(), "a device without nullDescriptor was profiled with it");
    ProfileInput withoutRobustness;
    withoutRobustness.indexing.pNext = nullptr;
    Require(!AgcDriver::ShaderDeviceProfile(withoutRobustness.Target(), withoutRobustness.device, withoutRobustness.limits).NullDescriptors(), "a device without VK_EXT_robustness2 was profiled with nullDescriptor");
    invalid([](auto& value) { value.core.fragmentStoresAndAtomics = VK_FALSE; }, "graphics stores and atomics");
    invalid([](auto& value) { value.limits.maxPushConstantsSize = 127u; }, "exceeds device limits");
    invalid([](auto& value) { value.limits.maxBoundDescriptorSets = 0u; }, "exceeds device limits");
    invalid([](auto& value) { --value.limits.maxStorageBufferRange; }, "ShaderData exceeds device limits");
    invalid([](auto& value) { value.capabilities.clear(); }, "missing runtime capabilities");
    invalid([](auto& value) { value.extensions[0] = "missing"; }, "missing runtime extensions");
    invalid([](auto& value) { value.capabilities.push_back(spv::CapabilitySampledImageArrayDynamicIndexing); }, "shaderSampledImageArrayDynamicIndexing");
    invalid([](auto& value) { value.capabilities.push_back(spv::CapabilityStorageImageArrayNonUniformIndexing); }, "shaderStorageImageArrayNonUniformIndexing");
    invalid([](auto& value) { value.capabilities.push_back(spv::CapabilityStorageImageReadWithoutFormat); }, "shaderStorageImageReadWithoutFormat");
    invalid([](auto& value) { value.capabilities.push_back(spv::CapabilityStorageImageMultisample); }, "shaderStorageImageMultisample");
    invalid([](auto& value) { value.capabilities.push_back(spv::CapabilityFragmentBarycentricKHR); }, "fragmentShaderBarycentric");
    invalid([](auto& value) { value.capabilities.push_back(spv::CapabilityMeshShadingEXT); }, "meshShader");
    invalid([](auto& value) { value.capabilities.push_back(spv::CapabilityDrawParameters); }, "shaderDrawParameters");
    invalid([](auto& value) { value.extensions.push_back("SPV_KHR_terminate_invocation"); }, "shaderTerminateInvocation");
    ProfileInput enabled;
    enabled.capabilities.push_back(spv::CapabilitySampledImageArrayDynamicIndexing);
    enabled.capabilities.push_back(spv::CapabilityStorageImageArrayNonUniformIndexing);
    enabled.core.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    enabled.indexing.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    const AgcDriver::ShaderDeviceProfile accepted(enabled.Target(), enabled.device, enabled.limits);
    Require(accepted.Target().supportedCapabilities.size() == 6u, "enabled descriptor indexing was rejected");
    ProfileInput drawParameters;
    drawParameters.capabilities.push_back(spv::CapabilityDrawParameters);
    VkPhysicalDeviceShaderDrawParametersFeatures drawFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES, &drawParameters.bda, VK_TRUE};
    drawParameters.device.pNext = &drawFeatures;
    const AgcDriver::ShaderDeviceProfile drawParametersAccepted(drawParameters.Target(), drawParameters.device, drawParameters.limits);
    Require(std::ranges::find(drawParametersAccepted.Target().supportedCapabilities, spv::CapabilityDrawParameters) != drawParametersAccepted.Target().supportedCapabilities.end(), "enabled shaderDrawParameters was rejected");
    ProfileInput terminating;
    VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR terminate{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES_KHR, &terminating.bda, VK_TRUE};
    terminating.device.pNext = &terminate;
    terminating.extensions.push_back("SPV_KHR_terminate_invocation");
    const AgcDriver::ShaderDeviceProfile terminated(terminating.Target(), terminating.device, terminating.limits);
    Require(std::ranges::find(terminated.Target().supportedExtensions, "SPV_KHR_terminate_invocation") != terminated.Target().supportedExtensions.end(), "enabled shaderTerminateInvocation was rejected");
    ProfileInput disabled;
    VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR disabledTerminate{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES_KHR, &disabled.bda, VK_FALSE};
    disabled.device.pNext = &disabledTerminate;
    disabled.extensions.push_back("SPV_KHR_terminate_invocation");
    Reject([&] { AgcDriver::ShaderDeviceProfile rejected(disabled.Target(), disabled.device, disabled.limits); }, "shaderTerminateInvocation");
    ProfileInput geometryEnabled;
    geometryEnabled.capabilities.push_back(spv::CapabilityGeometry);
    geometryEnabled.core.geometryShader = VK_TRUE;
    const AgcDriver::ShaderDeviceProfile geometryAccepted(geometryEnabled.Target(), geometryEnabled.device, geometryEnabled.limits);
    Require(std::ranges::find(geometryAccepted.Target().supportedCapabilities, spv::CapabilityGeometry) != geometryAccepted.Target().supportedCapabilities.end(), "enabled geometry shaders were rejected");
}

void CheckAbi() {
    using ShaderRecompiler::RuntimeAbi::Binding;
    using ShaderRecompiler::RuntimeAbi::BindingNumber;
    using ShaderRecompiler::RuntimeAbi::Stage;
    std::set<std::uint32_t> bindings;
    for (std::uint32_t stage = 0u; stage < ShaderRecompiler::RuntimeAbi::StageCount; ++stage) {
        for (std::uint32_t binding = 0u; binding < static_cast<std::uint32_t>(Binding::Count); ++binding) {
            Require(bindings.insert(BindingNumber(static_cast<Stage>(stage), static_cast<Binding>(binding))).second, "runtime ABI bindings overlap");
        }
    }
    Require(BindingNumber(Stage::Main, Binding::ShaderData) == 62u && BindingNumber(Stage::Fragment, Binding::ShaderData) == 127u, "runtime ABI binding numbers changed");
    Require(BindingNumber(Stage::Main, Binding::ImageTable) == ShaderRecompiler::RuntimeAbi::ImageTableBinding && BindingNumber(Stage::TessellationEvaluation, Binding::ImageTable) == ShaderRecompiler::RuntimeAbi::ImageTableBinding && !bindings.contains(ShaderRecompiler::RuntimeAbi::ImageTableBinding), "the shared image table binding overlaps a stage binding");
    Reject([] { BindingNumber(static_cast<Stage>(4u), Binding::Buffers); }, "invalid stage or binding");
    Reject([] { BindingNumber(Stage::Main, Binding::Count); }, "invalid stage or binding");
    Reject([] { ShaderRecompiler::RuntimeAbi::RequireVersion(0u); }, "incompatible version");
    ShaderRecompiler::RuntimeAbi::RequireVersion(ShaderRecompiler::RuntimeAbi::Version);
}

void CheckHeaps() {
    using namespace ShaderRecompiler;
    const auto allocate = [](const ImageResource& image, std::uint32_t count, std::uint32_t samplers = 0u) {
        IrProgram program;
        program.Metadata().shaderInfoComplete = true;
        program.Resources().info.images.assign(count, image);
        program.Resources().info.samplers.resize(samplers);
        return BindingAllocator{}.Allocate(program, {0u, 0u, 0u, 128u});
    };
    ImageResource image;
    image.resourceClass = ImageResourceClass::Sampled;
    image.numericClass = IrTextureNumericClass::Float;
    image.dimension = RdnaImageDimension::Dim2D;
    const auto single = allocate(image, 1u);
    const auto sixteenSamplers = allocate(image, 1u, 16u);
    Require(BindingAllocator{}.FindBinding(sixteenSamplers.layout, DescriptorBindingKind::Samplers).resources.size() == 32u, "sixteen logical samplers must fit with both descriptor variants");
    Require(BindingAllocator{}.FindBinding(allocate(image, 1u, 17u).layout, DescriptorBindingKind::Samplers).resources.size() == 34u, "seventeen logical samplers must fit with both descriptor variants");
    const auto twentySamplers = allocate(image, 1u, 20u);
    Require(BindingAllocator{}.FindBinding(twentySamplers.layout, DescriptorBindingKind::Samplers).resources.size() == 40u, "twenty logical samplers must fit with both descriptor variants");
    const auto thirtyTwoSamplers = allocate(image, 1u, 32u);
    Require(BindingAllocator{}.FindBinding(thirtyTwoSamplers.layout, DescriptorBindingKind::Samplers).resources.size() == 64u, "thirty-two logical samplers must fit with both descriptor variants");
    const auto full = allocate(image, RuntimeAbi::SampledHeapCapacity, RuntimeAbi::SamplerHeapCapacity / 2u);
    Require(single.layout.ShaderDataDwords() == full.layout.ShaderDataDwords() && single.layout.memoryOffsetDword == full.layout.memoryOffsetDword && !full.layout.UsesPushData(), "runtime layout depends on resource count");
    Require(full.layout.memoryOffsetDword == 0u && full.layout.DispatchThreadLimitDword() == 0u && full.layout.ShaderDataDwords() == 0u, "direct image resources allocated runtime metadata");
    Reject([&] { allocate(image, RuntimeAbi::SampledHeapCapacity + 1u); }, "heap capacity exceeded");
    Require(BindingAllocator{}.FindBinding(allocate(image, 40u).layout, DescriptorBindingForImage(image)).resources.size() == 40u, "a sampled heap does not hold 40 images of one class");
    Require(BindingAllocator{}.FindBinding(allocate(image, RuntimeAbi::SampledHeapCapacity).layout, DescriptorBindingForImage(image)).resources.size() == RuntimeAbi::SampledHeapCapacity, "a sampled heap does not hold a full class of images");
    Reject([&] { allocate(image, 1u, RuntimeAbi::SamplerHeapCapacity + 1u); }, "metadata capacity");
    image.resourceClass = ImageResourceClass::Storage;
    image.mipMode = ImageMipMode::DynamicStorage;
    image.mipCount = RuntimeAbi::StorageMipSlots;
    const auto storage = allocate(image, 1u);
    Require(BindingAllocator{}.FindBinding(storage.layout, DescriptorBindingForImage(image)).resources.size() == image.mipCount, "storage heap did not reserve each mip");
    const auto shared = allocate(image, 2u);
    Require(BindingAllocator{}.FindBinding(shared.layout, DescriptorBindingForImage(image)).resources.size() == 2u * image.mipCount, "a storage heap did not hold two dynamic-mip images");
    const auto capacity = RuntimeAbi::StorageHeapCapacity / image.mipCount;
    const auto filled = allocate(image, capacity);
    Require(BindingAllocator{}.FindBinding(filled.layout, DescriptorBindingForImage(image)).resources.size() == RuntimeAbi::StorageHeapCapacity, "a full storage heap was not allocated");
    Reject([&] { allocate(image, capacity + 1u); }, "heap capacity exceeded");
    Reject([&] { allocate(image, 0u, RuntimeAbi::SamplerHeapCapacity / 2u + 1u); }, "sampler pairs");
    const std::array dimensions{RdnaImageDimension::Dim1D, RdnaImageDimension::Dim1DArray, RdnaImageDimension::Dim2D, RdnaImageDimension::Dim2DArray, RdnaImageDimension::Dim3D, RdnaImageDimension::Dim2DMsaa, RdnaImageDimension::Dim2DMsaaArray};
    std::set<std::uint32_t> classes;
    for (std::uint32_t group = 0u; group < 8u; ++group) {
        for (const auto dimension : dimensions) {
            image = {};
            image.resourceClass = group < 4u ? ImageResourceClass::Sampled : ImageResourceClass::Storage;
            image.numericClass = group == 1u || group >= 5u ? IrTextureNumericClass::Uint : group == 2u ? IrTextureNumericClass::Sint : IrTextureNumericClass::Float;
            image.depthCompare = group == 3u;
            image.atomic = group >= 6u;
            image.atomic64 = group == 7u;
            image.dimension = dimension;
            const auto binding = DescriptorBindingForImage(image);
            Require(classes.insert(static_cast<std::uint32_t>(binding)).second, "typed image classes overlap");
            Require(RuntimeAbi::HeapCapacity(binding) == (group < 4u ? RuntimeAbi::SampledHeapCapacity : RuntimeAbi::StorageHeapCapacity), "typed image class has an invalid capacity");
        }
    }
    Require(classes.size() == RuntimeAbi::ImageBindingCount && *classes.begin() == 1u && *classes.rbegin() == RuntimeAbi::ImageBindingCount, "typed image class mapping is incomplete");
    Reject([] { RuntimeAbi::HeapCapacity(RuntimeAbi::Binding::ShaderData); }, "not a typed heap");
}

}

int main() {
    try {
        CheckAbi();
        CheckHeaps();
        CheckProfile();
        std::cout << "shader runtime ABI and device profile tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
