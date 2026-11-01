#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceFontSelectGraphicsAgc(void);
int APS5_VABI sceFontGraphicsAgcSurfaceInit(void);
int APS5_VABI sceFontGraphicsAgcDrawupFillTextureImage(void);
int APS5_VABI sceFontGraphicsAgcDrawupFillTexturePattern(void);
int APS5_VABI sceFontGraphicsSequenceGetAgcCommandsForJump(void);
int APS5_VABI sceFontGraphicsSequenceGetAgcCommandsForSubmit(void);

}
