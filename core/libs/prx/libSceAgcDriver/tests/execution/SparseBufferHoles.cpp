#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "VulkanTestDevice.hpp"
#include "FixtureDeviceSelection.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::size_t PageBytes = 65536;
constexpr std::size_t BufferBytes = 3 * PageBytes;
constexpr std::size_t ReservedBytes = 4 * PageBytes;
constexpr std::uint32_t WorkgroupSize = 64;
constexpr std::array<std::uint32_t, 3> Patterns{0x11223344u, 0x8899aabbu, 0x55667788u};

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
            context.physical = SelectFixturePhysicalDevice(devices, function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties"));
            VkPhysicalDeviceSubgroupProperties subgroup{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
            VkPhysicalDeviceProperties2 selected{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &subgroup};
            function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(context.physical, &selected);
            context.limits = selected.properties.limits;
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) ++family;
            Require(family < count, "no Vulkan compute queue");
            std::printf("Selected GPU name=%s vendor=0x%x device=0x%x subgroup=%u\n", selected.properties.deviceName, selected.properties.vendorID, selected.properties.deviceID, subgroup.subgroupSize);
            std::printf("sparse buffer holes GPU: api=%u.%u.%u; queueFamily=%u\n", VK_API_VERSION_MAJOR(selected.properties.apiVersion), VK_API_VERSION_MINOR(selected.properties.apiVersion), VK_API_VERSION_PATCH(selected.properties.apiVersion), family);
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
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

class GuestPages {
public:
    explicit GuestPages(bool committed) {
#ifdef _WIN32
        memory = static_cast<std::byte*>(VirtualAlloc(nullptr, ReservedBytes, MEM_RESERVE, PAGE_NOACCESS));
#else
        const auto block = mmap(nullptr, ReservedBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (block != MAP_FAILED) memory = static_cast<std::byte*>(block);
#endif
        Require(memory != nullptr, "cannot reserve sparse guest pages");
        try {
            for (std::size_t page = 0; page < Patterns.size(); ++page) {
                if (page == 1 && !committed) continue;
                auto* begin = memory + page * PageBytes;
#ifdef _WIN32
                Require(VirtualAlloc(begin, PageBytes, MEM_COMMIT, PAGE_READWRITE) == begin, "cannot commit sparse guest page");
#else
                Require(mprotect(begin, PageBytes, PROT_READ | PROT_WRITE) == 0, "cannot commit sparse guest page");
#endif
                for (std::size_t offset = 0; offset < PageBytes; offset += sizeof(std::uint32_t)) std::memcpy(begin + offset, &Patterns[page], sizeof(std::uint32_t));
            }
        } catch (...) {
            release();
            throw;
        }
    }

    ~GuestPages() { release(); }
    GuestPages(const GuestPages&) = delete;
    GuestPages& operator=(const GuestPages&) = delete;
    std::uint64_t Address() const { return reinterpret_cast<std::uint64_t>(memory); }

private:
    void release() noexcept {
#ifdef _WIN32
        if (memory != nullptr) VirtualFree(memory, 0, MEM_RELEASE);
#else
        if (memory != nullptr) munmap(memory, ReservedBytes);
#endif
        memory = nullptr;
    }

    std::byte* memory = nullptr;
};

void Emit(std::vector<std::uint32_t>& words, spv::Op opcode, std::initializer_list<std::uint32_t> operands) {
    words.push_back(static_cast<std::uint32_t>((operands.size() + 1u) << 16u) | opcode);
    words.insert(words.end(), operands);
}

std::vector<std::uint32_t> CopyModule() {
    enum : std::uint32_t { Void = 1, FunctionType, Uint, Uint3, Words, Block, BlockPointer, Source, Destination, UintPointer, InputPointer, Invocation, Zero, Main, Label, InvocationValue, Index, SourcePointer, Value, DestinationPointer, Bound };
    std::vector<std::uint32_t> words{spv::MagicNumber, 0x00010300u, 0u, Bound, 0u};
    Emit(words, spv::OpCapability, {spv::CapabilityShader});
    Emit(words, spv::OpMemoryModel, {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
    Emit(words, spv::OpEntryPoint, {spv::ExecutionModelGLCompute, Main, 0x6e69616du, 0u, Invocation});
    Emit(words, spv::OpExecutionMode, {Main, spv::ExecutionModeLocalSize, WorkgroupSize, 1, 1});
    Emit(words, spv::OpDecorate, {Invocation, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId});
    Emit(words, spv::OpDecorate, {Words, spv::DecorationArrayStride, 4});
    Emit(words, spv::OpDecorate, {Block, spv::DecorationBlock});
    Emit(words, spv::OpMemberDecorate, {Block, 0, spv::DecorationOffset, 0});
    Emit(words, spv::OpDecorate, {Source, spv::DecorationDescriptorSet, 0});
    Emit(words, spv::OpDecorate, {Source, spv::DecorationBinding, 0});
    Emit(words, spv::OpDecorate, {Destination, spv::DecorationDescriptorSet, 0});
    Emit(words, spv::OpDecorate, {Destination, spv::DecorationBinding, 1});
    Emit(words, spv::OpTypeVoid, {Void});
    Emit(words, spv::OpTypeFunction, {FunctionType, Void});
    Emit(words, spv::OpTypeInt, {Uint, 32, 0});
    Emit(words, spv::OpTypeVector, {Uint3, Uint, 3});
    Emit(words, spv::OpTypeRuntimeArray, {Words, Uint});
    Emit(words, spv::OpTypeStruct, {Block, Words});
    Emit(words, spv::OpTypePointer, {BlockPointer, spv::StorageClassStorageBuffer, Block});
    Emit(words, spv::OpVariable, {BlockPointer, Source, spv::StorageClassStorageBuffer});
    Emit(words, spv::OpVariable, {BlockPointer, Destination, spv::StorageClassStorageBuffer});
    Emit(words, spv::OpTypePointer, {UintPointer, spv::StorageClassStorageBuffer, Uint});
    Emit(words, spv::OpTypePointer, {InputPointer, spv::StorageClassInput, Uint3});
    Emit(words, spv::OpVariable, {InputPointer, Invocation, spv::StorageClassInput});
    Emit(words, spv::OpConstant, {Uint, Zero, 0});
    Emit(words, spv::OpFunction, {Void, Main, 0, FunctionType});
    Emit(words, spv::OpLabel, {Label});
    Emit(words, spv::OpLoad, {Uint3, InvocationValue, Invocation});
    Emit(words, spv::OpCompositeExtract, {Uint, Index, InvocationValue, 0});
    Emit(words, spv::OpAccessChain, {UintPointer, SourcePointer, Source, Zero, Index});
    Emit(words, spv::OpLoad, {Uint, Value, SourcePointer});
    Emit(words, spv::OpAccessChain, {UintPointer, DestinationPointer, Destination, Zero, Index});
    Emit(words, spv::OpStore, {DestinationPointer, Value});
    Emit(words, spv::OpReturn, {});
    Emit(words, spv::OpFunctionEnd, {});
    return words;
}

class CopyPipeline {
public:
    explicit CopyPipeline(const Context& context) : context(context) {
        try {
            const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}}};
            VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setLayoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
            setLayoutInfo.pBindings = bindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setLayoutInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
            const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &descriptorPool), "vkCreateDescriptorPool");
            VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocateInfo.descriptorPool = descriptorPool;
            allocateInfo.descriptorSetCount = 1;
            allocateInfo.pSetLayouts = &setLayout;
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocateInfo, &set), "vkAllocateDescriptorSets");
            const auto module = CopyModule();
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = module.size() * sizeof(std::uint32_t);
            moduleInfo.pCode = module.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &shader), "vkCreateShaderModule");
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &setLayout;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            pipelineInfo.stage.module = shader;
            pipelineInfo.stage.pName = "main";
            pipelineInfo.layout = pipelineLayout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        } catch (...) {
            release();
            throw;
        }
    }

    ~CopyPipeline() { release(); }
    CopyPipeline(const CopyPipeline&) = delete;
    CopyPipeline& operator=(const CopyPipeline&) = delete;

    void Copy(const VkDescriptorBufferInfo& source, Buffer& destination) const {
        Require(source.range == BufferBytes && destination.Bytes().size() == BufferBytes, "sparse buffer holes GPU copy has an unexpected extent");
        const std::array<VkDescriptorBufferInfo, 2> infos{{source, {destination.Handle(), 0, BufferBytes}}};
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
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        CommandBatch batch(context);
        RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(batch.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(batch.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(batch.Handle(), static_cast<std::uint32_t>(BufferBytes / sizeof(std::uint32_t) / WorkgroupSize), 1, 1);
        RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        batch.SubmitAndWait();
        destination.Invalidate();
    }

private:
    void release() noexcept {
        if (pipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (pipelineLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
        if (shader != VK_NULL_HANDLE) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, shader, nullptr);
        if (descriptorPool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, descriptorPool, nullptr);
        if (setLayout != VK_NULL_HANDLE) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }

    const Context& context;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkShaderModule shader = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

void Run(const Context& context, const CopyPipeline& pipeline, bool writable, const std::string& shape) {
    const auto label = std::string("sparse buffer holes GPU: ") + (writable ? "writable " : "readable ") + shape;
    const bool committed = shape == "committed";
    const auto requested = shape == "shortened" ? ReservedBytes : BufferBytes;
    GuestPages pages(committed);
    const auto ranges = AgcDriver::GuestMemory::DescribeCommitted(pages.Address(), requested);
    Require(ranges.whole == committed, label + ": unexpected guest commitment");
    if (!committed) {
        Require(ranges.ranges == std::vector<std::pair<std::uint64_t, std::uint64_t>>{{pages.Address(), pages.Address() + PageBytes}, {pages.Address() + 2 * PageBytes, pages.Address() + BufferBytes}}, label + ": guest hole is not inaccessible");
    }
    Buffer readback(context, BufferBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    std::memset(readback.Bytes().data(), 0xcc, BufferBytes);
    VkBuffer primed = VK_NULL_HANDLE;
    {
        Buffer earlier(context, BufferBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        primed = earlier.Handle();
        std::memset(earlier.Bytes().data(), 0xa5, BufferBytes);
    }
    GuestBufferMemory memory(context);
    if (writable) memory.AddWritable(pages.Address(), requested);
    else memory.AddReadable(pages.Address(), requested);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        memory.Upload(false);
    }
    std::uint32_t adjustment = 0;
    const auto descriptor = memory.Descriptor(pages.Address(), requested, adjustment);
    Require(descriptor.buffer == primed && descriptor.offset == 0 && descriptor.range == BufferBytes && adjustment == 0, label + ": the upload did not reuse the primed buffer at the expected extent");
    std::cout << label << " reused primed buffer; requested=" << requested << "; allocated=" << descriptor.range << '\n';
    pipeline.Copy(descriptor, readback);
    const auto bytes = readback.Bytes();
    std::size_t mismatches = 0;
    std::size_t staleWords = 0;
    for (std::size_t offset = 0; offset < BufferBytes; offset += sizeof(std::uint32_t)) {
        std::uint32_t actual = 0;
        std::memcpy(&actual, bytes.data() + offset, sizeof(actual));
        const auto page = offset / PageBytes;
        if (page == 1 && !committed) {
            mismatches += actual != 0;
            staleWords += actual == 0xa5a5a5a5u;
        } else {
            Require(actual == Patterns[page], label + ": GPU readback changed backed guest bytes");
        }
    }
    std::cout << label << " readback checked; words=" << BufferBytes / sizeof(std::uint32_t) << "; nonzeroHoleWords=" << mismatches << "; staleHoleWords=" << staleWords << '\n';
    Require(mismatches == 0, label + ": GPU readback exposed stale pooled bytes in guest holes");
    std::cout << label << " passed\n";
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
        const auto& context = device->GetContext();
        Require(context.limits.maxStorageBufferRange >= BufferBytes && context.limits.maxComputeWorkGroupSize[0] >= WorkgroupSize && context.limits.maxComputeWorkGroupInvocations >= WorkgroupSize && context.limits.maxComputeWorkGroupCount[0] >= BufferBytes / sizeof(std::uint32_t) / WorkgroupSize, "Vulkan limits cannot support the bounded sparse buffer readback");
        CopyPipeline pipeline(context);
        bool passed = true;
        for (const bool writable : {false, true}) {
            for (const auto* shape : {"interior", "shortened", "committed"}) {
                try {
                    Run(context, pipeline, writable, shape);
                } catch (const std::exception& error) {
                    if (std::string(error.what()).find(": GPU readback exposed stale pooled bytes in guest holes") == std::string::npos) throw;
                    std::cerr << error.what() << '\n';
                    passed = false;
                }
            }
        }
        if (!passed) return 1;
        std::puts("Sparse buffer hole GPU readback tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
