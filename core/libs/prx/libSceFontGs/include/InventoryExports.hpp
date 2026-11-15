#pragma once

#include "SceTypes.hpp"
#include <cstdint>

struct FontGraphicsFrame;
struct FontGraphicsCanvasSequence;

extern "C" {

int APS5_VABI sceFontGraphicsGnmSurfaceInit(void* surface, const void* config);
void* APS5_VABI sceFontSelectGraphicsGnm(std::uint32_t index);

int APS5_VABI sceFontGraphicsGnmDrawupFillTextureImage(FontGraphicsFrame* graphicsObject, const void* surface, void** output,
                                                     float x, float y, float width, float height);

int APS5_VABI sceFontGraphicsGnmDrawupFillTexturePattern(FontGraphicsFrame* graphicsObject, const void* surface, std::uint32_t format,
                                                       std::uint32_t select, std::uint32_t dimension1, std::uint32_t option1,
                                                       std::uint32_t option2, std::uint32_t dimension2, void** output);

int APS5_VABI sceFontGraphicsSequenceGetGnmCommandsForChain(FontGraphicsCanvasSequence* sequence, void** commands,
                                                          std::uint32_t* numCommands, void** commands2, std::uint32_t* numCommands2);

int APS5_VABI sceFontGraphicsSequenceGetGnmCommandsForSubmit(FontGraphicsCanvasSequence* sequence, void** commands,
                                                           std::uint32_t* numCommands);

}
