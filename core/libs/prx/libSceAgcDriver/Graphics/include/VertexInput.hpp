#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VERTEXINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VERTEXINPUT_HPP

#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

struct VertexFormat {
    VkFormat format;
    std::uint32_t bytes;
    std::uint32_t alignment;
    const char* scalar;
};

inline VertexFormat DecodeVertexFormat(const ShaderRecompiler::VertexAttribute& attribute) {
    Require(attribute.components >= 1 && attribute.components <= 4, "invalid vertex attribute component count");
    const auto components = attribute.formatComponents == 0u ? attribute.components : attribute.formatComponents;
    Require(components >= 1u && components <= 4u, "invalid vertex format component count");
    const auto format = (attribute.resource.fields[3] >> 12u) & 0x7fu;
    switch (format) {
        case 1: { const std::array formats{VK_FORMAT_R8_UNORM}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 2: { const std::array formats{VK_FORMAT_R8_SNORM}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 3: { const std::array formats{VK_FORMAT_R8_USCALED}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 4: { const std::array formats{VK_FORMAT_R8_SSCALED}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 5: { const std::array formats{VK_FORMAT_R8_UINT}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 1u, 1u, "u32"}; }
        case 6: { const std::array formats{VK_FORMAT_R8_SINT}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 1u, 1u, "i32"}; }
        case 7: { const std::array formats{VK_FORMAT_R16_UNORM}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 8: { const std::array formats{VK_FORMAT_R16_SNORM}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 9: { const std::array formats{VK_FORMAT_R16_USCALED}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 10: { const std::array formats{VK_FORMAT_R16_SSCALED}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 11: { const std::array formats{VK_FORMAT_R16_UINT}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 2u, 2u, "u32"}; }
        case 12: { const std::array formats{VK_FORMAT_R16_SINT}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 2u, 2u, "i32"}; }
        case 13: { const std::array formats{VK_FORMAT_R16_SFLOAT}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 14: { const std::array formats{VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 15: { const std::array formats{VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 16: { const std::array formats{VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 17: { const std::array formats{VK_FORMAT_R8_SSCALED, VK_FORMAT_R8G8_SSCALED}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 18: { const std::array formats{VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 1u, 1u, "u32"}; }
        case 19: { const std::array formats{VK_FORMAT_R8_SINT, VK_FORMAT_R8G8_SINT}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 1u, 1u, "i32"}; }
        case 20: { const std::array formats{VK_FORMAT_R32_UINT}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 4u, 4u, "u32"}; }
        case 21: { const std::array formats{VK_FORMAT_R32_SINT}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 4u, 4u, "i32"}; }
        case 22: { const std::array formats{VK_FORMAT_R32_SFLOAT}; const auto count = std::min(components, 1u); return {formats[count - 1], count * 4u, 4u, "f32"}; }
        case 23: { const std::array formats{VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 24: { const std::array formats{VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 25: { const std::array formats{VK_FORMAT_R16_USCALED, VK_FORMAT_R16G16_USCALED}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 26: { const std::array formats{VK_FORMAT_R16_SSCALED, VK_FORMAT_R16G16_SSCALED}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 27: { const std::array formats{VK_FORMAT_R16_UINT, VK_FORMAT_R16G16_UINT}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 2u, 2u, "u32"}; }
        case 28: { const std::array formats{VK_FORMAT_R16_SINT, VK_FORMAT_R16G16_SINT}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 2u, 2u, "i32"}; }
        case 29: { const std::array formats{VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 36: return {VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4u, 4u, "f32"};
        case 50: return {VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4u, 4u, "f32"};
        case 51: return {VK_FORMAT_A2B10G10R10_SNORM_PACK32, 4u, 4u, "f32"};
        case 52: return {VK_FORMAT_A2B10G10R10_USCALED_PACK32, 4u, 4u, "f32"};
        case 53: return {VK_FORMAT_A2B10G10R10_SSCALED_PACK32, 4u, 4u, "f32"};
        case 54: return {VK_FORMAT_A2B10G10R10_UINT_PACK32, 4u, 4u, "u32"};
        case 55: return {VK_FORMAT_A2B10G10R10_SINT_PACK32, 4u, 4u, "i32"};
        case 56: { const std::array formats{VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 57: { const std::array formats{VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 58: { const std::array formats{VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED, VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 59: { const std::array formats{VK_FORMAT_R8_SSCALED, VK_FORMAT_R8G8_SSCALED, VK_FORMAT_R8G8B8_SSCALED, VK_FORMAT_R8G8B8A8_SSCALED}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 60: { const std::array formats{VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8B8_UINT, VK_FORMAT_R8G8B8A8_UINT}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 1u, 1u, "u32"}; }
        case 61: { const std::array formats{VK_FORMAT_R8_SINT, VK_FORMAT_R8G8_SINT, VK_FORMAT_R8G8B8_SINT, VK_FORMAT_R8G8B8A8_SINT}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 1u, 1u, "i32"}; }
        case 62: { const std::array formats{VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 4u, 4u, "u32"}; }
        case 63: { const std::array formats{VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 4u, 4u, "i32"}; }
        case 64: { const std::array formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT}; const auto count = std::min(components, 2u); return {formats[count - 1], count * 4u, 4u, "f32"}; }
        case 65: { const std::array formats{VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16A16_UNORM}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 66: { const std::array formats{VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R16G16B16A16_SNORM}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 67: { const std::array formats{VK_FORMAT_R16_USCALED, VK_FORMAT_R16G16_USCALED, VK_FORMAT_R16G16B16_USCALED, VK_FORMAT_R16G16B16A16_USCALED}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 68: { const std::array formats{VK_FORMAT_R16_SSCALED, VK_FORMAT_R16G16_SSCALED, VK_FORMAT_R16G16B16_SSCALED, VK_FORMAT_R16G16B16A16_SSCALED}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 69: { const std::array formats{VK_FORMAT_R16_UINT, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16B16_UINT, VK_FORMAT_R16G16B16A16_UINT}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 2u, 2u, "u32"}; }
        case 70: { const std::array formats{VK_FORMAT_R16_SINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16B16_SINT, VK_FORMAT_R16G16B16A16_SINT}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 2u, 2u, "i32"}; }
        case 71: { const std::array formats{VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 72: { const std::array formats{VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32_UINT}; const auto count = std::min(components, 3u); return {formats[count - 1], count * 4u, 4u, "u32"}; }
        case 73: { const std::array formats{VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT}; const auto count = std::min(components, 3u); return {formats[count - 1], count * 4u, 4u, "i32"}; }
        case 74: { const std::array formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT}; const auto count = std::min(components, 3u); return {formats[count - 1], count * 4u, 4u, "f32"}; }
        case 75: { const std::array formats{VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32A32_UINT}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 4u, 4u, "u32"}; }
        case 76: { const std::array formats{VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 4u, 4u, "i32"}; }
        case 77: { const std::array formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT}; const auto count = std::min(components, 4u); return {formats[count - 1], count * 4u, 4u, "f32"}; }
        default: throw std::runtime_error("AGC graphics: unsupported vertex format " + std::to_string(format));
    }
}

inline std::string VertexAttributeSignature(const ShaderRecompiler::VertexAttribute& attribute) {
    const auto format = DecodeVertexFormat(attribute);
    return std::string(format.scalar) + (attribute.components == 1 ? "" : "x" + std::to_string(attribute.components));
}

struct VertexInputLayout {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

inline VertexInputLayout BuildVertexInputLayout(const Context& context, std::span<const ShaderRecompiler::VertexAttribute> attributes) {
    Require(attributes.size() <= context.limits.maxVertexInputBindings && attributes.size() <= context.limits.maxVertexInputAttributes, "vertex input count exceeds device limits");
    VertexInputLayout result;
    std::set<std::uint32_t> locations;
    for (const auto& attribute : attributes) {
        const auto& fields = attribute.resource.fields;
        const auto format = DecodeVertexFormat(attribute);
        Require(attribute.location < context.limits.maxVertexInputAttributes && locations.insert(attribute.location).second, "invalid or duplicate vertex attribute location");
        Require(attribute.fetchIndex <= 1, "unsupported vertex fetch index");
        Require((fields[1] & 0x80000000u) == 0 && (fields[3] & 0x00800000u) == 0 && (fields[3] >> 30u) == 0, "unsupported vertex buffer descriptor flags");
        const auto stride = (fields[1] >> 16u) & 0x3fffu;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        Require(address != 0 && address % format.alignment == 0 && stride % format.alignment == 0, "unaligned vertex buffer");
        Require(stride <= context.limits.maxVertexInputBindingStride, "vertex stride exceeds device limits");
        Require(context.formatProperties != nullptr, "missing vertex format property query");
        VkFormatProperties properties{};
        context.formatProperties(context.physical, format.format, &properties);
        Require((properties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != 0, "device does not support vertex format " + std::to_string(format.format));
        const auto binding = static_cast<std::uint32_t>(result.bindings.size());
        result.bindings.push_back({binding, stride, attribute.fetchIndex == 0 ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE});
        result.attributes.push_back({attribute.location, binding, format.format, 0});
    }
    return result;
}

// The whole byte range a vertex buffer descriptor covers (records * stride, or records when the
// stride is 0): what an indirect draw, whose counts only the GPU knows, copies for the fetch.
inline std::uint32_t VertexBufferOutOfBoundsSelect(const ShaderRecompiler::VertexAttribute& attribute) {
    return (attribute.resource.fields[3] >> 28u) & 3u;
}

inline bool VertexFetchOutOfRange(const ShaderRecompiler::VertexAttribute& attribute) {
    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
    const auto select = VertexBufferOutOfBoundsSelect(attribute);
    return stride == 0 && (select == 2u || select == 3u) && attribute.resource.fields[2] == 0;
}

inline std::uint64_t ZeroStrideAvailableBytes(const ShaderRecompiler::VertexAttribute& attribute) {
    const auto records = attribute.resource.fields[2];
    if (VertexBufferOutOfBoundsSelect(attribute) == 2u && records != 0) return std::max<std::uint64_t>(records, DecodeVertexFormat(attribute).bytes);
    return records;
}

inline std::size_t VertexBufferExtent(const ShaderRecompiler::VertexAttribute& attribute) {
    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
    const auto records = attribute.resource.fields[2];
    Require(attribute.fetchIndex <= 1, "unsupported vertex fetch index");
    const auto bytes = stride == 0 ? ZeroStrideAvailableBytes(attribute) : static_cast<std::uint64_t>(records) * stride;
    Require(bytes != 0 && bytes <= std::numeric_limits<std::size_t>::max(), "empty or oversized vertex buffer descriptor");
    const auto address = attribute.resource.fields[0] | (static_cast<std::uint64_t>(attribute.resource.fields[1] & 0xffffu) << 32u);
    Require(address != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid vertex buffer address range");
    return static_cast<std::size_t>(bytes);
}

inline std::size_t VertexBufferReadSize(const ShaderRecompiler::VertexAttribute& attribute, std::uint32_t maxIndex, std::uint32_t instances, std::uint32_t firstInstance = 0) {
    Require(instances != 0, "vertex input requires nonzero instance count");
    Require(firstInstance <= std::numeric_limits<std::uint32_t>::max() - (instances - 1u), "vertex input instance range overflow");
    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
    const auto records = attribute.resource.fields[2];
    Require(attribute.fetchIndex <= 1, "unsupported vertex fetch index");
    const auto index = attribute.fetchIndex == 0 ? maxIndex : firstInstance + instances - 1u;
    const auto bytes = DecodeVertexFormat(attribute).bytes;
    Require(stride == 0 || index < records, "vertex fetch exceeds descriptor record count");
    const auto available = stride == 0 ? ZeroStrideAvailableBytes(attribute) : static_cast<std::uint64_t>(records) * stride;
    const auto required = static_cast<std::uint64_t>(stride) * index + bytes;
    Require(required <= available && required <= std::numeric_limits<std::size_t>::max(), "vertex fetch exceeds descriptor byte range");
    const auto address = attribute.resource.fields[0] | (static_cast<std::uint64_t>(attribute.resource.fields[1] & 0xffffu) << 32u);
    Require(address != 0 && required <= std::numeric_limits<std::uint64_t>::max() - address, "invalid vertex buffer address range");
    return static_cast<std::size_t>(required);
}

struct VertexFetch {
    std::uint64_t begin;
    std::uint64_t end;
    std::uint32_t stride;
    std::uint32_t fetchIndex;
    std::uint32_t alignment;
};

struct VertexCopyPlan {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> copies;
    std::vector<std::size_t> copyOf;
    std::vector<std::uint64_t> offsets;
};

inline VertexCopyPlan PlanVertexCopies(std::span<const VertexFetch> fetches) {
    VertexCopyPlan plan;
    plan.copyOf.assign(fetches.size(), 0);
    plan.offsets.assign(fetches.size(), 0);
    std::vector<std::size_t> order(fetches.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const auto& x = fetches[a];
        const auto& y = fetches[b];
        if (x.stride != y.stride) return x.stride < y.stride;
        if (x.fetchIndex != y.fetchIndex) return x.fetchIndex < y.fetchIndex;
        return x.begin < y.begin;
    });
    std::size_t lead = fetches.size();
    for (const auto i : order) {
        const auto& fetch = fetches[i];
        Require(fetch.begin < fetch.end && fetch.alignment != 0, "empty vertex fetch range");
        const bool joins = lead != fetches.size() && fetch.stride != 0 && fetches[lead].stride == fetch.stride && fetches[lead].fetchIndex == fetch.fetchIndex && fetch.begin - fetches[lead].begin < fetch.stride && (fetch.begin - fetches[lead].begin) % fetch.alignment == 0;
        if (!joins) {
            lead = i;
            plan.copies.emplace_back(fetch.begin, fetch.end);
        }
        auto& copy = plan.copies.back();
        copy.second = std::max(copy.second, fetch.end);
        plan.copyOf[i] = plan.copies.size() - 1;
        plan.offsets[i] = fetch.begin - copy.first;
    }
    return plan;
}

inline std::optional<std::uint32_t> HighestDrawIndex(std::span<const std::byte> indices, std::uint32_t indexSize, bool skipRestart) {
    Require(indexSize == 2 || indexSize == 4, "unsupported index size");
    const auto restartIndex = indexSize == 2 ? 0xffffu : 0xffffffffu;
    std::optional<std::uint32_t> highest;
    for (std::size_t offset = 0; offset + indexSize <= indices.size(); offset += indexSize) {
        std::uint32_t index = 0;
        if (indexSize == 2) {
            std::uint16_t value = 0;
            std::memcpy(&value, indices.data() + offset, sizeof(value));
            index = value;
        } else {
            std::memcpy(&index, indices.data() + offset, sizeof(index));
        }
        if (skipRestart && index == restartIndex) continue;
        highest = std::max(highest.value_or(0u), index);
    }
    return highest;
}

}

#endif

