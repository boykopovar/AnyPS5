#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICS_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICS_HPP

#include <cstddef>
#include <cstdint>

#include "prx/libc/include/General.hpp"

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

static_assert(sizeof(FontGraphicsRegion) == 32);
static_assert(offsetof(FontGraphicsRegion, x) == 0);
static_assert(offsetof(FontGraphicsRegion, y) == 4);
static_assert(offsetof(FontGraphicsRegion, width) == 8);
static_assert(offsetof(FontGraphicsRegion, height) == 12);
static_assert(offsetof(FontGraphicsRegion, flags) == 16);
static_assert(offsetof(FontGraphicsRegion, tag) == 20);
static_assert(offsetof(FontGraphicsRegion, outerRadius) == 24);
static_assert(offsetof(FontGraphicsRegion, innerRadius) == 28);

struct FontGraphicsFillMethod {
    std::int32_t mode;
    std::int32_t type;
    std::uint32_t flags;
    std::uint32_t tag;
    std::byte reserved[16];
};

static_assert(sizeof(FontGraphicsFillMethod) == 32);
static_assert(offsetof(FontGraphicsFillMethod, mode) == 0);
static_assert(offsetof(FontGraphicsFillMethod, type) == 4);
static_assert(offsetof(FontGraphicsFillMethod, flags) == 8);
static_assert(offsetof(FontGraphicsFillMethod, tag) == 12);
static_assert(offsetof(FontGraphicsFillMethod, reserved) == 16);

struct FontGraphicsFillPlot {
    float mapping[4];
    const FontGraphicsRegion* region;
    std::uint16_t tag;
    std::uint16_t reserved26;
    std::uint16_t layout;
    std::uint8_t flags;
    std::byte reserved[33];
};

static_assert(sizeof(FontGraphicsFillPlot) == 64);
static_assert(offsetof(FontGraphicsFillPlot, mapping) == 0);
static_assert(offsetof(FontGraphicsFillPlot, region) == 16);
static_assert(offsetof(FontGraphicsFillPlot, tag) == 24);
static_assert(offsetof(FontGraphicsFillPlot, reserved26) == 26);
static_assert(offsetof(FontGraphicsFillPlot, layout) == 28);
static_assert(offsetof(FontGraphicsFillPlot, flags) == 30);
static_assert(offsetof(FontGraphicsFillPlot, reserved) == 31);

struct FontGraphicsFillRates {
    float mapping[4];
    const FontGraphicsFill* fill;
    std::uint16_t tag;
    std::uint16_t effect;
    std::uint16_t layout;
    std::uint8_t flags;
    std::byte reserved[33];
};

static_assert(sizeof(FontGraphicsFillRates) == 64);
static_assert(offsetof(FontGraphicsFillRates, mapping) == 0);
static_assert(offsetof(FontGraphicsFillRates, fill) == 16);
static_assert(offsetof(FontGraphicsFillRates, tag) == 24);
static_assert(offsetof(FontGraphicsFillRates, effect) == 26);
static_assert(offsetof(FontGraphicsFillRates, layout) == 28);
static_assert(offsetof(FontGraphicsFillRates, flags) == 30);
static_assert(offsetof(FontGraphicsFillRates, reserved) == 31);

namespace Font {

using GraphicsErrorCallback = void (APS5_VABI*)(int error);

void SetGraphicsErrorCallback(GraphicsErrorCallback callback);
int GraphicsRegionInit(FontGraphicsRegion* region, float x, float y, float width, float height);
int GraphicsRegionInitCircular(FontGraphicsRegion* region, float centerX, float centerY, float outerRadius, float innerRadius);
int GraphicsRegionInitRoundish(FontGraphicsRegion* region, float x, float y, float width, float height, float outerRadius, float innerRadius, std::uint32_t flags);
int GraphicsFillMethodInit(FontGraphicsFillMethod* method, std::int32_t mode, std::int32_t type);
int GraphicsFillPlotInit(FontGraphicsFillPlot* plot, const FontGraphicsRegion* region);
int GraphicsFillPlotSetLayout(FontGraphicsFillPlot* plot, std::int32_t first, std::int32_t second);
int GraphicsFillPlotSetMapping(FontGraphicsFillPlot* plot, float first, float second, float third, float fourth);
int GraphicsFillRatesInit(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect);
int GraphicsFillRatesSetFillEffect(FontGraphicsFillRates* rates, const FontGraphicsFill* fill, std::uint32_t effect);
int GraphicsFillRatesSetLayout(FontGraphicsFillRates* rates, std::int32_t first, std::int32_t second);
int GraphicsFillRatesSetMapping(FontGraphicsFillRates* rates, float first, float second, float third, float fourth);

}

#endif
