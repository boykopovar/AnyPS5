#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

unsigned APS5_VABI _ZN3WTF19numGraphemeClustersENS_10StringViewE(NKWebKit::WtfAbi::StringView string);
unsigned APS5_VABI _ZN3WTF30numCodeUnitsInGraphemeClustersENS_10StringViewEj(NKWebKit::WtfAbi::StringView string, unsigned numGraphemeClusters);

}
