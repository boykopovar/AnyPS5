#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/HistoryItemAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WtfAbi::URL APS5_VABI _ZNK7WebCore11HistoryItem3urlEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
NKWebKit::WtfAbi::URL APS5_VABI _ZNK7WebCore11HistoryItem11originalURLEv(const NKWebKit::WebCoreAbi::HistoryItem* self);

}
