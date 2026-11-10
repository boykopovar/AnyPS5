#pragma once

#include "SceTypes.hpp"

struct DiscId;

struct DiscId2;

extern "C" {

int APS5_VABI sceDiscIdGet(DiscId* discId);
int APS5_VABI sceDiscIdGet2(DiscId2* discId, std::uint32_t flags);

}
