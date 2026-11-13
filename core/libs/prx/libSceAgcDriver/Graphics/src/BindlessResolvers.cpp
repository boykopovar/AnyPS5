#include "prx/libSceAgcDriver/Graphics/include/BindlessResolvers.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestSamplerResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "Optimization/DescriptorBindingBuilder.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "BindlessImageAbi.hpp"
#include "DeviceImageTableAbi.hpp"
#include "ImageTableAbi.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {

namespace {

namespace Abi = ShaderRecompiler::DeviceImageTableAbi;
namespace Heap = ShaderRecompiler::BindlessImageAbi;

struct Prepared {
    std::array<std::uint32_t, Abi::DescriptorWords> words{};
    GuestTextureResource resource{};
    std::uint64_t guestBytes = 0;
    bool firstLayer = false;
    bool depthCompare = false;
    std::uint32_t mode = 0;
    std::uint32_t samplerClass = 0;
    bool integerImage = false;
    bool integerBorder = false;
    std::array<std::uint32_t, Abi::SamplerWords> sampler{};
};

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

ShaderRecompiler::DescriptorImageShape ShapeOf(ShaderRecompiler::RdnaImageDimension dimension) {
    using Dimension = ShaderRecompiler::RdnaImageDimension;
    using Shape = ShaderRecompiler::DescriptorImageShape;
    switch (dimension) {
        case Dimension::Dim1D: return Shape::Image1D;
        case Dimension::Dim1DArray: return Shape::Image1DArray;
        case Dimension::Dim2D: return Shape::Image2D;
        case Dimension::Dim2DArray: return Shape::Image2DArray;
        case Dimension::Dim3D: return Shape::Image3D;
        case Dimension::Dim2DMsaa: return Shape::Image2DMultisample;
        default: break;
    }
    throw std::runtime_error("AGC graphics: a bindless image mode has no descriptor image shape");
}

}

bool GuestBindlessMemory::Read(std::uint64_t address, std::span<std::uint32_t> words) {
    const auto bytes = words.size() * sizeof(std::uint32_t);
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) return false;
    GuestMemory::ReadCommittedUnsynced(address, std::as_writable_bytes(words));
    return true;
}

std::uint64_t GuestBindlessMemory::Collect(std::uint64_t address, std::size_t bytes) {
    return GuestMemory::CollectWrites(address, bytes);
}

bool GuestBindlessMemory::Unchanged(std::uint64_t address, std::size_t bytes, std::uint64_t generation) {
    return generation != 0 && GuestMemory::UnchangedSince(address, bytes, generation);
}

std::uint64_t GuestBindlessMemory::MappingSerial() {
    return GuestMemory::ForgetSerial();
}

bool GuestBindlessMemory::Mapped(std::uint64_t address, std::size_t bytes) {
    return GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes) || !GuestMemory::CommittedRanges(address, bytes).empty();
}

DriverBindlessResolver::DriverBindlessResolver(const Context& context, InterfaceLookup lookup) : context(context), lookup(lookup != nullptr ? std::move(lookup) : InterfaceLookup(&ShaderRecompiler::ResourceMaterializer::FindBindlessInterface)) {
}

BindlessPrepared DriverBindlessResolver::Prepare(const DeviceImageKeys::Key& key) {
    using Materializer = ShaderRecompiler::ResourceMaterializer;
    const auto id = static_cast<std::uint64_t>(key[Abi::InterfaceWord]) | (static_cast<std::uint64_t>(key[Abi::InterfaceWord + 1u]) << 32u);
    const auto found = lookup(id);
    if (found == nullptr) throw std::runtime_error("AGC graphics: a bindless key names the image interface " + Hex(id) + ", which no compiled program registered");
    ShaderRecompiler::DescriptorValue descriptor;
    descriptor.dwordCount = Abi::DescriptorWords;
    std::copy_n(key.begin(), Abi::DescriptorWords, descriptor.dwords.begin());
    const auto match = Materializer::TryRuntimeImageMode(found->image, descriptor, found->modes);
    if (!match.mode.has_value()) {
        const auto reason = ShaderRecompiler::ImageTableAbi::PoisonReasonName(match.reason);
        throw BindlessKeyRefusal("AGC graphics: the T# matches no runtime mode of its image interface (" + std::string(reason) + (match.detail.empty() ? std::string() : ": " + match.detail) + ")");
    }
    const auto& mode = found->modes.at(*match.mode);
    BindlessPrepared prepared;
    prepared.key = key;
    if (mode.constantSwizzle) {
        prepared.settled = DeviceImageValue{0u, *match.mode, 0u, 0u};
        return prepared;
    }
    if (found->image.resourceClass == ShaderRecompiler::ImageResourceClass::Storage) throw BindlessKeyRefusal("AGC graphics: a storage image read or written through the bindless heap is not implemented (storage tables stay on their per-draw binding)");
    if (mode.dimension == ShaderRecompiler::RdnaImageDimension::Dim2DMsaa || mode.dimension == ShaderRecompiler::RdnaImageDimension::Dim2DMsaaArray) throw BindlessKeyRefusal("AGC graphics: a multisampled image read through the bindless heap is not implemented");
    if ((mode.emulatedCompare & ShaderRecompiler::EmulatedCompare::Enabled) != 0u) throw BindlessKeyRefusal("AGC graphics: comparison sampling of a color texture through the bindless heap is not implemented");
    auto state = std::make_shared<Prepared>();
    std::copy_n(key.begin(), Abi::DescriptorWords, state->words.begin());
    if (found->image.r128) {
        state->words = R128TextureWords(state->words);
        static const bool stats = [] { const char* value = std::getenv("APS5_BINDLESS_STATS"); return value != nullptr && value[0] != '\0' && value[0] != '0'; }();
        static std::atomic<std::uint32_t> reports{0};
        if (stats && reports.fetch_add(1u, std::memory_order_relaxed) < 16u) {
            std::fprintf(stderr, "[bindless] 128-bit T# %08x %08x %08x %08x read as %08x %08x %08x %08x 0 %08x 0 0 (MAX_MIP from LAST_LEVEL%s)\n",
                key[0], key[1], key[2], key[3], state->words[0], state->words[1], state->words[2], state->words[3], state->words[5], state->words[3] != key[3] ? ", rebased to the view's base level" : "");
        }
    }
    state->resource = DecodeTextureResource(state->words);
    const auto shape = ShapeOf(mode.dimension);
    state->firstLayer = shape == ShaderRecompiler::DescriptorImageShape::Image2D && state->resource.dimension == TextureDimension::k2DArray;
    if (!state->firstLayer && !MatchesGuestDimension(shape, state->resource.dimension)) throw BindlessKeyRefusal("AGC graphics: guest texture dimension disagrees with the bindless mode's image shape (shape " + std::to_string(static_cast<int>(shape)) + ", dimension " + std::to_string(static_cast<int>(state->resource.dimension)) + ")");
    static_cast<void>(ResolveTextureFormat(state->resource.format));
    state->guestBytes = DescribeSurface(state->resource).guestBytes;
    const auto address = state->resource.baseAddress;
    const auto bytes = static_cast<std::size_t>(state->guestBytes);
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes) && GuestMemory::CommittedRanges(address, bytes, false).empty()) throw std::runtime_error("AGC graphics: the bindless entry's surface at " + Hex(address) + " (" + Hex(state->guestBytes) + " bytes) is not mapped");
    state->depthCompare = mode.depthCompare;
    state->mode = *match.mode;
    state->samplerClass = key[Abi::SamplerClassWord];
    const auto integer = [](const ShaderRecompiler::ImageResource& candidate) { return candidate.numericClass == ShaderRecompiler::IrTextureNumericClass::Uint || candidate.numericClass == ShaderRecompiler::IrTextureNumericClass::Sint; };
    state->integerImage = integer(mode);
    bool integerModes = false;
    bool floatModes = false;
    for (const auto& candidate : found->modes) {
        if (Materializer::RequiresPointSampler(candidate) != Materializer::RequiresPointSampler(mode)) continue;
        (integer(candidate) ? integerModes : floatModes) = true;
    }
    state->integerBorder = integerModes && !floatModes;
    std::copy_n(key.begin() + Abi::SamplerWord, Abi::SamplerWords, state->sampler.begin());
    if (state->samplerClass != Heap::NoSamplerClass) {
        if (state->samplerClass == Heap::TableSamplerClass) {
            if ((state->sampler[0] & (1u << 15u)) != 0u) throw BindlessKeyRefusal("AGC graphics: a sampler table entry with unnormalized coordinates is not implemented for table images");
            if (((state->sampler[0] >> 29u) & 3u) != 0u) throw BindlessKeyRefusal("AGC graphics: a sampler table entry with a min or max reduction is not implemented for table images");
        } else if (state->samplerClass != Heap::DirectSamplerClass) {
            throw BindlessKeyRefusal("AGC graphics: a bindless key names the unknown sampler class " + Hex(state->samplerClass));
        }
        if (Materializer::RequiresPointSampler(mode)) state->sampler[2] = ShaderRecompiler::PointFilteredSamplerWord(state->sampler[0], state->sampler[2]);
    }
    prepared.address = address;
    prepared.bytes = state->guestBytes;
    prepared.generation = GuestMemory::CollectWrites(address, bytes);
    prepared.state = std::move(state);
    return prepared;
}

BindlessResolution DriverBindlessResolver::Commit(const BindlessPrepared& prepared) {
    const auto state = std::static_pointer_cast<Prepared>(prepared.state);
    Require(state != nullptr, "a bindless entry reached commit without its prepared state");
    auto texture = CachedTableTexture(context, state->words, state->resource, state->guestBytes, state->depthCompare);
    Require(texture != nullptr, "the texture cache returned no texture for a bindless entry");
    BindlessResolution resolution;
    resolution.view = state->firstLayer ? texture->FirstLayerView() : texture->View();
    Require(resolution.view != VK_NULL_HANDLE, "a bindless entry's texture has no view of the shape its mode reads");
    resolution.layout = texture->Layout();
    Require(resolution.layout == VK_IMAGE_LAYOUT_GENERAL || resolution.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "a bindless entry's texture is in image layout " + std::to_string(static_cast<int>(resolution.layout)) + ", which the heap's sampled descriptors cannot read");
    resolution.value = DeviceImageValue{0u, state->mode, 0u, 0u};
    resolution.address = prepared.address;
    resolution.bytes = prepared.bytes;
    resolution.generation = prepared.generation;
    resolution.dcc = state->resource.dccAddress != 0u;
    resolution.perUse = texture->IsDepthCopy() || texture->RefreshedPerUse() || resolution.dcc;
    if (state->samplerClass != Heap::NoSamplerClass) {
        const bool table = state->samplerClass == Heap::TableSamplerClass;
        const bool integerBorder = !table && state->integerBorder;
        auto descriptor = DecodeSamplerResource(state->sampler, false, !table, integerBorder);
        descriptor.compareEnable = state->depthCompare;
        if (table) {
            Sampler::Validate(context, descriptor);
            Require(!Sampler::ReadsOpaqueBlackBorder(descriptor), "a sampler table entry with an opaque black border needs dynamic texture BC-swizzle pairing, which is not implemented");
            Require(descriptor.addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER && descriptor.addressModeV != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER && descriptor.addressModeW != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, "a sampler table entry with a border needs dynamic texture numeric-class pairing, which is not implemented");
            Require(!descriptor.nonSeamlessCube || context.nonSeamlessCubeMap, "a sampler table entry that disables seamless cube filtering needs VK_EXT_non_seamless_cube_map because dynamic texture/sampler pairing is not implemented");
        }
        const bool border = descriptor.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || descriptor.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || descriptor.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        Require(!border || integerBorder == state->integerImage, "an image table entry's sampler border color numeric class disagrees with its sampled image class, which needs dynamic pairing and is not implemented");
        Require(!SingleLevelSamplerWords(state->sampler, true, false).has_value(), "an image table entry paired with anisotropic ANISO_OVERRIDE needs dynamic texture mip-level pairing, which is not implemented");
        std::shared_ptr<Sampler> sampler = context.samplerCache != nullptr ? context.samplerCache->Get(context, state->sampler, state->depthCompare, false, !table, integerBorder) : std::make_shared<Sampler>(context, descriptor);
        const std::array<std::shared_ptr<Sampler>, 1> paired{sampler};
        RequireFilterMinmax(context, texture->ViewFormat(), 1u, paired);
        RequireBorderSwizzle(state->resource.bcSwizzle, 1u, paired);
        RequireNonSeamlessCube(state->resource.dimension == TextureDimension::kCube, 1u, paired);
        resolution.sampler = sampler->Handle();
        resolution.samplerOwner = std::move(sampler);
    }
    resolution.owner = std::move(texture);
    return resolution;
}

bool DriverBindlessResolver::RefreshGeneration(BindlessPrepared& prepared) {
    if (prepared.settled.has_value() || !prepared.refusal.empty()) return prepared.refusal.empty();
    if (prepared.state == nullptr || prepared.bytes == 0u) return false;
    prepared.generation = GuestMemory::CollectWrites(prepared.address, static_cast<std::size_t>(prepared.bytes));
    return true;
}

bool DriverBindlessResolver::Current(const BindlessResolution& resolution) {
    const auto texture = std::static_pointer_cast<Texture>(resolution.owner);
    return texture == nullptr || !texture->Released();
}

}
