#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Check;
using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::DescriptorCache;
using Testing::Require;

constexpr std::uint32_t Buffers = 4097;
constexpr std::uint32_t Spacing = 256;

class Device {
public:
    Device() {
#ifdef _WIN32
        library = SDL_LoadObject("vulkan-1.dll");
#else
        library = SDL_LoadObject("libvulkan.so.1");
#endif
        Require(library != nullptr, "cannot load Vulkan");
        try {
            instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
            Require(instanceProc != nullptr, "missing Vulkan instance resolver");
            VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            application.apiVersion = VK_API_VERSION_1_1;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            info.pApplicationInfo = &application;
            Check(function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
            std::uint32_t count = 0;
            const auto enumerate = function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
            Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            Require(count != 0, "no Vulkan device");
            std::vector<VkPhysicalDevice> devices(count);
            Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            const auto properties = function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
            context.physical = devices.front();
            for (const auto candidate : devices) {
                VkPhysicalDeviceProperties described{};
                properties(candidate, &described);
                if (described.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                    context.physical = candidate;
                    break;
                }
            }
            VkPhysicalDeviceProperties selected{};
            properties(context.physical, &selected);
            context.limits = selected.limits;
            std::printf("device %s\n", selected.deviceName);
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) ++family;
            Require(family < count, "no Vulkan compute queue");
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.queueFamilyIndex = family;
            Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.pool), "vkCreateCommandPool");
        } catch (...) {
            release();
            throw;
        }
    }

    ~Device() { release(); }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    const Context& GetContext() const { return context; }

private:
    template<typename TFunction>
    TFunction function(const char* name) const {
        const auto result = reinterpret_cast<TFunction>(instanceProc(instance, name));
        Require(result != nullptr, name);
        return result;
    }

    void release() noexcept {
        if (context.pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(context.device, context.pool, nullptr);
        context.bufferPool.reset();
        if (context.device != VK_NULL_HANDLE) function<PFN_vkDestroyDevice>("vkDestroyDevice")(context.device, nullptr);
        if (instance != VK_NULL_HANDLE) function<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
        if (library != nullptr) SDL_UnloadObject(library);
    }

    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    Context context{};
};

void Emit(std::vector<std::uint32_t>& words, std::uint32_t opcode, std::initializer_list<std::uint32_t> operands) {
    words.push_back(static_cast<std::uint32_t>((operands.size() + 1u) << 16u) | opcode);
    words.insert(words.end(), operands);
}

std::vector<std::uint32_t> SumLastTwoModule() {
    enum : std::uint32_t { Void = 1, FunctionType, Uint, Block, Length, Array, ArrayPointer, Variable, UintPointer, Zero, Last, BeforeLast, Main, Label, LastPointer, LastValue, BeforeLastPointer, BeforeLastValue, Sum, Destination, Bound };
    constexpr std::uint32_t storageBuffer = 12;
    std::vector<std::uint32_t> words{0x07230203u, 0x00010300u, 0u, Bound, 0u};
    Emit(words, 17, {1});
    Emit(words, 14, {0, 1});
    Emit(words, 15, {5, Main, 0x6e69616du, 0u});
    Emit(words, 16, {Main, 17, 1, 1, 1});
    Emit(words, 71, {Block, 2});
    Emit(words, 72, {Block, 0, 35, 0});
    Emit(words, 71, {Variable, 34, 0});
    Emit(words, 71, {Variable, 33, 0});
    Emit(words, 19, {Void});
    Emit(words, 33, {FunctionType, Void});
    Emit(words, 21, {Uint, 32, 0});
    Emit(words, 30, {Block, Uint});
    Emit(words, 43, {Uint, Length, Buffers});
    Emit(words, 28, {Array, Block, Length});
    Emit(words, 32, {ArrayPointer, storageBuffer, Array});
    Emit(words, 59, {ArrayPointer, Variable, storageBuffer});
    Emit(words, 32, {UintPointer, storageBuffer, Uint});
    Emit(words, 43, {Uint, Zero, 0});
    Emit(words, 43, {Uint, Last, Buffers - 1u});
    Emit(words, 43, {Uint, BeforeLast, Buffers - 2u});
    Emit(words, 54, {Void, Main, 0, FunctionType});
    Emit(words, 248, {Label});
    Emit(words, 65, {UintPointer, LastPointer, Variable, Last, Zero});
    Emit(words, 61, {Uint, LastValue, LastPointer});
    Emit(words, 65, {UintPointer, BeforeLastPointer, Variable, BeforeLast, Zero});
    Emit(words, 61, {Uint, BeforeLastValue, BeforeLastPointer});
    Emit(words, 128, {Uint, Sum, LastValue, BeforeLastValue});
    Emit(words, 65, {UintPointer, Destination, Variable, Zero, Zero});
    Emit(words, 62, {Destination, Sum});
    Emit(words, 253, {});
    Emit(words, 56, {});
    return words;
}

std::uint32_t Input(std::uint32_t buffer) {
    return buffer * 3u + 1u;
}

void CheckAllocation(const Context& context) {
    DescriptorCache cache(context);
    const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Buffers, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    const std::array<std::uint32_t, 4> key{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Buffers, VK_SHADER_STAGE_COMPUTE_BIT};
    auto setLayout = cache.Layout(key, std::span(&binding, 1));
    const std::array<VkDescriptorPoolSize, 1> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Buffers}}};
    const auto first = cache.Allocate(setLayout, sizes);
    const auto second = cache.Allocate(setLayout, sizes);
    Require(first.set != VK_NULL_HANDLE && second.set != VK_NULL_HANDLE, "a set of " + std::to_string(Buffers) + " storage buffers, above the chain pool's capacity, got no descriptor set");
    Require(first.pool != second.pool && cache.Counters().pools == 0, "the oversized sets do not have a pool each");
    cache.Free(first);
    cache.Free(second);
    const auto again = cache.Allocate(setLayout, sizes);
    Require(again.set != VK_NULL_HANDLE && again.pool != VK_NULL_HANDLE && cache.Counters().pools == 0, "an oversized set after the others were freed got no descriptor set");
    cache.Free(again);
}

void CheckDispatch(const Context& context) {
    DescriptorCache cache(context);
    const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Buffers, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    const std::array<std::uint32_t, 4> key{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Buffers, VK_SHADER_STAGE_COMPUTE_BIT};
    auto setLayout = cache.Layout(key, std::span(&binding, 1));
    const std::array<VkDescriptorPoolSize, 1> sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Buffers}}};
    const auto first = cache.Allocate(setLayout, sizes);
    const auto second = cache.Allocate(setLayout, sizes);
    Require(first.set != VK_NULL_HANDLE && second.set != VK_NULL_HANDLE, "a set of " + std::to_string(Buffers) + " storage buffers, above the chain pool's capacity, got no descriptor set");

    AgcDriver::Graphics::Buffer storage(context, static_cast<std::size_t>(Buffers) * Spacing, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto* words = reinterpret_cast<std::uint32_t*>(storage.Bytes().data());
    for (std::uint32_t buffer = 0; buffer < Buffers; ++buffer) words[buffer * (Spacing / 4u)] = Input(buffer);
    std::vector<VkDescriptorBufferInfo> infos;
    for (std::uint32_t buffer = 0; buffer < Buffers; ++buffer) infos.push_back({storage.Handle(), static_cast<VkDeviceSize>(buffer) * Spacing, 4});
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = second.set;
    write.descriptorCount = Buffers;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = infos.data();
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);

    const auto module = SumLastTwoModule();
    VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleInfo.codeSize = module.size() * sizeof(std::uint32_t);
    moduleInfo.pCode = module.data();
    VkShaderModule shader = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &shader), "vkCreateShaderModule");
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shader;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = pipelineLayout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        AgcDriver::Graphics::CommandBatch batch(context);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(batch.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(batch.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &second.set, 0, nullptr);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(batch.Handle(), 1, 1, 1);
        AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        batch.SubmitAndWait();
    }
    context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
    context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
    context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, shader, nullptr);
    storage.Invalidate();
    const auto expected = Input(Buffers - 1u) + Input(Buffers - 2u);
    Require(words[0] == expected, "the dispatch through the oversized set wrote " + std::to_string(words[0]) + ", expected " + std::to_string(expected) + " from its last two storage buffers");

    cache.Free(first);
    cache.Free(second);
}

const Context& SharedContext() {
    static std::unique_ptr<Device> device;
    static std::string failure;
    if (!device && failure.empty()) {
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            failure = error.what();
        }
    }
    if (!device) Testing::Skip("no usable Vulkan device: " + failure);
    const auto& limits = device->GetContext().limits;
    if (limits.maxPerStageDescriptorStorageBuffers < Buffers || limits.maxPerStageResources < Buffers || limits.maxDescriptorSetStorageBuffers < Buffers) {
        Testing::Skip("the device binds fewer than " + std::to_string(Buffers) + " storage buffers per stage or set");
    }
    return device->GetContext();
}

const Testing::Case oversizedAllocation{"OversizedDescriptorSets_SetsAboveChainCapacity_GetAPoolEachAndCanBeReallocated", [] {
    CheckAllocation(SharedContext());
}};

const Testing::Case oversizedDispatch{"OversizedDescriptorSets_DispatchThroughOversizedSet_ReadsItsLastBuffers", [] {
    CheckDispatch(SharedContext());
}};

} // namespace
