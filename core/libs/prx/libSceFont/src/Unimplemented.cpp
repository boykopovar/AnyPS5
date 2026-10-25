#include "prx/libSceFont/include/Api.hpp"
// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libc/include/General.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"

static_assert(sizeof(FontGraphicsRegion) == 32);
static_assert(offsetof(FontGraphicsRegion, x) == 0);
static_assert(offsetof(FontGraphicsRegion, y) == 4);
static_assert(offsetof(FontGraphicsRegion, width) == 8);
static_assert(offsetof(FontGraphicsRegion, height) == 12);
static_assert(offsetof(FontGraphicsRegion, flags) == 16);
static_assert(offsetof(FontGraphicsRegion, tag) == 20);
static_assert(offsetof(FontGraphicsRegion, outerRadius) == 24);
static_assert(offsetof(FontGraphicsRegion, innerRadius) == 28);

static_assert(sizeof(FontGraphicsFillMethod) == 32);
static_assert(offsetof(FontGraphicsFillMethod, mode) == 0);
static_assert(offsetof(FontGraphicsFillMethod, type) == 4);
static_assert(offsetof(FontGraphicsFillMethod, flags) == 8);
static_assert(offsetof(FontGraphicsFillMethod, tag) == 12);
static_assert(offsetof(FontGraphicsFillMethod, reserved) == 16);

static_assert(sizeof(FontGraphicsFillPlot) == 64);
static_assert(offsetof(FontGraphicsFillPlot, mapping) == 0);
static_assert(offsetof(FontGraphicsFillPlot, region) == 16);
static_assert(offsetof(FontGraphicsFillPlot, tag) == 24);
static_assert(offsetof(FontGraphicsFillPlot, reserved26) == 26);
static_assert(offsetof(FontGraphicsFillPlot, layout) == 28);
static_assert(offsetof(FontGraphicsFillPlot, flags) == 30);
static_assert(offsetof(FontGraphicsFillPlot, reserved) == 31);

static_assert(sizeof(FontGraphicsFillRates) == 64);
static_assert(offsetof(FontGraphicsFillRates, mapping) == 0);
static_assert(offsetof(FontGraphicsFillRates, fill) == 16);
static_assert(offsetof(FontGraphicsFillRates, tag) == 24);
static_assert(offsetof(FontGraphicsFillRates, effect) == 26);
static_assert(offsetof(FontGraphicsFillRates, layout) == 28);
static_assert(offsetof(FontGraphicsFillRates, flags) == 30);
static_assert(offsetof(FontGraphicsFillRates, reserved) == 31);

static_assert(sizeof(FontGraphicsPoolUsage) == 20);
static_assert(offsetof(FontGraphicsPoolUsage, capacity) == 0);
static_assert(offsetof(FontGraphicsPoolUsage, used) == 4);
static_assert(offsetof(FontGraphicsPoolUsage, available) == 8);
static_assert(offsetof(FontGraphicsPoolUsage, peak) == 12);
static_assert(offsetof(FontGraphicsPoolUsage, change) == 16);

static_assert(sizeof(FontGraphicsMemoryUsage) == 48);
static_assert(offsetof(FontGraphicsMemoryUsage, storage) == 0);
static_assert(offsetof(FontGraphicsMemoryUsage, allocations) == 20);
static_assert(offsetof(FontGraphicsMemoryUsage, address) == 40);

static_assert(sizeof(FontGraphicsDeviceUsage) == 96);
static_assert(offsetof(FontGraphicsDeviceUsage, first) == 0);
static_assert(offsetof(FontGraphicsDeviceUsage, second) == 48);

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceFontControl(std::int32_t command, void* object, std::uintptr_t argument) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontCreateGraphicsDevice(const FontMemory* memory, const FontGraphicsDeviceParam* param, FontGraphicsDevice** device) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontCreateGraphicsService(const FontMemory* memory, const FontGraphicsServiceParam* param, FontGraphicsService** service) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontCreateGraphicsServiceWithEdition(const FontMemory* memory, const FontGraphicsServiceParam* param, std::uint64_t unusedSlot, FontGraphicsService** service) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontCreateWords(const FontMemory* memory, FontTextSource* source, std::uint64_t unusedSlot, FontWords** words) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontDestroyGraphicsDevice(FontGraphicsDevice** device) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontDestroyGraphicsService(FontGraphicsService** service) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontDestroyWords(FontWords** words) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGetFontGlyphsOutlineProfile(FontHandle font, FontOutlineProfile* profile) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGetFontMetrics(FontHandle font, std::uint32_t mode, FontMetrics* metrics) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGetFontStyleInformation(FontHandle font, FontStyleInformation* information) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGetGlyphExpandBufferState(FontHandle font, FontGlyphExpandBufferState* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGlyphRenderImage(FontGlyph glyph, const FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* output) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGlyphRenderImageHorizontal(FontGlyph glyph, const FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* output) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGlyphRenderImageVertical(FontGlyph glyph, const FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* output) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsBeginFrame(FontGraphicsFrame* frame, std::uint64_t flags, FontGraphicsResource** resource) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsDrawingCancel(FontGraphicsFrame** frame) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsDrawingFinish(FontGraphicsFrame** frame, FontGraphicsDevice* device) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsEndFrame(FontGraphicsFrame* frame) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsExchangeResource(FontGraphicsResource* resource, FontGraphicsDesignResource* replacement, FontGraphicsDesignResource** previous) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsFillMethodInit(FontGraphicsFillMethod* method, std::int32_t mode, std::int32_t type) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsFillPlotInit(FontGraphicsFillPlot* plot, const FontGraphicsRegion* region) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsFillPlotSetLayout(FontGraphicsFillPlot* plot, std::int32_t first, std::int32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsFillPlotSetMapping(FontGraphicsFillPlot* plot, float first, float second, float third, float fourth) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsFillRatesInit(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsFillRatesSetFillEffect(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsFillRatesSetLayout(FontGraphicsFillRates* rates, std::int32_t first, std::int32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsFillRatesSetMapping(FontGraphicsFillRates* rates, float first, float second, float third, float fourth) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsGetDeviceUsage(FontGraphicsDevice* device, FontGraphicsDeviceUsage* usage) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsRegionInit(FontGraphicsRegion* region, float x, float y, float width, float height) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsRegionInitCircular(FontGraphicsRegion* region, float centerX, float centerY, float outerRadius, float innerRadius) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsRegionInitRoundish(FontGraphicsRegion* region, float x, float y, float width, float height, float outerRadius, float innerRadius, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsRelease(void** object) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsRenderResource(FontGraphicsFrame* frame, FontGraphicsDesignResource* design, float x, float y, FontGraphicsResource** resource) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetFramePolicy(FontGraphicsResource* resource, std::int32_t policy, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupClipping(FontGraphicsResource* resource, const FontGraphicsRegion* region) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupColorRates(FontGraphicsResource* resource, float red, float green, float blue, float alpha) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupFillMethod(FontGraphicsResource* resource, const FontGraphicsFillMethod* method) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupFillRates(FontGraphicsResource* resource, const FontGraphicsFillRates* rates) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupGlyphFill(FontGraphicsResource* resource, const FontGraphicsFill* fill) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupGlyphFillPlot(FontGraphicsResource* resource, const FontGraphicsFillPlot* plot) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupHandleDefault(FontGraphicsResource* resource, std::uint64_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupLocation(FontGraphicsResource* resource, float x, float y) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupPositioning(FontGraphicsResource* resource, std::uint32_t flags, float x, float y) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupRotation(FontGraphicsResource* resource, float angle, float pivotX, float pivotY) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupScaling(FontGraphicsResource* resource, float scaleX, float scaleY, float pivotX, float pivotY) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupShapeFill(FontGraphicsResource* resource, const FontGraphicsFill* fill) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsSetupShapeFillPlot(FontGraphicsResource* resource, const FontGraphicsFillPlot* plot) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsStructureCanvas(FontGraphicsDevice* device, std::uint32_t width, std::uint32_t height, std::int32_t format, FontGraphicsCanvas** canvas) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsStructureCanvasSequence(FontGraphicsDevice* device, FontGraphicsCanvas* canvas, const FontGraphicsSurface* surface, std::uint64_t flags, FontGraphicsCanvasSequence** sequence) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsStructureDesign(FontGraphicsDevice* device, std::int32_t format, FontGraphicsDesign** design) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsStructureDesignResource(FontGraphicsDevice* device, FontGraphicsDesign* design, FontGraphicsDesignResource** resource) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsStructureSurfaceTexture(FontGraphicsDevice* device, const FontGraphicsSurface* surface, FontGraphicsSurfaceTexture** texture) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateClipping(FontGraphicsResource* resource, const FontGraphicsRegion* region) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateColorRates(FontGraphicsResource* resource, float red, float green, float blue, float alpha) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateFillMethod(FontGraphicsResource* resource, const FontGraphicsFillMethod* method) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateFillRates(FontGraphicsResource* resource, const FontGraphicsFillRates* rates) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateGlyphFill(FontGraphicsResource* resource, const FontGraphicsFill* fill) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateGlyphFillPlot(FontGraphicsResource* resource, const FontGraphicsFillPlot* plot) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateLocation(FontGraphicsResource* resource, float x, float y) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdatePositioning(FontGraphicsResource* resource, std::uint32_t flags, float x, float y) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateRotation(FontGraphicsResource* resource, float angle, float pivotX, float pivotY) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateScaling(FontGraphicsResource* resource, float scaleX, float scaleY, float pivotX, float pivotY) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateShapeFill(FontGraphicsResource* resource, const FontGraphicsFill* fill) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsUpdateShapeFillPlot(FontGraphicsResource* resource, const FontGraphicsFillPlot* plot) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontSupportGlyphs(const FontGlyphSupport* support) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontWordsFindWordCharacters(FontWords* words, FontTextCharacter* firstCharacter, FontTextCharacter* endCharacter, FontTextCharacter** lastCharacter, FontTextCharacter** nextCharacter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsDrawupFillTextureImageObject(FontGraphicsFrame* frame, const void* data, std::uint32_t dataSize, float x, float y, float width, float height, FontGraphicsFill** fill) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontGraphicsDrawupFillTexturePatternObject(FontGraphicsFrame* frame, const void* data, std::uint32_t dataSize, std::uint32_t firstMode, std::uint32_t secondMode, std::int32_t firstCount, std::int32_t firstLayout, std::int32_t secondLayout, std::int32_t secondCount, FontGraphicsFill** fill) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

#pragma GCC visibility pop
