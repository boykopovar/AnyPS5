#pragma once

#include "prx/libSceFont/include/FontTypes.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct FontGraphicsDevice;

struct FontGraphicsDeviceParam;

struct FontGraphicsService;

struct FontGraphicsServiceParam;

struct FontWords;

struct FontGlyphSupport;

struct FontOutlineProfile;

struct FontMetrics;

struct FontStyleInformation;

struct FontGlyphExpandBufferState;

struct FontGraphicsResource;

struct FontGraphicsFrame;

struct FontGraphicsDesignResource;

struct FontGraphicsDesign;

struct FontGraphicsCanvas;

struct FontGraphicsCanvasSequence;

struct FontGraphicsSurface;

struct FontGraphicsSurfaceTexture;

struct FontGraphicsFill;

struct FontGraphicsRegion {
    float x;
    float y;
    float width;
    float height;
    std::uint32_t flags;
    std::uint32_t tag;
    float outerRadius;
    float innerRadius;
};

struct FontGraphicsFillMethod {
    std::int32_t mode;
    std::int32_t type;
    std::uint32_t flags;
    std::uint32_t tag;
    std::byte reserved[16];
};

struct FontGraphicsFillPlot {
    float mapping[4];
    const FontGraphicsRegion* region;
    std::uint16_t tag;
    std::uint16_t reserved26;
    std::uint16_t layout;
    std::uint8_t flags;
    std::byte reserved[33];
};

struct FontGraphicsFillRates {
    float mapping[4];
    const FontGraphicsFill* fill;
    std::uint16_t tag;
    std::uint16_t effect;
    std::uint16_t layout;
    std::uint8_t flags;
    std::byte reserved[33];
};

struct FontGraphicsPoolUsage {
    std::uint32_t capacity;
    std::uint32_t used;
    std::uint32_t available;
    std::uint32_t peak;
    std::int32_t change;
};

struct FontGraphicsMemoryUsage {
    FontGraphicsPoolUsage storage;
    FontGraphicsPoolUsage allocations;
    void* address;
};

struct FontGraphicsDeviceUsage {
    FontGraphicsMemoryUsage first;
    FontGraphicsMemoryUsage second;
};

namespace Font {

using GraphicsErrorCallback = void (APS5_VABI*)(int error);

}

extern "C" {

int APS5_VABI sceFontMemoryInit(FontMemory* memory, void* regionAddress, std::uint32_t regionSize, const FontMemoryInterface* iface, void* mspaceObject, FontMemoryDestroyFunction destroyCallback, void* destroyObject);
int APS5_VABI sceFontMemoryTerm(FontMemory* memory);
int APS5_VABI sceFontCreateLibraryWithEdition(const FontMemory* memory, const void* createParams, std::uint64_t edition, FontLibrary* pLibrary);
int APS5_VABI sceFontCreateLibrary(const FontMemory* memory, const void* createParams, FontLibrary* pLibrary);
int APS5_VABI sceFontDestroyLibrary(FontLibrary* pLibrary);
int APS5_VABI sceFontCreateRendererWithEdition(const FontMemory* memory, const void* createParams, std::uint64_t edition, FontRenderer* pRenderer);
int APS5_VABI sceFontCreateRenderer(const FontMemory* memory, const void* createParams, FontRenderer* pRenderer);
int APS5_VABI sceFontDestroyRenderer(FontRenderer* pRenderer);
int APS5_VABI sceFontRendererGetOutlineBufferSize(FontRenderer fontRenderer, std::uint32_t* size);
int APS5_VABI sceFontRendererResetOutlineBuffer(FontRenderer fontRenderer);
int APS5_VABI sceFontRendererSetOutlineBufferPolicy(FontRenderer fontRenderer, std::uint64_t bufferPolicy, std::uint32_t basalSize, std::uint32_t limitSize);
int APS5_VABI sceFontGetPixelResolution(FontLibrary library, std::uint32_t* subPixelCount);
int APS5_VABI sceFontAttachDeviceCacheBuffer(FontLibrary library, void* buffer, std::uint32_t size);
int APS5_VABI sceFontClearDeviceCache(FontLibrary library);
int APS5_VABI sceFontDettachDeviceCacheBuffer(FontLibrary library, void** buffer, std::uint32_t* size);
int APS5_VABI sceFontSupportExternalFonts(FontLibrary library, std::uint32_t fontMax, std::uint32_t formats);
int APS5_VABI sceFontSupportSystemFonts(FontLibrary library);
void APS5_VABI sceFontSetGraphicsErrorCallback(Font::GraphicsErrorCallback callback);
int APS5_VABI sceFontSetFontsOpenMode(FontLibrary library, std::uint32_t openMode);
int APS5_VABI sceFontOpenFontMemory(FontLibrary library, const void* fontAddress, std::uint32_t fontSize, const FontOpenDetail* openDetail, FontHandle* pFontHandle);
int APS5_VABI sceFontOpenFontFile(FontLibrary library, const char* path, std::uint32_t openMode, const FontOpenDetail* openDetail, FontHandle* pFontHandle);
int APS5_VABI sceFontOpenFontInstance(FontHandle fontHandle, FontHandle setupFont, FontHandle* pFontHandle);
int APS5_VABI sceFontOpenFontSet(FontLibrary library, std::uint32_t fontSetType, std::uint32_t openMode, const FontOpenDetail* openDetail, FontHandle* pFontHandle);
int APS5_VABI sceFontCloseFont(FontHandle fontHandle);
int APS5_VABI sceFontGetLibrary(FontHandle fontHandle, FontLibrary* pLibrary);
int APS5_VABI sceFontGetFontGlyphsCount(FontHandle fontHandle, std::uint32_t* glyphsCount);
int APS5_VABI sceFontGetCharGlyphCode(FontHandle fontHandle, std::uint32_t code, std::uint32_t* glyphCode);
int APS5_VABI sceFontGetFontResolution(FontHandle fontHandle, std::uint32_t* pResolution, float* pScalePixel);
int APS5_VABI sceFontBindRenderer(FontHandle fontHandle, FontRenderer renderer);
int APS5_VABI sceFontRebindRenderer(FontHandle fontHandle);
int APS5_VABI sceFontUnbindRenderer(FontHandle fontHandle);
int APS5_VABI sceFontGetCharGlyphMetrics(FontHandle fontHandle, std::uint32_t code, FontGlyphMetrics* metrics);
int APS5_VABI sceFontGetRenderCharGlyphMetrics(FontHandle fontHandle, std::uint32_t code, FontGlyphMetrics* metrics);
int APS5_VABI sceFontGetHorizontalLayout(FontHandle fontHandle, FontHorizontalLayout* layout);
int APS5_VABI sceFontGetVerticalLayout(FontHandle fontHandle, FontVerticalLayout* layout);
int APS5_VABI sceFontGetKerning(FontHandle fontHandle, std::uint32_t preCode, std::uint32_t code, FontKerning* kerning);
int APS5_VABI sceFontGetRenderScaledKerning(FontHandle fontHandle, std::uint32_t preCode, std::uint32_t code, FontKerning* kerning);
int APS5_VABI sceFontRenderCharGlyphImage(FontHandle fontHandle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result);
int APS5_VABI sceFontRenderCharGlyphImageHorizontal(FontHandle fontHandle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result);
int APS5_VABI sceFontRenderCharGlyphImageVertical(FontHandle fontHandle, std::uint32_t code, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* result);
void APS5_VABI sceFontRenderSurfaceInit(FontRenderSurface* renderSurface, void* buffer, int bufWidthByte, int pixelSizeByte, int widthPixel, int heightPixel);
void APS5_VABI sceFontRenderSurfaceSetScissor(FontRenderSurface* renderSurface, int x0, int y0, int w, int h);
int APS5_VABI sceFontRenderSurfaceSetStyleFrame(FontRenderSurface* renderSurface, FontStyleFrame* styleFrame);
int APS5_VABI sceFontGenerateCharGlyph(FontHandle fontHandle, std::uint32_t code, const FontGenerateGlyphDetail* detail, FontGlyph* pGlyph);
int APS5_VABI sceFontDeleteGlyph(const FontMemory* memory, FontGlyph* pGlyph);
int APS5_VABI sceFontGlyphDefineAttribute(FontGlyph glyph, std::int32_t attribute, std::int32_t* previousAttribute);
int APS5_VABI sceFontGlyphGetAttribute(FontGlyph glyph, int attribute, int* nowAttribute);
int APS5_VABI sceFontGlyphGetGlyphForm(FontGlyph glyph);
int APS5_VABI sceFontGlyphGetMetricsForm(FontGlyph glyph);
int APS5_VABI sceFontGlyphGetScalePixel(FontGlyph glyph, float* w, float* h);
const FontGlyphMetrics* APS5_VABI sceFontGlyphRefersMetrics(FontGlyph glyph);
const FontGlyphMetricsHorizontal* APS5_VABI sceFontGlyphRefersMetricsHorizontal(FontGlyph glyph);
const FontGlyphMetricsHorizontalAdvance* APS5_VABI sceFontGlyphRefersMetricsHorizontalAdvance(FontGlyph glyph);
const FontGlyphMetricsHorizontalX* APS5_VABI sceFontGlyphRefersMetricsHorizontalX(FontGlyph glyph);
FontGlyphOutline* APS5_VABI sceFontGlyphRefersOutline(FontGlyph glyph);
int APS5_VABI sceFontDefineAttribute(FontHandle fontHandle, int attribute, int* oldAttribute);
int APS5_VABI sceFontGetAttribute(FontHandle fontHandle, int attribute, int* nowAttribute);
int APS5_VABI sceFontSetScriptLanguage(FontHandle fontHandle, int fontScript, int fontLanguage);
int APS5_VABI sceFontGetScriptLanguage(FontHandle fontHandle, int fontScript, int* fontLanguage);
int APS5_VABI sceFontSetTypographicDesign(FontHandle fontHandle, int typographic, int feature);
int APS5_VABI sceFontGetTypographicDesign(FontHandle fontHandle, int typographic, int* feature);
int APS5_VABI sceFontSetScalePixel(FontHandle fontHandle, float w, float h);
int APS5_VABI sceFontSetScalePoint(FontHandle fontHandle, float w, float h);
int APS5_VABI sceFontSetResolutionDpi(FontHandle fontHandle, std::uint32_t hDpi, std::uint32_t vDpi);
int APS5_VABI sceFontSetEffectSlant(FontHandle fontHandle, float slantRatio);
int APS5_VABI sceFontSetEffectWeight(FontHandle fontHandle, float weightXScale, float weightYScale, std::uint32_t mode);
int APS5_VABI sceFontGetScalePixel(FontHandle fontHandle, float* w, float* h);
int APS5_VABI sceFontGetScalePoint(FontHandle fontHandle, float* w, float* h);
int APS5_VABI sceFontGetResolutionDpi(FontHandle fontHandle, std::uint32_t* hDpi, std::uint32_t* vDpi);
int APS5_VABI sceFontGetEffectSlant(FontHandle fontHandle, float* slantRatio);
int APS5_VABI sceFontGetEffectWeight(FontHandle fontHandle, float* weightXScale, float* weightYScale, std::uint32_t* mode);
int APS5_VABI sceFontSetupRenderScalePixel(FontHandle fontHandle, float w, float h);
int APS5_VABI sceFontSetupRenderScalePoint(FontHandle fontHandle, float w, float h);
int APS5_VABI sceFontSetupRenderEffectSlant(FontHandle fontHandle, float slantRatio);
int APS5_VABI sceFontSetupRenderEffectWeight(FontHandle fontHandle, float weightXScale, float weightYScale, std::uint32_t mode);
int APS5_VABI sceFontGetRenderScalePixel(FontHandle fontHandle, float* w, float* h);
int APS5_VABI sceFontGetRenderScalePoint(FontHandle fontHandle, float* w, float* h);
int APS5_VABI sceFontGetRenderEffectSlant(FontHandle fontHandle, float* slantRatio);
int APS5_VABI sceFontGetRenderEffectWeight(FontHandle fontHandle, float* weightXScale, float* weightYScale, std::uint32_t* mode);
int APS5_VABI sceFontStyleFrameInit(FontStyleFrame* styleFrame);
int APS5_VABI sceFontStyleFrameSetScalePixel(FontStyleFrame* styleFrame, float w, float h);
int APS5_VABI sceFontStyleFrameSetScalePoint(FontStyleFrame* styleFrame, float w, float h);
int APS5_VABI sceFontStyleFrameSetResolutionDpi(FontStyleFrame* styleFrame, std::uint32_t hDpi, std::uint32_t vDpi);
int APS5_VABI sceFontStyleFrameSetEffectSlant(FontStyleFrame* styleFrame, float slantRatio);
int APS5_VABI sceFontStyleFrameSetEffectWeight(FontStyleFrame* styleFrame, float weightXScale, float weightYScale, std::uint32_t mode);
int APS5_VABI sceFontStyleFrameUnsetScale(FontStyleFrame* styleFrame);
int APS5_VABI sceFontStyleFrameUnsetEffectSlant(FontStyleFrame* styleFrame);
int APS5_VABI sceFontStyleFrameUnsetEffectWeight(FontStyleFrame* styleFrame);
int APS5_VABI sceFontStyleFrameGetScalePixel(const FontStyleFrame* styleFrame, float* w, float* h);
int APS5_VABI sceFontStyleFrameGetScalePoint(const FontStyleFrame* styleFrame, float* w, float* h);
int APS5_VABI sceFontStyleFrameGetResolutionDpi(const FontStyleFrame* styleFrame, std::uint32_t* hDpi, std::uint32_t* vDpi);
int APS5_VABI sceFontStyleFrameGetEffectSlant(const FontStyleFrame* styleFrame, float* slantRatio);
int APS5_VABI sceFontStyleFrameGetEffectWeight(const FontStyleFrame* styleFrame, float* weightXScale, float* weightYScale, std::uint32_t* mode);
int APS5_VABI sceFontTextSourceInit(FontTextSource* textSource, const void* textAddress, std::uint32_t textSizeByte, FontTextParseFunction textParser, void* textObject);
int APS5_VABI sceFontTextSourceRewind(FontTextSource* textSource);
int APS5_VABI sceFontTextSourceSetDefaultFont(FontTextSource* textSource, FontHandle defaultFont);
int APS5_VABI sceFontTextSourceSetWritingForm(FontTextSource* textSource, std::int32_t writingForm);
int APS5_VABI sceFontCreateString(const FontMemory* fontMemory, FontTextSource* textSource, const FontCreateStringDetail* detail, FontString* pFontString);
int APS5_VABI sceFontDestroyString(FontString* pFontString);
std::uint32_t APS5_VABI sceFontStringGetTerminateCode(FontString fontString);
void* APS5_VABI sceFontStringGetTerminateOrder(FontString fontString);
int APS5_VABI sceFontStringGetWritingForm(FontString fontString);
FontTextCharacter* APS5_VABI sceFontStringRefersTextCharacters(FontString fontString, std::uint32_t* characterCount);
const FontTextCharacter* APS5_VABI sceFontStringRefersRenderCharacters(FontString fontString, FontTextCharacter* startCharacter, FontTextCharacter* lastCharacter, std::uint32_t* characterCount);
int APS5_VABI sceFontCharacterGetBidiLevel(const FontTextCharacter* textCharacter, int* bidiLevel);
int APS5_VABI sceFontCharacterGetSyllableStringState(const FontTextCharacter* textCharacter, int* syllableStringState);
int APS5_VABI sceFontCharacterGetTextFontCode(const FontTextCharacter* textCharacter, FontHandle* pFontHandle, std::uint32_t* textCode);
int APS5_VABI sceFontCharacterGetTextOrder(const FontTextCharacter* textCharacter, void** pTextOrder);
std::uint32_t APS5_VABI sceFontCharacterLooksFormatCharacters(const FontTextCharacter* textCharacter);
std::uint32_t APS5_VABI sceFontCharacterLooksWhiteSpace(const FontTextCharacter* textCharacter);
FontTextCharacter* APS5_VABI sceFontCharacterRefersTextBack(const FontTextCharacter* textCharacter);
FontTextCharacter* APS5_VABI sceFontCharacterRefersTextNext(const FontTextCharacter* textCharacter);
FontTextCodes* APS5_VABI sceFontCharactersRefersTextCodes(const FontTextCharacter* textCharacter, const FontTextCharacter* termCharacter, FontTextCodes* textCodes);
FontTextCodes* APS5_VABI sceFontTextCodesStepNext(FontTextCodes* textCodesStep);
FontTextCodes* APS5_VABI sceFontTextCodesStepBack(FontTextCodes* textCodesStep);
int APS5_VABI sceFontWritingInit(FontWriting* fontWriting, FontString fontString, const FontTextCharacter* fontCharacter);
int APS5_VABI sceFontWritingGetRenderMetrics(FontWriting* fontWriting, FontWritingMetrics* writingMetrics);
const FontWritingStep* APS5_VABI sceFontWritingRefersRenderStep(FontWriting* fontWriting);
FontTextCharacter* APS5_VABI sceFontWritingRefersRenderStepCharacter(FontWriting* fontWriting, const FontWritingLetterStep** pLetterStep);
int APS5_VABI sceFontWritingSetMaskInvisible(FontWriting* fontWriting, std::int32_t mask);
int APS5_VABI sceFontControl(std::int32_t command, void* object, std::uintptr_t argument);
int APS5_VABI sceFontCreateGraphicsDevice(const FontMemory* memory, const FontGraphicsDeviceParam* param, FontGraphicsDevice** device);
int APS5_VABI sceFontCreateGraphicsService(const FontMemory* memory, const FontGraphicsServiceParam* param, FontGraphicsService** service);
int APS5_VABI sceFontCreateGraphicsServiceWithEdition(const FontMemory* memory, const FontGraphicsServiceParam* param, std::uint64_t unusedSlot, FontGraphicsService** service);
int APS5_VABI sceFontCreateWords(const FontMemory* memory, FontTextSource* source, std::uint64_t unusedSlot, FontWords** words);
int APS5_VABI sceFontDestroyGraphicsDevice(FontGraphicsDevice** device);
int APS5_VABI sceFontDestroyGraphicsService(FontGraphicsService** service);
int APS5_VABI sceFontDestroyWords(FontWords** words);
int APS5_VABI sceFontGetFontGlyphsOutlineProfile(FontHandle font, FontOutlineProfile* profile);
int APS5_VABI sceFontGetFontMetrics(FontHandle font, std::uint32_t mode, FontMetrics* metrics);
int APS5_VABI sceFontGetFontStyleInformation(FontHandle font, FontStyleInformation* information);
int APS5_VABI sceFontGetGlyphExpandBufferState(FontHandle font, FontGlyphExpandBufferState* state);
int APS5_VABI sceFontGlyphRenderImage(FontGlyph glyph, const FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* output);
int APS5_VABI sceFontGlyphRenderImageHorizontal(FontGlyph glyph, const FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* output);
int APS5_VABI sceFontGlyphRenderImageVertical(FontGlyph glyph, const FontStyleFrame* styleFrame, FontRenderer renderer, FontRenderSurface* surface, float x, float y, FontGlyphMetrics* metrics, FontRenderOutput* output);
int APS5_VABI sceFontGraphicsBeginFrame(FontGraphicsFrame* frame, std::uint64_t flags, FontGraphicsResource** resource);
int APS5_VABI sceFontGraphicsDrawingCancel(FontGraphicsFrame** frame);
int APS5_VABI sceFontGraphicsDrawingFinish(FontGraphicsFrame** frame, FontGraphicsDevice* device);
int APS5_VABI sceFontGraphicsEndFrame(FontGraphicsFrame* frame);
int APS5_VABI sceFontGraphicsExchangeResource(FontGraphicsResource* resource, FontGraphicsDesignResource* replacement, FontGraphicsDesignResource** previous);
int APS5_VABI sceFontGraphicsFillMethodInit(FontGraphicsFillMethod* method, std::int32_t mode, std::int32_t type);
int APS5_VABI sceFontGraphicsFillPlotInit(FontGraphicsFillPlot* plot, const FontGraphicsRegion* region);
int APS5_VABI sceFontGraphicsFillPlotSetLayout(FontGraphicsFillPlot* plot, std::int32_t first, std::int32_t second);
int APS5_VABI sceFontGraphicsFillPlotSetMapping(FontGraphicsFillPlot* plot, float first, float second, float third, float fourth);
int APS5_VABI sceFontGraphicsFillRatesInit(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect);
int APS5_VABI sceFontGraphicsFillRatesSetFillEffect(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect);
int APS5_VABI sceFontGraphicsFillRatesSetLayout(FontGraphicsFillRates* rates, std::int32_t first, std::int32_t second);
int APS5_VABI sceFontGraphicsFillRatesSetMapping(FontGraphicsFillRates* rates, float first, float second, float third, float fourth);
int APS5_VABI sceFontGraphicsGetDeviceUsage(FontGraphicsDevice* device, FontGraphicsDeviceUsage* usage);
int APS5_VABI sceFontGraphicsRegionInit(FontGraphicsRegion* region, float x, float y, float width, float height);
int APS5_VABI sceFontGraphicsRegionInitCircular(FontGraphicsRegion* region, float centerX, float centerY, float outerRadius, float innerRadius);
int APS5_VABI sceFontGraphicsRegionInitRoundish(FontGraphicsRegion* region, float x, float y, float width, float height, float outerRadius, float innerRadius, std::uint32_t flags);
int APS5_VABI sceFontGraphicsRelease(void** object);
int APS5_VABI sceFontGraphicsRenderResource(FontGraphicsFrame* frame, FontGraphicsDesignResource* design, float x, float y, FontGraphicsResource** resource);
int APS5_VABI sceFontGraphicsSetFramePolicy(FontGraphicsResource* resource, std::int32_t policy, std::uint32_t value);
int APS5_VABI sceFontGraphicsSetupClipping(FontGraphicsResource* resource, const FontGraphicsRegion* region);
int APS5_VABI sceFontGraphicsSetupColorRates(FontGraphicsResource* resource, float red, float green, float blue, float alpha);
int APS5_VABI sceFontGraphicsSetupFillMethod(FontGraphicsResource* resource, const FontGraphicsFillMethod* method);
int APS5_VABI sceFontGraphicsSetupFillRates(FontGraphicsResource* resource, const FontGraphicsFillRates* rates);
int APS5_VABI sceFontGraphicsSetupGlyphFill(FontGraphicsResource* resource, const FontGraphicsFill* fill);
int APS5_VABI sceFontGraphicsSetupGlyphFillPlot(FontGraphicsResource* resource, const FontGraphicsFillPlot* plot);
int APS5_VABI sceFontGraphicsSetupHandleDefault(FontGraphicsResource* resource, std::uint64_t flags);
int APS5_VABI sceFontGraphicsSetupLocation(FontGraphicsResource* resource, float x, float y);
int APS5_VABI sceFontGraphicsSetupPositioning(FontGraphicsResource* resource, std::uint32_t flags, float x, float y);
int APS5_VABI sceFontGraphicsSetupRotation(FontGraphicsResource* resource, float angle, float pivotX, float pivotY);
int APS5_VABI sceFontGraphicsSetupScaling(FontGraphicsResource* resource, float scaleX, float scaleY, float pivotX, float pivotY);
int APS5_VABI sceFontGraphicsSetupShapeFill(FontGraphicsResource* resource, const FontGraphicsFill* fill);
int APS5_VABI sceFontGraphicsSetupShapeFillPlot(FontGraphicsResource* resource, const FontGraphicsFillPlot* plot);
int APS5_VABI sceFontGraphicsStructureCanvas(FontGraphicsDevice* device, std::uint32_t width, std::uint32_t height, std::int32_t format, FontGraphicsCanvas** canvas);
int APS5_VABI sceFontGraphicsStructureCanvasSequence(FontGraphicsDevice* device, FontGraphicsCanvas* canvas, const FontGraphicsSurface* surface, std::uint64_t flags, FontGraphicsCanvasSequence** sequence);
int APS5_VABI sceFontGraphicsStructureDesign(FontGraphicsDevice* device, std::int32_t format, FontGraphicsDesign** design);
int APS5_VABI sceFontGraphicsStructureDesignResource(FontGraphicsDevice* device, FontGraphicsDesign* design, FontGraphicsDesignResource** resource);
int APS5_VABI sceFontGraphicsStructureSurfaceTexture(FontGraphicsDevice* device, const FontGraphicsSurface* surface, FontGraphicsSurfaceTexture** texture);
int APS5_VABI sceFontGraphicsUpdateClipping(FontGraphicsResource* resource, const FontGraphicsRegion* region);
int APS5_VABI sceFontGraphicsUpdateColorRates(FontGraphicsResource* resource, float red, float green, float blue, float alpha);
int APS5_VABI sceFontGraphicsUpdateFillMethod(FontGraphicsResource* resource, const FontGraphicsFillMethod* method);
int APS5_VABI sceFontGraphicsUpdateFillRates(FontGraphicsResource* resource, const FontGraphicsFillRates* rates);
int APS5_VABI sceFontGraphicsUpdateGlyphFill(FontGraphicsResource* resource, const FontGraphicsFill* fill);
int APS5_VABI sceFontGraphicsUpdateGlyphFillPlot(FontGraphicsResource* resource, const FontGraphicsFillPlot* plot);
int APS5_VABI sceFontGraphicsUpdateLocation(FontGraphicsResource* resource, float x, float y);
int APS5_VABI sceFontGraphicsUpdatePositioning(FontGraphicsResource* resource, std::uint32_t flags, float x, float y);
int APS5_VABI sceFontGraphicsUpdateRotation(FontGraphicsResource* resource, float angle, float pivotX, float pivotY);
int APS5_VABI sceFontGraphicsUpdateScaling(FontGraphicsResource* resource, float scaleX, float scaleY, float pivotX, float pivotY);
int APS5_VABI sceFontGraphicsUpdateShapeFill(FontGraphicsResource* resource, const FontGraphicsFill* fill);
int APS5_VABI sceFontGraphicsUpdateShapeFillPlot(FontGraphicsResource* resource, const FontGraphicsFillPlot* plot);
int APS5_VABI sceFontSupportGlyphs(const FontGlyphSupport* support);
int APS5_VABI sceFontWordsFindWordCharacters(FontWords* words, FontTextCharacter* firstCharacter, FontTextCharacter* endCharacter, FontTextCharacter** lastCharacter, FontTextCharacter** nextCharacter);
int APS5_VABI sceFontGraphicsDrawupFillTextureImageObject(FontGraphicsFrame* graphicsObject, const void* fillObject, std::uint32_t mode, void** output, float x, float y, float width, float height);
int APS5_VABI sceFontGraphicsDrawupFillTexturePatternObject(FontGraphicsFrame* graphicsObject, const void* fillObject, std::uint32_t mode, std::uint32_t format, std::uint32_t select, std::uint32_t dimension1, std::uint32_t option1, std::uint32_t option2, std::uint32_t dimension2, void** output);
int APS5_VABI sceFontCreateWritingLine(const FontMemory* memory, std::int32_t writingForm, const void* writingLineDetail, void** pWritingLine);
int APS5_VABI sceFontDestroyWritingLine(void** pWritingLine);
int APS5_VABI sceFontWritingLineClear(void* writingLine);
int APS5_VABI sceFontWritingLineWritesOrder(void* writingLine, std::uint64_t writingAttribute, const FontWritingMetrics* writingMetrics, void* writingOrderer);
const FontWritingLineStep* APS5_VABI sceFontWritingLineRefersRenderStep(void* writingLine);
int APS5_VABI sceFontWritingLineGetRenderMetrics(void* writingLine, FontWritingMetrics* writingMetrics);
int APS5_VABI sceFontWritingLineGetOrderingSpace(void* writingLine, float* advanceSpace, float* headSpace, float* inlineSpace, float* tailSpace);

int APS5_VABI sceFontGraphicsCanvasStartDrawing();
int APS5_VABI sceFontGraphicsGetDrawingFrameHandle();
int APS5_VABI sceFontGraphicsGetIndexedVertexesGlyph();
int APS5_VABI sceFontGraphicsProcessWaitAllSequences();
int APS5_VABI sceFontGraphicsStructureVertexesGlyph();
int APS5_VABI sceFontGraphicsTextureRefersSurface();
int APS5_VABI sceFontGraphicsUndefineIndexedVertexesGlyph();
int APS5_VABI sceFontGraphicsAppendGradientSegment(FontGraphicsFrame* graphicsObject, void* gradient, float position, const void* color);
int APS5_VABI sceFontSupportSystemUpdation();
int APS5_VABI sceFontGraphicsCanvasRenewal();
int APS5_VABI sceFontGraphicsDesignStartDrawing();
int APS5_VABI sceFontGraphicsGetVertexesGlyphMetrics();
int APS5_VABI sceFontGraphicsUpdateSlantShift();
int APS5_VABI sceFontGraphicsTextureMakeFillTexture();
int APS5_VABI sceFontGraphicsTextureMakeFillTextureImage();
int APS5_VABI sceFontGraphicsCanvasSetSurfaceFill();
int APS5_VABI sceFontGraphicsDrawVertexesGlyph();
int APS5_VABI sceFontGraphicsDrawRegionShape();
int APS5_VABI sceFontGraphicsProcessWaitSequence();
int APS5_VABI sceFontGraphicsTextureGetSurface();
int APS5_VABI sceFontGraphicsUndefineIndexedVertexesGlyphs();
int APS5_VABI sceFontGetCharVariationGlyphCode();
int APS5_VABI sceFontGraphicsSetupSlantShift();
int APS5_VABI sceFontGraphicsAdjustFillRadialGradient();
int APS5_VABI sceFontGraphicsDrawupFillFlatColor(FontGraphicsFrame* graphicsObject, const void* color, void** output);
int APS5_VABI sceFontGraphicsDrawupFillLinearGradient(FontGraphicsFrame* graphicsObject, std::uint32_t mode, const void* color1, const void* color2, void** output, float extent0, float extent1, float x0, float y0, float x1, float y1);
int APS5_VABI sceFontGraphicsSetVertexesGlyphScale();
int APS5_VABI sceFontGlyphGetOutlineVertexes();
int APS5_VABI sceFontGraphicsDrawupFillRadialGradient(FontGraphicsFrame* graphicsObject, std::uint32_t mode, const void* color1, const void* color2, void** output, float extent0, float extent1, float x0, float y0, float x1, float y1);
int APS5_VABI sceFontGraphicsAdjustGradientSpread();
int APS5_VABI sceFontGraphicsCanvasSetSurfaceFillWithLayout();
int APS5_VABI sceFontGraphicsCanvasGetPreprocess();
int APS5_VABI sceFontGraphicsReportError();
int APS5_VABI sceFontGraphicsDrawRectangleShape();
int APS5_VABI sceFontGraphicsProcessSetSyncPolicy();
int APS5_VABI sceFontGraphicsCanvasSetSurfaceFillWithMapping();
int APS5_VABI sceFontGraphicsUndefineIndexedVertexesGlyphAll();
int APS5_VABI sceFontGraphicsGetVertexesGlyphScale();
int APS5_VABI sceFontGraphicsCanvasClearPreprocess();
int APS5_VABI sceFontGraphicsFindIndexedVertexesGlyph();
int APS5_VABI sceFontGraphicsDefineIndexedVertexesGlyph();
int APS5_VABI sceFontGraphicsProcessRenderSequence();
int APS5_VABI sceFontOutlineVertexesInterfaceInit();
int APS5_VABI sceFontGraphicsSurfaceSetTargetView();

}
