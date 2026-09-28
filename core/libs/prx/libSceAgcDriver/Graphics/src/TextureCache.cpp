#include "prx/libSceAgcDriver/Graphics/include/TextureCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace AgcDriver::Graphics {

TextureCache::TextureCache(const Context& context) : context(context) {
    Require(context.detiler != nullptr, "texture cache requires a device detiler");
}

void TextureCache::trim() {
    for (auto it = entries.begin(); it != entries.end() && (retainedBytes > budget || entries.size() > 64);) {
        if (it->texture.use_count() != 1) {
            ++it;
            continue;
        }
        retainedBytes -= it->snapshot.size() + it->texture->AllocationBytes();
        it = entries.erase(it);
    }
}

std::shared_ptr<Texture> TextureCache::Get(std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components) {
    Require(words.size() == 8, "texture cache descriptor must contain eight DWORDs");
    trim();
    std::array<std::uint32_t, 8> key;
    std::copy(words.begin(), words.end(), key.begin());
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->descriptor != key) continue;
        if (GuestMemory::EqualsCommitted(resource.baseAddress, it->snapshot)) {
            auto result = it->texture;
            entries.splice(entries.end(), entries, it);
            return result;
        }
        retainedBytes -= it->snapshot.size() + it->texture->AllocationBytes();
        entries.erase(it);
        break;
    }
    const auto mips = ComputeMipLayout(resource.tileMode, resource.format, resource.width, resource.height, resource.mipCount);
    const auto layers = resource.dimension == TextureDimension::k2DArray || resource.dimension == TextureDimension::kCube ? resource.depthOrLastArray + 1u : 1u;
    const auto bytes = ComputeSurfaceSize(mips, layers);
    Require(bytes != 0 && bytes <= std::numeric_limits<std::size_t>::max(), "texture cache surface size overflow");
    std::vector<std::byte> snapshot(static_cast<std::size_t>(bytes));
    GuestMemory::ReadCommitted(resource.baseAddress, snapshot);
    auto texture = std::make_shared<Texture>(context, *context.detiler, resource, components, snapshot);
    const auto retained = snapshot.size() + texture->AllocationBytes();
    entries.push_back({key, std::move(snapshot), texture});
    retainedBytes += retained;
    trim();
    return texture;
}

}
