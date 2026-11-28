#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ResourceErrorAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::ResourceError APS5_VABI _ZN7WebCore13internalErrorERKN3WTF3URLE(const NKWebKit::WtfAbi::URL* url);

}
