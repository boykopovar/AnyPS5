#include <cstdio>
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {
namespace {
float EffectiveMinLod(const GuestTextureResource& resource) {
    if (resource.minLod <= resource.baseLevel * 256u) return 0.0f;
    return std::min(static_cast<float>(resource.minLod) / 256.0f, static_cast<float>(resource.lastLevel));
}

bool DescriptorSingleLevel(std::span<const std::uint32_t> words) {
    return ((words[3] >> 12u) & 0xfu) == ((words[3] >> 16u) & 0xfu);
}

bool FirstLayerDescriptor(std::span<const std::uint32_t> words, std::array<std::uint32_t, 8>& narrowed) {
    if (words.size() != narrowed.size()) return false;
    const auto resource = DecodeTextureResource(words);
    if (resource.dimension != TextureDimension::k2DArray || resource.depthOrLastArray <= resource.baseArray) return false;
    std::copy(words.begin(), words.end(), narrowed.begin());
    narrowed[4] = (narrowed[4] & ~0x1fffu) | resource.baseArray;
    return true;
}

bool MatchesGuestDimension(ShaderRecompiler::DescriptorImageShape shape, TextureDimension dimension) {
    switch (shape) {
        case ShaderRecompiler::DescriptorImageShape::Image1D: return dimension == TextureDimension::k1D;
        case ShaderRecompiler::DescriptorImageShape::Image2D: return dimension == TextureDimension::k2D;
        case ShaderRecompiler::DescriptorImageShape::Image2DArray: return dimension == TextureDimension::k2DArray || dimension == TextureDimension::kCube;
        case ShaderRecompiler::DescriptorImageShape::ImageCube: return dimension == TextureDimension::kCube;
        case ShaderRecompiler::DescriptorImageShape::Image3D: return dimension == TextureDimension::k3D;
        case ShaderRecompiler::DescriptorImageShape::Image1DArray: return dimension == TextureDimension::k1DArray;
        case ShaderRecompiler::DescriptorImageShape::Image2DMultisample: return dimension == TextureDimension::k2DMsaa;
        case ShaderRecompiler::DescriptorImageShape::Image2DMultisampleArray: return dimension == TextureDimension::k2DMsaa;
    }
    throw std::runtime_error("AGC graphics: MatchesGuestDimension encountered an unknown descriptor image shape");
}

bool ArrayLayersServeView(const GuestTextureResource& surface, const GuestTextureResource& view) {
    if (view.dimension == surface.dimension && view.depthOrLastArray == surface.depthOrLastArray) return true;
    if (surface.dimension != TextureDimension::k2DArray) return false;
    if (view.dimension == TextureDimension::k2D) return view.baseArray == 0;
    return view.dimension == TextureDimension::k2DArray && view.depthOrLastArray <= surface.depthOrLastArray;
}

}
}
