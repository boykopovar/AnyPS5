#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <cstdlib>

int main() {
    using namespace AgcDriver::Graphics;
    for (auto format : {VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT}) {
        const unsigned bytes = format == VK_FORMAT_R8_UNORM ? 1 : format == VK_FORMAT_R8G8B8A8_UNORM ? 4 : 8;
        const auto guest = FindGuestTextureFormat(format, bytes);
        if (!guest || ResolveTextureFormat(*guest) != format || BytesPerElement(*guest) != bytes) std::abort();
        if (FindGuestTextureFormat(format, bytes + 1)) std::abort();
    }
    if (FindGuestTextureFormat(VK_FORMAT_B8G8R8A8_UNORM, 4)) std::abort();
    if (FindGuestTextureFormat(VK_FORMAT_UNDEFINED, 4)) std::abort();
    if (FindGuestTextureFormat(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_UNORM, 4) != 56) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SNORM, 4) != 57) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SRGB, 4) != 130) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SRGB, 4) != FindGuestTextureFormat(VK_FORMAT_R8G8B8A8_SRGB, 4)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_A2R10G10B10_UNORM_PACK32, 4) != 50) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_A2R10G10B10_UNORM_PACK32, 4) != FindGuestTextureFormat(VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_R8G8B8A8_UNORM, 4) != 56) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_R16G16B16A16_SFLOAT, 8) != 71) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_UNORM, 8)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SNORM, 8)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SRGB, 1)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SRGB, 2)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SRGB, 8)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_A2R10G10B10_UNORM_PACK32, 1)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_A2R10G10B10_UNORM_PACK32, 2)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_A2R10G10B10_UNORM_PACK32, 8)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_UNDEFINED, 4)) std::abort();
    for (auto uncompressed : {1u, 7u, 14u, 22u, 50u, 56u, 71u, 128u, 130u}) {
        if (IsBlockCompressed(uncompressed)) std::abort();
        if (BlockWidth(uncompressed) != 1u || BlockHeight(uncompressed) != 1u) std::abort();
    }
    for (auto compressed : {169u, 170u, 171u, 172u, 173u, 174u, 175u, 176u, 177u, 178u, 179u, 180u, 181u, 182u}) {
        if (!IsBlockCompressed(compressed)) std::abort();
        if (BlockWidth(compressed) != 4u || BlockHeight(compressed) != 4u) std::abort();
    }
}
