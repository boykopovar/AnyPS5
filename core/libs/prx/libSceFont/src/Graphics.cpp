#include <atomic>
#include <cstdint>

#include "prx/libSceFont/include/FontGraphics.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"

namespace {

constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL = static_cast<int>(0x80460086);
constexpr int FONT_GRAPHICS_ERROR_INVALID_REGION = static_cast<int>(0x80460087);
constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT = static_cast<int>(0x80460088);
constexpr int FONT_GRAPHICS_ERROR_INVALID_FILL_RATES = static_cast<int>(0x80460089);

constexpr std::uint32_t RegionTag = 0xf60;
constexpr std::uint32_t RectangleRegionFlags = 0xf;
constexpr std::uint32_t CircularRegionFlags = 0xff;
constexpr std::uint32_t FillMethodTag = 0xf01b;
constexpr std::uint32_t FillMethodFlags = 0xf;
constexpr std::uint16_t FillPlotTag = 0xf01c;
constexpr std::uint16_t FillRatesTag = 0xf01a;
constexpr std::uint16_t DefaultLayout = 0xf77;
constexpr std::uint32_t FillEffects = 0x1033f;

std::atomic<Font::GraphicsErrorCallback> g_errorCallback{nullptr};

int Fail(int error) {
    if (const auto callback = g_errorCallback.load()) callback(error);
    return error;
}

bool IsObject(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer) + 1 >= 2;
}

bool IsValidFill(const FontGraphicsFill* fill) {
    const auto value = reinterpret_cast<std::intptr_t>(fill);
    if (value >= -1 && value < 3) return true;
    const std::uint8_t mode = *reinterpret_cast<const std::uint8_t*>(fill);
    return (mode & 0xf) == 0 && mode >= 0x10 && mode <= 0x50;
}

bool IsValidEffect(std::uint32_t effect) {
    return effect < 17 && ((FillEffects >> effect) & 1) != 0;
}

bool IsValidLayoutNibble(std::uint32_t nibble) {
    return nibble <= 4 && nibble != 3;
}

bool IsValidLayout(std::int32_t first, std::int32_t second) {
    const bool firstValid = first == 0 || first == 0x170 || first == 0x207 || first == 0x400 || first == 0x800 || first == 0xf77;
    const auto bits = static_cast<std::uint32_t>(second);
    return firstValid && IsValidLayoutNibble((bits >> 4) & 0xf) && IsValidLayoutNibble(bits & 0xf);
}

bool IsUsableRegion(const FontGraphicsRegion& region) {
    if (region.tag != RegionTag) return false;
    if (!(region.outerRadius >= 0.0f) || !(region.innerRadius >= 0.0f)) return false;
    const float half = (region.width < region.height ? region.width : region.height) * 0.5f;
    if (region.outerRadius == 0.0f) return half >= region.innerRadius;
    return half >= region.outerRadius && region.outerRadius >= region.innerRadius;
}

std::uint8_t MappedFlags(std::uint8_t flags) {
    return static_cast<std::uint8_t>((flags & 0xfc) | 1);
}

}

namespace Font {

void SetGraphicsErrorCallback(GraphicsErrorCallback callback) {
    g_errorCallback.store(callback);
}

int GraphicsRegionInit(FontGraphicsRegion* region, float x, float y, float width, float height) {
    if (!region || 0.0f > width || 0.0f > height) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    *region = {x, y, width, height, RectangleRegionFlags, RegionTag, 0.0f, 0.0f};
    return SCE_FONT_OK;
}

int GraphicsRegionInitCircular(FontGraphicsRegion* region, float centerX, float centerY, float outerRadius, float innerRadius) {
    if (innerRadius > outerRadius || !region || 0.0f >= outerRadius || 0.0f >= innerRadius) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    const float diameter = outerRadius + outerRadius;
    *region = {centerX - outerRadius, centerY - outerRadius, diameter, diameter, CircularRegionFlags, RegionTag, outerRadius, innerRadius};
    return SCE_FONT_OK;
}

int GraphicsRegionInitRoundish(FontGraphicsRegion* region, float x, float y, float width, float height, float outerRadius, float innerRadius, std::uint32_t flags) {
    if (!region || 0.0f > innerRadius || 0.0f > outerRadius || 0.0f >= height || 0.0f >= width || flags > 0xff) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    if (outerRadius > height * 0.5f || outerRadius > width * 0.5f) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    if (outerRadius < innerRadius && outerRadius != 0.0f) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    *region = {x, y, width, height, flags, RegionTag, outerRadius, innerRadius};
    return SCE_FONT_OK;
}

int GraphicsFillMethodInit(FontGraphicsFillMethod* method, std::int32_t mode, std::int32_t type) {
    const auto modeBits = static_cast<std::uint32_t>(mode);
    const bool modeValid = (modeBits & 0xf) == 0 && (modeBits >> 4) < 6;
    const bool typeValid = static_cast<std::uint32_t>(type) - 1 < 7 || type == 12;
    if (!method || !modeValid || !typeValid) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    *method = {};
    method->mode = mode;
    method->type = type;
    method->flags = FillMethodFlags;
    method->tag = FillMethodTag;
    return SCE_FONT_OK;
}

int GraphicsFillPlotInit(FontGraphicsFillPlot* plot, const FontGraphicsRegion* region) {
    if (!IsObject(plot)) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT);
    if (IsObject(region) && !IsUsableRegion(*region)) {
        plot->tag = 0;
        return Fail(FONT_GRAPHICS_ERROR_INVALID_REGION);
    }
    *plot = {};
    if (!region) {
        plot->flags = 2;
    } else if (!IsObject(region)) {
        plot->flags = 6;
        plot->layout = DefaultLayout;
    } else {
        plot->mapping[0] = region->x;
        plot->mapping[1] = region->y;
        plot->mapping[2] = region->x + region->width;
        plot->mapping[3] = region->y + region->height;
        plot->flags = 1;
        plot->layout = DefaultLayout;
    }
    plot->region = region;
    plot->tag = FillPlotTag;
    return SCE_FONT_OK;
}

int GraphicsFillPlotSetLayout(FontGraphicsFillPlot* plot, std::int32_t first, std::int32_t second) {
    if (!IsObject(plot) || plot->tag != FillPlotTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT);
    if (!IsValidLayout(first, second)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    const auto combined = static_cast<std::uint32_t>(first) | static_cast<std::uint32_t>(second);
    plot->layout = static_cast<std::uint16_t>(combined);
    plot->flags = static_cast<std::uint8_t>((plot->flags & 0xf6) | (combined != 0 ? 1 : 0) | 8);
    return SCE_FONT_OK;
}

int GraphicsFillPlotSetMapping(FontGraphicsFillPlot* plot, float first, float second, float third, float fourth) {
    if (!IsObject(plot) || plot->tag != FillPlotTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_PLOT);
    plot->mapping[0] = first;
    plot->mapping[1] = second;
    plot->mapping[2] = third;
    plot->mapping[3] = fourth;
    plot->flags = MappedFlags(plot->flags);
    return SCE_FONT_OK;
}

int GraphicsFillRatesInit(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect) {
    if (!IsObject(rates)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    *rates = {};
    if (!IsValidFill(fill)) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL);
    if (!IsValidEffect(effect)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    rates->fill = fill;
    rates->tag = FillRatesTag;
    rates->effect = static_cast<std::uint16_t>(effect);
    rates->layout = DefaultLayout;
    rates->flags = 2;
    return SCE_FONT_OK;
}

int GraphicsFillRatesSetFillEffect(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect) {
    if (!IsObject(rates) || rates->tag != FillRatesTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_RATES);
    if (!IsValidFill(fill)) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL);
    if (!IsValidEffect(effect)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    rates->effect = static_cast<std::uint16_t>(effect);
    rates->fill = fill;
    return SCE_FONT_OK;
}

int GraphicsFillRatesSetLayout(FontGraphicsFillRates* rates, std::int32_t first, std::int32_t second) {
    if (!IsObject(rates) || rates->tag != FillRatesTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_RATES);
    if (!IsValidLayout(first, second)) return Fail(SCE_FONT_ERROR_INVALID_PARAMETER);
    const auto combined = static_cast<std::uint32_t>(first) | static_cast<std::uint32_t>(second);
    rates->flags = static_cast<std::uint8_t>((rates->flags & 0xfe) | (combined != 0 ? 1 : 0));
    rates->layout = 0;
    return SCE_FONT_OK;
}

int GraphicsFillRatesSetMapping(FontGraphicsFillRates* rates, float first, float second, float third, float fourth) {
    if (!IsObject(rates) || rates->tag != FillRatesTag) return Fail(FONT_GRAPHICS_ERROR_INVALID_FILL_RATES);
    rates->mapping[0] = first;
    rates->mapping[1] = second;
    rates->mapping[2] = third;
    rates->mapping[3] = fourth;
    rates->flags = MappedFlags(rates->flags);
    return SCE_FONT_OK;
}

}
