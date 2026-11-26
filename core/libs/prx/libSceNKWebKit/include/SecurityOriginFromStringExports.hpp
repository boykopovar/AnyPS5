#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

WebCore::Ref<NKWebKit::WebCoreAbi::SecurityOrigin> APS5_VABI _ZN7WebCore14SecurityOrigin16createFromStringERKN3WTF6StringE(const NKWebKit::WtfAbi::String* originString);

}
