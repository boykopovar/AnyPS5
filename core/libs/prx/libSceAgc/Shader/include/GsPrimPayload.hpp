#pragma once

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcGetGsPrimPayload(std::uint32_t* payload, const Shader* gs);

}
