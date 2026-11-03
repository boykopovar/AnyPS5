#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceFrontPanelDisplayInitialize();
int APS5_VABI sceFrontPanelDisplaySetOutputMode();
int APS5_VABI sceFrontPanelDisplayStickReadState();

}
