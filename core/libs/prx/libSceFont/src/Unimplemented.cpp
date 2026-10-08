// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"

#pragma GCC visibility push(default)

namespace {

[[noreturn]] void UnsupportedQuery(const char* function, const char* query) {
    throw std::runtime_error(std::string(function) + ": no reference gives the arguments, the result or the error codes of " + query + " (error code not verified)");
}

}


extern "C" {

int APS5_VABI sceFontControl() {
    UnsupportedQuery(__func__, "the font control call");
}

int APS5_VABI sceFontCreateGraphicsDevice() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontCreateGraphicsService() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontCreateGraphicsServiceWithEdition() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontDestroyGraphicsDevice() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontDestroyGraphicsService() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGetFontGlyphsOutlineProfile() {
    UnsupportedQuery(__func__, "the font glyph outline profile query");
}

int APS5_VABI sceFontGetFontMetrics() {
    UnsupportedQuery(__func__, "the font metrics query");
}

int APS5_VABI sceFontGetFontStyleInformation() {
    UnsupportedQuery(__func__, "the font style information query");
}

int APS5_VABI sceFontGetGlyphExpandBufferState() {
    UnsupportedQuery(__func__, "the glyph expand buffer query");
}

int APS5_VABI sceFontGraphicsBeginFrame() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsDrawingCancel() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsDrawingFinish() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsEndFrame() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsExchangeResource() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsFillMethodInit() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsFillPlotInit() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsFillPlotSetLayout() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsFillPlotSetMapping() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsFillRatesInit() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsFillRatesSetFillEffect() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsFillRatesSetLayout() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsFillRatesSetMapping() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsGetDeviceUsage() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsRegionInit() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsRegionInitCircular() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsRegionInitRoundish() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsRelease() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsRenderResource() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetFramePolicy() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupClipping() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupColorRates() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupFillMethod() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupFillRates() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupGlyphFill() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupGlyphFillPlot() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupHandleDefault() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupLocation() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupPositioning() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupRotation() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupScaling() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupShapeFill() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsSetupShapeFillPlot() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsStructureCanvas() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsStructureCanvasSequence() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsStructureDesign() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsStructureDesignResource() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsStructureSurfaceTexture() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateClipping() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateColorRates() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateFillMethod() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateFillRates() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateGlyphFill() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateGlyphFillPlot() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateLocation() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdatePositioning() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateRotation() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateScaling() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateShapeFill() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsUpdateShapeFillPlot() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontSupportGlyphs() {
    UnsupportedQuery(__func__, "the glyph support query");
}

int APS5_VABI sceFontGraphicsDrawupFillTextureImageObject() {
    UnsupportedQuery(__func__, "the font graphics family");
}

int APS5_VABI sceFontGraphicsDrawupFillTexturePatternObject() {
    UnsupportedQuery(__func__, "the font graphics family");
}

}

#pragma GCC visibility pop
