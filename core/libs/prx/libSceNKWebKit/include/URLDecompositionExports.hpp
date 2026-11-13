#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/URLDecompositionAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16URLDecomposition4hashEv(const NKWebKit::WebCoreAbi::URLDecomposition* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16URLDecomposition4hostEv(const NKWebKit::WebCoreAbi::URLDecomposition* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16URLDecomposition4portEv(const NKWebKit::WebCoreAbi::URLDecomposition* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16URLDecomposition6searchEv(const NKWebKit::WebCoreAbi::URLDecomposition* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16URLDecomposition8hostnameEv(const NKWebKit::WebCoreAbi::URLDecomposition* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16URLDecomposition8pathnameEv(const NKWebKit::WebCoreAbi::URLDecomposition* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16URLDecomposition8protocolEv(const NKWebKit::WebCoreAbi::URLDecomposition* self);

}
