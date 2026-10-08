#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VERTEXINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VERTEXINPUT_HPP

#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <algorithm>
#include <limits>
#include <list>
#include <unordered_map>
#include <mutex>
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
    const auto format = (attribute.resource.fields[3] >> 12u) & 0x7fu;
    switch (format) {
        case 1: { const std::array formats{VK_FORMAT_R8_UNORM}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 2: { const std::array formats{VK_FORMAT_R8_SNORM}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 3: { const std::array formats{VK_FORMAT_R8_USCALED}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 4: { const std::array formats{VK_FORMAT_R8_SSCALED}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 5: { const std::array formats{VK_FORMAT_R8_UINT}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 1u, 1u, "u32"}; }
        case 6: { const std::array formats{VK_FORMAT_R8_SINT}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 1u, 1u, "i32"}; }
        case 7: { const std::array formats{VK_FORMAT_R16_UNORM}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 8: { const std::array formats{VK_FORMAT_R16_SNORM}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 9: { const std::array formats{VK_FORMAT_R16_USCALED}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 10: { const std::array formats{VK_FORMAT_R16_SSCALED}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 11: { const std::array formats{VK_FORMAT_R16_UINT}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 2u, 2u, "u32"}; }
        case 12: { const std::array formats{VK_FORMAT_R16_SINT}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 2u, 2u, "i32"}; }
        case 13: { const std::array formats{VK_FORMAT_R16_SFLOAT}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 14: { const std::array formats{VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 15: { const std::array formats{VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 16: { const std::array formats{VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 17: { const std::array formats{VK_FORMAT_R8_SSCALED, VK_FORMAT_R8G8_SSCALED}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 18: { const std::array formats{VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 1u, 1u, "u32"}; }
        case 19: { const std::array formats{VK_FORMAT_R8_SINT, VK_FORMAT_R8G8_SINT}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 1u, 1u, "i32"}; }
        case 20: { const std::array formats{VK_FORMAT_R32_UINT}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 4u, 4u, "u32"}; }
        case 21: { const std::array formats{VK_FORMAT_R32_SINT}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 4u, 4u, "i32"}; }
        case 22: { const std::array formats{VK_FORMAT_R32_SFLOAT}; const auto count = std::min(attribute.components, 1u); return {formats[count - 1], count * 4u, 4u, "f32"}; }
        case 23: { const std::array formats{VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 24: { const std::array formats{VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 25: { const std::array formats{VK_FORMAT_R16_USCALED, VK_FORMAT_R16G16_USCALED}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 26: { const std::array formats{VK_FORMAT_R16_SSCALED, VK_FORMAT_R16G16_SSCALED}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 27: { const std::array formats{VK_FORMAT_R16_UINT, VK_FORMAT_R16G16_UINT}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 2u, 2u, "u32"}; }
        case 28: { const std::array formats{VK_FORMAT_R16_SINT, VK_FORMAT_R16G16_SINT}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 2u, 2u, "i32"}; }
        case 29: { const std::array formats{VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 36: return {VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4u, 4u, "f32"};
        case 50: return {VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4u, 4u, "f32"};
        case 51: return {VK_FORMAT_A2B10G10R10_SNORM_PACK32, 4u, 4u, "f32"};
        case 52: return {VK_FORMAT_A2B10G10R10_USCALED_PACK32, 4u, 4u, "f32"};
        case 53: return {VK_FORMAT_A2B10G10R10_SSCALED_PACK32, 4u, 4u, "f32"};
        case 54: return {VK_FORMAT_A2B10G10R10_UINT_PACK32, 4u, 4u, "u32"};
        case 55: return {VK_FORMAT_A2B10G10R10_SINT_PACK32, 4u, 4u, "i32"};
        case 56: { const std::array formats{VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 57: { const std::array formats{VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 58: { const std::array formats{VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED, VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 59: { const std::array formats{VK_FORMAT_R8_SSCALED, VK_FORMAT_R8G8_SSCALED, VK_FORMAT_R8G8B8_SSCALED, VK_FORMAT_R8G8B8A8_SSCALED}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 1u, 1u, "f32"}; }
        case 60: { const std::array formats{VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8B8_UINT, VK_FORMAT_R8G8B8A8_UINT}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 1u, 1u, "u32"}; }
        case 61: { const std::array formats{VK_FORMAT_R8_SINT, VK_FORMAT_R8G8_SINT, VK_FORMAT_R8G8B8_SINT, VK_FORMAT_R8G8B8A8_SINT}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 1u, 1u, "i32"}; }
        case 62: { const std::array formats{VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 4u, 4u, "u32"}; }
        case 63: { const std::array formats{VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 4u, 4u, "i32"}; }
        case 64: { const std::array formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT}; const auto count = std::min(attribute.components, 2u); return {formats[count - 1], count * 4u, 4u, "f32"}; }
        case 65: { const std::array formats{VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16A16_UNORM}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 66: { const std::array formats{VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R16G16B16A16_SNORM}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 67: { const std::array formats{VK_FORMAT_R16_USCALED, VK_FORMAT_R16G16_USCALED, VK_FORMAT_R16G16B16_USCALED, VK_FORMAT_R16G16B16A16_USCALED}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 68: { const std::array formats{VK_FORMAT_R16_SSCALED, VK_FORMAT_R16G16_SSCALED, VK_FORMAT_R16G16B16_SSCALED, VK_FORMAT_R16G16B16A16_SSCALED}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 69: { const std::array formats{VK_FORMAT_R16_UINT, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16B16_UINT, VK_FORMAT_R16G16B16A16_UINT}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 2u, 2u, "u32"}; }
        case 70: { const std::array formats{VK_FORMAT_R16_SINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16B16_SINT, VK_FORMAT_R16G16B16A16_SINT}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 2u, 2u, "i32"}; }
        case 71: { const std::array formats{VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 2u, 2u, "f32"}; }
        case 72: { const std::array formats{VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32_UINT}; const auto count = std::min(attribute.components, 3u); return {formats[count - 1], count * 4u, 4u, "u32"}; }
        case 73: { const std::array formats{VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT}; const auto count = std::min(attribute.components, 3u); return {formats[count - 1], count * 4u, 4u, "i32"}; }
        case 74: { const std::array formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT}; const auto count = std::min(attribute.components, 3u); return {formats[count - 1], count * 4u, 4u, "f32"}; }
        case 75: { const std::array formats{VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32A32_UINT}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 4u, 4u, "u32"}; }
        case 76: { const std::array formats{VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 4u, 4u, "i32"}; }
        case 77: { const std::array formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT}; const auto count = std::min(attribute.components, 4u); return {formats[count - 1], count * 4u, 4u, "f32"}; }
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
    std::vector<std::uint32_t> alignments;
};

inline void ValidateVertexAddresses(std::span<const ShaderRecompiler::VertexAttribute> attributes, const VertexInputLayout& layout) {
    Require(attributes.size() == layout.alignments.size(), "vertex input layout count mismatch");
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        const auto& fields = attributes[i].resource.fields;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        Require(address != 0 && address % layout.alignments[i] == 0, "unaligned vertex buffer");
    }
}

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
        result.alignments.push_back(format.alignment);
    }
    return result;
}

class VertexInputCache {
public:
    explicit VertexInputCache(const Context& context, std::size_t capacity = 1024) : context(context), capacity(capacity) {
        Require(capacity != 0, "vertex input cache capacity is zero");
    }

    std::shared_ptr<const VertexInputLayout> Get(std::uint64_t variant, std::span<const ShaderRecompiler::VertexAttribute> attributes) {
        if (attributes.empty()) return empty;
        if (variant == 0) return std::make_shared<const VertexInputLayout>(BuildVertexInputLayout(context, attributes));
        std::lock_guard lock(mutex);
        const auto found = entries.find(variant);
        if (found != entries.end() && sameShape(attributes, found->second.shape)) {
            ValidateVertexAddresses(attributes, *found->second.layout);
            order.splice(order.begin(), order, found->second.order);
            return found->second.layout;
        }
        auto layout = std::make_shared<const VertexInputLayout>(BuildVertexInputLayout(context, attributes));
        std::vector<ShaderRecompiler::VertexAttribute> shape(attributes.begin(), attributes.end());
        for (auto& attribute : shape) {
            attribute.resource.fields[0] = 0;
            attribute.resource.fields[1] &= 0xffff0000u;
            attribute.resource.fields[2] = 0;
        }
        if (found != entries.end()) {
            found->second.shape = std::move(shape);
            found->second.layout = layout;
            order.splice(order.begin(), order, found->second.order);
            return layout;
        }
        order.push_front(variant);
        try {
            entries.emplace(variant, Entry{layout, std::move(shape), order.begin()});
        } catch (...) {
            order.pop_front();
            throw;
        }
        if (entries.size() > capacity) {
            entries.erase(order.back());
            order.pop_back();
        }
        return layout;
    }

    std::size_t Size() const {
        std::lock_guard lock(mutex);
        return entries.size();
    }

private:
    static bool sameShape(std::span<const ShaderRecompiler::VertexAttribute> a, std::span<const ShaderRecompiler::VertexAttribute> b) {
        return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](const auto& x, const auto& y) {
            return x.location == y.location && x.components == y.components && x.fetchIndex == y.fetchIndex &&
                (x.resource.fields[1] & 0xffff0000u) == y.resource.fields[1] && x.resource.fields[3] == y.resource.fields[3];
        });
    }
    struct Entry {
        std::shared_ptr<const VertexInputLayout> layout;
        std::vector<ShaderRecompiler::VertexAttribute> shape;
        std::list<std::uint64_t>::iterator order;
    };
    Context context;
    std::size_t capacity;
    const std::shared_ptr<const VertexInputLayout> empty = std::make_shared<const VertexInputLayout>();
    mutable std::mutex mutex;
    std::list<std::uint64_t> order;
    std::unordered_map<std::uint64_t, Entry> entries;
};

// The whole byte range a vertex buffer descriptor covers (records * stride, or records when the
// stride is 0): what an indirect draw, whose counts only the GPU knows, copies for the fetch.
inline std::size_t VertexBufferExtent(const ShaderRecompiler::VertexAttribute& attribute) {
    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
    const auto records = attribute.resource.fields[2];
    Require(attribute.fetchIndex <= 1, "unsupported vertex fetch index");
    const auto bytes = stride == 0 ? static_cast<std::uint64_t>(records) : static_cast<std::uint64_t>(records) * stride;
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
    const auto available = stride == 0 ? static_cast<std::uint64_t>(records) : static_cast<std::uint64_t>(records) * stride;
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

}

#endif
