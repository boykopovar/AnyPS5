#include "BdaAbi.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentationScaler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include <chrono>
#include <cstdlib>
#include <SDL_loadso.h>
#include <SDL_error.h>
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <optional>
#include <algorithm>
#include <cstring>
#include <limits>
#include <list>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>

namespace AgcDriver {
namespace {

void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + ": Vulkan result " + std::to_string(result));
    }
}

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("Vulkan presentation: ") + reason);
}

// A compute dispatch's Vulkan objects, shared by every dispatch of the same compiled variant and
// kept by the recorder until the batches using them completed.
struct ComputePipelineObjects {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyShaderModule destroyModule = nullptr;
    PFN_vkDestroyPipelineLayout destroyLayout = nullptr;
    PFN_vkDestroyPipeline destroyPipeline = nullptr;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    ~ComputePipelineObjects() {
        if (pipeline != VK_NULL_HANDLE) destroyPipeline(device, pipeline, nullptr);
        if (layout != VK_NULL_HANDLE) destroyLayout(device, layout, nullptr);
        if (module != VK_NULL_HANDLE) destroyModule(device, module, nullptr);
    }
};

// The ShaderResources content cache dispatches share with recorded draws: Graphics::ResourceCache,
// one process-wide instance (see SharedResourceCache) that the State references so this file keeps
// its Find/Insert/Remove/Clear calls; the device clears it at teardown before its descriptor caches
// go. APS5_NO_RESOURCE_CACHE=1 builds every dispatch's resources as before.
using ResourceCache = Graphics::ResourceCache;

}

struct VulkanDevice::State {
    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    PFN_vkGetDeviceProcAddr deviceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    void* window = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    VkFence acquireFence = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;
    struct RetiredSwapchain {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkSemaphore> rendered;
    };
    std::vector<VkSemaphore> rendered;
    std::vector<RetiredSwapchain> retiredSwapchains;
    VkCommandBuffer clearCommands = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkBuffer uploadBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uploadMemory = VK_NULL_HANDLE;
    void* uploadMapping = nullptr;
    VkDeviceSize uploadSize = 0;
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    std::vector<std::uint32_t> capabilities{1};
    std::vector<std::string_view> spirvExtensions;
    bool tessellationShader = false;
    bool meshShader = false;
    bool fragmentShaderBarycentric = false;
    bool depthClipControl = false;
    bool depthClamp = false;
    VkDeviceSize hostImportAlignment = 0;
    bool depthRangeUnrestricted = false;
    bool samplerAnisotropy = false;
    bool textureCompressionBC = false;
    // VK_KHR_timeline_semaphore enabled: the recorder's unlocked waits are available.
    bool timelineSemaphores = false;
    std::unique_ptr<Graphics::TextureDetiler> detiler;
    std::unique_ptr<Graphics::Recorder> recorder;
    std::map<std::uint64_t, std::shared_ptr<ComputePipelineObjects>> computePipelines;
    std::unique_ptr<Graphics::GpuColorTransfer> colorTransfer;
    std::shared_ptr<Graphics::BufferPool> bufferPool;
    std::unique_ptr<Graphics::TextureCache> textureCache;
    std::unique_ptr<Graphics::PipelineCache> pipelineCache;
    std::unique_ptr<Graphics::DescriptorCache> descriptorCache;
    std::unique_ptr<Graphics::SamplerCache> samplerCache;
    Graphics::ResourceCache& resourceCache = Graphics::SharedResourceCache();
    // Recorded dispatches that write a copied buffer (their results reach guest memory by a CPU
    // write-back when the batch is reaped), listed until that write-back ran. An indirect dispatch
    // whose arguments one of them writes must read them on the CPU (DispatchIndirect); GPU-direct
    // writes into host imports need no entry. Under GuestMemory::GpuMutex; shared with the completion
    // actions so they never outlive it.
    std::shared_ptr<std::vector<std::shared_ptr<Graphics::ShaderResources>>> copiedWriters = std::make_shared<std::vector<std::shared_ptr<Graphics::ShaderResources>>>();
    VkPhysicalDeviceMeshShaderPropertiesEXT meshLimits{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    std::unique_ptr<PresentationScaler> scaler;
    // The swapchain image AcquireImage took for the next present(), consumed by that present.
    bool imageAcquired = false;
    std::uint32_t acquiredIndex = 0;
    // The presentation in flight between present() and QueuePresent(): its swapchain image, and the
    // resident image it blits from, alive until the render fence has been waited for.
    bool presentPending = false;
    std::uint32_t presentIndex = 0;
    std::shared_ptr<void> presentKept;
    // APS5_DUMP_FRAMES: the presented frame is read back into this buffer by the same submission and
    // written as a BMP by the writer thread after the render fence.
    std::unique_ptr<Graphics::Buffer> dumpBuffer;
    bool dumpRecorded = false;
    int dumpIndex = 0;
    std::thread dumpWriter;

    template<typename TFunction>
    TFunction InstanceFunction(const char* name) const {
        auto function = reinterpret_cast<TFunction>(instanceProc(instance, name));
        if (function == nullptr) {
            throw std::runtime_error(std::string("Vulkan instance function missing: ") + name);
        }
        return function;
    }

    template<typename TFunction>
    TFunction DeviceFunction(const char* name) const {
        auto function = reinterpret_cast<TFunction>(deviceProc(device, name));
        if (function == nullptr) {
            throw std::runtime_error(std::string("Vulkan device function missing: ") + name);
        }
        return function;
    }

    void Upload(std::span<const std::byte> pixels) {
        APS5_LOG_OUT("Upload pixels=%zu uploadSize=%llu buffer=%p memory=%p mapping=%p", pixels.size(), static_cast<unsigned long long>(uploadSize), reinterpret_cast<void*>(uploadBuffer), reinterpret_cast<void*>(uploadMemory), uploadMapping);
        if (uploadSize < pixels.size()) {
            APS5_LOG_OUT("Upload reallocating oldSize=%llu newSize=%zu", static_cast<unsigned long long>(uploadSize), pixels.size());
            if (uploadMapping) DeviceFunction<PFN_vkUnmapMemory>("vkUnmapMemory")(device, uploadMemory);
            uploadMapping = nullptr;
            if (uploadBuffer) DeviceFunction<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device, uploadBuffer, nullptr);
            uploadBuffer = VK_NULL_HANDLE;
            if (uploadMemory) DeviceFunction<PFN_vkFreeMemory>("vkFreeMemory")(device, uploadMemory, nullptr);
            uploadMemory = VK_NULL_HANDLE;
            uploadSize = 0;
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            buffer.size = pixels.size();
            buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            check(DeviceFunction<PFN_vkCreateBuffer>("vkCreateBuffer")(device, &buffer, nullptr, &uploadBuffer), "vkCreateBuffer display upload");
            VkMemoryRequirements requirements{};
            DeviceFunction<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(device, uploadBuffer, &requirements);
            std::uint32_t memoryType = memoryProperties.memoryTypeCount;
            const auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
                if ((requirements.memoryTypeBits & (1u << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) {
                    memoryType = i;
                    break;
                }
            }
            require(memoryType < memoryProperties.memoryTypeCount, "coherent host upload memory is unavailable");
            APS5_LOG_OUT("Upload requirements size=%llu alignment=%llu typeBits=0x%x memoryType=%u", static_cast<unsigned long long>(requirements.size), static_cast<unsigned long long>(requirements.alignment), requirements.memoryTypeBits, memoryType);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = memoryType;
            check(DeviceFunction<PFN_vkAllocateMemory>("vkAllocateMemory")(device, &allocation, nullptr, &uploadMemory), "vkAllocateMemory display upload");
            check(DeviceFunction<PFN_vkBindBufferMemory>("vkBindBufferMemory")(device, uploadBuffer, uploadMemory, 0), "vkBindBufferMemory display upload");
            check(DeviceFunction<PFN_vkMapMemory>("vkMapMemory")(device, uploadMemory, 0, pixels.size(), 0, &uploadMapping), "vkMapMemory display upload");
            uploadSize = pixels.size();
            APS5_LOG_OUT("Upload allocation complete buffer=%p memory=%p mapping=%p size=%llu", reinterpret_cast<void*>(uploadBuffer), reinterpret_cast<void*>(uploadMemory), uploadMapping, static_cast<unsigned long long>(uploadSize));
        }
        std::memcpy(uploadMapping, pixels.data(), pixels.size());
        APS5_LOG_OUT("Upload memcpy complete bytes=%zu", pixels.size());
    }

    void DestroyRetiredSwapchains() {
        if (retiredSwapchains.empty()) return;
        const auto destroySemaphore = DeviceFunction<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        const auto destroySwapchain = DeviceFunction<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR");
        for (const auto& retired : retiredSwapchains) {
            for (auto semaphore : retired.rendered) {
                if (semaphore) destroySemaphore(device, semaphore, nullptr);
            }
            destroySwapchain(device, retired.swapchain, nullptr);
        }
        retiredSwapchains.clear();
    }

    ~State() {
        if (dumpWriter.joinable()) dumpWriter.join();
        if (device != VK_NULL_HANDLE) {
            const auto idle = reinterpret_cast<PFN_vkDeviceWaitIdle>(deviceProc(device, "vkDeviceWaitIdle"))(device);
            if (idle != VK_SUCCESS && idle != VK_ERROR_DEVICE_LOST) std::terminate();
            // A resident image kept for an unfinished presentation goes before the caches it came from.
            presentKept.reset();
            dumpBuffer.reset();
            recorder.reset();
            // Cached graphics pipelines (with their framebuffers, modules, render passes and layouts)
            // belong to this device and must be destroyed while it lives.
            Graphics::ClearCachedPipelines(device);
            computePipelines.clear();
            // Every ShaderResources (kept by the recorder or the resource cache) is gone now, so the
            // sets and samplers they borrowed can go.
            resourceCache.Clear();
            descriptorCache.reset();
            samplerCache.reset();
            textureCache.reset();
            detiler.reset();
            colorTransfer.reset();
            scaler.reset();
            pipelineCache.reset();
            bufferPool.reset();
            const auto destroyFence = reinterpret_cast<PFN_vkDestroyFence>(deviceProc(device, "vkDestroyFence"));
            if (acquireFence) destroyFence(device, acquireFence, nullptr);
            if (renderFence) destroyFence(device, renderFence, nullptr);
            const auto destroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(deviceProc(device, "vkDestroySemaphore"));
            for (auto semaphore : rendered) {
                if (semaphore) destroySemaphore(device, semaphore, nullptr);
            }
            DestroyRetiredSwapchains();
            if (uploadMapping) reinterpret_cast<PFN_vkUnmapMemory>(deviceProc(device, "vkUnmapMemory"))(device, uploadMemory);
            if (uploadBuffer) reinterpret_cast<PFN_vkDestroyBuffer>(deviceProc(device, "vkDestroyBuffer"))(device, uploadBuffer, nullptr);
            if (uploadMemory) reinterpret_cast<PFN_vkFreeMemory>(deviceProc(device, "vkFreeMemory"))(device, uploadMemory, nullptr);
            if (swapchain) reinterpret_cast<PFN_vkDestroySwapchainKHR>(deviceProc(device, "vkDestroySwapchainKHR"))(device, swapchain, nullptr);
            const auto destroyPool = reinterpret_cast<PFN_vkDestroyCommandPool>(deviceProc(device, "vkDestroyCommandPool"));
            const auto destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(deviceProc(device, "vkDestroyDevice"));
            if (pool != VK_NULL_HANDLE) {
                destroyPool(device, pool, nullptr);
            }
            destroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            if (surface) reinterpret_cast<PFN_vkDestroySurfaceKHR>(instanceProc(instance, "vkDestroySurfaceKHR"))(instance, surface, nullptr);
            reinterpret_cast<PFN_vkDestroyInstance>(instanceProc(instance, "vkDestroyInstance"))(instance, nullptr);
        }
        if (library != nullptr) {
            SDL_UnloadObject(library);
        }
    }
};

VulkanDevice::VulkanDevice(const PresentationWindow* window) : state(std::make_unique<State>()) {
    APS5_LOG_OUT("VulkanDevice constructor window=%p", static_cast<const void*>(window));
#ifdef _WIN32
    state->library = SDL_LoadObject("vulkan-1.dll");
#else
    state->library = SDL_LoadObject("libvulkan.so.1");
#endif
    if (state->library == nullptr) {
        throw std::runtime_error(std::string("Vulkan loader: ") + SDL_GetError());
    }
    APS5_LOG_OUT("Vulkan loader loaded library=%p", state->library);
    state->instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(state->library, "vkGetInstanceProcAddr"));
    if (state->instanceProc == nullptr) {
        throw std::runtime_error("Vulkan loader: vkGetInstanceProcAddr missing");
    }
    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "AnyPS5 libSceAgcDriver";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &application;
    std::vector<const char*> instanceExtensions;
    if (window != nullptr) {
        APS5_LOG_OUT("Presentation window context=%p extent=%ux%u extensions=%zu", window->context, window->width, window->height, window->extensions.size());
        require(window->context && window->createSurface && window->getDrawableSize && window->width && window->height, "invalid window descriptor");
        instanceExtensions.assign(window->extensions.begin(), window->extensions.end());
        std::uint32_t availableCount = 0;
        const auto enumerateExtensions = state->InstanceFunction<PFN_vkEnumerateInstanceExtensionProperties>("vkEnumerateInstanceExtensionProperties");
        check(enumerateExtensions(nullptr, &availableCount, nullptr), "vkEnumerateInstanceExtensionProperties");
        std::vector<VkExtensionProperties> available(availableCount);
        check(enumerateExtensions(nullptr, &availableCount, available.data()), "vkEnumerateInstanceExtensionProperties");
        for (const auto* name : instanceExtensions) {
            require(name != nullptr, "null instance extension");
            if (std::none_of(available.begin(), available.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; })) {
                throw std::runtime_error(std::string("Vulkan presentation: required instance extension missing: ") + name);
            }
        }
        create.enabledExtensionCount = static_cast<std::uint32_t>(instanceExtensions.size());
        create.ppEnabledExtensionNames = instanceExtensions.data();
    }
    check(state->InstanceFunction<PFN_vkCreateInstance>("vkCreateInstance")(&create, nullptr, &state->instance), "vkCreateInstance");
    APS5_LOG_OUT("Vulkan instance created instance=%p", reinterpret_cast<void*>(state->instance));
    if (window != nullptr) {
        state->surface = window->createSurface(window->context, state->instance);
        require(state->surface != VK_NULL_HANDLE, "window returned a null surface");
        state->window = window->context;
        APS5_LOG_OUT("Vulkan surface created surface=%p window=%p", reinterpret_cast<void*>(state->surface), state->window);
    }
    state->deviceProc = state->InstanceFunction<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
    const auto enumerate = state->InstanceFunction<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
    std::uint32_t count = 0;
    check(enumerate(state->instance, &count, nullptr), "vkEnumeratePhysicalDevices");
    APS5_LOG_OUT("Physical device count=%u", count);
    std::vector<VkPhysicalDevice> devices(count);
    check(enumerate(state->instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
    devices.resize(count);
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    std::uint32_t family = 0;
    const std::array<const char*, 1> presentationExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (auto physical : devices) {
        VkPhysicalDeviceProperties properties{};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(physical, &properties);
        APS5_LOG_OUT("Physical device candidate=%p name=%s api=0x%x", reinterpret_cast<void*>(physical), properties.deviceName, properties.apiVersion);
        if (properties.apiVersion < VK_API_VERSION_1_1) {
            continue;
        }
        if (window != nullptr) {
            std::uint32_t extensionCount = 0;
            auto enumerateExtensions = state->InstanceFunction<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            check(enumerateExtensions(physical, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> extensions(extensionCount);
            check(enumerateExtensions(physical, nullptr, &extensionCount, extensions.data()), "vkEnumerateDeviceExtensionProperties");
            const bool supported = std::all_of(presentationExtensions.begin(), presentationExtensions.end(), [&](const char* name) {
                return std::any_of(extensions.begin(), extensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; });
            });
            if (!supported) continue;
        }
        std::uint32_t families = 0;
        auto getFamilies = state->InstanceFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        getFamilies(physical, &families, nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);
        getFamilies(physical, &families, queues.data());
        for (std::uint32_t i = 0; i < families; ++i) {
            if (queues[i].queueCount != 0 && (queues[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                if (window != nullptr) {
                    VkBool32 supported = VK_FALSE;
                    check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>("vkGetPhysicalDeviceSurfaceSupportKHR")(physical, i, state->surface, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");
                    if (!supported) continue;
                }
                selected = physical;
                family = i;
                break;
            }
        }
        if (selected != VK_NULL_HANDLE) {
            break;
        }
    }
    if (selected == VK_NULL_HANDLE) {
        throw std::runtime_error(window ? "Vulkan: no Vulkan 1.1 device with graphics, compute and swapchain presentation" : "Vulkan: no Vulkan 1.1 graphics and compute queue");
    }
    APS5_LOG_OUT("Physical device selected physical=%p queueFamily=%u", reinterpret_cast<void*>(selected), family);
    VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    properties.pNext = &state->subgroup;
    state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
    state->properties = properties.properties;
    state->physical = selected;
    if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) != 0) {
        state->capabilities.push_back(spv::CapabilityGroupNonUniform);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformBallot);
        if ((state->subgroup.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT) != 0) state->capabilities.push_back(spv::CapabilityGroupNonUniformShuffle);
    }
    APS5_LOG_OUT("Selected GPU name=%s vendor=0x%x device=0x%x subgroup=%u", state->properties.deviceName, state->properties.vendorID, state->properties.deviceID, state->subgroup.subgroupSize);
    state->InstanceFunction<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(selected, &state->memoryProperties);
    for (std::uint32_t i = 0; i < state->memoryProperties.memoryHeapCount; ++i) APS5_LOG_OUT("Memory heap %u size=%.1f MiB flags=0x%x", i, state->memoryProperties.memoryHeaps[i].size / 1048576.0, state->memoryProperties.memoryHeaps[i].flags);
    for (std::uint32_t i = 0; i < state->memoryProperties.memoryTypeCount; ++i) APS5_LOG_OUT("Memory type %u heap=%u flags=0x%x", i, state->memoryProperties.memoryTypes[i].heapIndex, state->memoryProperties.memoryTypes[i].propertyFlags);
    std::uint32_t extensionCount = 0;
    const auto enumerateDeviceExtensions = state->InstanceFunction<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
    check(enumerateDeviceExtensions(selected, nullptr, &extensionCount, nullptr), "vkEnumerateDeviceExtensionProperties");
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    check(enumerateDeviceExtensions(selected, nullptr, &extensionCount, availableExtensions.data()), "vkEnumerateDeviceExtensionProperties");
    const auto hasExtension = [&](const char* name) { return std::any_of(availableExtensions.begin(), availableExtensions.end(), [&](const auto& item) { return std::strcmp(item.extensionName, name) == 0; }); };
    auto byteFeatures = QueryBdaByteFeatures(selected, state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), availableExtensions);
    auto bdaFeatures = QueryBdaFeatures(selected, state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), availableExtensions);
    require(hasExtension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME), "VK_KHR_shader_float_controls is unavailable");
    VkPhysicalDeviceFloatControlsProperties floatControls{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};
    VkPhysicalDeviceProperties2 floatProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &floatControls};
    state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &floatProperties);
    require(floatControls.shaderSignedZeroInfNanPreserveFloat32 == VK_TRUE, "shaderSignedZeroInfNanPreserveFloat32 is unavailable");
    const std::array<const char*, 2> meshExtensions{VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME};
    const bool meshAvailable = std::all_of(meshExtensions.begin(), meshExtensions.end(), hasExtension);
    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    if (meshAvailable) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &meshFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->meshShader = meshFeatures.meshShader == VK_TRUE;
        VkPhysicalDeviceProperties2 meshProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &state->meshLimits};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &meshProperties);
    }
    meshFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    meshFeatures.meshShader = state->meshShader;
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR};
    if (hasExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &barycentricFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->fragmentShaderBarycentric = barycentricFeatures.fragmentShaderBarycentric == VK_TRUE;
    }
    std::vector<const char*> deviceExtensions;
    if (window != nullptr) deviceExtensions.assign(presentationExtensions.begin(), presentationExtensions.end());
    if (state->fragmentShaderBarycentric) {
        deviceExtensions.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
        state->capabilities.push_back(spv::CapabilityFragmentBarycentricKHR);
        state->spirvExtensions.push_back("SPV_KHR_fragment_shader_barycentric");
    }
    deviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    state->capabilities.push_back(spv::CapabilitySignedZeroInfNanPreserve);
    state->spirvExtensions.push_back("SPV_KHR_float_controls");
    deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
    state->capabilities.push_back(4448);
    state->spirvExtensions.push_back("SPV_KHR_8bit_storage");
    state->capabilities.push_back(11);
    state->capabilities.push_back(5347);
    state->spirvExtensions.push_back("SPV_KHR_physical_storage_buffer");
    state->depthRangeUnrestricted = hasExtension(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
    if (state->depthRangeUnrestricted) deviceExtensions.push_back(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME);
    // Guest dispatches cover whole thread groups past the edge of small images; robust image access
    // drops those writes instead of faulting the device.
    const bool imageRobustness = hasExtension(VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME);
    if (imageRobustness) deviceExtensions.push_back(VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME);
    // Guest memory is host memory: importing it lets address-based shaders use it in place instead of
    // copying every registered allocation per draw.
    if (hasExtension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) && std::getenv("APS5_NO_HOST_IMPORT") == nullptr) {
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &hostProperties};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(selected, &properties);
        state->hostImportAlignment = hostProperties.minImportedHostPointerAlignment;
#ifdef _WIN32
        // Imported pages are locked; locking is bounded by the process working-set minimum.
        if (const char* value = std::getenv("APS5_WORKING_SET_MIB")) {
            const SIZE_T bytes = static_cast<SIZE_T>(std::strtoull(value, nullptr, 10)) << 20u;
            if (!SetProcessWorkingSetSizeEx(GetCurrentProcess(), bytes, bytes * 2, QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE)) std::fprintf(stderr, "[gpu] SetProcessWorkingSetSizeEx failed: %lu\n", GetLastError());
        }
#endif
        deviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
    }
    VkPhysicalDeviceDepthClipControlFeaturesEXT depthClipFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT};
    if (hasExtension(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME)) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &depthClipFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->depthClipControl = depthClipFeatures.depthClipControl == VK_TRUE;
        if (state->depthClipControl) deviceExtensions.push_back(VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME);
    }
    if (state->meshShader) {
        deviceExtensions.insert(deviceExtensions.end(), meshExtensions.begin(), meshExtensions.end());
        state->capabilities.push_back(5283);
        state->spirvExtensions.push_back("SPV_EXT_mesh_shader");
    }
    APS5_LOG_OUT("Vulkan features tessellationAvailable=%u mesh=%u depthClip=%u depthRangeUnrestricted=%u", static_cast<unsigned>(state->tessellationShader), static_cast<unsigned>(state->meshShader), static_cast<unsigned>(state->depthClipControl), static_cast<unsigned>(state->depthRangeUnrestricted));
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    VkPhysicalDeviceFeatures available{};
    state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(selected, &available);
    require(available.vertexPipelineStoresAndAtomics && available.fragmentStoresAndAtomics, "graphics shader buffer writes and atomics are unavailable");
    VkPhysicalDeviceFeatures enabled{};
    enabled.shaderInt64 = VK_TRUE;
    enabled.vertexPipelineStoresAndAtomics = VK_TRUE;
    enabled.fragmentStoresAndAtomics = VK_TRUE;
    enabled.tessellationShader = available.tessellationShader;
    state->tessellationShader = enabled.tessellationShader == VK_TRUE;
    if (state->tessellationShader) state->capabilities.push_back(3);
    require(available.samplerAnisotropy && available.textureCompressionBC, "device lacks sampler anisotropy or BC texture compression support required for texture sampling");
    enabled.samplerAnisotropy = VK_TRUE;
    enabled.textureCompressionBC = VK_TRUE;
    // Guest shaders routinely read past descriptor ranges; robust access turns that into zeros
    // instead of a GPU fault that loses the device.
    enabled.robustBufferAccess = available.robustBufferAccess;
    // PA_CL_CLIP_CNTL near/far clip disable maps to depth clamping.
    enabled.depthClamp = available.depthClamp;
    state->depthClamp = enabled.depthClamp == VK_TRUE;
    // Recompiled storage-image access declares no format (the guest descriptor decides it).
    enabled.shaderStorageImageWriteWithoutFormat = available.shaderStorageImageWriteWithoutFormat;
    enabled.shaderStorageImageReadWithoutFormat = available.shaderStorageImageReadWithoutFormat;
    // Gathers with non-constant offsets (ImageGatherExtended).
    enabled.shaderImageGatherExtended = available.shaderImageGatherExtended;
    if (enabled.shaderImageGatherExtended) state->capabilities.push_back(spv::CapabilityImageGatherExtended);
    if (enabled.shaderStorageImageWriteWithoutFormat) state->capabilities.push_back(spv::CapabilityStorageImageWriteWithoutFormat);
    if (enabled.shaderStorageImageReadWithoutFormat) state->capabilities.push_back(spv::CapabilityStorageImageReadWithoutFormat);
    state->samplerAnisotropy = true;
    state->textureCompressionBC = true;
    deviceInfo.pEnabledFeatures = &enabled;
    deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (state->meshShader) {
        meshFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &meshFeatures;
    }
    if (state->depthClipControl) {
        depthClipFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
        deviceInfo.pNext = &depthClipFeatures;
    }
    byteFeatures.pNext = const_cast<void*>(deviceInfo.pNext);
    if (state->fragmentShaderBarycentric) {
        barycentricFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &barycentricFeatures;
    }
    VkPhysicalDeviceImageRobustnessFeaturesEXT imageRobustnessFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT, nullptr, VK_TRUE};
    if (imageRobustness) {
        imageRobustnessFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &imageRobustnessFeatures;
    }
    // Timeline semaphores let a queue worker wait for recorded batches without holding the GPU mutex
    // (see Recorder::WaitSerial). The instance is 1.1, so the KHR extension is used even on 1.2+
    // devices. Debug aid: APS5_NO_TIMELINE=1 leaves it off (drains wait under the mutex as before).
    VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timelineFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR};
    if (hasExtension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) && std::getenv("APS5_NO_TIMELINE") == nullptr) {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &timelineFeatures};
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(selected, &features);
        state->timelineSemaphores = timelineFeatures.timelineSemaphore == VK_TRUE;
    }
    timelineFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR};
    timelineFeatures.timelineSemaphore = VK_TRUE;
    if (state->timelineSemaphores) {
        deviceExtensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
        timelineFeatures.pNext = byteFeatures.pNext;
        byteFeatures.pNext = &timelineFeatures;
    } else {
        std::fprintf(stderr, "[gpu] timeline semaphores unavailable or disabled; drains wait under the GPU mutex\n");
    }
    bdaFeatures.pNext = &byteFeatures;
    deviceInfo.pNext = &bdaFeatures;
    check(state->InstanceFunction<PFN_vkCreateDevice>("vkCreateDevice")(selected, &deviceInfo, nullptr, &state->device), "vkCreateDevice");
    state->DeviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(state->device, family, 0, &state->queue);
    APS5_LOG_OUT("Vulkan device ready device=%p queue=%p family=%u", reinterpret_cast<void*>(state->device), reinterpret_cast<void*>(state->queue), family);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = family;
    check(state->DeviceFunction<PFN_vkCreateCommandPool>("vkCreateCommandPool")(state->device, &poolInfo, nullptr, &state->pool), "vkCreateCommandPool");
    state->bufferPool = std::make_shared<Graphics::BufferPool>(graphicsContext());
    state->pipelineCache = std::make_unique<Graphics::PipelineCache>(graphicsContext());
    state->detiler = std::make_unique<Graphics::TextureDetiler>(graphicsContext());
    state->textureCache = std::make_unique<Graphics::TextureCache>(graphicsContext());
    state->colorTransfer = std::make_unique<Graphics::GpuColorTransfer>(graphicsContext());
    state->descriptorCache = std::make_unique<Graphics::DescriptorCache>(graphicsContext());
    state->samplerCache = std::make_unique<Graphics::SamplerCache>();
    state->recorder = std::make_unique<Graphics::Recorder>(graphicsContext(), state->timelineSemaphores);
    state->recorder->Activate();
    if (window != nullptr) {
        require(window->getDrawableSize != nullptr, "missing window drawable size query");
        std::uint32_t drawableWidth = 0;
        std::uint32_t drawableHeight = 0;
        window->getDrawableSize(window->context, &drawableWidth, &drawableHeight);
        require(drawableWidth != 0 && drawableHeight != 0, "window has a zero drawable size at creation");
        VkSurfaceCapabilitiesKHR surface{};
        check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR")(selected, state->surface, &surface), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        state->extent = {drawableWidth, drawableHeight};
        APS5_LOG_OUT("Surface capabilities drawable=%ux%u min=%ux%u max=%ux%u minImages=%u maxImages=%u usage=0x%x", drawableWidth, drawableHeight, surface.minImageExtent.width, surface.minImageExtent.height, surface.maxImageExtent.width, surface.maxImageExtent.height, surface.minImageCount, surface.maxImageCount, surface.supportedUsageFlags);
        // A surface that reports its extent (a fullscreen transition can make it differ from the
        // drawable size for a moment) wins: the swapchain must match the surface.
        if (surface.currentExtent.width != std::numeric_limits<std::uint32_t>::max() && surface.currentExtent.width != 0 && surface.currentExtent.height != 0) {
            drawableWidth = surface.currentExtent.width;
            drawableHeight = surface.currentExtent.height;
            state->extent = {drawableWidth, drawableHeight};
        }
        require(drawableWidth >= surface.minImageExtent.width && drawableWidth <= surface.maxImageExtent.width && drawableHeight >= surface.minImageExtent.height && drawableHeight <= surface.maxImageExtent.height, "unsupported output extent");
        require((surface.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0, "surface does not support transfer destination images");
        require((surface.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0, "opaque composition is unavailable");
        require((surface.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0, "identity surface transform is unavailable");
        std::uint32_t formatCount = 0;
        auto getFormats = state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>("vkGetPhysicalDeviceSurfaceFormatsKHR");
        check(getFormats(selected, state->surface, &formatCount, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        check(getFormats(selected, state->surface, &formatCount, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
        require(std::any_of(formats.begin(), formats.end(), [](const auto& format) { return format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; }), "BGRA8 sRGB-nonlinear surface format is unavailable");
        VkSwapchainCreateInfoKHR swapchain{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapchain.surface = state->surface;
        swapchain.minImageCount = surface.minImageCount;
        swapchain.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swapchain.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchain.imageExtent = state->extent;
        swapchain.imageArrayLayers = 1;
        swapchain.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchain.clipped = VK_FALSE;
        check(state->DeviceFunction<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(state->device, &swapchain, nullptr, &state->swapchain), "vkCreateSwapchainKHR");
        std::uint32_t imageCount = 0;
        auto getImages = state->DeviceFunction<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
        check(getImages(state->device, state->swapchain, &imageCount, nullptr), "vkGetSwapchainImagesKHR");
        state->images.resize(imageCount);
        check(getImages(state->device, state->swapchain, &imageCount, state->images.data()), "vkGetSwapchainImagesKHR");
        APS5_LOG_OUT("Swapchain created swapchain=%p extent=%ux%u images=%u", reinterpret_cast<void*>(state->swapchain), state->extent.width, state->extent.height, imageCount);
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        for (auto* destination : {&state->acquireFence, &state->renderFence}) {
            check(state->DeviceFunction<PFN_vkCreateFence>("vkCreateFence")(state->device, &fence, nullptr, destination), "vkCreateFence");
        }
        state->images.resize(imageCount);
        state->rendered.resize(imageCount, VK_NULL_HANDLE);
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = state->pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        check(state->DeviceFunction<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(state->device, &allocation, &state->clearCommands), "vkAllocateCommandBuffers");
        state->scaler = std::make_unique<PresentationScaler>(graphicsContext(), VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
    }
}

VulkanDevice::~VulkanDevice() = default;

void VulkanDevice::WaitIdle() {
    APS5_LOG_CHARS_OUT_DEBUG("VulkanDevice::WaitIdle begin");
    if (state->recorder) {
        // Announced with this call's return address: the [recorder] site table then names the
        // driver site that drains (suspend point, label fallback, fill fallback, device replacement).
        if (!state->recorder->Idle()) Graphics::Recorder::CountSync(0, __builtin_return_address(0));
        state->recorder->Sync();
    }
    check(state->DeviceFunction<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(state->device), "vkDeviceWaitIdle");
    APS5_LOG_CHARS_OUT_DEBUG("VulkanDevice::WaitIdle complete");
}

namespace {

bool OpportunisticReap() {
    static const bool enabled = std::getenv("APS5_NO_OPPORTUNISTIC_REAP") == nullptr;
    return enabled;
}

}

void VulkanDevice::SubmitRecorded(bool reapFirst) {
    if (!state->recorder) return;
    // Finished batches are retired first (a non-blocking fence status check on the front of the
    // in-flight list), so PendingWriteOverlaps/HasCompletions scans stay short and a label recorded
    // after a completing batch already finished can go to the GPU instead of behind a completion.
    // Not for cross-queue submitters: a retired batch's write-back may sync a later batch under the
    // mutex (see the header), which a poller or compute worker must never do for queue 0.
    if (reapFirst && OpportunisticReap()) state->recorder->Reap();
    state->recorder->Submit();
}

bool VulkanDevice::CanWaitUnlocked() const {
    return state->recorder && state->recorder->HasTimeline();
}

std::uint64_t VulkanDevice::SubmitAndEpoch() {
    if (!state->recorder) return 0;
    if (OpportunisticReap()) state->recorder->Reap();
    return state->recorder->SubmitAndEpoch();
}

void VulkanDevice::WaitRecorded(std::uint64_t serial) {
    if (state->recorder) state->recorder->WaitSerial(serial);
}

void VulkanDevice::ReapRecorded(std::uint64_t serial) {
    if (!state->recorder) return;
    // The drain's site in the [recorder] table is this call's caller (Driver::execute's drain,
    // PrepareDispatch's presync), not this wrapper; FinishUpTo consumes the announcement.
    Graphics::Recorder::AnnounceSyncSite(__builtin_return_address(0));
    state->recorder->FinishUpTo(serial);
}

void VulkanDevice::ReapRecorded() {
    if (state->recorder) state->recorder->Reap();
}

std::optional<std::uint64_t> VulkanDevice::PendingLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, std::uint32_t& queue) const {
    if (!state->recorder) return std::nullopt;
    return state->recorder->PendingLabel(address, bytes, afterStamp, queue);
}

bool VulkanDevice::OpenWriteOverlaps(std::uint64_t address, std::size_t bytes) const {
    return state->recorder && state->recorder->OpenWriteOverlaps(address, bytes);
}

int VulkanDevice::WriteLabelOnGpu(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue, bool reapFirst) {
    if (!state->recorder || bytes.empty() || bytes.size() > 65536 || address % 4 != 0 || bytes.size() % 4 != 0) return 4;
    auto& recorder = *state->recorder;
    // Batches that already finished are retired before the checks: their completions ran, so they
    // neither keep the recorder busy nor force this label behind a completion. Only for the
    // graphics worker: a reap runs completions (copied write-backs, whose guest stores can re-enter
    // the flush hook and sync a later batch) under the mutex, which a compute worker must never do
    // against queue 0; its label goes behind a completion instead (reason 5, still no wait).
    // Debug aid: APS5_LABEL_REAP_ALL_QUEUES=1 reaps on every queue as before.
    static const bool reapAllQueues = std::getenv("APS5_LABEL_REAP_ALL_QUEUES") != nullptr;
    if (reapFirst && (queue == 0 || reapAllQueues) && OpportunisticReap()) recorder.Reap();
    if (recorder.Idle()) return 1;
    static const bool drain = std::getenv("APS5_DRAIN_COMPLETION_LABELS") != nullptr;
    const auto context = graphicsContext();
    const auto* import = Graphics::HostImportFor(context, address, bytes.size());
    if (import == nullptr) {
        // Memory the GPU has no view of: the store is a completion action of the batch (it runs after
        // the recorded work, in order, like a label behind write-backs) instead of a device drain.
        if (drain) return 3;
        recorder.AfterCompletions(address, bytes, stamp, queue, false);
        return 6;
    }
    // The store on the GPU, into the open batch: everything recorded so far completes before it,
    // and it is visible to the host after. Both barriers stay per store: the first orders
    // consecutive stores to one address (WAW), the second makes each store host-visible on its own,
    // before the rest of the batch completes.
    const auto recordStore = [&] {
        const auto commands = recorder.Commands();
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        state->DeviceFunction<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(commands, import->buffer, address - import->base, bytes.size(), bytes.data());
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    };
    if (recorder.HasCompletions()) {
        // A copied buffer's write-back is still to run: the store must land after it. It is stored
        // by a completion action of the batch (a CPU memcpy when the batch is reaped) instead of
        // draining the device here. The same bytes are also recorded on the GPU into the open batch
        // first, so GPU work recorded after this point that reads the range through the host import
        // (a shader binding constants a DUMP_CONST_RAM dumped, a dispatch consuming a COPY_DATA or
        // DMA_DATA fill) sees them in order; the completion memcpy stores identical bytes again
        // after any write-back that overwrote them, so either order of the two stores is correct.
        // CPU readers are covered by the pending-write note (the flush hook syncs) and waits by the
        // label table. Debug aid: APS5_DRAIN_COMPLETION_LABELS=1 drains as before; try it first when
        // a title misbehaves with this path.
        if (drain) return 2;
        recordStore();
        GuestMemory::MarkWritten(address, bytes.size());
        // Commands() opened the batch the store went into, so the completion is appended to that
        // batch and runs at its reap, after the write-backs of every batch before it.
        recorder.AfterCompletions(address, bytes, stamp, queue, true);
        return 5;
    }
    recordStore();
    // The table entry goes in before the pending-write note: the note bumps the write generation a
    // poller watches, and a poller that sees the bump then finds the label without the GPU mutex.
    recorder.NoteLabel(address, bytes, stamp, queue);
    recorder.NotePendingWrite(address, bytes.size());
    GuestMemory::MarkWritten(address, bytes.size());
    // No submit per label: the batch goes out at the queue worker's next non-label packet, at a
    // wait on its range, after APS5_LABEL_FLUSH_US, or at the submission's end (Driver.cpp).
    // Debug aid: APS5_LABEL_SUBMIT_NOW=1 submits every label at once, as before.
    static const bool submitNow = std::getenv("APS5_LABEL_SUBMIT_NOW") != nullptr;
    if (submitNow) recorder.Submit();
    return 0;
}

bool VulkanDevice::FillBuffer(std::uint64_t address, std::size_t bytes, std::span<const std::uint32_t, 4> pattern) {
    if (!state->recorder || bytes == 0 || bytes % 16 != 0 || address % 16 != 0) return false;
    auto& recorder = *state->recorder;
    // Finished batches are retired first (one fence status check, as WriteLabelOnGpu does): their
    // completion labels landed and their copied writers are delisted, so a batch the idle GPU already
    // finished does not force a wait below. Before the import lookup: a completion may refresh the
    // import table. Debug aid: APS5_NO_OPPORTUNISTIC_REAP=1 skips it.
    if (OpportunisticReap()) recorder.Reap();
    const auto context = graphicsContext();
    const Graphics::HostImport* import = Graphics::HostImportFor(context, address, bytes);
    if (import == nullptr) return false;
    // Earlier recorded work that stores into the range must land before the fill, or it would
    // overwrite the fill later. Stores the GPU makes (direct dispatch writes, GPU labels, earlier
    // fills, GPU-direct write-backs) are ordered by the fill's own barrier below (ALL_COMMANDS ->
    // TRANSFER), so only the stores the CPU makes when a batch is reaped need a wait: a recorded
    // dispatch whose copied buffer overlaps the range (state->copiedWriters, listed until its
    // write-back ran; per object over all its written V# ranges, as DispatchIndirect tests it) and a
    // completion-deferred label (Recorder::AfterCompletions) over the range. The label table has no
    // range query and a fill spans MiBs, so a small range is looked up per dword (a GPU label there
    // counts too: a harmless extra wait) and a larger one waits whenever any completion label is
    // pending at all. Batches recorded after the last such store keep running (SyncThrough).
    // Debug aid: APS5_FILL_SYNC=1 waits for every recorded store over the range, as before.
    static const bool alwaysSync = std::getenv("APS5_FILL_SYNC") != nullptr;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // APS5_PROFILE_DRAW: the fills' outcomes, cumulative, every 10 s (under the GpuMutex like the
    // rest) on a [fill-sync] line; Driver.cpp's [fill] line counts the fills and their bytes.
    // `labelSynced` counts syncs taken because a completion label was pending: over the range for
    // a small fill, anywhere at all for a larger one.
    static std::uint64_t fills = 0, synced = 0, labelSynced = 0, ordered = 0;
    static double syncedMs = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    ++fills;
    if (recorder.PendingWriteOverlaps(address, bytes)) {
        // Recorded draws with copied writes keep their own registry (Graphics::DrawCopiedWriters):
        // their CPU write-back would land after a fill recorded on the GPU, so they count too.
        bool sync = alwaysSync || std::any_of(state->copiedWriters->begin(), state->copiedWriters->end(), [&](const auto& writer) { return writer->WritesOverlap(address, bytes); })
            || std::any_of(Graphics::DrawCopiedWriters()->begin(), Graphics::DrawCopiedWriters()->end(), [&](const auto& writer) { return writer->WritesOverlap(address, bytes); });
        if (!sync && Graphics::Recorder::PendingCompletionLabels() != 0) {
            // A label the CPU will store after a completion must not be overwritten by the fill:
            // only a label inside the filled range matters, whatever the range's size.
            sync = recorder.PendingLabelIn(address, bytes);
            if (sync) ++labelSynced;
        }
        if (sync) {
            ++synced;
            const auto syncStart = std::chrono::steady_clock::now();
            Graphics::Recorder::CountSync(4);
            recorder.SyncThrough(address, bytes);
            syncedMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - syncStart).count();
            // The sync ran completions, whose write-backs can refresh the import table and retire
            // the import looked up above; the caller stores the fill itself when it is gone.
            import = Graphics::HostImportFor(context, address, bytes);
            if (import == nullptr) return false;
        } else {
            ++ordered;
        }
    }
    if (profile) {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport > std::chrono::seconds(10)) {
            lastReport = now;
            std::fprintf(stderr, "[fill-sync] fills: %llu synced (%llu with a completion label pending) waited %.0f ms, %llu ordered by barrier, %llu with no pending write\n", static_cast<unsigned long long>(synced), static_cast<unsigned long long>(labelSynced), syncedMs, static_cast<unsigned long long>(ordered), static_cast<unsigned long long>(fills - synced - ordered));
        }
    }
    const auto commands = recorder.Commands();
    // Every earlier recorded read or write of the range precedes the fill (WAR by the execution
    // dependency, WAW by the writes made available), whichever stage made it.
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    const auto offset = address - import->base;
    if (pattern[0] == pattern[1] && pattern[1] == pattern[2] && pattern[2] == pattern[3]) {
        state->DeviceFunction<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, import->buffer, offset, bytes, pattern[0]);
    } else {
        // A 16-byte pattern is seeded once and doubled in place until the range is covered.
        auto seed = std::make_shared<Graphics::Buffer>(context, 16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(seed->Bytes().data(), pattern.data(), 16);
        recorder.Keep(seed);
        const VkBufferCopy first{0, offset, 16};
        state->DeviceFunction<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, seed->Handle(), import->buffer, 1, &first);
        for (std::size_t done = 16; done < bytes;) {
            const auto chunk = std::min(done, bytes - done);
            Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            const VkBufferCopy copy{offset, offset + done, chunk};
            state->DeviceFunction<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, import->buffer, import->buffer, 1, &copy);
            done += chunk;
        }
    }
    // The filled bytes are visible to everything recorded after: shaders, transfers, the host, and
    // an indirect dispatch reading its group counts in place (DispatchIndirect adds its own
    // ALL_COMMANDS -> DRAW_INDIRECT barrier as well). A later GPU label store over the range is
    // ordered behind the fill by its ALL_COMMANDS -> TRANSFER barrier (WriteLabelOnGpu).
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT);
    recorder.NotePendingWrite(address, bytes);
    GuestMemory::MarkWritten(address, bytes);
    return true;
}

void* VulkanDevice::Window() const {
    return state->window;
}

void VulkanDevice::Resize(std::uint32_t width, std::uint32_t height) {
    APS5_LOG_OUT_DEBUG("Resize requested=%ux%u current=%ux%u", width, height, state->extent.width, state->extent.height);
    require(state->swapchain != VK_NULL_HANDLE, "cannot resize an unavailable swapchain");
    if (width == 0 || height == 0) {
        state->extent = {0, 0};
        return;
    }
    if (state->extent.width == width && state->extent.height == height) return;
    WaitIdle();
    VkSurfaceCapabilitiesKHR surface{};
    check(state->InstanceFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR")(state->physical, state->surface, &surface), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR resize");
    // The window's drawable size and the surface's extent can disagree for a moment while the window
    // is being resized; the swapchain must match the surface, and the next present catches up.
    if (surface.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        width = surface.currentExtent.width;
        height = surface.currentExtent.height;
        if (width == 0 || height == 0) {
            state->extent = {0, 0};
            return;
        }
        if (state->extent.width == width && state->extent.height == height) return;
    }
    require(width >= surface.minImageExtent.width && width <= surface.maxImageExtent.width && height >= surface.minImageExtent.height && height <= surface.maxImageExtent.height, "unsupported resized output extent");
    require((surface.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0 && (surface.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0 && (surface.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0, "resized surface capabilities are unsupported");
    VkSwapchainCreateInfoKHR create{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create.surface = state->surface;
    create.minImageCount = surface.minImageCount;
    create.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    create.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create.imageExtent = {width, height};
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create.oldSwapchain = state->swapchain;
    state->retiredSwapchains.reserve(state->retiredSwapchains.size() + 1);
    VkSwapchainKHR replacement = VK_NULL_HANDLE;
    check(state->DeviceFunction<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(state->device, &create, nullptr, &replacement), "vkCreateSwapchainKHR resize");
    state->retiredSwapchains.push_back({state->swapchain, std::move(state->rendered)});
    state->swapchain = replacement;
    state->extent = create.imageExtent;
    auto getImages = state->DeviceFunction<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
    std::uint32_t count = 0;
    check(getImages(state->device, replacement, &count, nullptr), "vkGetSwapchainImagesKHR resize");
    state->images.resize(count);
    check(getImages(state->device, replacement, &count, state->images.data()), "vkGetSwapchainImagesKHR resize");
    state->images.resize(count);
    state->rendered.assign(count, VK_NULL_HANDLE);
    APS5_LOG_OUT_DEBUG("Resize complete swapchain=%p extent=%ux%u images=%u", reinterpret_cast<void*>(state->swapchain), state->extent.width, state->extent.height, count);
}

bool VulkanDevice::Presentable() const {
    return state->extent.width != 0 && state->extent.height != 0;
}

bool VulkanDevice::PresentClear(std::uint32_t width, std::uint32_t height, bool opaque) {
    APS5_LOG_OUT_DEBUG("PresentClear width=%u height=%u opaque=%u", width, height, static_cast<unsigned>(opaque));
    return present(width, height, opaque, {});
}

void VulkanDevice::PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels) {
    APS5_LOG_OUT("PresentPixels width=%u height=%u pixels=%zu expected=%llu", width, height, pixels.size(), static_cast<unsigned long long>(width) * height * 4u);
    require(width != 0 && height != 0 && width <= 16384 && height <= 16384, "invalid display image extent");
    require(pixels.size() == static_cast<std::uint64_t>(width) * height * 4, "invalid display pixel buffer size");
    // Debug aid: APS5_DUMP_FRAMES=<n> saves the first n presented frames as frame_<index>.bmp.
    static const int dumpLimit = [] {
        const char* value = std::getenv("APS5_DUMP_FRAMES");
        return value ? std::atoi(value) : 0;
    }();
    static int dumped = 0;
    if (dumped < dumpLimit) {
        char name[32];
        std::snprintf(name, sizeof(name), "frame_%03d.bmp", dumped++);
        if (std::FILE* file = std::fopen(name, "wb")) {
            const std::uint32_t imageBytes = width * height * 4;
            const std::uint32_t header[13] = {0, 0, 54, 40, width, static_cast<std::uint32_t>(-static_cast<std::int32_t>(height)), 1u | (32u << 16u), 0, imageBytes, 2835, 2835, 0, 0};
            const std::uint16_t magic = 0x4d42;
            const std::uint32_t fileBytes = 54 + imageBytes;
            std::fwrite(&magic, 2, 1, file);
            std::fwrite(&fileBytes, 4, 1, file);
            std::fwrite(header + 1, 4, 12, file);
            std::fwrite(pixels.data(), 1, pixels.size(), file);
            std::fclose(file);
        }
    }
    // Tests and tools present pixels synchronously; the game path splits the steps (see Driver::Present).
    if (!present(width, height, true, pixels)) return;
    FinishPresent();
    QueuePresent();
}

namespace {

// APS5_DUMP_SCALE=<n> (default 4) keeps every n-th pixel in each direction, so long runs of 4K
// frames stay small.
std::uint32_t DumpScale() {
    static const std::uint32_t scale = [] {
        const char* value = std::getenv("APS5_DUMP_SCALE");
        const int parsed = value ? std::atoi(value) : 4;
        return static_cast<std::uint32_t>(parsed > 0 ? parsed : 1);
    }();
    return scale;
}

// frame_<index>.bmp: every DumpScale()-th pixel of a BGRA8 image, 32-bit top-down.
void WriteFrameBmp(int index, std::uint32_t fullWidth, std::uint32_t fullHeight, std::span<const std::byte> full) {
    const auto scale = DumpScale();
    const std::uint32_t width = (fullWidth + scale - 1) / scale;
    const std::uint32_t height = (fullHeight + scale - 1) / scale;
    std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) std::memcpy(pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4, full.data() + (static_cast<std::size_t>(y) * scale * fullWidth + x * scale) * 4, 4);
    }
    char name[32];
    std::snprintf(name, sizeof(name), "frame_%03d.bmp", index);
    if (std::FILE* file = std::fopen(name, "wb")) {
        const std::uint32_t imageBytes = width * height * 4;
        const std::uint32_t header[13] = {0, 0, 54, 40, width, static_cast<std::uint32_t>(-static_cast<std::int32_t>(height)), 1u | (32u << 16u), 0, imageBytes, 2835, 2835, 0, 0};
        const std::uint16_t magic = 0x4d42;
        const std::uint32_t fileBytes = 54 + imageBytes;
        std::fwrite(&magic, 2, 1, file);
        std::fwrite(&fileBytes, 4, 1, file);
        std::fwrite(header + 1, 4, 12, file);
        std::fwrite(pixels.data(), 1, pixels.size(), file);
        std::fclose(file);
    }
}

// Whether a resident image of the display buffer can be blitted to the swapchain as it is: one 2D
// level of the display's extent in a normalized 4-byte color format (a blit converts components,
// so an integer or float reinterpretation would present wrong values). `filter` gets the best
// filter the format allows.
bool ResidentPresentable(const Graphics::Context& context, const Graphics::StorageTexture& image, const DisplayBuffer& buffer, VkFilter& filter) {
    const auto& descriptor = image.Descriptor();
    if (descriptor.width != buffer.width || descriptor.height != buffer.height || descriptor.mipCount != 1 || image.ImageLayers() != 1 || image.ImageDepth() != 1) return false;
    if (image.GuestBytes() != DisplayBufferSize(buffer)) return false;
    const auto format = Graphics::StorageFormatForGuest(context, descriptor.format);
    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            break;
        default: return false;
    }
    VkFormatProperties properties{};
    context.formatProperties(context.physical, format, &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) return false;
    filter = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    return true;
}

}

bool VulkanDevice::PresentDisplayBuffer(const DisplayBuffer& buffer) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // The display buffer is usually a resident render target whose results are still on the GPU: it
    // is blitted from that image, which skips the write-back retile, the 33 MB read of guest memory,
    // the upload and the detile dispatch. The deferred write-back stays pending as for any storage
    // image. Debug aid: APS5_NO_RESIDENT_PRESENT=1 always goes through guest memory.
    static const bool noResidentPresent = std::getenv("APS5_NO_RESIDENT_PRESENT") != nullptr;
    static std::uint64_t residentPresents = 0, refreshedPresents = 0, notPending = 0, unsuitable = 0, gpuDumps = 0;
    static auto lastReport = std::chrono::steady_clock::now();
    const auto bytes = DisplayBufferSize(buffer);
    std::shared_ptr<Graphics::StorageTexture> resident;
    VkFilter filter = VK_FILTER_LINEAR;
    if (!noResidentPresent) {
        resident = Graphics::StorageTexture::FindPending(buffer.address, bytes);
        if (resident == nullptr) {
            ++notPending;
        } else if (!ResidentPresentable(graphicsContext(), *resident, buffer, filter)) {
            resident.reset();
            ++unsuitable;
        } else {
            // 64 KiB blocks the CPU wrote since the image last matched guest memory would be shown
            // stale (the write-back keeps the CPU's bytes for them): Refresh merges them into the
            // image first, as a sampled texture of the image would (see cachedTexture). The uncached
            // walk: the collect memo is per worker packet, so on this thread a memoized answer could
            // miss a CPU write that landed after a worker's walk of the same range.
            GuestMemory::CollectWritesUncached(buffer.address, bytes);
            if (!GuestMemory::UnchangedSince(buffer.address, bytes, resident->Generation())) {
                resident->Refresh();
                ++refreshedPresents;
            }
            ++residentPresents;
        }
    }
    // Debug aid: APS5_DUMP_FRAMES=<n> also covers the GPU display path: the presented frame is read
    // back by the same submission and written after its fence (see writeFrameDump).
    // APS5_NO_GPU_DUMP=1 decodes the tiled guest buffer on the CPU instead, as before.
    static const int dumpLimit = [] {
        const char* value = std::getenv("APS5_DUMP_FRAMES");
        return value ? std::atoi(value) : 0;
    }();
    static const bool cpuDump = std::getenv("APS5_NO_GPU_DUMP") != nullptr;
    static int dumped = 0;
    bool dumpFrame = false;
    if (dumped < dumpLimit) {
        if (cpuDump) {
            const auto full = ReadDisplayBuffer(buffer);
            WriteFrameBmp(dumped++, buffer.width, buffer.height, full);
        } else {
            state->dumpIndex = dumped++;
            dumpFrame = true;
            ++gpuDumps;
        }
    }
    if (profile && std::chrono::steady_clock::now() - lastReport > std::chrono::seconds(10)) {
        lastReport = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[flip] %llu presents from the resident image (%llu refreshed first), through guest memory: %llu not pending, %llu unsuitable; %llu GPU frame dumps\n", static_cast<unsigned long long>(residentPresents), static_cast<unsigned long long>(refreshedPresents), static_cast<unsigned long long>(notPending), static_cast<unsigned long long>(unsuitable), static_cast<unsigned long long>(gpuDumps));
    }
    if (!present(buffer.width, buffer.height, true, {}, &buffer, resident, filter, dumpFrame)) {
        // A dropped frame (swapchain out of date) keeps the dump numbering contiguous.
        if (dumpFrame) --dumped;
        return false;
    }
    return true;
}

bool VulkanDevice::AcquireImage() {
    PerformanceTimer timing("Vulkan.Acquire");
    require(state->swapchain != VK_NULL_HANDLE, "device has no swapchain");
    require(state->extent.width != 0 && state->extent.height != 0, "output window is minimized");
    require(!state->presentPending, "the previous presentation was not finished");
    require(!state->imageAcquired, "the previously acquired image was not presented");
    auto wait = state->DeviceFunction<PFN_vkWaitForFences>("vkWaitForFences");
    auto reset = state->DeviceFunction<PFN_vkResetFences>("vkResetFences");
    const std::array<VkFence, 2> fences{state->acquireFence, state->renderFence};
    check(reset(state->device, static_cast<std::uint32_t>(fences.size()), fences.data()), "vkResetFences");
    APS5_LOG_CHARS_OUT_DEBUG("present fences reset");
    std::uint32_t index = 0;
    timing.Mark("fence_reset");
    // An out-of-date swapchain (the window changed) drops this frame; the next Resize recreates it.
    const auto acquired = state->DeviceFunction<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR")(state->device, state->swapchain, 5'000'000'000ULL, VK_NULL_HANDLE, state->acquireFence, &index);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        state->extent = {0, 0};
        return false;
    }
    if (acquired != VK_SUBOPTIMAL_KHR) check(acquired, "vkAcquireNextImageKHR");
    timing.Mark("acquire_image");
    APS5_LOG_OUT_DEBUG("vkAcquireNextImageKHR index=%u imageCount=%zu", index, state->images.size());
    check(wait(state->device, 1, &state->acquireFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences acquire");
    timing.Mark("acquire_fence_wait");
    APS5_LOG_OUT_DEBUG("Acquire fence complete index=%u", index);
    require(index < state->images.size() && index < state->rendered.size(), "acquired image index is out of range");
    auto& rendered = state->rendered[index];
    if (rendered != VK_NULL_HANDLE) {
        state->DestroyRetiredSwapchains();
    } else {
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(state->DeviceFunction<PFN_vkCreateSemaphore>("vkCreateSemaphore")(state->device, &semaphore, nullptr, &rendered), "vkCreateSemaphore presentation");
    }
    timing.Mark("retired_swapchains");
    state->acquiredIndex = index;
    state->imageAcquired = true;
    return true;
}

bool VulkanDevice::present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display, const std::shared_ptr<Graphics::StorageTexture>& resident, VkFilter residentFilter, bool dumpFrame) {
    PerformanceTimer timing("Vulkan.Present");
    APS5_LOG_OUT_DEBUG("present begin width=%u height=%u opaque=%u pixels=%zu", width, height, static_cast<unsigned>(opaque), pixels.size());
    require(state->swapchain != VK_NULL_HANDLE, "device has no swapchain");
    require(state->extent.width != 0 && state->extent.height != 0, "output window is minimized");
    require(!state->presentPending, "the previous presentation was not finished");
    // The game path acquired the image before taking GpuMutex (Driver::Present); tests, tools and
    // APS5_SYNC_FLIP=1 acquire here.
    if (!state->imageAcquired && !AcquireImage()) return false;
    state->imageAcquired = false;
    const auto index = state->acquiredIndex;
    require(index < state->images.size() && index < state->rendered.size(), "acquired image index is out of range");
    auto& rendered = state->rendered[index];
    if (display != nullptr && resident == nullptr) {
        static_cast<void>(DisplayBufferSize(*display));
        state->colorTransfer->Upload(display->address, width, height, Graphics::ColorTileMode::RenderTarget);
    }
    if (!pixels.empty()) state->Upload(pixels);
    // The frame's recorded work (and a refresh of the resident image) must reach the queue before the
    // presentation's own submission, which reads the image in queue order.
    SubmitRecorded();
    timing.Mark("pixel_upload");
    APS5_LOG_OUT_DEBUG("present source=%s bytes=%zu", pixels.empty() ? "clear" : "pixels", pixels.size());
    auto commands = state->clearCommands;
    check(state->DeviceFunction<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(commands, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(state->DeviceFunction<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin), "vkBeginCommandBuffer");
    APS5_LOG_OUT_DEBUG("Presentation command buffer begin commands=%p image=%p", reinterpret_cast<void*>(commands), reinterpret_cast<void*>(state->images[index]));
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = state->images[index];
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto pipelineBarrier = state->DeviceFunction<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    if (pixels.empty() && display == nullptr) {
        APS5_LOG_OUT_DEBUG("Recording swapchain clear opaque=%u image=%p", static_cast<unsigned>(opaque), reinterpret_cast<void*>(barrier.image));
        VkClearColorValue clear{};
        clear.float32[3] = opaque ? 1.0f : 0.0f;
        state->DeviceFunction<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &barrier.subresourceRange);
    } else {
        APS5_LOG_OUT_DEBUG("Recording swapchain scaled blit width=%u height=%u bytes=%zu buffer=%p image=%p", width, height, pixels.size(), reinterpret_cast<void*>(state->uploadBuffer), reinterpret_cast<void*>(barrier.image));
        require(state->scaler != nullptr, "presentation scaler is unavailable");
        state->scaler->EnsureSourceImage(width, height);
        // A resident image is blitted straight to the swapchain; a frame dump goes through the
        // scaler's BGRA8 image so the readback has one format.
        const bool direct = resident != nullptr && !dumpFrame;
        VkImageMemoryBarrier residentBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        if (resident != nullptr) {
            // The draws and dispatches that produced the image were submitted before this command
            // buffer; their writes become visible to the blit here. The layout stays GENERAL, the
            // one the storage image is tracked in.
            residentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            residentBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            residentBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            residentBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            residentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            residentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            residentBarrier.image = resident->Image();
            residentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
            pipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &residentBarrier);
            if (!direct) state->scaler->RecordBlitInto(commands, resident->Image(), VK_IMAGE_LAYOUT_GENERAL, residentFilter);
        } else {
            if (display != nullptr) {
                // Bit 56 selects the 10-bit A2B10G10R10 variant; R8G8B8A8-ordered formats need red and blue
                // swapped for the B8G8R8A8 swapchain.
                constexpr std::uint64_t tenBitFormat = 0x0100000000000000ull;
                const bool tenBit = (display->pixelFormat & tenBitFormat) != 0;
                state->colorTransfer->Detile(commands, (display->pixelFormat & ~tenBitFormat) == 0x8000000022000000ull, tenBit);
            }
            state->scaler->RecordUpload(commands, display != nullptr ? state->colorTransfer->LinearBuffer() : state->uploadBuffer);
        }
        VkClearColorValue letterbox{};
        letterbox.float32[3] = 1.0f;
        state->DeviceFunction<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &letterbox, 1, &barrier.subresourceRange);
        VkImageMemoryBarrier letterboxBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        letterboxBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        letterboxBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        letterboxBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        letterboxBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        letterboxBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        letterboxBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        letterboxBarrier.image = barrier.image;
        letterboxBarrier.subresourceRange = barrier.subresourceRange;
        pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &letterboxBarrier);
        if (direct) PresentationScaler::RecordBlitFrom(graphicsContext(), commands, resident->Image(), VK_IMAGE_LAYOUT_GENERAL, width, height, residentFilter, barrier.image, state->extent.width, state->extent.height);
        else state->scaler->RecordBlit(commands, barrier.image, state->extent.width, state->extent.height);
        if (resident != nullptr) {
            // Later batches write the image again (the next frame into this buffer, a refresh): they
            // start after the blit has read it.
            residentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            residentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &residentBarrier);
        }
        if (dumpFrame) {
            const auto dumpBytes = static_cast<std::size_t>(state->scaler->SourceWidth()) * state->scaler->SourceHeight() * 4;
            if (!state->dumpBuffer || state->dumpBuffer->Bytes().size() != dumpBytes) state->dumpBuffer = std::make_unique<Graphics::Buffer>(graphicsContext(), dumpBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            state->scaler->RecordReadback(commands, state->dumpBuffer->Handle());
        }
    }
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    pipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    check(state->DeviceFunction<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer");
    APS5_LOG_CHARS_OUT_DEBUG("Presentation command buffer recorded");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &rendered;
    timing.Mark("command_record_scale");
    check(state->DeviceFunction<PFN_vkQueueSubmit>("vkQueueSubmit")(state->queue, 1, &submit, state->renderFence), "vkQueueSubmit clear");
    timing.Mark("queue_submit");
    APS5_LOG_CHARS_OUT_DEBUG("Presentation vkQueueSubmit OK");
    // The image stays alive until FinishPresent has waited for the blit (storage-cache eviction only
    // drops the cache's reference).
    state->presentKept = resident;
    state->presentIndex = index;
    state->presentPending = true;
    // Only a submitted readback is written after the fence (a failed submit tears the device down).
    state->dumpRecorded = dumpFrame;
    return true;
}

void VulkanDevice::FinishPresent() {
    if (!state->presentPending) return;
    PerformanceTimer timing("Vulkan.PresentWait");
    const auto wait = state->DeviceFunction<PFN_vkWaitForFences>("vkWaitForFences");
    check(wait(state->device, 1, &state->renderFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences clear");
    timing.Mark("render_fence_wait");
    APS5_LOG_CHARS_OUT_DEBUG("Presentation render fence complete");
    if (state->dumpRecorded) {
        state->dumpRecorded = false;
        writeFrameDump();
        timing.Mark("frame_dump");
    }
}

void VulkanDevice::writeFrameDump() {
    require(state->dumpBuffer != nullptr && state->scaler != nullptr, "frame dump was not recorded");
    // The readback buffer is reused by the next dump, so its bytes are copied out before the writer
    // thread (one at a time) downscales and writes them.
    std::vector<std::byte> full(state->dumpBuffer->Bytes().begin(), state->dumpBuffer->Bytes().end());
    if (state->dumpWriter.joinable()) state->dumpWriter.join();
    const auto index = state->dumpIndex;
    const auto width = state->scaler->SourceWidth();
    const auto height = state->scaler->SourceHeight();
    static const auto start = std::chrono::steady_clock::now();
    std::fprintf(stderr, "[gpu] frame_%03d.bmp: %ux%u display read back on the GPU at %.1f s\n", index, width, height, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    state->dumpWriter = std::thread([index, width, height, pixels = std::move(full)] { WriteFrameBmp(index, width, height, pixels); });
}

void VulkanDevice::QueuePresent() {
    if (!state->presentPending) return;
    PerformanceTimer timing("Vulkan.QueuePresent");
    state->presentPending = false;
    // The finished submission was the last reader of the resident image.
    state->presentKept.reset();
    const auto index = state->presentIndex;
    require(index < state->rendered.size(), "presented image index is out of range");
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &state->rendered[index];
    present.swapchainCount = 1;
    present.pSwapchains = &state->swapchain;
    present.pImageIndices = &index;
    const auto presented = state->DeviceFunction<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")(state->queue, &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) state->extent = {0, 0};
    else check(presented, "vkQueuePresentKHR");
    timing.Mark("queue_present");
    APS5_LOG_OUT_DEBUG("vkQueuePresentKHR queued imageIndex=%u", index);
}

ShaderRecompiler::SpirvTarget VulkanDevice::Target() const {
    const auto& limits = state->properties.limits;
    ShaderRecompiler::SpirvTarget target{VK_API_VERSION_1_1, state->meshShader ? 0x00010400u : 0x00010300u, state->subgroup.subgroupSize, ShaderRecompiler::BdaAbi::Version, state->capabilities, state->spirvExtensions, false, {limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupSize[1], limits.maxComputeWorkGroupSize[2]}, limits.maxComputeWorkGroupInvocations, limits.maxComputeSharedMemorySize, {}, {}};
    if (state->meshShader) {
        const auto& mesh = state->meshLimits;
        target.mesh = ShaderRecompiler::MeshTargetLimits{{mesh.maxMeshWorkGroupSize[0], mesh.maxMeshWorkGroupSize[1], mesh.maxMeshWorkGroupSize[2]}, mesh.maxMeshWorkGroupInvocations, std::min(mesh.maxMeshSharedMemorySize, mesh.maxMeshPayloadAndSharedMemorySize), mesh.maxMeshOutputVertices, mesh.maxMeshOutputPrimitives, mesh.maxMeshOutputComponents, std::min(mesh.maxMeshOutputMemorySize, mesh.maxMeshPayloadAndOutputMemorySize), mesh.meshOutputPerVertexGranularity, mesh.meshOutputPerPrimitiveGranularity};
    }
    if (state->tessellationShader) target.tessellation = ShaderRecompiler::TessellationTargetLimits{limits.maxTessellationPatchSize, limits.maxTessellationControlPerVertexInputComponents, limits.maxTessellationControlPerVertexOutputComponents, limits.maxTessellationControlPerPatchOutputComponents, limits.maxTessellationControlTotalOutputComponents, limits.maxTessellationEvaluationInputComponents, limits.maxTessellationEvaluationOutputComponents};
    return target;
}

Graphics::Context VulkanDevice::graphicsContext() const {
    auto context = Graphics::Context{
        state->device,
        state->physical,
        state->queue,
        state->pool,
        state->deviceProc,
        state->InstanceFunction<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties"),
        state->InstanceFunction<PFN_vkGetPhysicalDeviceImageFormatProperties>("vkGetPhysicalDeviceImageFormatProperties"),
        state->memoryProperties,
        state->properties.limits,
        state->tessellationShader,
        state->meshShader,
        state->meshLimits,
        state->depthClipControl,
        state->depthRangeUnrestricted,
        true,
        state->subgroup,
        state->fragmentShaderBarycentric,
        state->samplerAnisotropy,
        state->textureCompressionBC,
        state->detiler.get(),
        state->colorTransfer.get(),
        state->bufferPool,
        state->textureCache.get(),
        state->pipelineCache ? state->pipelineCache->Handle() : VK_NULL_HANDLE,
        state->depthClamp,
        state->hostImportAlignment
    };
    context.recorder = state->recorder.get();
    context.descriptorCache = state->descriptorCache.get();
    context.samplerCache = state->samplerCache.get();
    return context;
}

void VulkanDevice::Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots) {
    APS5_LOG_OUT("VulkanDevice::Draw indices=%u instances=%u indexSize=%u address=0x%llx shaders=%zu colorTarget=%u", draw.indexCount, draw.instanceCount, draw.indexSize, static_cast<unsigned long long>(draw.indexAddress), shaders.size(), static_cast<unsigned>(graphics.hasColorTarget));
    const auto context = graphicsContext();
    Graphics::Draw(context, graphics, draw, shaders, snapshots);
    APS5_LOG_CHARS_OUT("VulkanDevice::Draw complete");
}

namespace {

// The resource cache serves dispatches unless disabled; Revalidate proves a hit by texture identity,
// which needs the texture caches: without them every lookup makes a fresh object, so a hit could
// never validate and would only upload everything twice.
bool ResourceCacheEnabled() {
    static const bool noResourceCache = std::getenv("APS5_NO_RESOURCE_CACHE") != nullptr;
    static const bool noTextureCache = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    return !noResourceCache && !noTextureCache;
}

// The content key of a compute stage, naming the device: the cache is process-wide and the driver
// replaces the headless device with the windowed one while workers may still use the old one, so
// the key names the device that built the entry (its descriptor set and pooled buffers belong to it).
ResourceCache::Key DispatchContentKey(const Graphics::CompiledShader& shader, VkDevice device) {
    auto key = Graphics::ShaderResources::ContentKey(shader);
    const auto deviceHandle = reinterpret_cast<std::uint64_t>(device);
    key.push_back(static_cast<std::uint32_t>(deviceHandle));
    key.push_back(static_cast<std::uint32_t>(deviceHandle >> 32u));
    return key;
}

}

struct PreparedDispatch {
    // Stage A done; stage B (Complete) runs in the dispatch under the mutex. Null when the resource
    // cache held the key's object at prepare time: the dispatch looks it up and revalidates it.
    std::shared_ptr<Graphics::ShaderResources> resources;
    ResourceCache::Key key;
    // APS5_PROFILE_DRAW: stage A's time, added to the [dispatch] phase totals by the dispatch.
    double prepareMs = 0;
    // The stage-A pre-sync (see PrepareDispatch): the newest recorder serial waited for without the
    // mutex, whose batches the dispatch reaps under it before stage B (0: nothing waited for).
    std::uint64_t presyncSerial = 0;
};

namespace {

// APS5_PROFILE_DRAW: the pre-syncs made and the time they waited (unlocked), as [presync] every 10 s.
struct PresyncCounters {
    std::atomic<std::uint64_t> checked{0};
    std::atomic<std::uint64_t> presyncs{0};
    std::atomic<std::uint64_t> waitedUs{0};
    std::atomic<std::int64_t> lastReport{0};
};

PresyncCounters& Presyncs() {
    static PresyncCounters counters;
    return counters;
}

}

std::shared_ptr<PreparedDispatch> VulkanDevice::PrepareDispatch(const ShaderRecompiler::RecompileResult& shader, std::span<const Graphics::GuestMemorySnapshot> snapshots) {
    // APS5_LOCKED_BUILD=1: the whole build under the mutex, as before the split.
    static const bool lockedBuild = std::getenv("APS5_LOCKED_BUILD") != nullptr;
    if (lockedBuild || shader.spirv.size() < 5 || shader.spirv[0] != 0x07230203u) return nullptr;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = std::chrono::steady_clock::now();
    const Graphics::CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &shader, 0};
    const auto context = graphicsContext();
    auto prepared = std::make_shared<PreparedDispatch>();
    // A cached build (the map locks itself) is revalidated under the mutex by the dispatch, which
    // looks it up again by the key made here; the object stays local, only its surfaces matter
    // below. Anything inserted between here and the dispatch is simply replaced by this build.
    std::shared_ptr<Graphics::ShaderResources> cached;
    if (ResourceCacheEnabled() && shader.variantId != 0) {
        prepared->key = DispatchContentKey(compute, context.device);
        cached = state->resourceCache.Find(prepared->key);
    }
    if (cached == nullptr) prepared->resources = std::make_shared<Graphics::ShaderResources>(context, compute, snapshots, true);
    // The pre-sync. Stage B's image lookups that read guest memory on the CPU (PresyncSurfaces)
    // wait, through the flush hook and under the mutex, for the recorded work writing those
    // surfaces: every other worker queues behind that wait. So the wait is made here instead,
    // without the mutex: the lock-free pending-write snapshot says whether any recorded batch
    // writes a surface; if so the mutex is taken only long enough to learn the newest such batch
    // (submitting the open one when it is that batch), the timeline wait runs unlocked, and the
    // dispatch reaps those batches at its own lock so the hook then finds nothing pending. Work
    // noted in between falls back to the locked wait as before. A cached object's Revalidate
    // repeats the same lookups when its stamps fail, so it gets the same pre-sync. Needs timeline
    // semaphores. APS5_NO_PRESYNC=1 disables it.
    static const bool noPresync = std::getenv("APS5_NO_PRESYNC") != nullptr;
    if (!noPresync && CanWaitUnlocked()) {
        auto& counters = Presyncs();
        counters.checked.fetch_add(1, std::memory_order_relaxed);
        const auto surfaces = cached != nullptr ? cached->PresyncSurfaces() : prepared->resources->PresyncSurfaces();
        bool overlaps = false;
        for (const auto& [address, bytes] : surfaces) overlaps = overlaps || Graphics::Recorder::SnapshotWriteOverlaps(address, static_cast<std::size_t>(bytes));
        if (overlaps) {
            std::uint64_t serial = 0;
            {
                // No reap here (SubmitAndEpoch's would run completions, which a compute worker must
                // not do on queue 0's behalf, see SubmitRecorded): the dispatch reaps after the wait.
                std::lock_guard lock(GuestMemory::GpuMutex());
                if (state->recorder) {
                    bool open = false;
                    for (const auto& [address, bytes] : surfaces) {
                        const auto info = state->recorder->DescribePendingWrite(address, static_cast<std::size_t>(bytes));
                        if (!info.has_value()) continue;
                        open = open || info->open;
                        serial = std::max(serial, info->serial);
                    }
                    if (open) serial = state->recorder->SubmitAndEpoch();
                }
            }
            if (serial != 0) {
                const auto waitStart = std::chrono::steady_clock::now();
                WaitRecorded(serial);
                prepared->presyncSerial = serial;
                counters.presyncs.fetch_add(1, std::memory_order_relaxed);
                counters.waitedUs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - waitStart).count()), std::memory_order_relaxed);
            }
        }
        if (profile) {
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            auto last = counters.lastReport.load();
            if (nowMs - last >= 10000 && counters.lastReport.compare_exchange_strong(last, nowMs)) std::fprintf(stderr, "[presync] %llu dispatches checked, %llu pre-syncs waited %.0f ms (cumulative)\n", static_cast<unsigned long long>(counters.checked.load()), static_cast<unsigned long long>(counters.presyncs.load()), counters.waitedUs.load() / 1000.0);
        }
    }
    if (profile) prepared->prepareMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return prepared;
}

void VulkanDevice::Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared) {
    static_cast<void>(dispatch(shader, x, y, z, 0, snapshots, programAddress, std::move(prepared)));
}

VulkanDevice::IndirectOutcome VulkanDevice::DispatchIndirect(const ShaderRecompiler::RecompileResult& shader, std::uint64_t arguments, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared) {
    return dispatch(shader, 0, 0, 0, arguments, snapshots, programAddress, std::move(prepared));
}

VulkanDevice::IndirectOutcome VulkanDevice::dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t arguments, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared) {
    PerformanceTimer timing("Vulkan.Dispatch");
    // Group counts for the trace lines; an indirect dispatch does not know them.
    char groupsText[40];
    if (arguments != 0) std::snprintf(groupsText, sizeof(groupsText), "indirect");
    else std::snprintf(groupsText, sizeof(groupsText), "%ux%ux%u", x, y, z);
    APS5_LOG_OUT_DEBUG("Dispatch groups=%s spirvWords=%zu bindings=%zu pushConstants=%zu", groupsText, shader.spirv.size(), shader.bindings.size(), shader.pushConstants.size());
    if (shader.spirv.size() < 5 || shader.spirv[0] != 0x07230203u) {
        throw std::runtime_error("Vulkan dispatch: invalid SPIR-V");
    }
    const std::array<Graphics::CompiledShader, 1> shaders{{{ShaderRecompiler::ShaderStage::Compute, &shader, 0}}};
    const auto pushStages = Graphics::PushConstantStages(shaders);
    if (pushStages != 0 && state->properties.limits.maxPushConstantsSize < Graphics::PipelinePushConstantBytes) {
        throw std::runtime_error("Vulkan dispatch: compute push constant range exceeds device limit");
    }
    const auto pushBytes = Graphics::AssemblePushConstants(shaders);
    const auto context = graphicsContext();
    const auto* limit = state->properties.limits.maxComputeWorkGroupCount;
    if (arguments == 0 && (x > limit[0] || y > limit[1] || z > limit[2])) {
        throw std::runtime_error("Vulkan dispatch: workgroup count exceeds device limits");
    }
    // Pipelines are shared by dispatches of one compiled variant; descriptor set layouts built from the
    // same bindings are compatible, so the pipeline layout of the first dispatch serves them all.
    const std::uint64_t pipelineKey = shader.variantId != 0 ? (shader.variantId << 1u) | (pushStages != 0 ? 1u : 0u) : 0u;
    std::shared_ptr<ComputePipelineObjects> objects;
    if (pipelineKey != 0) {
        if (const auto found = state->computePipelines.find(pipelineKey); found != state->computePipelines.end()) objects = found->second;
    }
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // Debug aid: APS5_SYNC_DISPATCH=1 waits for every dispatch, as before batching.
    static const bool syncEachDispatch = std::getenv("APS5_SYNC_DISPATCH") != nullptr;
    auto phaseStart = std::chrono::steady_clock::now();
    const auto dispatchStart = phaseStart;
    std::string phases;
    // Totals per phase across dispatches, reported every 1000 dispatches under APS5_PROFILE_DRAW.
    static std::map<std::string, double> phaseTotals;
    static std::uint64_t profiledDispatches = 0;
    // The same phases for indirect dispatches alone, every 10 s on an [indirect] line: what the
    // 'indirect' GpuMutex hold ([lock] line) spends its time on inside this function (the label
    // record before it is timed in the driver, [labels] line), with the longest hold seen and the
    // copied-writer lists the indirect decision scans. All under the mutex, like the rest.
    struct IndirectHold {
        std::map<std::string, double> phaseMs;
        std::uint64_t count = 0;
        double totalMs = 0;
        double maxMs = 0;
        std::uint64_t writersScanned = 0;
        std::uint64_t drawWritersScanned = 0;
        std::size_t maxWriters = 0;
        std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    };
    static IndirectHold indirectHold;
    const auto phase = [&](const char* name) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration<double, std::milli>(now - phaseStart).count();
        char text[64];
        std::snprintf(text, sizeof(text), " %s=%.0fms", name, ms);
        phases += text;
        phaseTotals[name] += ms;
        if (arguments != 0) indirectHold.phaseMs[name] += ms;
        phaseStart = now;
        // Slow phases are reported as they finish, so a dispatch that never completes shows where it is.
        if (ms > 1000) std::fprintf(stderr, "[dispatch] %s took %.0f ms (%zu words)\n", name, ms, shader.spirv.size());
    };
    if (profile && ++profiledDispatches % 1000 == 0) {
        std::string report;
        for (const auto& [name, ms] : phaseTotals) report += " " + name + "=" + std::to_string(static_cast<long long>(ms / 1000)) + "s";
        std::fprintf(stderr, "[dispatch] %llu dispatches, phase totals:%s\n", static_cast<unsigned long long>(profiledDispatches), report.c_str());
    }
    // A dispatch whose compiled content repeats an earlier one reuses that build's descriptor set
    // when it is still valid (see ResourceCache); push constants still come from this dispatch.
    static std::uint64_t cacheHits = 0, cacheMisses = 0, cacheInvalidated = 0;
    static auto cacheReport = std::chrono::steady_clock::now();
    std::shared_ptr<Graphics::ShaderResources> resources;
    ResourceCache::Key contentKey;
    const bool cacheable = ResourceCacheEnabled() && shader.variantId != 0;
    // The batches the pre-sync (PrepareDispatch) waited for are reaped first: their completions
    // (CPU write-backs) land before stage B or a cached object's Revalidate reads, and the flush
    // hook then finds nothing pending over their surfaces.
    if (prepared != nullptr && prepared->presyncSerial != 0) {
        // Timed alone: the phase entry exists only when a reap ran, and measures just the reap.
        phaseStart = std::chrono::steady_clock::now();
        ReapRecorded(prepared->presyncSerial);
        phase("reap");
    }
    // The 'resources' phase below, split (APS5_PROFILE_DRAW) into what runs under the mutex: the
    // sub-phases are extra rows named "resources: ..." in the [dispatch] totals and the [indirect]
    // line, so the hold's biggest part (stage B's image lookups, the whole build of an address-based
    // shader) is named, and "resources: hook waits" says how much of the build was the nested flush
    // hook waiting for recorded work (a locked wait: it overlaps the other rows).
    const auto subPhase = [&](const char* name, double ms) {
        if (!profile) return;
        phaseTotals[name] += ms;
        if (arguments != 0) indirectHold.phaseMs[name] += ms;
    };
    const auto subPhaseStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto waitedBefore = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    if (prepared != nullptr && prepared->resources != nullptr) {
        // Stage A ran without the mutex (PrepareDispatch); stage B completes the build here.
        resources = std::move(prepared->resources);
        contentKey = std::move(prepared->key);
        // The build's own sub-phase totals before and after: the differences are stage B's parts
        // (an address-based build also runs its stage A here, under the mutex: 'A locked').
        const auto before = profile ? resources->Timing() : Graphics::ShaderResources::BuildTiming{};
        resources->Complete();
        if (profile) {
            const auto& after = resources->Timing();
            const auto images = after.bindingsMs - before.bindingsMs;
            const auto upload = after.uploadMs - before.uploadMs;
            const auto descriptors = after.descriptorsMs - before.descriptorsMs;
            const auto stageA = after.prepareMs - before.prepareMs;
            subPhase("resources: B images", images);
            subPhase("resources: B upload", upload);
            subPhase("resources: B descriptors", descriptors);
            subPhase("resources: B bda+other", std::max(0.0, after.completeMs - before.completeMs - images - upload - descriptors));
            subPhase("resources: A locked (bda)", stageA);
            char text[64];
            std::snprintf(text, sizeof(text), " resourcesA=%.0fms", prepared->prepareMs);
            phases += text;
            phaseTotals["resources A (unlocked)"] += prepared->prepareMs;
        }
        if (cacheable) {
            ++cacheMisses;
            if (resources->Reusable()) state->resourceCache.Insert(contentKey, resources);
        }
    } else if (cacheable) {
        // PrepareDispatch made the key already when it found the cached object.
        contentKey = prepared != nullptr && !prepared->key.empty() ? std::move(prepared->key) : DispatchContentKey(shaders[0], context.device);
        if (auto cached = state->resourceCache.Find(contentKey)) {
            if (cached->Revalidate(shaders[0])) {
                resources = std::move(cached);
                ++cacheHits;
            } else {
                state->resourceCache.Remove(contentKey);
                ++cacheInvalidated;
            }
            if (profile) subPhase("resources: revalidate", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - subPhaseStart).count());
        }
    }
    if (resources == nullptr) {
        const auto buildStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        resources = std::make_shared<Graphics::ShaderResources>(context, shaders[0], snapshots);
        if (profile) subPhase("resources: full build (locked)", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - buildStart).count());
        if (cacheable) {
            ++cacheMisses;
            if (resources->Reusable()) state->resourceCache.Insert(contentKey, resources);
        }
    }
    if (profile) subPhase("resources: hook waits", Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
    if (profile && std::chrono::steady_clock::now() - cacheReport > std::chrono::seconds(10)) {
        cacheReport = std::chrono::steady_clock::now();
        std::fprintf(stderr, "[rescache] %llu hits, %llu misses, %llu invalidated, %zu entries (dispatch + draw)\n", static_cast<unsigned long long>(cacheHits), static_cast<unsigned long long>(cacheMisses), static_cast<unsigned long long>(cacheInvalidated), state->resourceCache.Size());
    }
    timing.Mark("shader_resources");
    phase("resources");
    if (objects == nullptr) {
        objects = std::make_shared<ComputePipelineObjects>();
        objects->device = state->device;
        objects->destroyModule = state->DeviceFunction<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
        objects->destroyLayout = state->DeviceFunction<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout");
        objects->destroyPipeline = state->DeviceFunction<PFN_vkDestroyPipeline>("vkDestroyPipeline");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = shader.spirv.size() * sizeof(std::uint32_t);
        moduleInfo.pCode = shader.spirv.data();
        check(state->DeviceFunction<PFN_vkCreateShaderModule>("vkCreateShaderModule")(state->device, &moduleInfo, nullptr, &objects->module), "vkCreateShaderModule");
        timing.Mark("validate_shader_module");
        const auto setLayout = resources->Layout();
        const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, Graphics::PipelinePushConstantBytes};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;
        layoutInfo.pushConstantRangeCount = pushStages != 0 ? 1 : 0;
        layoutInfo.pPushConstantRanges = pushStages != 0 ? &push : nullptr;
        check(state->DeviceFunction<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(state->device, &layoutInfo, nullptr, &objects->layout), "vkCreatePipelineLayout");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = objects->module;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = objects->layout;
        if (profile && shader.spirv.size() > 100000) std::fprintf(stderr, "[dispatch] creating a pipeline for %zu SPIR-V words\n", shader.spirv.size());
        check(state->DeviceFunction<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(state->device, context.pipelineCache, 1, &pipelineInfo, nullptr, &objects->pipeline), "vkCreateComputePipelines");
        timing.Mark("pipeline_create");
        if (pipelineKey != 0) state->computePipelines[pipelineKey] = objects;
        phase("pipeline");
    }
    // Recorded into the device's open batch: the CPU moves on to the next command while the GPU
    // works; the batch is waited for where the guest expects results (labels, flips, CPU reads).
    auto& recorder = *state->recorder;
    IndirectOutcome outcome{0, 0};
    const Graphics::HostImport* argumentImport = nullptr;
    if (arguments != 0) {
        // Decided here, after the resource build (which may retire imports and wait for recorded
        // work) and before anything of this dispatch is recorded, so the CPU fallback's sync cannot
        // split the dispatch across two batches and the import cannot be dropped before the record.
        // A label over the argument dwords (a completion-deferred one lands by a CPU store the GPU
        // read would miss; a GPU one is ordered but rare enough to share the fallback). Only those
        // dwords: a global "any completion label pending" test would send most indirect dispatches
        // to the CPU, as ~5 such labels are pending per frame. Stamp 0 matches every live entry.
        const auto labelPending = [&] {
            std::uint32_t labelQueue = 0;
            for (std::uint64_t dword = arguments; dword < arguments + 12; dword += 4) {
                if (recorder.PendingLabel(dword, 4, 0, labelQueue).has_value()) return true;
            }
            return false;
        };
        if (Graphics::StorageTexture::FlushPending(arguments, 12, nullptr, "indirect dispatch arguments")) {
            // As the flush hook does: the stores were only recorded and the CPU is about to read them.
            outcome.cpuReason = 1;
            Graphics::Recorder::CountSync(2);
            recorder.Sync();
        } else if (labelPending() || std::any_of(state->copiedWriters->begin(), state->copiedWriters->end(), [&](const auto& writer) { return writer->WritesOverlap(arguments, 12); })
                   || std::any_of(Graphics::DrawCopiedWriters()->begin(), Graphics::DrawCopiedWriters()->end(), [&](const auto& writer) { return writer->WritesOverlap(arguments, 12); })) {
            outcome.cpuReason = 2;
        } else if ((argumentImport = Graphics::HostImportFor(context, arguments, 12)) == nullptr) {
            // Valid until the next refreshImports, which only runs under GuestMemory::GpuMutex (held
            // here, by every Dispatch caller) and Keeps a retired VkBuffer while the recorder is busy.
            outcome.cpuReason = 3;
        }
        if (profile) {
            indirectHold.writersScanned += state->copiedWriters->size();
            indirectHold.drawWritersScanned += Graphics::DrawCopiedWriters()->size();
            indirectHold.maxWriters = std::max(indirectHold.maxWriters, state->copiedWriters->size());
        }
        phase("decide");
        if (outcome.cpuReason != 0) {
            // The read waits for the producing batch through the flush hook, as the driver's resolve
            // did for every indirect dispatch before; the counts then go through the direct checks.
            const auto readStart = std::chrono::steady_clock::now();
            std::array<std::uint32_t, 3> groups{};
            GuestMemory::Read(arguments, std::as_writable_bytes(std::span(groups)), 4);
            outcome.argumentReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count();
            x = groups[0];
            y = groups[1];
            z = groups[2];
            // Timed before `arguments` is cleared, so the read counts as an indirect phase.
            phase("argument read");
            arguments = 0;
            std::snprintf(groupsText, sizeof(groupsText), "%ux%ux%u", x, y, z);
            if (x > limit[0] || y > limit[1] || z > limit[2]) throw std::runtime_error("Vulkan dispatch: workgroup count exceeds device limits");
        }
    }
    // From here on the phases of a CPU-resolved indirect dispatch are charged as direct ones
    // (`arguments` is 0); the indirect hold total below still covers the whole call.
    const bool indirect = arguments != 0 || outcome.cpuReason != 0;
    const auto commands = recorder.Commands();
    recorder.Keep(objects);
    recorder.Keep(resources);
    if (argumentImport != nullptr) {
        // The group counts were stored by earlier recorded work (a dispatch in place, a fill) or the
        // host; the indirect read follows all of it.
        Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
    }
    // Results of earlier recorded work are visible to this dispatch, its own to everything after.
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    state->DeviceFunction<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, objects->pipeline);
    resources->Bind(commands, VK_PIPELINE_BIND_POINT_COMPUTE, objects->layout);
    if (pushStages != 0) {
        state->DeviceFunction<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, objects->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, Graphics::PipelinePushConstantBytes, pushBytes.data());
    }
    const auto gpuTiming = recorder.BeginGpuTiming(programAddress != 0 ? programAddress : shader.variantId);
    if (argumentImport != nullptr) state->DeviceFunction<PFN_vkCmdDispatchIndirect>("vkCmdDispatchIndirect")(commands, argumentImport->buffer, arguments - argumentImport->base);
    else state->DeviceFunction<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, x, y, z);
    Graphics::RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT);
    recorder.EndGpuTiming(gpuTiming);
    resources->MarkGpuWrites(recorder);
    // Only copied written buffers (and BDA fault checks) need work once the GPU is done; without them
    // the batch can signal its labels from the GPU.
    if (resources->NeedsCompletion()) {
        // Listed for DispatchIndirect until the write-back ran (a non-reusable object, so once).
        auto writers = state->copiedWriters;
        recorder.OnComplete([resources, writers] {
            // Delisted before the write-back: one that fails must not keep indirect dispatches on the CPU.
            writers->erase(std::remove(writers->begin(), writers->end(), resources), writers->end());
            resources->WriteBackBuffers();
        });
        // Listed after the registration: a throw there leaves nothing that would pin the CPU path forever.
        writers->push_back(resources);
    }
    timing.Mark("command_record");
    phase("record");
    APS5_LOG_OUT_DEBUG("Dispatch recorded groups=%s", groupsText);
    // Address-based shaders pin guest allocations until their write-back, which the completion runs
    // when the batch finished (deferred lease release, see Graphics::SyncLeaseWork): a guest thread
    // that needs a leased allocation syncs the recorder itself through the registry's pin waiter.
    // APS5_SYNC_LEASE_DISPATCH=1 completes such dispatches at once, as before.
    if (syncEachDispatch || (resources->HoldsLease() && Graphics::SyncLeaseWork())) {
        Graphics::Recorder::CountSync(3);
        const auto syncStart = std::chrono::steady_clock::now();
        recorder.Sync();
        phase("sync");
        if (resources->HoldsLease()) Graphics::CountLeaseOutcome(true, 0);
        if (profile) {
            // APS5_PROFILE_DRAW: the [recorder] line's "address-based" syncs by program, every 10 s,
            // so the programs still waiting here are known (under the GpuMutex, like the rest).
            struct Waits { std::uint64_t count = 0; double ms = 0; };
            static std::map<std::uint64_t, Waits> byProgram;
            static std::uint64_t syncs = 0;
            static double waitedMs = 0;
            static auto lastReport = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration<double, std::milli>(now - syncStart).count();
            auto& waits = byProgram[programAddress != 0 ? programAddress : shader.variantId];
            ++waits.count;
            waits.ms += ms;
            ++syncs;
            waitedMs += ms;
            if (now - lastReport > std::chrono::seconds(10)) {
                lastReport = now;
                std::vector<std::pair<std::uint64_t, Waits>> hot(byProgram.begin(), byProgram.end());
                std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.ms > b.second.ms; });
                std::fprintf(stderr, "[address-sync] %llu address-based dispatch syncs waited %.1f s in total; by program (10 s):", static_cast<unsigned long long>(syncs), waitedMs / 1000);
                for (std::size_t i = 0; i < hot.size() && i < 8; ++i) std::fprintf(stderr, " 0x%llx x%llu %.0fms", static_cast<unsigned long long>(hot[i].first), static_cast<unsigned long long>(hot[i].second.count), hot[i].second.ms);
                std::fprintf(stderr, "\n");
                byProgram.clear();
            }
        }
    } else if (resources->HoldsLease()) {
        // The lease is in the open batch (kept above, nothing submitted since): the pin waiter
        // finishes the recorder up to that batch's serial. Prints the [address-sync] leases line.
        Graphics::CountLeaseOutcome(false, recorder.Submissions() + 1);
    }
    // Debug aid: APS5_TRACE_DISPATCH_IO prints every dispatch's resources (after write-back when synced).
    static const bool traceIo = std::getenv("APS5_TRACE_DISPATCH_IO") != nullptr;
    if (traceIo) std::fprintf(stderr, "[dispatch-io] %s:%s\n", groupsText, resources->Describe().c_str());
    if (profile) {
        const auto now = std::chrono::steady_clock::now();
        const auto totalMs = std::chrono::duration<double, std::milli>(now - dispatchStart).count();
        if (totalMs > 100) std::fprintf(stderr, "[dispatch] %s %zu words:%s\n", groupsText, shader.spirv.size(), phases.c_str());
        if (indirect) {
            auto& hold = indirectHold;
            ++hold.count;
            hold.totalMs += totalMs;
            hold.maxMs = std::max(hold.maxMs, totalMs);
            if (now - hold.lastReport > std::chrono::seconds(10)) {
                hold.lastReport = now;
                std::string report;
                for (const auto& [name, ms] : hold.phaseMs) {
                    char text[80];
                    std::snprintf(text, sizeof(text), " %s %.0f ms", name.c_str(), ms);
                    report += text;
                }
                std::fprintf(stderr, "[indirect] %llu indirect dispatches spent %.0f ms inside the device call (10 s; max %.1f ms), by phase (the 'resources: ...' rows split 'resources'; 'hook waits' overlaps them):%s; copied-writer lists scanned per decision: dispatch avg %.1f (max %zu), draw avg %.1f\n", static_cast<unsigned long long>(hold.count), hold.totalMs, hold.maxMs, report.c_str(), hold.count != 0 ? static_cast<double>(hold.writersScanned) / hold.count : 0.0, hold.maxWriters, hold.count != 0 ? static_cast<double>(hold.drawWritersScanned) / hold.count : 0.0);
                hold.phaseMs.clear();
                hold.count = 0;
                hold.totalMs = 0;
                hold.maxMs = 0;
                hold.writersScanned = 0;
                hold.drawWritersScanned = 0;
                hold.maxWriters = 0;
            }
        }
    }
    return outcome;
}

}
