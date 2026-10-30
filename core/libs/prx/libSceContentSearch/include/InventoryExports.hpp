#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceContentSearchAddAvContentCreatedEventCondition();
int APS5_VABI sceContentSearchDisableAvContentCreatedEvent();
int APS5_VABI sceContentSearchEnableAvContentCreatedEvent();
int APS5_VABI sceContentSearchGetContentIdByAvContentCreatedEvent();

}
