#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICS_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICS_HPP

#include <cstdint>

#include "prx/libSceFont/include/Api.hpp"

namespace Font {

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
int GraphicsDrawupFillFlatColor(FontGraphicsFrame* graphicsObject, const void* color, void** output);
int GraphicsDrawupFillLinearGradient(FontGraphicsFrame* graphicsObject, std::uint32_t mode, const void* color1, const void* color2, void** output, float extent0, float extent1, float x0, float y0, float x1, float y1);
int GraphicsDrawupFillRadialGradient(FontGraphicsFrame* graphicsObject, std::uint32_t mode, const void* color1, const void* color2, void** output, float extent0, float extent1, float x0, float y0, float x1, float y1);
int GraphicsAppendGradientSegment(FontGraphicsFrame* graphicsObject, void* gradient, float position, const void* color);
int GraphicsDrawupFillTextureImageObject(FontGraphicsFrame* graphicsObject, const void* fillObject, std::uint32_t mode, void** output, float x, float y, float width, float height);
int GraphicsDrawupFillTexturePatternObject(FontGraphicsFrame* graphicsObject, const void* fillObject, std::uint32_t mode, std::uint32_t format, std::uint32_t select, std::uint32_t dimension1, std::uint32_t option1, std::uint32_t option2, std::uint32_t dimension2, void** output);

}

#endif
