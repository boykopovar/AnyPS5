#include <cstdio>
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <bit>
#include <bitset>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <span>
#include <array>
#include <sstream>
#include <string_view>

namespace AgcDriver::Graphics {
namespace {
VkBlendFactor colorFactorWithOpaqueDestination(VkBlendFactor factor) {
    switch (factor) {
        case VK_BLEND_FACTOR_DST_ALPHA: return VK_BLEND_FACTOR_ONE;
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ZERO;
        case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return VK_BLEND_FACTOR_ZERO;
        default: return factor;
    }
}

VkBlendFactor alphaFactorWithOpaqueDestination(VkBlendFactor factor) {
    switch (factor) {
        case VK_BLEND_FACTOR_DST_ALPHA: case VK_BLEND_FACTOR_DST_COLOR: return VK_BLEND_FACTOR_ONE;
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ZERO;
        default: return factor;
    }
}

bool secondSource(VkBlendFactor factor) {
    return factor == VK_BLEND_FACTOR_SRC1_COLOR || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR || factor == VK_BLEND_FACTOR_SRC1_ALPHA || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
}

VkBlendOp blendOp(std::uint32_t value) {
    switch (value) {
        case 0: return VK_BLEND_OP_ADD;
        case 1: return VK_BLEND_OP_SUBTRACT;
        case 2: return VK_BLEND_OP_MIN;
        case 3: return VK_BLEND_OP_MAX;
        case 4: return VK_BLEND_OP_REVERSE_SUBTRACT;
        default: throw std::runtime_error("AGC graphics: unsupported blend operation " + std::to_string(value));
    }
}

constexpr std::uint8_t TermComplement = 0x80u;
constexpr std::uint8_t TermUnsupported = 0xffu;

std::uint8_t exportTerm(std::uint32_t component) {
    return static_cast<std::uint8_t>(1u + component);
}

std::uint8_t destinationTerm(std::uint32_t component) {
    return static_cast<std::uint8_t>(5u + component);
}

std::uint8_t guestBlendTerm(VkBlendFactor factor, std::uint32_t component) {
    switch (factor) {
        case VK_BLEND_FACTOR_ZERO: return 0u;
        case VK_BLEND_FACTOR_ONE: return TermComplement;
        case VK_BLEND_FACTOR_SRC_COLOR: return exportTerm(component);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return TermComplement | exportTerm(component);
        case VK_BLEND_FACTOR_SRC_ALPHA: return exportTerm(0u);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return TermComplement | exportTerm(0u);
        case VK_BLEND_FACTOR_DST_COLOR: return destinationTerm(component);
        case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return TermComplement | destinationTerm(component);
        case VK_BLEND_FACTOR_DST_ALPHA: return destinationTerm(0u);
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return TermComplement | destinationTerm(0u);
        case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return component == 0u ? TermComplement : TermUnsupported;
        default: return TermUnsupported;
    }
}

std::uint8_t routedTerm(std::uint32_t route, std::uint32_t component) {
    namespace Code = ShaderRecompiler::ReversedBlendCode;
    switch (route) {
        case Code::Own: return exportTerm(component);
        case Code::Component0: return exportTerm(0u);
        case Code::Zero: return 0u;
        default: return TermComplement;
    }
}

std::uint8_t hostBlendTerm(VkBlendFactor factor, std::uint32_t component, const std::array<std::uint32_t, 4>& color, const std::array<std::uint32_t, 4>& second) {
    switch (factor) {
        case VK_BLEND_FACTOR_ZERO: return 0u;
        case VK_BLEND_FACTOR_ONE: return TermComplement;
        case VK_BLEND_FACTOR_SRC_COLOR: return routedTerm(color[component], component);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return TermComplement ^ routedTerm(color[component], component);
        case VK_BLEND_FACTOR_SRC_ALPHA: return routedTerm(color[3], 3u);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return TermComplement ^ routedTerm(color[3], 3u);
        case VK_BLEND_FACTOR_DST_COLOR: return destinationTerm(component);
        case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return TermComplement | destinationTerm(component);
        case VK_BLEND_FACTOR_DST_ALPHA: return destinationTerm(3u);
        case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return TermComplement | destinationTerm(3u);
        case VK_BLEND_FACTOR_SRC1_COLOR: return routedTerm(second[component], component);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return TermComplement ^ routedTerm(second[component], component);
        case VK_BLEND_FACTOR_SRC1_ALPHA: return routedTerm(second[3], 3u);
        case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return TermComplement ^ routedTerm(second[3], 3u);
        default: return TermUnsupported;
    }
}

struct BlendRouting {
    std::array<VkBlendFactor, 4> factors;
    std::uint32_t code;
};

std::optional<BlendRouting> routeReversedBlend(const std::array<VkBlendFactor, 2>& color, const std::array<VkBlendFactor, 2>& alpha) {
    namespace Code = ShaderRecompiler::ReversedBlendCode;
    static constexpr std::array<VkBlendFactor, 14> colorFactors{
        VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_SRC1_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA};
    static constexpr std::array<VkBlendFactor, 8> alphaFactors{
        VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA};
    std::array<std::array<std::uint8_t, 2>, 4> wanted{};
    std::uint32_t replaceable = 0;
    for (std::uint32_t component = 0; component < 4; ++component) {
        for (std::size_t position = 0; position < 2; ++position) {
            wanted[component][position] = guestBlendTerm(component == 0u ? alpha[position] : color[position], component);
            if (wanted[component][position] == TermUnsupported) return std::nullopt;
        }
        if (wanted[component][0] == TermComplement) replaceable |= 1u << component;
    }
    const auto search = [&](bool routed) -> std::optional<BlendRouting> {
        const auto usable = [&](VkBlendFactor factor) { return routed || !secondSource(factor); };
        const std::uint32_t routeCount = routed ? 4u : 1u;
        std::uint32_t ones = 0;
        do {
            std::array<std::uint32_t, 4> colorRoutes{};
            for (std::uint32_t component = 0; component < 4; ++component) colorRoutes[component] = ((ones >> component) & 1u) != 0 ? Code::One : Code::Own;
            const auto matches = [&](VkBlendFactor factor, std::size_t position, std::uint32_t component, const std::array<std::uint32_t, 4>& second) {
                const auto term = hostBlendTerm(factor, component, colorRoutes, second);
                if (position == 1 || colorRoutes[component] == Code::Own) return term == wanted[component][position];
                return term == exportTerm(component);
            };
            for (std::uint32_t secondAlpha = 0; secondAlpha < routeCount; ++secondAlpha) {
                const std::array<std::uint32_t, 4> second{Code::Own, Code::Own, Code::Own, secondAlpha};
                std::array<std::optional<VkBlendFactor>, 2> alphaFactor{};
                for (std::size_t position = 0; position < 2; ++position) {
                    const auto found = std::find_if(alphaFactors.begin(), alphaFactors.end(), [&](VkBlendFactor factor) { return usable(factor) && matches(factor, position, 3u, second); });
                    if (found != alphaFactors.end()) alphaFactor[position] = *found;
                }
                if (!alphaFactor[0] || !alphaFactor[1]) continue;
                std::array<std::array<std::array<std::uint8_t, 3>, colorFactors.size()>, 2> masks{};
                for (std::size_t position = 0; position < 2; ++position) {
                    for (std::size_t index = 0; index < colorFactors.size(); ++index) {
                        if (!usable(colorFactors[index])) continue;
                        for (std::uint32_t component = 0; component < 3; ++component) {
                            auto routes = second;
                            for (std::uint32_t route = 0; route < routeCount; ++route) {
                                routes[component] = route;
                                if (matches(colorFactors[index], position, component, routes)) masks[position][index][component] |= static_cast<std::uint8_t>(1u << route);
                            }
                        }
                    }
                }
                for (std::size_t source = 0; source < colorFactors.size(); ++source) {
                    for (std::size_t destination = 0; destination < colorFactors.size(); ++destination) {
                        std::uint32_t code = Code::Enable;
                        for (std::uint32_t component = 0; component < 4; ++component) {
                            const auto shared = component == 3u ? 1u << secondAlpha : static_cast<std::uint32_t>(masks[0][source][component] & masks[1][destination][component]);
                            if (shared == 0u) {
                                code = 0u;
                                break;
                            }
                            const auto route = static_cast<std::uint32_t>(std::countr_zero(shared));
                            code |= (colorRoutes[component] << (Code::ColorShift + Code::RouteBits * component)) | (route << (Code::SecondShift + Code::RouteBits * component));
                        }
                        if (code != 0u) return BlendRouting{{colorFactors[source], colorFactors[destination], *alphaFactor[0], *alphaFactor[1]}, routed ? code : 0u};
                    }
                }
            }
            ones = (ones - replaceable) & replaceable;
        } while (routed && ones != 0u);
        return std::nullopt;
    };
    if (auto native = search(false)) return native;
    return search(true);
}

struct DecodedColorFormat {
    VkFormat format;
    std::uint32_t elementBytes;
    std::uint8_t componentMapping = 0xe4u;
    ShaderRecompiler::ColorExportPacking packing = ShaderRecompiler::ColorExportPacking::None;
};

bool UintPacked(ShaderRecompiler::ColorExportPacking packing) {
    return packing == ShaderRecompiler::ColorExportPacking::Uint || packing == ShaderRecompiler::ColorExportPacking::UintClamp8 || packing == ShaderRecompiler::ColorExportPacking::UintClamp16 || packing == ShaderRecompiler::ColorExportPacking::UintMask8 || packing == ShaderRecompiler::ColorExportPacking::UintMask16;
}

const char* PackedColorTargetName(ShaderRecompiler::ColorExportPacking packing) {
    if (packing == ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4) return "4_4_4_4 unorm";
    if (UintPacked(packing)) return "unsigned integer";
    return packing == ShaderRecompiler::ColorExportPacking::Float11_11_10 ? "11_11_10 float" : "10_11_11 unorm";
}

ShaderRecompiler::ColorExportPacking NarrowUintPacking(VkFormat format, ShaderRecompiler::ColorExportPacking eight, ShaderRecompiler::ColorExportPacking sixteen) {
    if (format == VK_FORMAT_R8_UINT || format == VK_FORMAT_R8G8_UINT || format == VK_FORMAT_R8G8B8A8_UINT) return eight;
    if (format == VK_FORMAT_R16_UINT || format == VK_FORMAT_R16G16_UINT || format == VK_FORMAT_R16G16B16A16_UINT) return sixteen;
    return ShaderRecompiler::ColorExportPacking::Uint;
}

bool WordPacked(ShaderRecompiler::ColorExportPacking packing) {
    return packing == ShaderRecompiler::ColorExportPacking::Unorm10_11_11 || packing == ShaderRecompiler::ColorExportPacking::Float11_11_10 || packing == ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4;
}

DecodedColorFormat DecodeColorFormatRaw(std::uint32_t format, std::uint32_t number, std::uint32_t swap) {
    constexpr std::uint32_t unorm = 0, snorm = 1, sint = 5, srgb = 6, floating = 7;
    constexpr std::uint32_t uint = 4;
    const auto fail = [&]() -> DecodedColorFormat {
        throw std::runtime_error("AGC graphics: unsupported color format " + std::to_string(format) + " number type " + std::to_string(number) + " component swap " + std::to_string(swap));
    };
    const bool alternate = swap == 1;
    const auto single = [&](VkFormat vkFormat, std::uint32_t bytes) { return DecodedColorFormat{vkFormat, bytes, static_cast<std::uint8_t>((0xe4u & ~3u) | swap)}; };
    if (swap > 1 && format != 1 && format != 2 && format != 4 && format != 7 && format != 10 && format != 12 && format != 16 && format != 19) return fail();
    switch (format) {
        case 1:
            if (number == unorm) return single(VK_FORMAT_R8_UNORM, 1);
            if (number == snorm) return single(VK_FORMAT_R8_SNORM, 1);
            if (number == uint) return single(VK_FORMAT_R8_UINT, 1);
            if (number == srgb && swap == 0) return single(VK_FORMAT_R8_SRGB, 1);
            return fail();
        case 2:
            if (number == unorm) return single(VK_FORMAT_R16_UNORM, 2);
            if (number == snorm) return single(VK_FORMAT_R16_SNORM, 2);
            if (number == floating) return single(VK_FORMAT_R16_SFLOAT, 2);
            if (number == uint) return single(VK_FORMAT_R16_UINT, 2);
            return fail();
        case 3: {
            if (swap > 1) return fail();
            const auto mapping = static_cast<std::uint8_t>(alternate ? 0xecu : 0xe4u);
            if (number == unorm) return {VK_FORMAT_R8G8_UNORM, 2, mapping};
            if (number == snorm) return {VK_FORMAT_R8G8_SNORM, 2, mapping};
            if (number == uint) return {VK_FORMAT_R8G8_UINT, 2, mapping};
            return fail();
        }
        case 4:
            if (number == floating) return single(VK_FORMAT_R32_SFLOAT, 4);
            if (number == uint) return single(VK_FORMAT_R32_UINT, 4);
            if (number == sint) return single(VK_FORMAT_R32_SINT, 4);
            return fail();
        case 5: {
            if (swap > 1) return fail();
            const auto mapping = static_cast<std::uint8_t>(alternate ? 0xecu : 0xe4u);
            if (number == floating) return {VK_FORMAT_R16G16_SFLOAT, 4, mapping};
            if (number == unorm) return {VK_FORMAT_R16G16_UNORM, 4, mapping};
            if (number == snorm) return {VK_FORMAT_R16G16_SNORM, 4, mapping};
            if (number == uint) return {VK_FORMAT_R16G16_UINT, 4, mapping};
            return fail();
        }
        case 6:
            if (swap != 0 || (number != floating && number != unorm)) return fail();
            if (number == unorm) return {VK_FORMAT_R32_UINT, 4, 0xe4u, ShaderRecompiler::ColorExportPacking::Unorm10_11_11};
            return {VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4};
        case 7:
            if ((swap != 0 && swap != 2) || number != floating) return fail();
            return {VK_FORMAT_R32_UINT, 4, static_cast<std::uint8_t>(swap == 2 ? 0xc6u : 0xe4u), ShaderRecompiler::ColorExportPacking::Float11_11_10};
        case 9:
            if (number != unorm) return fail();
            return {alternate ? VK_FORMAT_A2R10G10B10_UNORM_PACK32 : VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4};
        case 10: {
            const auto reversed = static_cast<std::uint8_t>(swap == 2 ? 0x1bu : swap == 3 ? 0x93u : 0xe4u);
            if (number == unorm) return {alternate ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, 4, reversed};
            if (number == snorm) return {alternate ? VK_FORMAT_B8G8R8A8_SNORM : VK_FORMAT_R8G8B8A8_SNORM, 4, reversed};
            if (number == srgb) return {alternate ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB, 4, reversed};
            if (number == uint && !alternate) return {VK_FORMAT_R8G8B8A8_UINT, 4, reversed};
            return fail();
        }
        case 11:
            if (swap != 0) return fail();
            if (number == floating) return {VK_FORMAT_R32G32_SFLOAT, 8};
            if (number == uint) return {VK_FORMAT_R32G32_UINT, 8};
            return fail();
        case 12: {
            const auto reversed = static_cast<std::uint8_t>(alternate ? 0xc6u : swap == 2 ? 0x1bu : swap == 3 ? 0x93u : 0xe4u);
            if (number == floating) return {VK_FORMAT_R16G16B16A16_SFLOAT, 8, reversed};
            if (number == unorm) return {VK_FORMAT_R16G16B16A16_UNORM, 8, reversed};
            if (number == snorm) return {VK_FORMAT_R16G16B16A16_SNORM, 8, reversed};
            if (number == uint) return {VK_FORMAT_R16G16B16A16_UINT, 8, reversed};
            return fail();
        }
        case 14:
            if (swap != 0) return fail();
            if (number == floating) return {VK_FORMAT_R32G32B32A32_SFLOAT, 16};
            if (number == uint) return {VK_FORMAT_R32G32B32A32_UINT, 16};
            return fail();
        case 16:
            if ((swap != 0 && swap != 2) || number != unorm) return fail();
            return {VK_FORMAT_R5G6B5_UNORM_PACK16, 2, static_cast<std::uint8_t>(swap == 2 ? 0xe4u : 0xc6u)};
        case 17:
            if (swap != 0 || number != unorm) return fail();
            return {VK_FORMAT_A1R5G5B5_UNORM_PACK16, 2, 0xc6u};
        case 19:
            if (swap != 2 || number != unorm) return fail();
            return {VK_FORMAT_R16_UINT, 2, 0x1bu, ShaderRecompiler::ColorExportPacking::Unorm4_4_4_4};
        default:
            return fail();
    }
}

bool intersectViewportScissor(VkRect2D& result, const Registers& registers, std::uint32_t offset) {
    const auto tl = read(registers, offset);
    const auto br = read(registers, offset + 1);
    if ((tl & 0x8000u) != 0 || (br & 0x80008000u) != 0) return false;
    const auto x = tl & 0xffffu;
    const auto y = (tl >> 16u) & 0x7fffu;
    const auto right = br & 0xffffu;
    const auto bottom = br >> 16u;
    const auto oldRight = static_cast<std::uint32_t>(result.offset.x) + result.extent.width;
    const auto oldBottom = static_cast<std::uint32_t>(result.offset.y) + result.extent.height;
    const auto left = std::max(static_cast<std::uint32_t>(result.offset.x), x);
    const auto top = std::max(static_cast<std::uint32_t>(result.offset.y), y);
    result.offset = {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)};
    result.extent = {std::min(oldRight, right) > left ? std::min(oldRight, right) - left : 0, std::min(oldBottom, bottom) > top ? std::min(oldBottom, bottom) - top : 0};
    return true;
}

void unite(VkRect2D& bounds, const VkRect2D& rect) {
    if (rect.extent.width == 0 || rect.extent.height == 0) return;
    if (bounds.extent.width == 0 || bounds.extent.height == 0) {
        bounds = rect;
        return;
    }
    const auto left = std::min(bounds.offset.x, rect.offset.x);
    const auto top = std::min(bounds.offset.y, rect.offset.y);
    const auto right = std::max(static_cast<std::int64_t>(bounds.offset.x) + bounds.extent.width, static_cast<std::int64_t>(rect.offset.x) + rect.extent.width);
    const auto bottom = std::max(static_cast<std::int64_t>(bounds.offset.y) + bounds.extent.height, static_cast<std::int64_t>(rect.offset.y) + rect.extent.height);
    bounds = {{left, top}, {static_cast<std::uint32_t>(right - left), static_cast<std::uint32_t>(bottom - top)}};
}

void intersect(VkRect2D& result, const Registers& registers, std::uint32_t offset, bool screen) {
    const auto tl = read(registers, offset);
    const auto br = read(registers, offset + 1);
    if (!screen) Require((tl & 0x8000u) == 0 && (br & 0x80008000u) == 0, "scissor reserved bits are unsupported");
    const auto x = tl & 0xffffu;
    const auto y = (tl >> 16u) & (screen ? 0xffffu : 0x7fffu);
    const auto right = br & 0xffffu;
    const auto bottom = br >> 16u;
    const auto oldRight = static_cast<std::uint32_t>(result.offset.x) + result.extent.width;
    const auto oldBottom = static_cast<std::uint32_t>(result.offset.y) + result.extent.height;
    const auto left = std::max(static_cast<std::uint32_t>(result.offset.x), x);
    const auto top = std::max(static_cast<std::uint32_t>(result.offset.y), y);
    result.offset = {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)};
    result.extent = {std::min(oldRight, right) > left ? std::min(oldRight, right) - left : 0, std::min(oldBottom, bottom) > top ? std::min(oldBottom, bottom) - top : 0};
}

struct ExpandedPlanes {
    bool depth = false;
    bool stencil = false;
};

ExpandedPlanes outOfPlacePlanes(const Registers& cx) {
    const auto control = find(cx, 0x000);
    if (control == cx.end() || (control->second & 0x60u) == 0) return {};
    const auto value = [&](std::uint32_t offset) {
        const auto found = find(cx, offset);
        return found == cx.end() ? 0u : found->second;
    };
    const auto base = [&](std::uint32_t low, std::uint32_t high) { return (static_cast<std::uint64_t>(value(high) & 0xffu) << 40u) | (static_cast<std::uint64_t>(value(low)) << 8u); };
    const auto view = value(0x002);
    return {(value(0x010) & 3u) != 0 && (view & 0x01000000u) == 0 && base(0x012, 0x01a) != base(0x014, 0x01c), (value(0x011) & 1u) != 0 && (view & 0x02000000u) == 0 && base(0x013, 0x01b) != base(0x015, 0x01d)};
}

}

VkExtent2D SampleLocationGrid(const State& state) {
    return state.sampleLocationGrid ? VkExtent2D{2, 2} : VkExtent2D{1, 1};
}

std::optional<SampleOrder> SameSetSampleOrder(const State& state) {
    if (!state.customSampleLocations || !state.sampleLocationGrid || state.samples < 2u || state.samples > 8u) return std::nullopt;
    SampleOrder order{};
    for (std::uint32_t pixel = 0; pixel < order.size(); ++pixel) {
        std::uint32_t word = KeptSampleOrder;
        std::uint32_t taken = 0;
        for (std::uint32_t sample = 0; sample < state.samples; ++sample) {
            std::uint32_t guest = 0;
            while (guest < state.samples && (((taken >> guest) & 1u) != 0u || state.sampleLocations[pixel * 8u + guest] != state.sampleLocations[sample])) ++guest;
            if (guest == state.samples) return std::nullopt;
            taken |= 1u << guest;
            word = (word & ~(0xfu << (sample * 4u))) | (guest << (sample * 4u));
        }
        order[pixel] = word;
    }
    return order;
}

bool ReordersSamples(const SampleOrder& order) {
    return std::ranges::any_of(order, [](std::uint32_t word) { return word != 0u && word != KeptSampleOrder; });
}

std::uint32_t InvertSampleOrder(std::uint32_t order) {
    std::uint32_t inverse = 0;
    for (std::uint32_t sample = 0; sample < 8u; ++sample) inverse |= sample << (((order >> (sample * 4u)) & 0x7u) * 4u);
    return inverse;
}

std::uint32_t HostSampleBits(std::uint32_t guestBits, std::uint32_t order) {
    std::uint32_t host = 0;
    for (std::uint32_t sample = 0; sample < 8u; ++sample) host |= ((guestBits >> ((order >> (sample * 4u)) & 0x7u)) & 1u) << sample;
    return host | (guestBits & ~0xffu);
}

bool SampleMaskFollowsOrder(const State& state, const SampleOrder& order) {
    if (!ReordersSamples(order) || !PartialSampleMask(state) || !UniformSampleMask(state)) return false;
    const auto full = (1u << state.samples) - 1u;
    const auto mask = state.quadSampleMask[0] & full;
    return std::ranges::any_of(order, [&](std::uint32_t word) { return (HostSampleBits(mask, word) & full) != mask; });
}

}
