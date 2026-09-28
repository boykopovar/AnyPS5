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
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SRGB, 4) != 130) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_R8G8B8A8_UNORM, 4) != 56) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_R16G16B16A16_SFLOAT, 8) != 71) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_UNORM, 8)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_B8G8R8A8_SRGB, 1)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8)) std::abort();
    if (FindGuestColorTargetFormat(VK_FORMAT_UNDEFINED, 4)) std::abort();
}
