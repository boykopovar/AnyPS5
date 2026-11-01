#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VERTEXINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VERTEXINPUT_HPP

#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <algorithm>
#include <atomic>
#include <list>
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <cstdio>
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

inline bool NullVertexDescriptor(const ShaderRecompiler::VertexAttribute& attribute) {
    const auto& fields = attribute.resource.fields;
    return fields[0] == 0 && fields[1] == 0 && fields[2] == 0 && fields[3] == 0;
}

inline VertexFormat DecodeVertexFormat(const ShaderRecompiler::VertexAttribute& attribute, std::uint32_t minimumComponents = 0u) {
    Require(attribute.components >= 1 && attribute.components <= 4, "invalid vertex attribute component count");
    if (NullVertexDescriptor(attribute)) {
        const std::array formats{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};
        return {formats[attribute.components - 1], attribute.components * 4u, 4u, "f32"};
    }
    const auto components = std::max(attribute.formatComponents == 0u ? attribute.components : attribute.formatComponents, minimumComponents);
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

inline std::uint32_t VertexReadBytes(const ShaderRecompiler::VertexAttribute& attribute, VkFormat host) {
    for (auto components = 1u; components <= 4u; ++components) {
        const auto format = DecodeVertexFormat(attribute, components);
        if (format.format == host) return format.bytes;
    }
    throw std::runtime_error("AGC graphics: vertex format " + std::to_string(host) + " is not a form of the attribute's buffer format");
}

struct VertexInputLayout {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

inline std::array<char, 160> VertexDescriptorFlagsMessage(const ShaderRecompiler::VertexAttribute& attribute) {
    std::array<char, 160> message{};
    const auto& fields = attribute.resource.fields;
    std::snprintf(message.data(), message.size(), "unsupported vertex buffer descriptor flags at location %u (V#=[%08x %08x %08x %08x])",
        attribute.location, fields[0], fields[1], fields[2], fields[3]);
    return message;
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
        if ((fields[1] & 0x80000000u) != 0 || (fields[3] & 0x00800000u) != 0 || (fields[3] >> 30u) != 0) {
            Require(false, VertexDescriptorFlagsMessage(attribute).data());
        }
        const auto stride = (fields[1] >> 16u) & 0x3fffu;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        Require(NullVertexDescriptor(attribute) || (address != 0 && address % format.alignment == 0 && stride % format.alignment == 0), "unaligned vertex buffer");
        Require(stride <= context.limits.maxVertexInputBindingStride, "vertex stride exceeds device limits");
        Require(context.formatProperties != nullptr, "missing vertex format property query");
        const auto supported = [&](VkFormat candidate) {
            VkFormatProperties properties{};
            context.formatProperties(context.physical, candidate, &properties);
            return (properties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != 0;
        };
        auto host = format.format;
        auto hostSupported = supported(host);
        for (auto components = 1u; !hostSupported && components <= 4u; ++components) {
            host = DecodeVertexFormat(attribute, components).format;
            hostSupported = supported(host);
        }
        Require(hostSupported, "device does not support vertex format " + std::to_string(format.format));
        const auto binding = static_cast<std::uint32_t>(result.bindings.size());
        result.bindings.push_back({binding, stride, attribute.fetchIndex == 0 ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE});
        result.attributes.push_back({attribute.location, binding, host, 0});
    }
    return result;
}

inline void ValidateVertexAddresses(std::span<const ShaderRecompiler::VertexAttribute> attributes) {
    for (const auto& attribute : attributes) {
        if (NullVertexDescriptor(attribute)) continue;
        const auto& fields = attribute.resource.fields;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        if (address == 0 || address % DecodeVertexFormat(attribute).alignment != 0) Require(false, "unaligned vertex buffer");
    }
}

class VertexInputCache {
public:
    explicit VertexInputCache(const Context& context, std::size_t capacity = 1024) : context(context), capacity(capacity) {
        Require(capacity != 0, "vertex input cache capacity is zero");
    }

    std::shared_ptr<const VertexInputLayout> TryGet(std::uint64_t variant, std::span<const ShaderRecompiler::VertexAttribute> attributes) {
        if (attributes.empty() || variant == 0 || bypassed(variant)) return nullptr;
        return get(variant, attributes);
    }

    std::size_t Size() const {
        std::lock_guard lock(mutex);
        return entries.size();
    }

private:
    std::shared_ptr<const VertexInputLayout> get(std::uint64_t variant, std::span<const ShaderRecompiler::VertexAttribute> attributes) {
        std::lock_guard lock(mutex);
        const auto found = entries.find(variant);
        if (found != entries.end()) {
            if (!sameShape(attributes, found->second.shape)) {
                bypass(variant);
                order.erase(found->second.order);
                entries.erase(found);
                return nullptr;
            }
            ValidateVertexAddresses(attributes);
            order.splice(order.begin(), order, found->second.order);
            return found->second.layout;
        }
        if (bypassed(variant)) return nullptr;
        auto layout = std::make_shared<const VertexInputLayout>(BuildVertexInputLayout(context, attributes));
        std::vector<ShaderRecompiler::VertexAttribute> shape(attributes.begin(), attributes.end());
        order.push_front(variant);
        try {
            entries.emplace(variant, Entry{layout, std::move(shape), order.begin()});
        } catch (...) {
            order.pop_front();
            throw;
        }
        if (entries.size() > capacity) {
            bypass(order.back());
            entries.erase(order.back());
            order.pop_back();
        }
        return layout;
    }

    static bool sameShape(std::span<const ShaderRecompiler::VertexAttribute> a, std::span<const ShaderRecompiler::VertexAttribute> b) {
        return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](const auto& x, const auto& y) {
            return x.location == y.location && x.components == y.components && x.formatComponents == y.formatComponents && x.fetchIndex == y.fetchIndex &&
                (x.resource.fields[1] & 0xffff0000u) == (y.resource.fields[1] & 0xffff0000u) && x.resource.fields[3] == y.resource.fields[3] && NullVertexDescriptor(x) == NullVertexDescriptor(y);
        });
    }
    static std::size_t bypassIndex(std::uint64_t variant) {
        return static_cast<std::size_t>((variant ^ (variant >> 32u)) & 65535u);
    }
    bool bypassed(std::uint64_t variant) const {
        const auto index = bypassIndex(variant);
        return (bypassBits[index / 64u].load(std::memory_order_relaxed) & (std::uint64_t{1} << (index % 64u))) != 0;
    }
    void bypass(std::uint64_t variant) {
        const auto index = bypassIndex(variant);
        bypassBits[index / 64u].fetch_or(std::uint64_t{1} << (index % 64u), std::memory_order_relaxed);
    }
    struct Entry {
        std::shared_ptr<const VertexInputLayout> layout;
        std::vector<ShaderRecompiler::VertexAttribute> shape;
        std::list<std::uint64_t>::iterator order;
    };
    Context context;
    std::size_t capacity;
    std::array<std::atomic<std::uint64_t>, 1024> bypassBits{};
    mutable std::mutex mutex;
    std::list<std::uint64_t> order;
    std::unordered_map<std::uint64_t, Entry> entries;
};

inline std::uint32_t VertexBufferOutOfBoundsSelect(const ShaderRecompiler::VertexAttribute& attribute) {
    return (attribute.resource.fields[3] >> 28u) & 3u;
}

inline bool VertexFetchOutOfRange(const ShaderRecompiler::VertexAttribute& attribute) {
    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
    if (stride != 0 || NullVertexDescriptor(attribute)) return false;
    const auto select = VertexBufferOutOfBoundsSelect(attribute);
    const auto records = attribute.resource.fields[2];
    if (select == 0u) return true;
    if (select == 3u) return records < DecodeVertexFormat(attribute).bytes;
    return records == 0;
}

inline std::uint64_t ZeroStrideAvailableBytes(const ShaderRecompiler::VertexAttribute& attribute) {
    return VertexFetchOutOfRange(attribute) ? 0u : DecodeVertexFormat(attribute).bytes;
}

inline std::size_t VertexBufferExtent(const ShaderRecompiler::VertexAttribute& attribute) {
    if (NullVertexDescriptor(attribute)) return DecodeVertexFormat(attribute).bytes;
    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
    const auto records = attribute.resource.fields[2];
    Require(attribute.fetchIndex <= 1, "unsupported vertex fetch index");
    const auto bytes = stride == 0 ? ZeroStrideAvailableBytes(attribute) : records == 0 ? 0 : static_cast<std::uint64_t>(records - 1u) * stride + DecodeVertexFormat(attribute).bytes;
    Require(bytes != 0 && bytes <= std::numeric_limits<std::size_t>::max(), "empty or oversized vertex buffer descriptor");
    const auto address = attribute.resource.fields[0] | (static_cast<std::uint64_t>(attribute.resource.fields[1] & 0xffffu) << 32u);
    Require(address != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid vertex buffer address range");
    return static_cast<std::size_t>(bytes);
}

inline std::size_t VertexBufferReadSize(const ShaderRecompiler::VertexAttribute& attribute, std::uint32_t maxIndex, std::uint32_t instances, std::uint32_t firstInstance = 0) {
    Require(instances != 0, "vertex input requires nonzero instance count");
    Require(firstInstance <= std::numeric_limits<std::uint32_t>::max() - (instances - 1u), "vertex input instance range overflow");
    if (NullVertexDescriptor(attribute)) return DecodeVertexFormat(attribute).bytes;
    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
    const auto records = attribute.resource.fields[2];
    Require(attribute.fetchIndex <= 1, "unsupported vertex fetch index");
    const auto index = attribute.fetchIndex == 0 ? maxIndex : firstInstance + instances - 1u;
    const auto bytes = DecodeVertexFormat(attribute).bytes;
    const auto select = VertexBufferOutOfBoundsSelect(attribute);
    const auto recordsUnchecked = stride == 0 ? select != 1u : select <= 1u;
    Require(recordsUnchecked || index < records, "vertex fetch exceeds descriptor record count");
    const auto required = static_cast<std::uint64_t>(stride) * index + bytes;
    Require(stride != 0 || required <= ZeroStrideAvailableBytes(attribute), "vertex fetch exceeds descriptor byte range");
    Require(required <= std::numeric_limits<std::size_t>::max(), "vertex fetch exceeds addressable byte range");
    const auto address = attribute.resource.fields[0] | (static_cast<std::uint64_t>(attribute.resource.fields[1] & 0xffffu) << 32u);
    Require(address != 0 && required <= std::numeric_limits<std::uint64_t>::max() - address, "invalid vertex buffer address range");
    return static_cast<std::size_t>(required);
}

inline std::size_t VertexBufferReadOffset(const ShaderRecompiler::VertexAttribute& attribute, std::uint32_t firstVertex, std::uint32_t firstInstance) {
    Require(attribute.fetchIndex <= 1, "unsupported vertex fetch index");
    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
    const auto index = attribute.fetchIndex == 0 ? firstVertex : firstInstance;
    return static_cast<std::size_t>(static_cast<std::uint64_t>(stride) * index);
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
    std::vector<std::uint32_t> alignments;
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
            plan.alignments.push_back(fetch.alignment);
        }
        plan.alignments.back() = std::max(plan.alignments.back(), fetch.alignment);
        auto& copy = plan.copies.back();
        copy.second = std::max(copy.second, fetch.end);
        plan.copyOf[i] = plan.copies.size() - 1;
        plan.offsets[i] = fetch.begin - copy.first;
    }
    return plan;
}

inline std::optional<std::uint32_t> ZeroFetchVertex(std::span<const ShaderRecompiler::VertexAttribute> attributes) {
    std::uint32_t zeroFrom = 0;
    for (const auto& attribute : attributes) {
        const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
        if (NullVertexDescriptor(attribute) || attribute.fetchIndex != 0 || stride == 0) continue;
        if (VertexBufferOutOfBoundsSelect(attribute) > 1u) return std::nullopt;
        zeroFrom = std::max(zeroFrom, attribute.resource.fields[2]);
    }
    return zeroFrom;
}

inline std::optional<std::uint32_t> ClampDrawIndices(std::span<std::byte> indices, std::uint32_t indexSize, bool skipRestart, std::uint32_t limit) {
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
        if (index > limit) {
            index = limit;
            if (indexSize == 2) {
                const auto value = static_cast<std::uint16_t>(index);
                std::memcpy(indices.data() + offset, &value, sizeof(value));
            } else {
                std::memcpy(indices.data() + offset, &index, sizeof(index));
            }
        }
        highest = std::max(highest.value_or(0u), index);
    }
    return highest;
}

template <typename TIndex>
std::optional<std::uint32_t> HighestDrawIndexOf(std::span<const std::byte> indices, bool skipRestart) {
    constexpr auto restartIndex = std::numeric_limits<TIndex>::max();
    const auto count = indices.size() / sizeof(TIndex);
    const auto* data = indices.data();
    TIndex highest = 0;
    if (!skipRestart) {
        for (std::size_t i = 0; i < count; ++i) {
            TIndex index;
            std::memcpy(&index, data + i * sizeof(TIndex), sizeof(TIndex));
            highest = std::max(highest, index);
        }
        if (count == 0) return std::nullopt;
        return highest;
    }
    TIndex kept = 0;
    for (std::size_t i = 0; i < count; ++i) {
        TIndex index;
        std::memcpy(&index, data + i * sizeof(TIndex), sizeof(TIndex));
        const TIndex valid = index == restartIndex ? TIndex{0} : TIndex(~TIndex{0});
        highest = std::max(highest, static_cast<TIndex>(index & valid));
        kept |= valid;
    }
    if (kept == 0) return std::nullopt;
    return highest;
}

inline std::optional<std::uint32_t> HighestDrawIndex(std::span<const std::byte> indices, std::uint32_t indexSize, bool skipRestart) {
    Require(indexSize == 2 || indexSize == 4, "unsupported index size");
    return indexSize == 2 ? HighestDrawIndexOf<std::uint16_t>(indices, skipRestart) : HighestDrawIndexOf<std::uint32_t>(indices, skipRestart);
}

inline std::uint32_t CompactListRestart(std::span<std::byte> indices, std::uint32_t indexSize, std::uint32_t primitiveSize) {
    Require(indexSize == 2 || indexSize == 4, "unsupported index size");
    Require(primitiveSize != 0, "invalid list primitive size");
    const auto restartIndex = indexSize == 2 ? 0xffffu : 0xffffffffu;
    std::size_t written = 0;
    std::size_t primitiveStart = 0;
    for (std::size_t offset = 0; offset + indexSize <= indices.size(); offset += indexSize) {
        std::uint32_t index = 0;
        if (indexSize == 2) {
            std::uint16_t value = 0;
            std::memcpy(&value, indices.data() + offset, sizeof(value));
            index = value;
        } else {
            std::memcpy(&index, indices.data() + offset, sizeof(index));
        }
        if (index == restartIndex) {
            written = primitiveStart;
            continue;
        }
        std::memmove(indices.data() + written * indexSize, indices.data() + offset, indexSize);
        ++written;
        if (written - primitiveStart == primitiveSize) primitiveStart = written;
    }
    return static_cast<std::uint32_t>(primitiveStart);
}

inline std::vector<std::uint32_t> MeshRestartTable(std::span<const std::byte> indices, std::uint32_t indexSize) {
    Require(indexSize == 2 || indexSize == 4, "unsupported index size");
    const auto restartIndex = indexSize == 2 ? 0xffffu : 0xffffffffu;
    std::vector<std::uint32_t> starts(indices.size() / indexSize);
    std::uint32_t stripStart = 0;
    bool restarted = false;
    for (std::size_t position = 0; position < starts.size(); ++position) {
        std::uint32_t index = 0;
        if (indexSize == 2) {
            std::uint16_t value = 0;
            std::memcpy(&value, indices.data() + position * 2u, sizeof(value));
            index = value;
        } else {
            std::memcpy(&index, indices.data() + position * 4u, sizeof(index));
        }
        if (index == restartIndex) {
            restarted = true;
            stripStart = static_cast<std::uint32_t>(position) + 1u;
        }
        starts[position] = stripStart;
    }
    if (!restarted) starts.clear();
    return starts;
}

inline std::vector<std::size_t> SoloZeroPaddedFetchIndices(const std::vector<VertexFetch>& fetches, const std::vector<std::size_t>& fetchValid) {
    Require(fetchValid.size() == fetches.size(), "fetch validity does not match the fetch count");
    std::vector<std::size_t> solo;
    for (std::size_t i = 0; i < fetches.size(); ++i) {
        if (fetchValid[i] < fetches[i].end - fetches[i].begin) solo.push_back(i);
    }
    return solo;
}

}

#endif
