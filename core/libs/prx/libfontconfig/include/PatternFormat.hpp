#pragma once

#include "prx/libfontconfig/include/Types.hpp"
#include "SceTypes.hpp"

extern "C" FcChar8* APS5_VABI FcPatternFormat(FcPattern* pat, const FcChar8* format);
