#include <Testing/Test.hpp>
#include "BdaShader.hpp"
#include "BdaAbi.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

namespace {

using namespace AgcDriver::Graphics;
namespace Abi = ShaderRecompiler::BdaAbi;
using Testing::Case;
using Testing::Require;

constexpr auto AddressableUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
constexpr std::uint64_t Guest = 0x7fff12340001ULL;
constexpr std::uint64_t SpanGuest = 0x7fff56780000ULL;
constexpr std::uint32_t Sentinel = 0xdeadbeef;
constexpr std::array<std::uint32_t, 4> Sentinels{Sentinel, Sentinel, Sentinel, Sentinel};
constexpr Abi::FaultReason NoFault = static_cast<Abi::FaultReason>(0);

class Pipeline {
public:
    Pipeline(const Context& context, std::span<const std::uint32_t> code, const std::array<Buffer*, 3>& buffers) : context(context) {
        try {
            std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
            for (std::uint32_t i = 0; i < bindings.size(); ++i) bindings[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setInfo.bindingCount = bindings.size();
            setInfo.pBindings = bindings.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &setInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
            const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &size;
            Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1, &setLayout};
            Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &set), "vkAllocateDescriptorSets");
            for (std::uint32_t i = 0; i < buffers.size(); ++i) {
                const VkDescriptorBufferInfo buffer{buffers[i]->Handle(), 0, buffers[i]->Bytes().size()};
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = set;
                write.dstBinding = i;
                write.descriptorCount = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.pBufferInfo = &buffer;
                context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 1, &write, 0, nullptr);
            }
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &setLayout;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layoutInfo, nullptr, &layout), "vkCreatePipelineLayout");
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = code.size_bytes();
            moduleInfo.pCode = code.data();
            Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
            pipelineInfo.layout = layout;
            Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        } catch (...) { release(); throw; }
    }

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    ~Pipeline() { release(); }

    void Run(std::uint32_t groups) {
        CommandBatch batch(context);
        const auto commands = batch.Handle();
        VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &upload, 0, nullptr, 0, nullptr);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, groups, 1, 1);
        VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
        batch.SubmitAndWait();
    }

private:
    void release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        if (layout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, layout, nullptr);
        if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        if (setLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }

    const Context& context;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

class BdaExecution {
public:
    BdaExecution() : context(GpuContext()) {
        first.Bytes()[0] = std::byte{0x11};
        first.Bytes()[1] = std::byte{0x22};
        first.Bytes()[2] = std::byte{0x33};
        second.Bytes()[0] = std::byte{0x44};
        second.Bytes()[1] = std::byte{0x55};
        for (std::uint32_t dword = 0; dword < 8u; ++dword) {
            const std::uint32_t value = At(dword);
            std::memcpy(wide.Bytes().data() + dword * 4u, &value, sizeof(value));
        }
    }

    static std::uint32_t At(std::uint32_t dword) {
        return 0xa0b0c000u + dword;
    }

    void RemoveReadPermission() {
        ranges[0].permissions = 0;
    }

    void Read(std::uint64_t address, std::uint32_t bits, std::uint32_t expected, Abi::FaultReason reason, std::uint32_t count = 2, std::uint32_t groups = 1, std::int64_t offset = 0) {
        const auto label = "read of " + std::to_string(bits) + " bits at " + Hex(address) + " offset " + std::to_string(offset) + " with " + std::to_string(count) + " table ranges";
        const Abi::Header header{Abi::Version, count, sizeof(Abi::Range), 0};
        std::memcpy(table.Bytes().data(), &header, sizeof(header));
        std::memcpy(table.Bytes().data() + sizeof(header), ranges.data(), sizeof(ranges));
        std::memset(fault.Bytes().data(), 0, fault.Bytes().size());
        std::memcpy(output.Bytes().data(), &Sentinel, sizeof(Sentinel));
        Pipeline pipeline(context, MakeBdaTestShader(address, bits, offset), {&table, &fault, &output});
        pipeline.Run(groups);
        Abi::Fault report{};
        std::uint32_t result = 0;
        std::memcpy(&report, fault.Bytes().data(), sizeof(report));
        std::memcpy(&result, output.Bytes().data(), sizeof(result));
        if (reason == NoFault) {
            Require(report.state == Abi::FaultState::Empty && result == expected, label + ": BDA GPU read produced incorrect data or a fault");
        } else {
            Require(report.state == Abi::FaultState::Ready && report.reason == reason && report.instruction == 0x1234, label + ": BDA GPU fault was not published correctly");
            Require(result == Sentinel, label + ": faulting BDA shader continued to output a substitute value");
        }
    }

    void ReadDwords(std::uint64_t address, std::uint32_t dwords, bool coherent, bool stops, std::uint32_t value, bool faults) {
        const auto label = "read of " + std::to_string(dwords) + " dwords at " + Hex(address) + (coherent ? " coherent" : "") + (stops ? " stopping" : " continuing");
        const Abi::Header header{Abi::Version, 2, sizeof(Abi::Range), 0};
        std::memcpy(table.Bytes().data(), &header, sizeof(header));
        std::memcpy(table.Bytes().data() + sizeof(header), ranges.data(), sizeof(ranges));
        std::memset(fault.Bytes().data(), 0, fault.Bytes().size());
        std::memcpy(words.Bytes().data(), Sentinels.data(), sizeof(Sentinels));
        Pipeline pipeline(context, MakeBdaDwordReadTestShader(address, dwords, coherent, stops), {&table, &fault, &words});
        pipeline.Run(1);
        Abi::Fault report{};
        std::array<std::uint32_t, 4> result{};
        std::memcpy(&report, fault.Bytes().data(), sizeof(report));
        std::memcpy(result.data(), words.Bytes().data(), sizeof(result));
        auto expected = Sentinels;
        if (!faults || !stops) {
            expected[0] = value;
            for (std::uint32_t dword = 1; dword < dwords; ++dword) expected[dword] = 0;
        }
        if (faults) {
            Require(report.state == Abi::FaultState::Ready && report.reason == Abi::FaultReason::Unmapped && report.address == Guest + 5 && report.bytes == 1 && report.instruction == 0x1234, label + ": BDA dword read did not publish its first fault");
        } else {
            Require(report.state == Abi::FaultState::Empty, label + ": mapped BDA dword read published a fault");
        }
        Require(result == expected, label + (faults && stops ? ": faulting BDA dword read continued to output a substitute value" : ": BDA dword read produced incorrect data"));
    }

    void ReadSpan(std::uint64_t address, std::int32_t offset, std::uint32_t extracted, bool coherent, bool stops, std::array<std::uint32_t, 4> values, Abi::FaultReason reason = NoFault, std::uint64_t faultAddress = 0, std::uint32_t faultBytes = 0) {
        const auto label = "span read at " + Hex(address) + " offset " + std::to_string(offset) + " dwords mask " + std::to_string(extracted) + (coherent ? " coherent" : "") + (stops ? " stopping" : " continuing");
        const std::array<Abi::Range, 3> spanRanges{{ranges[0], ranges[1], {SpanGuest, SpanGuest + 32, wide.DeviceAddress(), Abi::Read, 0}}};
        const Abi::Header header{Abi::Version, 3, sizeof(Abi::Range), 0};
        std::memcpy(spanTable.Bytes().data(), &header, sizeof(header));
        std::memcpy(spanTable.Bytes().data() + sizeof(header), spanRanges.data(), sizeof(spanRanges));
        std::memset(fault.Bytes().data(), 0, fault.Bytes().size());
        std::memcpy(words.Bytes().data(), Sentinels.data(), sizeof(Sentinels));
        Pipeline pipeline(context, MakeBdaSpanReadTestShader(address, static_cast<std::uint32_t>(offset), extracted, coherent, stops), {&spanTable, &fault, &words});
        pipeline.Run(1);
        Abi::Fault report{};
        std::array<std::uint32_t, 4> result{};
        std::memcpy(&report, fault.Bytes().data(), sizeof(report));
        std::memcpy(result.data(), words.Bytes().data(), sizeof(result));
        const bool faults = reason != NoFault;
        if (faults) {
            Require(report.state == Abi::FaultState::Ready && report.reason == reason && report.address == faultAddress && report.bytes == faultBytes && report.instruction == 0x1234, label + ": BDA span read did not publish its first fault");
        } else {
            Require(report.state == Abi::FaultState::Empty, label + ": mapped BDA span read published a fault");
        }
        Require(result == (faults && stops ? Sentinels : values), label + (faults && stops ? ": faulting BDA span read continued to output a substitute value" : ": BDA span read produced incorrect data"));
    }

private:
    static const Context& GpuContext() {
        const auto& device = SharedBdaTestDevice();
        if (device.runsOnCpu) Testing::Skip("CPU Vulkan device: BDA execution not tested");
        return device.context;
    }

    const Context& context;
    Buffer first{context, 3, AddressableUsage};
    Buffer second{context, 2, AddressableUsage};
    std::array<Abi::Range, 2> ranges{{{Guest, Guest + 3, first.DeviceAddress(), Abi::Read, 0}, {Guest + 3, Guest + 5, second.DeviceAddress(), Abi::Read, 0}}};
    Buffer table{context, sizeof(Abi::Header) + sizeof(ranges), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    Buffer fault{context, sizeof(Abi::Fault), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    Buffer output{context, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    Buffer words{context, 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    Buffer wide{context, 32, AddressableUsage};
    Buffer spanTable{context, sizeof(Abi::Header) + 3 * sizeof(Abi::Range), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
};

const Case mappedReads{"EmitBdaRead_MappedRanges_ReadsBytesAcrossRanges", [] {
    BdaExecution execution;
    execution.Read(Guest, 8, 0x11, NoFault);
    execution.Read(Guest + 1, 16, 0x3322, NoFault);
    execution.Read(Guest + 1, 32, 0x55443322, NoFault);
}};

const Case unmappedReads{"EmitBdaRead_UnmappedAddress_PublishesUnmappedFault", [] {
    BdaExecution execution;
    execution.Read(Guest + 5, 8, 0, Abi::FaultReason::Unmapped, 2, 64);
    execution.Read(Guest - 1, 8, 0, Abi::FaultReason::Unmapped);
}};

const Case overflowingReads{"EmitBdaRead_AddressOverflow_PublishesOverflowFault", [] {
    BdaExecution execution;
    execution.Read(std::numeric_limits<std::uint64_t>::max() - 1, 32, 0, Abi::FaultReason::Overflow);
    execution.Read(std::numeric_limits<std::uint64_t>::max() - 2, 8, 0, Abi::FaultReason::Overflow, 2, 1, 4);
    execution.Read(1, 8, 0, Abi::FaultReason::Overflow, 2, 1, -4);
}};

const Case invalidTable{"EmitBdaRead_TableCountPastItsRanges_PublishesInvalidTableFault", [] {
    BdaExecution execution;
    execution.Read(Guest, 8, 0, Abi::FaultReason::InvalidTable, 3);
}};

const Case withoutPermission{"EmitBdaRead_RangeWithoutReadPermission_PublishesPermissionFault", [] {
    BdaExecution execution;
    execution.RemoveReadPermission();
    execution.Read(Guest, 8, 0, Abi::FaultReason::Permission);
}};

const Case dwordReads{"EmitBdaDwordReads_MappedOrUnmappedTail_ReadsDataOrPublishesFirstFault", [] {
    BdaExecution execution;
    for (const bool stops : {true, false}) {
        for (const bool coherent : {false, true}) {
            execution.ReadDwords(Guest + 1, 1, coherent, stops, 0x55443322, false);
            execution.ReadDwords(Guest + 3, 1, coherent, stops, 0x5544, true);
            execution.ReadDwords(Guest + 1, 4, coherent, stops, 0x55443322, true);
        }
    }
}};

const Case mappedSpans{"EmitBdaDwordReads_MappedSpan_ReadsOnlyExtractedDwords", [] {
    BdaExecution execution;
    const auto at = BdaExecution::At;
    for (const bool stops : {true, false}) {
        for (const bool coherent : {false, true}) {
            execution.ReadSpan(SpanGuest + 16, -12, 0b1010u, coherent, stops, {0, at(2), 0, at(4)});
            execution.ReadSpan(SpanGuest, 4, 0b0101u, coherent, stops, {at(1), 0, at(3), 0});
            execution.ReadSpan(SpanGuest + 24, 0, 0b0011u, coherent, stops, {at(6), at(7), 0, 0});
            execution.ReadSpan(SpanGuest + 20, 8, 0b0001u, coherent, stops, {at(7), 0, 0, 0});
        }
    }
}};

const Case faultingSpans{"EmitBdaDwordReads_SpanPastMappingOrOverflowing_PublishesFirstFault", [] {
    BdaExecution execution;
    const auto at = BdaExecution::At;
    for (const bool stops : {true, false}) {
        for (const bool coherent : {false, true}) {
            execution.ReadSpan(SpanGuest + 28, 0, 0b0101u, coherent, stops, {at(7), 0, 0, 0}, Abi::FaultReason::Unmapped, SpanGuest + 36, 1);
            execution.ReadSpan(Guest, 1, 0b1001u, coherent, stops, {0x55443322, 0, 0, 0}, Abi::FaultReason::Unmapped, Guest + 13, 1);
            execution.ReadSpan(4, -8, 0b0001u, coherent, stops, {0, 0, 0, 0}, Abi::FaultReason::Overflow, 4, 0);
        }
    }
}};

} // namespace
