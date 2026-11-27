#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/NodeAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::Node* APS5_VABI _ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE(NKWebKit::WebCoreAbi::Node* node);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE(const NKWebKit::WebCoreAbi::Node* current);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_(const NKWebKit::WebCoreAbi::Node* current, const NKWebKit::WebCoreAbi::Node* stayWithin);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE(const NKWebKit::WebCoreAbi::ContainerNode* current);

}
