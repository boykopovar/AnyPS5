#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::BufferPool;
using AgcDriver::Graphics::Check;
using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::Require;

constexpr std::uint32_t Stale = 0xababababu;
constexpr std::uint32_t Fresh = 0x11111111u;
constexpr VkMemoryPropertyFlags HostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
constexpr std::size_t RequestedBytes = 100;
constexpr std::uint32_t PastEnd = 28;

PFN_vkGetDeviceProcAddr RealDeviceProc = nullptr;
PFN_vkCreateBuffer RealCreateBuffer = nullptr;

std::vector<VkDeviceSize>& CreatedSizes() {
    static std::vector<VkDeviceSize> sizes;
    return sizes;
}

VKAPI_ATTR VkResult VKAPI_CALL CountedCreateBuffer(VkDevice device, const VkBufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkBuffer* buffer) {
    CreatedSizes().push_back(info->size);
    return RealCreateBuffer(device, info, allocator, buffer);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL CountingDeviceProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkCreateBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&CountedCreateBuffer);
    return RealDeviceProc(device, name);
}

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
            VkPhysicalDeviceFeatures supported{};
            function<PFN_vkGetPhysicalDeviceFeatures>("vkGetPhysicalDeviceFeatures")(context.physical, &supported);
            Require(supported.robustBufferAccess == VK_TRUE, "the device lacks robustBufferAccess");
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
            VkPhysicalDeviceFeatures features{};
            features.robustBufferAccess = VK_TRUE;
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.pEnabledFeatures = &features;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            RealDeviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            RealCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(RealDeviceProc(context.device, "vkCreateBuffer"));
            Require(RealCreateBuffer != nullptr, "missing vkCreateBuffer");
            context.deviceProc = &CountingDeviceProc;
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

std::vector<std::uint32_t> CopyPastEndModule() {
    enum : std::uint32_t { Void = 1, FunctionType, Uint, Words, Block, BlockPointer, Source, Destination, UintPointer, Zero, Index, Main, Label, SourcePointer, Value, DestinationPointer, Bound };
    constexpr std::uint32_t storageBuffer = 12;
    std::vector<std::uint32_t> words{0x07230203u, 0x00010300u, 0u, Bound, 0u};
    Emit(words, 17, {1});
    Emit(words, 14, {0, 1});
    Emit(words, 15, {5, Main, 0x6e69616du, 0u});
    Emit(words, 16, {Main, 17, 1, 1, 1});
    Emit(words, 71, {Words, 6, 4});
    Emit(words, 71, {Block, 2});
    Emit(words, 72, {Block, 0, 35, 0});
    Emit(words, 71, {Source, 34, 0});
    Emit(words, 71, {Source, 33, 0});
    Emit(words, 71, {Destination, 34, 0});
    Emit(words, 71, {Destination, 33, 1});
    Emit(words, 19, {Void});
    Emit(words, 33, {FunctionType, Void});
    Emit(words, 21, {Uint, 32, 0});
    Emit(words, 29, {Words, Uint});
    Emit(words, 30, {Block, Words});
    Emit(words, 32, {BlockPointer, storageBuffer, Block});
    Emit(words, 59, {BlockPointer, Source, storageBuffer});
    Emit(words, 59, {BlockPointer, Destination, storageBuffer});
    Emit(words, 32, {UintPointer, storageBuffer, Uint});
    Emit(words, 43, {Uint, Zero, 0});
    Emit(words, 43, {Uint, Index, PastEnd});
    Emit(words, 54, {Void, Main, 0, FunctionType});
    Emit(words, 248, {Label});
    Emit(words, 65, {UintPointer, SourcePointer, Source, Zero, Index});
    Emit(words, 61, {Uint, Value, SourcePointer});
    Emit(words, 65, {UintPointer, DestinationPointer, Destination, Zero, Zero});
    Emit(words, 62, {DestinationPointer, Value});
    Emit(words, 253, {});
    Emit(words, 56, {});
    return words;
}

std::uint32_t ReadPastEnd(const Context& context, AgcDriver::Graphics::Buffer& source, AgcDriver::Graphics::Buffer& destination) {
    const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}}};
    VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    setLayoutInfo.pBindings = bindings.data();
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setLayoutInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
    const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &descriptorPool), "vkCreateDescriptorPool");
    VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool = descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &setLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocateInfo, &set), "vkAllocateDescriptorSets");
    const std::array<VkDescriptorBufferInfo, 2> infos{{{source.Handle(), 0, VK_WHOLE_SIZE}, {destination.Handle(), 0, VK_WHOLE_SIZE}}};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::uint32_t binding = 0; binding < writes.size(); ++binding) {
        writes[binding] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[binding].dstSet = set;
        writes[binding].dstBinding = binding;
        writes[binding].descriptorCount = 1;
        writes[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[binding].pBufferInfo = &infos[binding];
    }
    context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);

    const auto module = CopyPastEndModule();
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
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(batch.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(batch.Handle(), 1, 1, 1);
        AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        batch.SubmitAndWait();
    }
    context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
    context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
    context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, shader, nullptr);
    context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, descriptorPool, nullptr);
    context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    destination.Invalidate();
    std::uint32_t value = 0;
    std::memcpy(&value, destination.Bytes().data(), sizeof(value));
    return value;
}

void RobustReadPastEnd(const Context& context) {
    constexpr VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto classBytes = BufferPool::Capacity(RequestedBytes, HostMemory);
    Require(classBytes > (PastEnd + 1) * sizeof(std::uint32_t), "the requested buffer's size class does not reach the word read past its end");
    {
        AgcDriver::Graphics::Buffer earlier(context, classBytes, usage);
        const auto bytes = earlier.Bytes();
        for (std::size_t offset = 0; offset + sizeof(Stale) <= bytes.size(); offset += sizeof(Stale)) std::memcpy(bytes.data() + offset, &Stale, sizeof(Stale));
    }
    AgcDriver::Graphics::Buffer source(context, RequestedBytes, usage);
    const auto bytes = source.Bytes();
    Require(bytes.size() == RequestedBytes, "the pooled buffer does not span the requested bytes");
    for (std::size_t offset = 0; offset + sizeof(Fresh) <= bytes.size(); offset += sizeof(Fresh)) std::memcpy(bytes.data() + offset, &Fresh, sizeof(Fresh));
    AgcDriver::Graphics::Buffer destination(context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(destination.Bytes().data(), 0, destination.Bytes().size());
    const auto value = ReadPastEnd(context, source, destination);
    char text[160];
    std::snprintf(text, sizeof(text), "a robust read of word %u of a %zu-byte pooled buffer returned 0x%08x, the bytes a %zu-byte earlier user left in its memory", PastEnd, RequestedBytes, value, classBytes);
    Require(value != Stale, text);
    Require(value == 0 || value == Fresh, "a robust read past the buffer returned neither zero nor a value inside it");
}

void ExpectCreated(std::initializer_list<VkDeviceSize> sizes, const std::string& what) {
    const std::vector<VkDeviceSize> expected(sizes);
    std::string got;
    for (const auto size : CreatedSizes()) got += " " + std::to_string(size);
    Require(CreatedSizes() == expected, what + ": VkBuffer sizes created:" + (got.empty() ? std::string(" none") : got));
    CreatedSizes().clear();
}

void ExactSizes(const Context& context) {
    constexpr VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto hostClass = BufferPool::Capacity(100, HostMemory);
    const auto deviceClass = BufferPool::Capacity(3000, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Require(BufferPool::Capacity(80, HostMemory) == hostClass && hostClass != 80, "80 and 100 bytes are not one host size class");
    Require(BufferPool::Capacity(2500, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == deviceClass && deviceClass != 2500, "2500 and 3000 bytes are not one device size class");
    CreatedSizes().clear();
    {
        AgcDriver::Graphics::Buffer fresh(context, 100, usage);
        ExpectCreated({100}, "a fresh 100-byte buffer");
    }
    {
        AgcDriver::Graphics::Buffer pooled(context, 80, usage);
        ExpectCreated({80}, "an 80-byte buffer taken from the " + std::to_string(hostClass) + "-byte class");
    }
    {
        AgcDriver::Graphics::Buffer again(context, 80, usage);
        ExpectCreated({}, "an 80-byte buffer reusing an 80-byte one");
    }
    {
        AgcDriver::Graphics::DeviceBuffer fresh(context, 3000, usage);
        ExpectCreated({3000}, "a fresh 3000-byte device buffer");
    }
    {
        AgcDriver::Graphics::DeviceBuffer pooled(context, 2500, usage);
        ExpectCreated({2500}, "a 2500-byte device buffer taken from the " + std::to_string(deviceClass) + "-byte class");
    }
}

}

int main() {
    try {
        std::unique_ptr<Device> device;
        try {
            device = std::make_unique<Device>();
        } catch (const std::exception& error) {
            if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
            std::printf("skipped, no usable Vulkan device: %s\n", error.what());
            return VulkanTestSkipped;
        }
        RobustReadPastEnd(device->GetContext());
        ExactSizes(device->GetContext());
        std::puts("pooled buffer range tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
