#pragma once

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcLinkShaders(ShaderRegister* context, ShaderRegister* primitive, const void* reserved, const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType);

}
