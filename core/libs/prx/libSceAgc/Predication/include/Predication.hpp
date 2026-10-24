#ifndef CORE_LIBS_PRX_LIBSCEAGC_PREDICATION_INCLUDE_PREDICATION_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PREDICATION_INCLUDE_PREDICATION_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcSetPacketPredication(uint32_t* packet, uint32_t predication);
int APS5_VABI sceAgcSetRangePredication(uint32_t* start, const volatile uint32_t* end, uint32_t predication);

}

#endif
