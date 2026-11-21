#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_FMASKTEXTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_FMASKTEXTURE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"

namespace AgcDriver::Graphics {

std::shared_ptr<Texture> StandaloneFmaskTexture_nid_no_patch(const Context& context, const GuestTextureResource& resource, VkComponentMapping components);

}

#endif
