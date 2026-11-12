#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace AgcDriver::Graphics;

#define TEST_SCALAR_OPS(X) \
    X(BeginQuery) \
    X(BindIndexBuffer) \
    X(BindPipeline) \
    X(CopyQueryPoolResults) \
    X(Dispatch) \
    X(DispatchIndirect) \
    X(Draw) \
    X(DrawIndexed) \
    X(DrawIndexedIndirect) \
    X(DrawIndexedIndirectCountKHR) \
    X(DrawIndirect) \
    X(DrawIndirectCountKHR) \
    X(DrawMeshTasksEXT) \
    X(DrawMeshTasksIndirectEXT) \
    X(EndQuery) \
    X(EndRenderPass) \
    X(EndRenderingKHR) \
    X(FillBuffer) \
    X(ResetQueryPool) \
    X(SetCheckpointNV) \
    X(SetCullModeEXT) \
    X(SetDepthBias) \
    X(SetDepthBounds) \
    X(SetDepthBoundsTestEnableEXT) \
    X(SetDepthCompareOpEXT) \
    X(SetDepthTestEnableEXT) \
    X(SetDepthWriteEnableEXT) \
    X(SetFrontFaceEXT) \
    X(SetStencilCompareMask) \
    X(SetStencilOpEXT) \
    X(SetStencilReference) \
    X(SetStencilTestEnableEXT) \
    X(SetStencilWriteMask) \
    X(WriteBufferMarkerAMD) \
    X(WriteTimestamp)

#define TEST_POINTER_OPS(X) \
    X(BeginRenderPass) \
    X(BeginRenderingKHR) \
    X(BindDescriptorSets) \
    X(BindVertexBuffers) \
    X(BlitImage) \
    X(ClearColorImage) \
    X(ClearDepthStencilImage) \
    X(CopyBuffer) \
    X(CopyBufferToImage) \
    X(CopyImage) \
    X(CopyImageToBuffer) \
    X(PipelineBarrier) \
    X(PushConstants) \
    X(PushDescriptorSetKHR) \
    X(ResolveImage) \
    X(SetScissor) \
    X(SetViewport) \
    X(UpdateBuffer)

#define TEST_ALL_OPS(X) TEST_SCALAR_OPS(X) TEST_POINTER_OPS(X)

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

void ExpectThrows(const std::function<void()>& action, std::string_view wanted, const std::string& what) {
    try {
        action();
    } catch (const std::exception& error) {
        Expect(std::string_view(error.what()).find(wanted) != std::string_view::npos, what + ": the error '" + error.what() + "' does not name " + std::string(wanted));
        return;
    }
    Expect(false, what + ": nothing was thrown");
}

template<typename T>
T fake(std::uintptr_t value) {
    return reinterpret_cast<T>(value);
}

std::vector<std::string> calls;

#define TEST_NAME(name) constexpr char name##Name[] = "vkCmd" #name;
TEST_ALL_OPS(TEST_NAME)
#undef TEST_NAME

void hex(std::string& out, const void* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    const auto* bytes = static_cast<const unsigned char*>(data);
    out += '[';
    for (std::size_t index = 0; index < size; ++index) {
        out += digits[bytes[index] >> 4u];
        out += digits[bytes[index] & 15u];
    }
    out += ']';
}

template<typename T>
void bytes(std::string& out, const T& value) {
    hex(out, &value, sizeof(T));
}

template<typename T>
void items(std::string& out, const T* values, std::uint64_t count) {
    if (count == 0) out += "[]";
    else if (values == nullptr) out += "null";
    else hex(out, values, sizeof(T) * count);
}

template<typename T, typename... TFields>
void masked(std::string& out, const T& value, TFields T::*... fields) {
    T copy;
    std::memcpy(static_cast<void*>(&copy), &value, sizeof(T));
    ((copy.*fields = nullptr), ...);
    hex(out, &copy, sizeof(T));
}

std::string begin(const char* name, VkCommandBuffer commands) {
    std::string line = name;
    line += '@';
    bytes(line, commands);
    return line;
}

void chain(std::string& out, const void* next);

void put(std::string& out, const VkSampleLocationsInfoEXT& info) {
    masked(out, info, &VkSampleLocationsInfoEXT::pNext, &VkSampleLocationsInfoEXT::pSampleLocations);
    chain(out, info.pNext);
    items(out, info.pSampleLocations, info.sampleLocationsCount);
}

void put(std::string& out, const VkRenderPassSampleLocationsBeginInfoEXT& info) {
    masked(out, info, &VkRenderPassSampleLocationsBeginInfoEXT::pNext, &VkRenderPassSampleLocationsBeginInfoEXT::pAttachmentInitialSampleLocations, &VkRenderPassSampleLocationsBeginInfoEXT::pPostSubpassSampleLocations);
    chain(out, info.pNext);
    for (std::uint32_t index = 0; index < info.attachmentInitialSampleLocationsCount; ++index) {
        bytes(out, info.pAttachmentInitialSampleLocations[index].attachmentIndex);
        put(out, info.pAttachmentInitialSampleLocations[index].sampleLocationsInfo);
    }
    for (std::uint32_t index = 0; index < info.postSubpassSampleLocationsCount; ++index) {
        bytes(out, info.pPostSubpassSampleLocations[index].subpassIndex);
        put(out, info.pPostSubpassSampleLocations[index].sampleLocationsInfo);
    }
}

void chain(std::string& out, const void* next) {
    if (next == nullptr) {
        out += "end";
        return;
    }
    const auto type = static_cast<const VkBaseInStructure*>(next)->sType;
    if (type == VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT) {
        out += "locations";
        put(out, *static_cast<const VkSampleLocationsInfoEXT*>(next));
    } else if (type == VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT) {
        out += "pass-locations";
        put(out, *static_cast<const VkRenderPassSampleLocationsBeginInfoEXT*>(next));
    } else {
        out += "unknown";
        bytes(out, type);
    }
}

template<typename T>
void plain(std::string& out, const T& value) {
    masked(out, value, &T::pNext);
    chain(out, value.pNext);
}

void put(std::string& out, const VkMemoryBarrier& barrier) { plain(out, barrier); }
void put(std::string& out, const VkBufferMemoryBarrier& barrier) { plain(out, barrier); }
void put(std::string& out, const VkImageMemoryBarrier& barrier) { plain(out, barrier); }
void put(std::string& out, const VkRenderingAttachmentInfo& attachment) { plain(out, attachment); }

void put(std::string& out, const VkRenderPassBeginInfo& info) {
    masked(out, info, &VkRenderPassBeginInfo::pNext, &VkRenderPassBeginInfo::pClearValues);
    chain(out, info.pNext);
    items(out, info.pClearValues, info.clearValueCount);
}

void put(std::string& out, const VkRenderingInfo& info) {
    masked(out, info, &VkRenderingInfo::pNext, &VkRenderingInfo::pColorAttachments, &VkRenderingInfo::pDepthAttachment, &VkRenderingInfo::pStencilAttachment);
    chain(out, info.pNext);
    for (std::uint32_t index = 0; info.pColorAttachments != nullptr && index < info.colorAttachmentCount; ++index) put(out, info.pColorAttachments[index]);
    if (info.pDepthAttachment != nullptr) {
        out += "depth";
        put(out, *info.pDepthAttachment);
    }
    if (info.pStencilAttachment != nullptr && info.pStencilAttachment == info.pDepthAttachment) {
        out += "stencil-is-depth";
    } else if (info.pStencilAttachment != nullptr) {
        out += "stencil";
        put(out, *info.pStencilAttachment);
    }
}

void put(std::string& out, const VkWriteDescriptorSet& write) {
    masked(out, write, &VkWriteDescriptorSet::pNext, &VkWriteDescriptorSet::pImageInfo, &VkWriteDescriptorSet::pBufferInfo, &VkWriteDescriptorSet::pTexelBufferView);
    chain(out, write.pNext);
    switch (write.descriptorType) {
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        items(out, write.pImageInfo, write.descriptorCount);
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        items(out, write.pBufferInfo, write.descriptorCount);
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        items(out, write.pTexelBufferView, write.descriptorCount);
        break;
    default:
        out += "unexpected descriptor type";
    }
}

template<typename T>
void structs(std::string& out, const T* values, std::uint64_t count) {
    if (count == 0) {
        out += "[]";
        return;
    }
    if (values == nullptr) {
        out += "null";
        return;
    }
    for (std::uint64_t index = 0; index < count; ++index) put(out, values[index]);
}

void record(std::string line) {
    calls.push_back(std::move(line));
}

template<const char* Name, int Variant, typename TFunction>
struct Scalar;

template<const char* Name, int Variant, typename... TArguments>
struct Scalar<Name, Variant, void (VKAPI_PTR*)(VkCommandBuffer, TArguments...)> {
    static VKAPI_ATTR void VKAPI_CALL Call(VkCommandBuffer commands, TArguments... arguments) {
        auto line = (Variant == 0 ? std::string() : "variant" + std::to_string(Variant) + ":") + begin(Name, commands);
        (bytes(line, arguments), ...);
        record(std::move(line));
    }
};

VKAPI_ATTR void VKAPI_CALL stubBeginRenderPass(VkCommandBuffer commands, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
    auto line = begin(BeginRenderPassName, commands);
    structs(line, info, 1);
    bytes(line, contents);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubBeginRenderingKHR(VkCommandBuffer commands, const VkRenderingInfo* info) {
    auto line = begin(BeginRenderingKHRName, commands);
    structs(line, info, 1);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubBindDescriptorSets(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, std::uint32_t firstSet, std::uint32_t setCount, const VkDescriptorSet* sets, std::uint32_t offsetCount, const std::uint32_t* offsets) {
    auto line = begin(BindDescriptorSetsName, commands);
    bytes(line, bindPoint);
    bytes(line, layout);
    bytes(line, firstSet);
    bytes(line, setCount);
    items(line, sets, setCount);
    bytes(line, offsetCount);
    items(line, offsets, offsetCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubBindVertexBuffers(VkCommandBuffer commands, std::uint32_t firstBinding, std::uint32_t bindingCount, const VkBuffer* buffers, const VkDeviceSize* offsets) {
    auto line = begin(BindVertexBuffersName, commands);
    bytes(line, firstBinding);
    bytes(line, bindingCount);
    items(line, buffers, bindingCount);
    items(line, offsets, bindingCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubBlitImage(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t regionCount, const VkImageBlit* regions, VkFilter filter) {
    auto line = begin(BlitImageName, commands);
    bytes(line, source);
    bytes(line, sourceLayout);
    bytes(line, destination);
    bytes(line, destinationLayout);
    bytes(line, regionCount);
    items(line, regions, regionCount);
    bytes(line, filter);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubClearColorImage(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearColorValue* color, std::uint32_t rangeCount, const VkImageSubresourceRange* ranges) {
    auto line = begin(ClearColorImageName, commands);
    bytes(line, image);
    bytes(line, layout);
    items(line, color, 1);
    bytes(line, rangeCount);
    items(line, ranges, rangeCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubClearDepthStencilImage(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearDepthStencilValue* value, std::uint32_t rangeCount, const VkImageSubresourceRange* ranges) {
    auto line = begin(ClearDepthStencilImageName, commands);
    bytes(line, image);
    bytes(line, layout);
    items(line, value, 1);
    bytes(line, rangeCount);
    items(line, ranges, rangeCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubCopyBuffer(VkCommandBuffer commands, VkBuffer source, VkBuffer destination, std::uint32_t regionCount, const VkBufferCopy* regions) {
    auto line = begin(CopyBufferName, commands);
    bytes(line, source);
    bytes(line, destination);
    bytes(line, regionCount);
    items(line, regions, regionCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubCopyBufferToImage(VkCommandBuffer commands, VkBuffer source, VkImage destination, VkImageLayout layout, std::uint32_t regionCount, const VkBufferImageCopy* regions) {
    auto line = begin(CopyBufferToImageName, commands);
    bytes(line, source);
    bytes(line, destination);
    bytes(line, layout);
    bytes(line, regionCount);
    items(line, regions, regionCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubCopyImage(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t regionCount, const VkImageCopy* regions) {
    auto line = begin(CopyImageName, commands);
    bytes(line, source);
    bytes(line, sourceLayout);
    bytes(line, destination);
    bytes(line, destinationLayout);
    bytes(line, regionCount);
    items(line, regions, regionCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubCopyImageToBuffer(VkCommandBuffer commands, VkImage source, VkImageLayout layout, VkBuffer destination, std::uint32_t regionCount, const VkBufferImageCopy* regions) {
    auto line = begin(CopyImageToBufferName, commands);
    bytes(line, source);
    bytes(line, layout);
    bytes(line, destination);
    bytes(line, regionCount);
    items(line, regions, regionCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubPipelineBarrier(VkCommandBuffer commands, VkPipelineStageFlags sourceStages, VkPipelineStageFlags destinationStages, VkDependencyFlags dependencies, std::uint32_t memoryCount, const VkMemoryBarrier* memory, std::uint32_t bufferCount, const VkBufferMemoryBarrier* buffers, std::uint32_t imageCount, const VkImageMemoryBarrier* images) {
    auto line = begin(PipelineBarrierName, commands);
    bytes(line, sourceStages);
    bytes(line, destinationStages);
    bytes(line, dependencies);
    bytes(line, memoryCount);
    structs(line, memory, memoryCount);
    bytes(line, bufferCount);
    structs(line, buffers, bufferCount);
    bytes(line, imageCount);
    structs(line, images, imageCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubPushConstants(VkCommandBuffer commands, VkPipelineLayout layout, VkShaderStageFlags stages, std::uint32_t offset, std::uint32_t size, const void* values) {
    auto line = begin(PushConstantsName, commands);
    bytes(line, layout);
    bytes(line, stages);
    bytes(line, offset);
    bytes(line, size);
    items(line, static_cast<const unsigned char*>(values), size);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubPushDescriptorSetKHR(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, std::uint32_t set, std::uint32_t writeCount, const VkWriteDescriptorSet* writes) {
    auto line = begin(PushDescriptorSetKHRName, commands);
    bytes(line, bindPoint);
    bytes(line, layout);
    bytes(line, set);
    bytes(line, writeCount);
    structs(line, writes, writeCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubResolveImage(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t regionCount, const VkImageResolve* regions) {
    auto line = begin(ResolveImageName, commands);
    bytes(line, source);
    bytes(line, sourceLayout);
    bytes(line, destination);
    bytes(line, destinationLayout);
    bytes(line, regionCount);
    items(line, regions, regionCount);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubSetScissor(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkRect2D* scissors) {
    auto line = begin(SetScissorName, commands);
    bytes(line, first);
    bytes(line, count);
    items(line, scissors, count);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubSetViewport(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkViewport* viewports) {
    auto line = begin(SetViewportName, commands);
    bytes(line, first);
    bytes(line, count);
    items(line, viewports, count);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubUpdateBuffer(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data) {
    auto line = begin(UpdateBufferName, commands);
    bytes(line, buffer);
    bytes(line, offset);
    bytes(line, size);
    items(line, static_cast<const unsigned char*>(data), size);
    record(std::move(line));
}

VKAPI_ATTR void VKAPI_CALL stubUpdateDescriptorSets(VkDevice, std::uint32_t, const VkWriteDescriptorSet*, std::uint32_t, const VkCopyDescriptorSet*) {}

VKAPI_ATTR VkResult VKAPI_CALL stubAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*) {
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL stubGetFenceStatus(VkDevice, VkFence) {
    return VK_SUCCESS;
}

const std::map<std::string_view, PFN_vkVoidFunction>& stubTable() {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
#define TEST_SCALAR_ENTRY(name) {name##Name, reinterpret_cast<PFN_vkVoidFunction>(&Scalar<name##Name, 0, PFN_vkCmd##name>::Call)},
        TEST_SCALAR_OPS(TEST_SCALAR_ENTRY)
#undef TEST_SCALAR_ENTRY
#define TEST_POINTER_ENTRY(name) {name##Name, reinterpret_cast<PFN_vkVoidFunction>(&stub##name)},
        TEST_POINTER_OPS(TEST_POINTER_ENTRY)
#undef TEST_POINTER_ENTRY
        {"vkUpdateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(&stubUpdateDescriptorSets)},
        {"vkAllocateDescriptorSets", reinterpret_cast<PFN_vkVoidFunction>(&stubAllocateDescriptorSets)},
        {"vkGetFenceStatus", reinterpret_cast<PFN_vkVoidFunction>(&stubGetFenceStatus)},
    };
    return table;
}

int procLookups = 0;

PFN_vkVoidFunction VKAPI_CALL stubProc(VkDevice, const char* name) {
    ++procLookups;
    const auto found = stubTable().find(name);
    return found != stubTable().end() ? found->second : nullptr;
}

PFN_vkVoidFunction VKAPI_CALL variantProc(VkDevice device, const char* name) {
    if (std::string_view(name) == DrawName) return reinterpret_cast<PFN_vkVoidFunction>(&Scalar<DrawName, 1, PFN_vkCmdDraw>::Call);
    return stubProc(device, name);
}

PFN_vkVoidFunction handedThunk = nullptr;

PFN_vkVoidFunction VKAPI_CALL thunkProc(VkDevice device, const char* name) {
    if (std::string_view(name) == DrawName) return handedThunk;
    return stubProc(device, name);
}

Context stubContext(PFN_vkGetDeviceProcAddr proc = &stubProc) {
    Context context{};
    context.device = fake<VkDevice>(0x9000);
    context.queue = fake<VkQueue>(0x9010);
    context.deviceProc = RecordCommandStreams(context.device, proc);
    context.graphicsPipelineLibrary = true;
    return context;
}

struct Functions {
#define TEST_MEMBER(name) PFN_vkCmd##name cmd##name = nullptr;
    TEST_ALL_OPS(TEST_MEMBER)
#undef TEST_MEMBER
};

Functions resolve(const Context& context) {
    Functions functions;
#define TEST_RESOLVE(name) functions.cmd##name = context.Function<PFN_vkCmd##name>(name##Name);
    TEST_ALL_OPS(TEST_RESOLVE)
#undef TEST_RESOLVE
    return functions;
}

struct Data {
    std::array<VkMemoryBarrier, 2> memory;
    std::array<VkBufferMemoryBarrier, 2> buffers;
    std::array<VkImageMemoryBarrier, 2> images;
    std::array<VkSampleLocationEXT, 8> locations;
    VkSampleLocationsInfoEXT imageLocations;
    VkSampleLocationsInfoEXT passLocations;
    std::array<VkAttachmentSampleLocationsEXT, 2> attachmentLocations;
    std::array<VkSubpassSampleLocationsEXT, 1> subpassLocations;
    VkRenderPassSampleLocationsBeginInfoEXT passSampleLocations;
    std::array<VkClearValue, 2> clearValues;
    VkRenderPassBeginInfo pass;
    std::array<VkRenderingAttachmentInfo, 3> colors;
    VkRenderingAttachmentInfo depth;
    VkRenderingAttachmentInfo stencil;
    VkRenderingInfo rendering;
    VkRenderingInfo renderingSplit;
    std::array<VkDescriptorImageInfo, 2> imageInfos;
    std::array<VkDescriptorBufferInfo, 2> bufferInfos;
    std::array<VkBufferView, 2> texelViews;
    std::array<VkWriteDescriptorSet, 3> writes;
    std::array<unsigned char, 96> pushBytes;
    std::array<unsigned char, 256> updateBytes;
    std::array<VkDescriptorSet, 3> sets;
    std::array<std::uint32_t, 2> dynamicOffsets;
    std::array<VkBuffer, 3> vertexBuffers;
    std::array<VkDeviceSize, 3> vertexOffsets;
    std::array<VkImageBlit, 2> blits;
    VkClearColorValue color;
    VkClearDepthStencilValue depthStencil;
    std::array<VkImageSubresourceRange, 2> ranges;
    std::array<VkBufferCopy, 3> bufferCopies;
    std::array<VkBufferImageCopy, 2> bufferImageCopies;
    std::array<VkImageCopy, 2> imageCopies;
    std::array<VkImageResolve, 2> resolves;
    std::array<VkViewport, 2> viewports;
    std::array<VkRect2D, 2> scissors;
};

void pattern(void* target, std::size_t size, unsigned char& seed) {
    auto* bytes = static_cast<unsigned char*>(target);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = seed = static_cast<unsigned char>(seed * 37u + 11u);
}

void makeData(Data& data, std::vector<unsigned char>& large) {
    std::memset(static_cast<void*>(&data), 0, sizeof(Data));
    unsigned char seed = 5;
    const auto fill = [&](auto& value) { pattern(&value, sizeof(value), seed); };
    for (std::size_t index = 0; index < data.memory.size(); ++index) data.memory[index] = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, static_cast<VkAccessFlags>(0x11u << index), static_cast<VkAccessFlags>(0x300u + index)};
    for (std::size_t index = 0; index < data.buffers.size(); ++index) data.buffers[index] = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, fake<VkBuffer>(0x2000 + index), 64u * index, 4096u + index};
    for (std::size_t index = 0; index < data.images.size(); ++index) data.images[index] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, fake<VkImage>(0x2100 + index), {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, static_cast<std::uint32_t>(index), 1}};
    for (std::size_t index = 0; index < data.locations.size(); ++index) data.locations[index] = {0.0625f * static_cast<float>(index + 1), 1.0f - 0.0625f * static_cast<float>(index)};
    data.imageLocations = {VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, nullptr, VK_SAMPLE_COUNT_4_BIT, {1, 1}, 4, data.locations.data()};
    data.images[1].pNext = &data.imageLocations;
    data.passLocations = {VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, nullptr, VK_SAMPLE_COUNT_8_BIT, {1, 1}, 8, data.locations.data()};
    data.attachmentLocations = {{{0, data.passLocations}, {2, data.imageLocations}}};
    data.subpassLocations = {{{0, data.passLocations}}};
    data.passSampleLocations = {VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT, nullptr, 2, data.attachmentLocations.data(), 1, data.subpassLocations.data()};
    fill(data.clearValues);
    data.pass = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, &data.passSampleLocations, fake<VkRenderPass>(0x2200), fake<VkFramebuffer>(0x2210), {{3, 4}, {640, 360}}, 2, data.clearValues.data()};
    for (std::size_t index = 0; index < data.colors.size(); ++index) {
        data.colors[index] = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, nullptr, fake<VkImageView>(0x2300 + index), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_RESOLVE_MODE_NONE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, {}};
        fill(data.colors[index].clearValue);
    }
    data.depth = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, nullptr, fake<VkImageView>(0x2310), VK_IMAGE_LAYOUT_GENERAL, VK_RESOLVE_MODE_NONE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, {}};
    data.stencil = data.depth;
    data.stencil.imageView = fake<VkImageView>(0x2320);
    data.stencil.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    data.rendering = {VK_STRUCTURE_TYPE_RENDERING_INFO, nullptr, 0, {{0, 0}, {1920, 1080}}, 1, 0, 3, data.colors.data(), &data.depth, &data.depth};
    data.renderingSplit = {VK_STRUCTURE_TYPE_RENDERING_INFO, nullptr, 0, {{8, 8}, {256, 128}}, 2, 0, 2, data.colors.data() + 1, &data.depth, &data.stencil};
    for (std::size_t index = 0; index < data.imageInfos.size(); ++index) data.imageInfos[index] = {VK_NULL_HANDLE, fake<VkImageView>(0x2400 + index), VK_IMAGE_LAYOUT_GENERAL};
    for (std::size_t index = 0; index < data.bufferInfos.size(); ++index) data.bufferInfos[index] = {fake<VkBuffer>(0x2500 + index), 256u * index, 1024u};
    data.texelViews = {fake<VkBufferView>(0x2600), fake<VkBufferView>(0x2601)};
    data.writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, VK_NULL_HANDLE, 0, 0, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, data.imageInfos.data(), data.bufferInfos.data(), nullptr};
    data.writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, VK_NULL_HANDLE, 1, 0, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, data.bufferInfos.data(), data.texelViews.data()};
    data.writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, VK_NULL_HANDLE, 2, 1, 2, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, data.imageInfos.data(), nullptr, data.texelViews.data()};
    fill(data.pushBytes);
    fill(data.updateBytes);
    data.sets = {fake<VkDescriptorSet>(0x2700), fake<VkDescriptorSet>(0x2701), fake<VkDescriptorSet>(0x2702)};
    data.dynamicOffsets = {256, 512};
    data.vertexBuffers = {fake<VkBuffer>(0x2800), fake<VkBuffer>(0x2801), fake<VkBuffer>(0x2802)};
    data.vertexOffsets = {0, 48, 96};
    fill(data.blits);
    fill(data.color);
    data.depthStencil = {0.5f, 0x7f};
    fill(data.ranges);
    fill(data.bufferCopies);
    fill(data.bufferImageCopies);
    fill(data.imageCopies);
    fill(data.resolves);
    data.viewports = {{{0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f}, {16.0f, 32.0f, 64.0f, 128.0f, 0.25f, 0.75f}}};
    fill(data.scissors);
    large.resize(65536);
    pattern(large.data(), large.size(), seed);
}

void scramble(Data& data, std::vector<unsigned char>& large) {
    std::memset(static_cast<void*>(&data), 0xEE, sizeof(Data));
    std::memset(large.data(), 0xEE, large.size());
}

void recordAll(VkCommandBuffer commands, const Functions& f, const Data& d, const std::vector<unsigned char>& large) {
    f.cmdBeginQuery(commands, fake<VkQueryPool>(0x1100), 7, VK_QUERY_CONTROL_PRECISE_BIT);
    f.cmdBeginRenderPass(commands, &d.pass, VK_SUBPASS_CONTENTS_INLINE);
    f.cmdBeginRenderingKHR(commands, &d.rendering);
    f.cmdBeginRenderingKHR(commands, &d.renderingSplit);
    f.cmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, fake<VkPipelineLayout>(0x1200), 1, 3, d.sets.data(), 2, d.dynamicOffsets.data());
    f.cmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(0x1201), 0, 1, d.sets.data() + 2, 0, nullptr);
    f.cmdBindIndexBuffer(commands, fake<VkBuffer>(0x1300), 256, VK_INDEX_TYPE_UINT16);
    f.cmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, fake<VkPipeline>(0x1400));
    f.cmdBindVertexBuffers(commands, 2, 3, d.vertexBuffers.data(), d.vertexOffsets.data());
    f.cmdBlitImage(commands, fake<VkImage>(0x1500), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fake<VkImage>(0x1501), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, d.blits.data(), VK_FILTER_LINEAR);
    f.cmdClearColorImage(commands, fake<VkImage>(0x1502), VK_IMAGE_LAYOUT_GENERAL, &d.color, 2, d.ranges.data());
    f.cmdClearDepthStencilImage(commands, fake<VkImage>(0x1503), VK_IMAGE_LAYOUT_GENERAL, &d.depthStencil, 1, d.ranges.data() + 1);
    f.cmdCopyBuffer(commands, fake<VkBuffer>(0x1301), fake<VkBuffer>(0x1302), 3, d.bufferCopies.data());
    f.cmdCopyBufferToImage(commands, fake<VkBuffer>(0x1303), fake<VkImage>(0x1504), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, d.bufferImageCopies.data());
    f.cmdCopyImage(commands, fake<VkImage>(0x1505), VK_IMAGE_LAYOUT_GENERAL, fake<VkImage>(0x1506), VK_IMAGE_LAYOUT_GENERAL, 2, d.imageCopies.data());
    f.cmdCopyImageToBuffer(commands, fake<VkImage>(0x1507), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fake<VkBuffer>(0x1304), 1, d.bufferImageCopies.data() + 1);
    f.cmdCopyQueryPoolResults(commands, fake<VkQueryPool>(0x1101), 2, 6, fake<VkBuffer>(0x1305), 64, 16, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    f.cmdDispatch(commands, 7, 8, 9);
    f.cmdDispatchIndirect(commands, fake<VkBuffer>(0x1306), 48);
    f.cmdDraw(commands, 3, 1, 5, 2);
    f.cmdDrawIndexed(commands, 36, 2, 12, -4, 1);
    f.cmdDrawIndexedIndirect(commands, fake<VkBuffer>(0x1307), 32, 4, 20);
    f.cmdDrawIndexedIndirectCountKHR(commands, fake<VkBuffer>(0x1308), 16, fake<VkBuffer>(0x1309), 8, 12, 20);
    f.cmdDrawIndirect(commands, fake<VkBuffer>(0x130a), 0, 2, 16);
    f.cmdDrawIndirectCountKHR(commands, fake<VkBuffer>(0x130b), 32, fake<VkBuffer>(0x130c), 4, 7, 16);
    f.cmdDrawMeshTasksEXT(commands, 4, 5, 6);
    f.cmdDrawMeshTasksIndirectEXT(commands, fake<VkBuffer>(0x130d), 24, 3, 12);
    f.cmdEndQuery(commands, fake<VkQueryPool>(0x1100), 7);
    f.cmdEndRenderPass(commands);
    f.cmdEndRenderingKHR(commands);
    f.cmdFillBuffer(commands, fake<VkBuffer>(0x130e), 128, VK_WHOLE_SIZE, 0xdeadbeefu);
    f.cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 2, d.memory.data(), 2, d.buffers.data(), 2, d.images.data());
    f.cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, d.memory.data(), 0, nullptr, 1, d.images.data() + 1);
    f.cmdPushConstants(commands, fake<VkPipelineLayout>(0x1202), VK_SHADER_STAGE_ALL_GRAPHICS, 16, static_cast<std::uint32_t>(d.pushBytes.size()), d.pushBytes.data());
    f.cmdPushDescriptorSetKHR(commands, VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(0x1203), 1, 3, d.writes.data());
    f.cmdResetQueryPool(commands, fake<VkQueryPool>(0x1102), 4, 8);
    f.cmdResolveImage(commands, fake<VkImage>(0x1508), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fake<VkImage>(0x1509), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, d.resolves.data());
    f.cmdSetCheckpointNV(commands, fake<const void*>(0x7000));
    f.cmdSetCullModeEXT(commands, VK_CULL_MODE_BACK_BIT);
    f.cmdSetDepthBias(commands, 1.5f, 2.0f, -0.25f);
    f.cmdSetDepthBounds(commands, 0.125f, 0.75f);
    f.cmdSetDepthBoundsTestEnableEXT(commands, VK_TRUE);
    f.cmdSetDepthCompareOpEXT(commands, VK_COMPARE_OP_GREATER_OR_EQUAL);
    f.cmdSetDepthTestEnableEXT(commands, VK_TRUE);
    f.cmdSetDepthWriteEnableEXT(commands, VK_FALSE);
    f.cmdSetFrontFaceEXT(commands, VK_FRONT_FACE_CLOCKWISE);
    f.cmdSetScissor(commands, 0, 2, d.scissors.data());
    f.cmdSetStencilCompareMask(commands, VK_STENCIL_FACE_FRONT_BIT, 0x0f);
    f.cmdSetStencilOpEXT(commands, VK_STENCIL_FACE_FRONT_AND_BACK, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_COMPARE_OP_EQUAL);
    f.cmdSetStencilReference(commands, VK_STENCIL_FACE_BACK_BIT, 0x42);
    f.cmdSetStencilTestEnableEXT(commands, VK_TRUE);
    f.cmdSetStencilWriteMask(commands, VK_STENCIL_FACE_FRONT_AND_BACK, 0xf0);
    f.cmdSetViewport(commands, 1, 2, d.viewports.data());
    f.cmdUpdateBuffer(commands, fake<VkBuffer>(0x130f), 64, d.updateBytes.size(), d.updateBytes.data());
    f.cmdUpdateBuffer(commands, fake<VkBuffer>(0x1310), 0, large.size(), large.data());
    f.cmdWriteBufferMarkerAMD(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, fake<VkBuffer>(0x1311), 4, 0x1234);
    f.cmdWriteTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, fake<VkQueryPool>(0x1103), 3);
}

const VkCommandBuffer Real = fake<VkCommandBuffer>(0x51000);

std::string firstDifference(const std::vector<std::string>& expected, const std::vector<std::string>& actual) {
    for (std::size_t index = 0; index < std::min(expected.size(), actual.size()); ++index) {
        if (expected[index] != actual[index]) return "call " + std::to_string(index) + ": expected " + expected[index].substr(0, 200) + ", replayed " + actual[index].substr(0, 200);
    }
    return std::to_string(expected.size()) + " direct calls, " + std::to_string(actual.size()) + " replayed calls";
}

void EveryOpReplaysItsDeepCopies() {
    const auto context = stubContext();
    const auto functions = resolve(context);
#define TEST_THUNK(name) Expect(reinterpret_cast<PFN_vkVoidFunction>(functions.cmd##name) != stubProc(context.device, name##Name), std::string(name##Name) + " resolved to the driver's function, not a command stream thunk"); \
    Expect(CommandStream::Records(name##Name), std::string(name##Name) + " is not recorded by the command stream");
    TEST_ALL_OPS(TEST_THUNK)
#undef TEST_THUNK
    Data data;
    std::vector<unsigned char> large;
    makeData(data, large);
    calls.clear();
    recordAll(Real, functions, data, large);
    const auto direct = calls;
    std::set<std::string> names;
    for (const auto& call : direct) names.insert(call.substr(0, call.find('@')));
    Expect(names.size() == 53, "the direct calls reached " + std::to_string(names.size()) + " of the 53 functions");
    CommandStream stream;
    calls.clear();
    recordAll(stream.Handle(), functions, data, large);
    Expect(calls.empty(), "recording into a stream handle called " + std::to_string(calls.size()) + " driver functions");
    Expect(stream.Ops() == direct.size(), "the stream holds " + std::to_string(stream.Ops()) + " ops for " + std::to_string(direct.size()) + " calls");
    Expect(stream.Bytes() > large.size() && stream.ReservedBytes() >= stream.Bytes(), "the stream counts " + std::to_string(stream.Bytes()) + " bytes in " + std::to_string(stream.ReservedBytes()) + " reserved");
    scramble(data, large);
    stream.Replay(Real);
    Expect(calls == direct, "the replay differs from the direct calls after the caller's memory was overwritten: " + firstDifference(direct, calls));
    calls.clear();
    stream.Replay(Real);
    Expect(calls == direct, "a second replay of the same stream differs: " + firstDifference(direct, calls));
}

void UntaggedHandlesPassThrough() {
    const auto functions = resolve(stubContext());
    const std::array<std::uintptr_t, 5> handles{0x1, 0x3, 0x7fffffffffffull, 0xA5C3000000001000ull, 0x5A3C000000001001ull};
    calls.clear();
    for (const auto handle : handles) {
        Expect(!CommandStream::IsHandle(fake<VkCommandBuffer>(handle)) && CommandStream::FromHandle(fake<VkCommandBuffer>(handle)) == nullptr, "handle " + std::to_string(handle) + " was taken for a stream handle");
        functions.cmdDraw(fake<VkCommandBuffer>(handle), 1, 2, 3, 4);
    }
    Expect(calls.size() == handles.size(), "untagged handles reached the driver " + std::to_string(calls.size()) + " times, not " + std::to_string(handles.size()));
    for (std::size_t index = 0; index < std::min(calls.size(), handles.size()); ++index) {
        std::string expected = begin(DrawName, fake<VkCommandBuffer>(handles[index]));
        for (const std::uint32_t value : {1u, 2u, 3u, 4u}) bytes(expected, value);
        Expect(calls[index] == expected, "an untagged draw reached the driver as " + calls[index]);
    }
    Expect(!CommandStream::IsHandle(VK_NULL_HANDLE), "a null command buffer was taken for a stream handle");
    CommandStream stream;
    Expect(CommandStream::IsHandle(stream.Handle()) && CommandStream::FromHandle(stream.Handle()) == &stream, "a stream's handle does not lead back to the stream");
    CommandStream other;
    Expect(other.Handle() != stream.Handle() && CommandStream::FromHandle(other.Handle()) == &other, "two streams share a handle");
    ExpectThrows([&] { stream.Replay(other.Handle()); }, "stream handle", "replaying into a stream handle");
}

void ResolverRefusals() {
    auto context = stubContext();
    ExpectThrows([&] { context.Function<PFN_vkVoidFunction>("vkCmdDrawMultiEXT"); }, "vkCmdDrawMultiEXT", "resolving a vkCmd function without a thunk");
    ExpectThrows([&] { context.Function<PFN_vkVoidFunction>("vkCmdBeginRendering"); }, "vkCmdBeginRendering", "resolving the core name of a function the stream records under its KHR name");
    Expect(!CommandStream::Records("vkCmdDrawMultiEXT") && !CommandStream::Records("vkCreateFence") && CommandStream::Records("vkCmdDraw"), "Records does not match the recorded vkCmd names");
    Expect(context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets") == reinterpret_cast<PFN_vkUpdateDescriptorSets>(&stubUpdateDescriptorSets), "a function that takes no command buffer was wrapped");
    ExpectThrows([&] { context.Function<PFN_vkVoidFunction>("vkCreateFence"); }, "missing Vulkan function: vkCreateFence", "resolving a function the device lacks");
    DeviceFunctions table{};
    FillDeviceFunctions(context, table);
    Expect(table.cmdDraw == context.Function<PFN_vkCmdDraw>("vkCmdDraw") && table.cmdBeginRendering == context.Function<PFN_vkCmdBeginRenderingKHR>("vkCmdBeginRenderingKHR"), "the device's function table does not hold the command stream thunks");
    Expect(reinterpret_cast<PFN_vkVoidFunction>(table.updateDescriptorSets) == reinterpret_cast<PFN_vkVoidFunction>(&stubUpdateDescriptorSets), "the device's function table wrapped vkUpdateDescriptorSets");
    context.functions = &table;
    Expect(context.Function<PFN_vkCmdDraw>(DrawName) == table.cmdDraw, "the device's function table and a lookup by name return different thunks");
    CommandStream stream;
    calls.clear();
    context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(stream.Handle(), 9, 8, 7, 6);
    context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(Real, 9, 8, 7, 6);
    Expect(stream.Ops() == 1 && calls.size() == 1, "the table's draw did not append to the stream handle and call the driver for the real one");
    stream.Replay(Real);
    Expect(calls.size() == 2 && calls[0] == calls[1], "the table's appended draw replayed differently from its direct call");
}

void ChainsAndDescriptorTypesTheStreamCannotCopy() {
    const auto functions = resolve(stubContext());
    Data data;
    std::vector<unsigned char> large;
    makeData(data, large);
    CommandStream stream;
    functions.cmdDraw(stream.Handle(), 1, 1, 0, 0);
    VkApplicationInfo unknown{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    auto barrier = data.images[0];
    barrier.pNext = &unknown;
    ExpectThrows([&] { functions.cmdPipelineBarrier(stream.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier); }, "vkCmdPipelineBarrier chains a structure of type 0", "an image barrier with an unknown chained structure");
    data.imageLocations.pNext = &unknown;
    ExpectThrows([&] { functions.cmdPipelineBarrier(stream.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, data.images.data()); }, "chains a structure of type 0", "sample locations with an unknown chained structure");
    data.imageLocations.pNext = nullptr;
    auto rendering = data.rendering;
    auto depth = data.depth;
    depth.pNext = &unknown;
    rendering.pDepthAttachment = &depth;
    rendering.pStencilAttachment = &depth;
    ExpectThrows([&] { functions.cmdBeginRenderingKHR(stream.Handle(), &rendering); }, "vkCmdBeginRenderingKHR chains", "a depth attachment with an unknown chained structure");
    auto write = data.writes[0];
    write.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
    ExpectThrows([&] { functions.cmdPushDescriptorSetKHR(stream.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(0x1203), 0, 1, &write); }, "vkCmdPushDescriptorSetKHR pushes descriptor type", "an inline uniform block push");
    auto pass = data.pass;
    data.passSampleLocations.pNext = &unknown;
    ExpectThrows([&] { functions.cmdBeginRenderPass(stream.Handle(), &pass, VK_SUBPASS_CONTENTS_INLINE); }, "vkCmdBeginRenderPass chains", "render pass sample locations with an unknown chained structure");
    Expect(stream.Ops() == 1, "refused appends left " + std::to_string(stream.Ops()) + " ops in the stream, not the one draw");
    calls.clear();
    stream.Replay(Real);
    Expect(calls.size() == 1 && calls[0].rfind(DrawName, 0) == 0, "a stream with refused appends replayed " + std::to_string(calls.size()) + " calls");
}

void ClosedStreamsRefuseAppends() {
    const auto functions = resolve(stubContext());
    CommandStream stream;
    functions.cmdDispatch(stream.Handle(), 1, 2, 3);
    stream.Close();
    Expect(stream.Closed(), "a closed stream does not report it");
    ExpectThrows([&] { functions.cmdDispatch(stream.Handle(), 4, 5, 6); }, "vkCmdDispatch was recorded into a command stream after its batch was closed", "an append after Close");
    Expect(stream.Ops() == 1, "an append after Close changed the stream");
    calls.clear();
    stream.Replay(Real);
    Expect(calls.size() == 1, "a closed stream did not replay its op");
    stream.Reset();
    Expect(!stream.Closed() && stream.Empty() && stream.Ops() == 0 && stream.Bytes() == 0, "Reset did not reopen and empty the stream");
    functions.cmdDispatch(stream.Handle(), 4, 5, 6);
    Expect(stream.Ops() == 1, "a reset stream did not take an append");
}

void StreamsRecycleTheirMemory() {
    const auto functions = resolve(stubContext());
    CommandStream stream;
    const auto round = [&](std::uint32_t draws) {
        for (std::uint32_t draw = 0; draw < draws; ++draw) functions.cmdDraw(stream.Handle(), draw, 1, draw * 3u, 0);
    };
    round(4000);
    const auto reserved = stream.ReservedBytes();
    Expect(stream.Ops() == 4000 && reserved > CommandStream::ChunkBytes && reserved <= CommandStream::RetainedBytes, "4000 draws reserved " + std::to_string(reserved) + " bytes over " + std::to_string(stream.Ops()) + " ops");
    for (int cycle = 0; cycle < 3; ++cycle) {
        stream.Reset();
        round(4000);
        Expect(stream.ReservedBytes() == reserved, "a recycled stream reserved " + std::to_string(stream.ReservedBytes()) + " bytes, not the " + std::to_string(reserved) + " it kept");
    }
    calls.clear();
    stream.Replay(Real);
    bool ordered = calls.size() == 4000;
    for (std::uint32_t draw = 0; ordered && draw < 4000; ++draw) {
        std::string expected = begin(DrawName, Real);
        for (const std::uint32_t value : {draw, 1u, draw * 3u, 0u}) bytes(expected, value);
        ordered = calls[draw] == expected;
    }
    Expect(ordered, "a recycled stream did not replay its 4000 draws in order");
    std::vector<unsigned char> payload(65536);
    unsigned char seed = 3;
    pattern(payload.data(), payload.size(), seed);
    for (int update = 0; update < 40; ++update) functions.cmdUpdateBuffer(stream.Handle(), fake<VkBuffer>(0x4000), 0, payload.size(), payload.data());
    Expect(stream.ReservedBytes() > CommandStream::RetainedBytes, "forty 64 KiB updates did not grow the stream past what it retains");
    stream.Reset();
    Expect(stream.ReservedBytes() <= CommandStream::RetainedBytes && stream.ReservedBytes() > 0, "a reset stream retains " + std::to_string(stream.ReservedBytes()) + " bytes");
    round(10);
    calls.clear();
    stream.Replay(Real);
    Expect(calls.size() == 10, "a trimmed stream replayed " + std::to_string(calls.size()) + " calls, not 10");
}

void EachResolverKeepsItsOwnFunctions() {
    const auto plain = resolve(stubContext());
    const auto variant = resolve(stubContext(&variantProc));
    Expect(plain.cmdDraw != variant.cmdDraw && plain.cmdDispatch == variant.cmdDispatch, "two resolvers with different draws share a draw thunk or split the shared dispatch");
    CommandStream stream;
    plain.cmdDraw(stream.Handle(), 1, 0, 0, 0);
    variant.cmdDraw(stream.Handle(), 2, 0, 0, 0);
    plain.cmdDraw(stream.Handle(), 3, 0, 0, 0);
    calls.clear();
    stream.Replay(Real);
    const auto prefixed = [](const std::string& line) { return line.rfind("variant1:", 0) == 0; };
    Expect(calls.size() == 3 && !prefixed(calls[0]) && prefixed(calls[1]) && !prefixed(calls[2]), "a stream did not replay each op through the function its thunk was resolved for");
    calls.clear();
    variant.cmdDraw(Real, 4, 0, 0, 0);
    Expect(calls.size() == 1 && prefixed(calls[0]), "the second resolver's draw did not pass through to its own function");
    handedThunk = reinterpret_cast<PFN_vkVoidFunction>(plain.cmdDraw);
    const auto handed = resolve(stubContext(&thunkProc));
    Expect(handed.cmdDraw == plain.cmdDraw, "a resolver that returns a thunk got it wrapped again");
}

}

int main() {
    try {
        EveryOpReplaysItsDeepCopies();
        UntaggedHandlesPassThrough();
        ResolverRefusals();
        ChainsAndDescriptorTypesTheStreamCannotCopy();
        ClosedStreamsRefuseAppends();
        StreamsRecycleTheirMemory();
        EachResolverKeepsItsOwnFunctions();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures != 0) {
        std::fprintf(stderr, "%d command stream checks failed\n", failures);
        return 1;
    }
    std::printf("command stream tests passed\n");
    return 0;
}
