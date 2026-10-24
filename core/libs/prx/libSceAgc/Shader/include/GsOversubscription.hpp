#pragma once

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcGetGsOversubscription(ShaderRegister* regs, const Shader* gs, std::uint32_t budget, float factor);

}
