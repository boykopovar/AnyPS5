#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"

extern "C" {

WebCore::Region APS5_VABI _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(const WebCore::RoundedRect& roundedRect, unsigned stepLength);

}
