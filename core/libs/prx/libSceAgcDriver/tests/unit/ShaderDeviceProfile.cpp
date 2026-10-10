#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/ShaderDeviceProfile.hpp"
#include "BdaAbi.hpp"
#include "Optimization/BindingAllocator.hpp"
#include <spirv/unified1/spirv.hpp>

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using Testing::Case;
using Testing::Require;

struct ProfileInput {
    std::vector<std::uint32_t> capabilities{spv::CapabilityShader, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    std::array<std::string, 2> extensionStrings{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    std::array<std::string_view, 2> extensions{extensionStrings[0], extensionStrings[1]};
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

    ProfileInput(const ProfileInput&) = delete;
    ProfileInput& operator=(const ProfileInput&) = delete;

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
    const auto error = Testing::RequireThrows<std::runtime_error>(action, std::string("an invalid input was accepted, expected a rejection with '") + expected + "'");
    Require(std::string(error.what()).find(expected) != std::string::npos, std::string("unexpected validation error: ") + error.what() + ", expected '" + expected + "'");
}

template<typename TMutate>
void RequireInvalidProfile(TMutate mutate, const char* expected) {
    ProfileInput candidate;
    mutate(candidate);
    Reject([&] { AgcDriver::ShaderDeviceProfile rejected(candidate.Target(), candidate.device, candidate.limits); }, expected);
}

ShaderRecompiler::BindingAllocationResult AllocateImages(const ShaderRecompiler::ImageResource& image, std::uint32_t count, std::uint32_t samplers = 0u) {
    ShaderRecompiler::IrProgram program;
    program.Metadata().shaderInfoComplete = true;
    program.Resources().info.images.assign(count, image);
    program.Resources().info.samplers.resize(samplers);
    return ShaderRecompiler::BindingAllocator{}.Allocate(program, {0u, 0u, 0u, 128u});
}

ShaderRecompiler::ImageResource SampledImage() {
    ShaderRecompiler::ImageResource image;
    image.resourceClass = ShaderRecompiler::ImageResourceClass::Sampled;
    image.numericClass = ShaderRecompiler::IrTextureNumericClass::Float;
    image.dimension = ShaderRecompiler::RdnaImageDimension::Dim2D;
    return image;
}

ShaderRecompiler::ImageResource FullMipStorageImage() {
    auto image = SampledImage();
    image.resourceClass = ShaderRecompiler::ImageResourceClass::Storage;
    image.mipMode = ShaderRecompiler::ImageMipMode::DynamicStorage;
    image.mipCount = ShaderRecompiler::RuntimeAbi::StorageHeapCapacity;
    return image;
}

const Case frozenProfile{"ShaderDeviceProfile_InputsChangedAfterConstruction_KeepsItsOwnCopy", [] {
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
}};

const Case missingFeatures{"ShaderDeviceProfile_MissingRequiredFeature_IsRejected", [] {
    RequireInvalidProfile([](auto& value) { value.device.pEnabledFeatures = nullptr; }, "enabled core features");
    RequireInvalidProfile([](auto& value) { value.bda.bufferDeviceAddress = VK_FALSE; }, "bufferDeviceAddress");
    RequireInvalidProfile([](auto& value) { value.device.pNext = nullptr; }, "bufferDeviceAddress");
    RequireInvalidProfile([](auto& value) { value.core.shaderInt64 = VK_FALSE; }, "shaderInt64");
    RequireInvalidProfile([](auto& value) { value.bytes.storageBuffer8BitAccess = VK_FALSE; }, "storageBuffer8BitAccess");
    RequireInvalidProfile([](auto& value) { value.robustness.nullDescriptor = VK_FALSE; }, "nullDescriptor");
    RequireInvalidProfile([](auto& value) { value.indexing.pNext = nullptr; }, "nullDescriptor");
    RequireInvalidProfile([](auto& value) { value.core.fragmentStoresAndAtomics = VK_FALSE; }, "graphics stores and atomics");
}};

const Case insufficientLimits{"ShaderDeviceProfile_RuntimeAbiExceedsDeviceLimits_IsRejected", [] {
    RequireInvalidProfile([](auto& value) { value.limits.maxPushConstantsSize = 127u; }, "exceeds device limits");
    RequireInvalidProfile([](auto& value) { value.limits.maxBoundDescriptorSets = 0u; }, "exceeds device limits");
    RequireInvalidProfile([](auto& value) { --value.limits.maxStorageBufferRange; }, "ShaderData exceeds device limits");
}};

const Case missingRuntimeSupport{"ShaderDeviceProfile_MissingRuntimeCapabilityOrExtension_IsRejected", [] {
    RequireInvalidProfile([](auto& value) { value.capabilities.clear(); }, "missing runtime capabilities");
    RequireInvalidProfile([](auto& value) { value.extensions[0] = "missing"; }, "missing runtime extensions");
}};

const Case unbackedCapabilities{"ShaderDeviceProfile_CapabilityWithoutEnabledFeature_IsRejected", [] {
    RequireInvalidProfile([](auto& value) { value.capabilities.push_back(spv::CapabilitySampledImageArrayDynamicIndexing); }, "shaderSampledImageArrayDynamicIndexing");
    RequireInvalidProfile([](auto& value) { value.capabilities.push_back(spv::CapabilityStorageImageArrayNonUniformIndexing); }, "shaderStorageImageArrayNonUniformIndexing");
    RequireInvalidProfile([](auto& value) { value.capabilities.push_back(spv::CapabilityStorageImageReadWithoutFormat); }, "shaderStorageImageReadWithoutFormat");
    RequireInvalidProfile([](auto& value) { value.capabilities.push_back(spv::CapabilityStorageImageMultisample); }, "shaderStorageImageMultisample");
    RequireInvalidProfile([](auto& value) { value.capabilities.push_back(spv::CapabilityFragmentBarycentricKHR); }, "fragmentShaderBarycentric");
    RequireInvalidProfile([](auto& value) { value.capabilities.push_back(spv::CapabilityMeshShadingEXT); }, "meshShader");
}};

const Case enabledIndexing{"ShaderDeviceProfile_DescriptorIndexingWithEnabledFeatures_IsAccepted", [] {
    ProfileInput enabled;
    enabled.capabilities.push_back(spv::CapabilitySampledImageArrayDynamicIndexing);
    enabled.capabilities.push_back(spv::CapabilityStorageImageArrayNonUniformIndexing);
    enabled.core.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    enabled.indexing.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    const AgcDriver::ShaderDeviceProfile accepted(enabled.Target(), enabled.device, enabled.limits);
    Require(accepted.Target().supportedCapabilities.size() == 6u, "enabled descriptor indexing was rejected");
}};

const Case uniqueBindings{"BindingNumber_EveryStageAndBinding_IsUniqueAndStable", [] {
    using ShaderRecompiler::RuntimeAbi::Binding;
    using ShaderRecompiler::RuntimeAbi::BindingNumber;
    using ShaderRecompiler::RuntimeAbi::Stage;
    std::set<std::uint32_t> bindings;
    for (std::uint32_t stage = 0u; stage < ShaderRecompiler::RuntimeAbi::StageCount; ++stage) {
        for (std::uint32_t binding = 0u; binding < static_cast<std::uint32_t>(Binding::Count); ++binding) {
            Require(bindings.insert(BindingNumber(static_cast<Stage>(stage), static_cast<Binding>(binding))).second, "runtime ABI bindings overlap");
        }
    }
    Require(BindingNumber(Stage::Main, Binding::ShaderData) == 62u && BindingNumber(Stage::Fragment, Binding::ShaderData) == 125u, "runtime ABI binding numbers changed");
}};

const Case invalidBindings{"BindingNumber_InvalidStageOrBinding_Throws", [] {
    using ShaderRecompiler::RuntimeAbi::Binding;
    using ShaderRecompiler::RuntimeAbi::BindingNumber;
    using ShaderRecompiler::RuntimeAbi::Stage;
    Reject([] { BindingNumber(static_cast<Stage>(4u), Binding::Buffers); }, "invalid stage or binding");
    Reject([] { BindingNumber(Stage::Main, Binding::Count); }, "invalid stage or binding");
}};

const Case abiVersion{"RequireVersion_OnlyTheCurrentVersion_IsAccepted", [] {
    Reject([] { ShaderRecompiler::RuntimeAbi::RequireVersion(0u); }, "incompatible version");
    ShaderRecompiler::RuntimeAbi::RequireVersion(ShaderRecompiler::RuntimeAbi::Version);
}};

const Case sampledHeap{"Allocate_SampledImages_UseDirectHeapsWithoutRuntimeMetadata", [] {
    using namespace ShaderRecompiler;
    const auto single = AllocateImages(SampledImage(), 1u);
    const auto full = AllocateImages(SampledImage(), RuntimeAbi::SampledHeapCapacity, RuntimeAbi::SamplerHeapCapacity / 2u);
    Require(single.layout.ShaderDataDwords() == full.layout.ShaderDataDwords() && single.layout.memoryOffsetDword == full.layout.memoryOffsetDword && !full.layout.UsesPushData(), "runtime layout depends on resource count");
    Require(full.layout.memoryOffsetDword == 0u && full.layout.DispatchThreadLimitDword() == 0u && full.layout.ShaderDataDwords() == 0u, "direct image resources allocated runtime metadata");
}};

const Case sampledHeapOverflow{"Allocate_MoreSampledImagesOrSamplersThanTheHeap_Throws", [] {
    using namespace ShaderRecompiler;
    Reject([] { static_cast<void>(AllocateImages(SampledImage(), RuntimeAbi::SampledHeapCapacity + 1u)); }, "heap capacity exceeded");
    Reject([] { static_cast<void>(AllocateImages(SampledImage(), 1u, RuntimeAbi::SamplerHeapCapacity + 1u)); }, "metadata capacity");
}};

const Case storageHeap{"Allocate_DynamicMipStorageImage_ReservesEachMip", [] {
    using namespace ShaderRecompiler;
    const auto image = FullMipStorageImage();
    const auto storage = AllocateImages(image, 1u);
    Require(BindingAllocator{}.FindBinding(storage.layout, DescriptorBindingForImage(image)).resources.size() == image.mipCount, "storage heap did not reserve each mip");
}};

const Case storageHeapOverflow{"Allocate_StorageMipsOrSamplerPairsBeyondTheHeap_Throws", [] {
    using namespace ShaderRecompiler;
    Reject([] { static_cast<void>(AllocateImages(FullMipStorageImage(), 2u)); }, "heap capacity exceeded");
    Reject([] { static_cast<void>(AllocateImages(FullMipStorageImage(), 0u, RuntimeAbi::SamplerHeapCapacity / 2u + 1u)); }, "sampler pairs");
}};

const Case typedImageClasses{"DescriptorBindingForImage_EveryTypedImageClass_GetsItsOwnHeap", [] {
    using namespace ShaderRecompiler;
    const std::array dimensions{RdnaImageDimension::Dim1D, RdnaImageDimension::Dim1DArray, RdnaImageDimension::Dim2D, RdnaImageDimension::Dim2DArray, RdnaImageDimension::Dim3D, RdnaImageDimension::Dim2DMsaa, RdnaImageDimension::Dim2DMsaaArray};
    std::set<std::uint32_t> classes;
    for (std::uint32_t group = 0u; group < 8u; ++group) {
        for (const auto dimension : dimensions) {
            ImageResource image{};
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
}};

} // namespace
