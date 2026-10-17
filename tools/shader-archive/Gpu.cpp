#include "Gpu.hpp"
#include "PipelineSpecialization.hpp"
#include <map>
#include <cstdlib>
#include <set>
#include <spirv/unified1/spirv.hpp>
#include <unordered_map>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ShaderArchive {
namespace {
constexpr std::uint32_t deviceModeId = 0x7fff0000u;
void VkCheck(VkResult result, const char *operation) {
    Check(result == VK_SUCCESS, std::string(operation) + ": VkResult " + std::to_string(result));
}
VkShaderStageFlagBits Stage(ShaderRecompiler::ShaderStage stage) {
    using S = ShaderRecompiler::ShaderStage;
    switch (stage) {
    case S::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    case S::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case S::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case S::Mesh:
        return VK_SHADER_STAGE_MESH_BIT_EXT;
    default:
        throw std::runtime_error("unsupported Vulkan shader stage");
    }
}
VKAPI_ATTR VkBool32 VKAPI_CALL Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                                     const VkDebugUtilsMessengerCallbackDataEXT *message, void *counter) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        static_cast<std::atomic_uint *>(counter)->fetch_add(1);
    std::fprintf(stderr, "[vulkan] %s\n", message->pMessage);
    return VK_FALSE;
}
std::vector<std::vector<VkDescriptorSetLayoutBinding>> Bindings(std::span<const std::uint32_t> w,
                                                                VkShaderStageFlagBits stage) {
    Check(w.size() >= 5 && w[0] == spv::MagicNumber && w[3] < 16000000, "invalid SPIR-V reflection header");
    const auto bound = w[3];
    std::vector<std::uint32_t> type(bound), binding(bound, ~0u), set(bound), constant(bound);
    std::vector<std::pair<unsigned, unsigned>> variables;
    for (std::size_t at = 5; at < w.size();) {
        const auto count = w[at] >> 16, op = w[at] & 65535;
        Check(count > 0 && count <= w.size() - at, "invalid SPIR-V instruction");
        if (op >= spv::OpTypeVoid && op <= spv::OpTypeForwardPointer && count >= 2) {
            Check(w[at + 1] < bound, "SPIR-V type ID");
            type[w[at + 1]] = at;
        }
        if ((op == spv::OpConstant || op == spv::OpSpecConstant) && count >= 4) {
            Check(w[at + 2] < bound, "SPIR-V constant ID");
            constant[w[at + 2]] = w[at + 3];
        }
        if (op == spv::OpDecorate && count >= 4) {
            Check(w[at + 1] < bound, "SPIR-V decoration ID");
            if (w[at + 2] == spv::DecorationBinding)
                binding[w[at + 1]] = w[at + 3];
            if (w[at + 2] == spv::DecorationDescriptorSet)
                set[w[at + 1]] = w[at + 3];
        }
        if (op == spv::OpVariable && count >= 4)
            variables.emplace_back(w[at + 2], w[at + 1]);
        at += count;
    }
    std::vector<std::vector<VkDescriptorSetLayoutBinding>> sets;
    for (auto [id, ptr] : variables) {
        Check(id < bound && ptr < bound, "SPIR-V variable ID");
        if (binding[id] == ~0u)
            continue;
        Check(set[id] < 8 && type[ptr] > 0, "SPIR-V descriptor set");
        auto p = type[ptr];
        Check((w[p] & 65535) == spv::OpTypePointer, "descriptor pointer");
        auto storage = w[p + 2], inner = w[p + 3];
        unsigned count = 1;
        for (unsigned depth = 0;; ++depth) {
            Check(inner < bound && type[inner] > 0 && depth < 16, "descriptor type chain");
            p = type[inner];
            if ((w[p] & 65535) != spv::OpTypeArray)
                break;
            Check(w[p + 3] < bound && constant[w[p + 3]] > 0 && constant[w[p + 3]] <= 65536 / count,
                  "descriptor array length");
            count *= constant[w[p + 3]];
            inner = w[p + 2];
        }
        const auto op = w[p] & 65535;
        VkDescriptorType descriptor{};
        if (op == spv::OpTypeSampler)
            descriptor = VK_DESCRIPTOR_TYPE_SAMPLER;
        else if (op == spv::OpTypeSampledImage)
            descriptor = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        else if (op == spv::OpTypeImage)
            descriptor = w[p + 3] == spv::DimBuffer
                             ? (w[p + 7] == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
                                              : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                             : (w[p + 7] == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        else {
            Check(op == spv::OpTypeStruct, "unsupported descriptor type");
            descriptor = storage == spv::StorageClassStorageBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                                                                   : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        if (sets.size() <= set[id])
            sets.resize(set[id] + 1);
        sets[set[id]].push_back({binding[id], descriptor, count, stage, nullptr});
    }
    for (auto &s : sets)
        std::sort(s.begin(), s.end(), [](auto a, auto b) { return a.binding < b.binding; });
    return sets;
}
} // namespace
struct GpuCompiler::State {
    void *library{};
    VkInstance instance{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    VkDebugUtilsMessengerEXT messenger{};
    PFN_vkGetInstanceProcAddr getInstance{};
    PFN_vkGetDeviceProcAddr getDevice{};
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceVulkan12Properties prop12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceVulkan13Properties prop13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};
    VkPhysicalDeviceMeshShaderPropertiesEXT meshProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    std::vector<unsigned> capabilities;
    std::vector<std::string_view> extensions;
    std::vector<std::string> enabledExtensions;
    bool mesh{}, barycentric{}, offsets{}, gpl{}, subgroupControl{};
    std::atomic_uint errors{0};
    struct Worker {
        VkPipelineCache cache{};
        std::map<std::string, std::pair<VkPipelineLayout, std::vector<VkDescriptorSetLayout>>> layouts;
    };
    std::vector<Worker> workers;
    template <class T> T I(const char *name) const {
        auto f = getInstance(instance, name);
        Check(f != nullptr, std::string("missing Vulkan function ") + name);
        return reinterpret_cast<T>(f);
    }
    template <class T> T D(const char *name) const {
        auto f = getDevice(device, name);
        Check(f != nullptr, std::string("missing Vulkan function ") + name);
        return reinterpret_cast<T>(f);
    }
    ~State() {
        if (device) {
            for (auto &w : workers) {
                for (auto &[key, l] : w.layouts) {
                    D<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(device, l.first, nullptr);
                    for (auto set : l.second)
                        D<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(device, set, nullptr);
                }
                if (w.cache)
                    D<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache")(device, w.cache, nullptr);
            }
            D<PFN_vkDestroyDevice>("vkDestroyDevice")(device, nullptr);
        }
        if (messenger)
            I<PFN_vkDestroyDebugUtilsMessengerEXT>("vkDestroyDebugUtilsMessengerEXT")(instance, messenger, nullptr);
        if (instance)
            I<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
#ifdef _WIN32
        if (library)
            FreeLibrary(static_cast<HMODULE>(library));
#else
        if (library)
            dlclose(library);
#endif
    }
};
GpuCompiler::GpuCompiler(const std::string &name, unsigned workers, bool validation)
    : state(std::make_unique<State>()) {
    auto &s = *state;
#ifdef _WIN32
    s.library = LoadLibraryW(L"vulkan-1.dll");
    Check(s.library, "load Vulkan loader");
    s.getInstance = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(static_cast<HMODULE>(s.library), "vkGetInstanceProcAddr"));
#else
    s.library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    Check(s.library, "load Vulkan loader");
    s.getInstance = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(s.library, "vkGetInstanceProcAddr"));
#endif
    Check(s.getInstance != nullptr, "Vulkan loader entry point");
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "AnyPS5 shader archive";
    app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance.pApplicationInfo = &app;
    const char *layer = "VK_LAYER_KHRONOS_validation";
    const char *debugExt = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    debug.pfnUserCallback = Debug;
    debug.pUserData = &s.errors;
    if (validation) {
        instance.enabledLayerCount = 1;
        instance.ppEnabledLayerNames = &layer;
        instance.enabledExtensionCount = 1;
        instance.ppEnabledExtensionNames = &debugExt;
        instance.pNext = &debug;
    }
    VkCheck(s.I<PFN_vkCreateInstance>("vkCreateInstance")(&instance, nullptr, &s.instance), "vkCreateInstance");
    if (validation)
        VkCheck(s.I<PFN_vkCreateDebugUtilsMessengerEXT>("vkCreateDebugUtilsMessengerEXT")(s.instance, &debug, nullptr,
                                                                                          &s.messenger),
                "debug messenger");
    unsigned count = 0;
    VkCheck(s.I<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices")(s.instance, &count, nullptr),
            "enumerate devices");
    std::vector<VkPhysicalDevice> devices(count);
    VkCheck(s.I<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices")(s.instance, &count, devices.data()),
            "enumerate devices");
    for (auto device : devices) {
        VkPhysicalDeviceProperties p{};
        s.I<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(device, &p);
        if (p.apiVersion < VK_API_VERSION_1_3 ||
            (!name.empty() && std::string(p.deviceName).find(name) == std::string::npos))
            continue;
        if (!s.physical || p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            s.physical = device;
            s.properties = p;
            if (!name.empty() || p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                break;
        }
    }
    Check(s.physical != VK_NULL_HANDLE, "no matching Vulkan 1.3 GPU");
    VkCheck(s.I<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties")(s.physical, nullptr,
                                                                                                  &count, nullptr),
            "enumerate extensions");
    std::vector<VkExtensionProperties> ext(count);
    VkCheck(s.I<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties")(s.physical, nullptr,
                                                                                                  &count, ext.data()),
            "enumerate extensions");
    auto has = [&](const char *n) {
        return std::any_of(ext.begin(), ext.end(), [&](const auto &e) { return std::strcmp(e.extensionName, n) == 0; });
    };
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.pNext = &f11;
    f11.pNext = &f12;
    f12.pNext = &f13;
    void **tail = &f13.pNext;
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR bary{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR};
    VkPhysicalDeviceMeshShaderFeaturesEXT mesh{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceShaderClockFeaturesKHR clock{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR};
    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT imageAtomic{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT};
    VkPhysicalDeviceMaintenance8FeaturesKHR maintenance8{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_8_FEATURES_KHR};
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT library{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT};
    auto chain = [&](const char *n, auto &f) {
        if (has(n)) {
            s.enabledExtensions.emplace_back(n);
            *tail = &f;
            tail = &f.pNext;
        }
    };
    chain(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, bary);
    chain(VK_EXT_MESH_SHADER_EXTENSION_NAME, mesh);
    chain(VK_KHR_SHADER_CLOCK_EXTENSION_NAME, clock);
    chain(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME, imageAtomic);
    chain(VK_KHR_MAINTENANCE_8_EXTENSION_NAME, maintenance8);
    chain(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME, library);
    if (has(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME))
        s.enabledExtensions.emplace_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    s.I<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(s.physical, &features);
    mesh.primitiveFragmentShadingRateMeshShader = VK_FALSE;
    s.mesh = mesh.meshShader;
    s.barycentric = bary.fragmentShaderBarycentric;
    s.offsets = maintenance8.maintenance8;
    s.gpl = library.graphicsPipelineLibrary;
    s.subgroupControl = f13.subgroupSizeControl;
    Check(f12.bufferDeviceAddress && features.features.shaderInt64 && f12.storageBuffer8BitAccess,
          "GPU lacks AnyPS5 BDA shader prerequisites");
    Check(f13.dynamicRendering, "GPU lacks dynamic rendering");
    VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    props.pNext = &s.subgroup;
    s.subgroup.pNext = &s.prop12;
    s.prop12.pNext = &s.prop13;
    if (s.mesh)
        s.prop13.pNext = &s.meshProperties;
    s.I<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(s.physical, &props);
    unsigned families = 0;
    s.I<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")(s.physical, &families,
                                                                                                  nullptr);
    std::vector<VkQueueFamilyProperties> queues(families);
    s.I<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")(s.physical, &families,
                                                                                                  queues.data());
    unsigned family = 0;
    while (family < families && ((queues[family].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) !=
                                 (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)))
        ++family;
    Check(family < families, "no graphics/compute family");
    float priority = 1;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    std::vector<const char *> names;
    for (const auto &n : s.enabledExtensions)
        names.push_back(n.c_str());
    VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device.pNext = &features;
    device.queueCreateInfoCount = 1;
    device.pQueueCreateInfos = &queue;
    device.enabledExtensionCount = names.size();
    device.ppEnabledExtensionNames = names.data();
    VkCheck(s.I<PFN_vkCreateDevice>("vkCreateDevice")(s.physical, &device, nullptr, &s.device), "vkCreateDevice");
    s.getDevice = s.I<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
    auto cap = [&](spv::Capability c, bool enabled = true) {
        if (enabled)
            s.capabilities.push_back(c);
    };
    const auto &f = features.features;
    cap(spv::CapabilityShader);
    cap(spv::CapabilityInt64, f.shaderInt64);
    cap(spv::CapabilityFloat64, f.shaderFloat64 && s.prop12.shaderSignedZeroInfNanPreserveFloat64);
    cap(spv::CapabilityInt16, f.shaderInt16);
    cap(spv::CapabilityFloat16, f12.shaderFloat16 && s.prop12.shaderRoundingModeRTEFloat16 &&
                                    s.prop12.shaderDenormPreserveFloat16 &&
                                    s.prop12.shaderSignedZeroInfNanPreserveFloat16);
    cap(spv::CapabilityInt64Atomics, f12.shaderBufferInt64Atomics);
    cap(spv::CapabilityPhysicalStorageBufferAddresses, f12.bufferDeviceAddress);
    cap(spv::CapabilityStorageBuffer8BitAccess, f12.storageBuffer8BitAccess);
    cap(spv::CapabilityStorageBuffer16BitAccess, f11.storageBuffer16BitAccess);
    cap(spv::CapabilityImageGatherExtended, f.shaderImageGatherExtended);
    cap(spv::CapabilityMinLod, f.shaderResourceMinLod);
    cap(spv::CapabilitySampleRateShading, f.sampleRateShading);
    cap(spv::CapabilityClipDistance, f.shaderClipDistance);
    cap(spv::CapabilityCullDistance, f.shaderCullDistance);
    cap(spv::CapabilityImageQuery);
    cap(spv::CapabilityDerivativeControl);
    cap(spv::CapabilitySampled1D);
    cap(spv::CapabilityImage1D);
    cap(spv::CapabilitySampledBuffer);
    cap(spv::CapabilityImageBuffer);
    cap(spv::CapabilityImageMSArray);
    cap(spv::CapabilityStorageImageExtendedFormats, f.shaderStorageImageExtendedFormats);
    cap(spv::CapabilityStorageImageMultisample, f.shaderStorageImageMultisample);
    cap(spv::CapabilityStorageImageReadWithoutFormat, f.shaderStorageImageReadWithoutFormat);
    cap(spv::CapabilityStorageImageWriteWithoutFormat, f.shaderStorageImageWriteWithoutFormat);
    cap(spv::CapabilitySampledImageArrayDynamicIndexing, f.shaderSampledImageArrayDynamicIndexing);
    cap(spv::CapabilityStorageImageArrayDynamicIndexing, f.shaderStorageImageArrayDynamicIndexing);
    cap(spv::CapabilityShaderNonUniform,
        f12.shaderSampledImageArrayNonUniformIndexing || f12.shaderStorageImageArrayNonUniformIndexing);
    cap(spv::CapabilitySampledImageArrayNonUniformIndexing, f12.shaderSampledImageArrayNonUniformIndexing);
    cap(spv::CapabilityStorageImageArrayNonUniformIndexing, f12.shaderStorageImageArrayNonUniformIndexing);
    const std::pair<spv::Capability, VkSubgroupFeatureFlags> subgroupCaps[] = {
        {spv::CapabilityGroupNonUniform, VK_SUBGROUP_FEATURE_BASIC_BIT},
        {spv::CapabilityGroupNonUniformVote, VK_SUBGROUP_FEATURE_VOTE_BIT},
        {spv::CapabilityGroupNonUniformArithmetic, VK_SUBGROUP_FEATURE_ARITHMETIC_BIT},
        {spv::CapabilityGroupNonUniformBallot, VK_SUBGROUP_FEATURE_BALLOT_BIT},
        {spv::CapabilityGroupNonUniformShuffle, VK_SUBGROUP_FEATURE_SHUFFLE_BIT},
        {spv::CapabilityGroupNonUniformShuffleRelative, VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT},
        {spv::CapabilityGroupNonUniformClustered, VK_SUBGROUP_FEATURE_CLUSTERED_BIT},
        {spv::CapabilityGroupNonUniformQuad, VK_SUBGROUP_FEATURE_QUAD_BIT}};
    for (auto [c, flag] : subgroupCaps)
        cap(c, s.subgroup.supportedOperations & flag);
    cap(spv::CapabilitySignedZeroInfNanPreserve, s.prop12.shaderSignedZeroInfNanPreserveFloat32);
    cap(spv::CapabilityDenormPreserve, f12.shaderFloat16 && s.prop12.shaderDenormPreserveFloat16);
    cap(spv::CapabilityRoundingModeRTE, f12.shaderFloat16 && s.prop12.shaderRoundingModeRTEFloat16);
    cap(spv::CapabilityFragmentBarycentricKHR, s.barycentric);
    cap(spv::CapabilityMeshShadingEXT, s.mesh);
    cap(spv::CapabilityShaderClockKHR, clock.shaderSubgroupClock && clock.shaderDeviceClock);
    cap(spv::CapabilityInt64ImageEXT, imageAtomic.shaderImageInt64Atomics);
    s.extensions = {"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage", "SPV_KHR_16bit_storage",
                    "SPV_KHR_float_controls", "SPV_EXT_descriptor_indexing"};
    if (s.barycentric)
        s.extensions.push_back("SPV_KHR_fragment_shader_barycentric");
    if (s.mesh)
        s.extensions.push_back("SPV_EXT_mesh_shader");
    if (clock.shaderSubgroupClock && clock.shaderDeviceClock)
        s.extensions.push_back("SPV_KHR_shader_clock");
    if (imageAtomic.shaderImageInt64Atomics)
        s.extensions.push_back("SPV_EXT_shader_image_int64");
    s.workers.resize(workers);
    for (auto &w : s.workers) {
        VkPipelineCacheCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        VkCheck(s.D<PFN_vkCreatePipelineCache>("vkCreatePipelineCache")(s.device, &ci, nullptr, &w.cache),
                "create pipeline cache");
    }
}
GpuCompiler::~GpuCompiler() = default;
ShaderRecompiler::SpirvTarget GpuCompiler::Target() const {
    const auto &s = *state;
    const auto &l = s.properties.limits;
    ShaderRecompiler::SpirvTarget t{};
    t.vulkanVersion = VK_API_VERSION_1_3;
    t.spirvVersion = 0x10600;
    t.bdaAbiVersion = 1;
    t.subgroupSize = s.subgroup.subgroupSize;
    t.supportedCapabilities = s.capabilities;
    t.supportedExtensions = s.extensions;
    t.fragmentShaderBarycentricEnabled = s.barycentric;
    t.nonConstantImageOffsets = s.offsets;
    t.maxWorkgroupSize = {l.maxComputeWorkGroupSize[0], l.maxComputeWorkGroupSize[1], l.maxComputeWorkGroupSize[2]};
    t.maxWorkgroupInvocations = l.maxComputeWorkGroupInvocations;
    t.maxWorkgroupSharedMemoryBytes = l.maxComputeSharedMemorySize;
    if (s.mesh) {
        const auto &m = s.meshProperties;
        t.mesh = ShaderRecompiler::MeshTargetLimits{
            {m.maxMeshWorkGroupSize[0], m.maxMeshWorkGroupSize[1], m.maxMeshWorkGroupSize[2]},
            m.maxMeshWorkGroupInvocations,
            std::min(m.maxMeshSharedMemorySize, m.maxMeshPayloadAndSharedMemorySize),
            m.maxMeshOutputVertices,
            m.maxMeshOutputPrimitives,
            m.maxMeshOutputComponents,
            std::min(m.maxMeshOutputMemorySize, m.maxMeshPayloadAndOutputMemorySize),
            m.meshOutputPerVertexGranularity,
            m.meshOutputPerPrimitiveGranularity};
    }
    return t;
}
unsigned GpuCompiler::Subgroup(unsigned wave, ShaderRecompiler::ShaderStage stage) const {
    const auto &s = *state;
    const auto flag = Stage(stage);
    if (s.subgroupControl && (s.prop13.requiredSubgroupSizeStages & flag) && wave >= s.prop13.minSubgroupSize &&
        wave <= s.prop13.maxSubgroupSize)
        return wave;
    return s.subgroup.subgroupSize;
}
Json::Value GpuCompiler::Info() const {
    const auto &s = *state;
    Json::Value v;
    v["name"] = s.properties.deviceName;
    v["vendor_id"] = s.properties.vendorID;
    v["device_id"] = s.properties.deviceID;
    v["driver_version"] = s.properties.driverVersion;
    v["api_version"] = s.properties.apiVersion;
    v["pipeline_cache_uuid"] = Hex(s.properties.pipelineCacheUUID);
    v["subgroup_size"] = s.subgroup.subgroupSize;
    v["min_subgroup_size"] = s.prop13.minSubgroupSize;
    v["max_subgroup_size"] = s.prop13.maxSubgroupSize;
    v["required_subgroup_stages"] = s.prop13.requiredSubgroupSizeStages;
    v["graphics_pipeline_library"] = s.gpl;
    v["mesh_shader"] = s.mesh;
    v["maintenance8"] = s.offsets;
    v["validation_errors"] = s.errors.load();
    for (const auto &e : s.enabledExtensions)
        v["enabled_extensions"].append(e);
    for (auto c : s.capabilities)
        v["spirv_capabilities"].append(c);
    return v;
}
void GpuCompiler::Compile(const ShaderRecompiler::RecompileRequest &request,
                          const ShaderRecompiler::CompiledVariant &variant, unsigned worker) {
    auto &s = *state;
    auto &w = s.workers.at(worker);
    const auto stage = Stage(request.shader.stage);
    const auto spirv = variant.artifact.spirv.Words();
    auto bindings = Bindings(spirv, stage);
    std::string key = std::to_string(unsigned(stage)) + ":";
    for (const auto &set : bindings) {
        key += '|';
        for (const auto &b : set)
            key += std::to_string(b.binding) + "," + std::to_string(b.descriptorType) + "," +
                   std::to_string(b.descriptorCount) + ";";
    }
    VkPipelineLayout layout{};
    auto found = w.layouts.find(key);
    if (found != w.layouts.end())
        layout = found->second.first;
    else {
        std::vector<VkDescriptorSetLayout> sets;
        try {
            for (const auto &b : bindings) {
                VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
                info.bindingCount = b.size();
                info.pBindings = b.data();
                VkDescriptorSetLayout set{};
                VkCheck(
                    s.D<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(s.device, &info, nullptr, &set),
                    "descriptor layout");
                sets.push_back(set);
            }
            VkPushConstantRange range{stage, 0, 128};
            VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            info.setLayoutCount = sets.size();
            info.pSetLayouts = sets.data();
            info.pushConstantRangeCount = 1;
            info.pPushConstantRanges = &range;
            VkCheck(s.D<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(s.device, &info, nullptr, &layout),
                    "pipeline layout");
            w.layouts.emplace(key, std::make_pair(layout, std::move(sets)));
        } catch (...) {
            for (auto set : sets)
                s.D<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(s.device, set, nullptr);
            throw;
        }
    }
    VkShaderModule module{};
    VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleInfo.codeSize = spirv.size() * 4;
    moduleInfo.pCode = spirv.data();
    VkCheck(s.D<PFN_vkCreateShaderModule>("vkCreateShaderModule")(s.device, &moduleInfo, nullptr, &module),
            "shader module");
    VkPipeline pipeline{};
    try {
        VkPipelineShaderStageRequiredSubgroupSizeCreateInfo required{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO};
        required.requiredSubgroupSize = variant.artifact.hostSubgroupSize;
        VkPipelineShaderStageCreateInfo shader{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        shader.stage = stage;
        shader.module = module;
        shader.pName = "main";
        const std::uint32_t deviceMode = (s.gpl ? 1u : 0u);
        const VkSpecializationMapEntry modeEntry{deviceModeId, 0, sizeof(deviceMode)};
        const VkSpecializationInfo specialization{1, &modeEntry, sizeof(deviceMode), &deviceMode};
        shader.pSpecializationInfo = &specialization;
        if (s.subgroupControl && (s.prop13.requiredSubgroupSizeStages & stage))
            shader.pNext = &required;
        else
            Check(required.requiredSubgroupSize == s.subgroup.subgroupSize, "unsupported stage subgroup size");
        if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            info.stage = shader;
            info.layout = layout;
            VkCheck(s.D<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(s.device, w.cache, 1, &info, nullptr,
                                                                                  &pipeline),
                    "compute pipeline");
        } else {
            Check(s.gpl, "GPU has no graphics pipeline library support");
            VkGraphicsPipelineLibraryCreateInfoEXT library{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT};
            library.flags = stage == VK_SHADER_STAGE_FRAGMENT_BIT
                                ? VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT
                                : VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
            VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
            library.pNext = &rendering;
            VkViewport viewport{0, 0, 1, 1, 0, 1};
            VkRect2D scissor{{0, 0}, {1, 1}};
            VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            vp.viewportCount = 1;
            vp.pViewports = &viewport;
            vp.scissorCount = 1;
            vp.pScissors = &scissor;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth = 1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
            depth.maxDepthBounds = 1;
            VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            info.pNext = &library;
            info.flags =
                VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
            info.stageCount = 1;
            info.pStages = &shader;
            info.layout = layout;
            if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
                info.pMultisampleState = &ms;
                info.pDepthStencilState = &depth;
            } else {
                info.pViewportState = &vp;
                info.pRasterizationState = &raster;
            }
            VkCheck(s.D<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(s.device, w.cache, 1, &info,
                                                                                    nullptr, &pipeline),
                    "graphics shader library");
        }
    } catch (...) {
        if (pipeline)
            s.D<PFN_vkDestroyPipeline>("vkDestroyPipeline")(s.device, pipeline, nullptr);
        s.D<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(s.device, module, nullptr);
        throw;
    }
    s.D<PFN_vkDestroyPipeline>("vkDestroyPipeline")(s.device, pipeline, nullptr);
    s.D<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(s.device, module, nullptr);
}
} // namespace ShaderArchive
