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

}

#endif
