#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/NodeAbi.hpp"

#include <cstdint>

extern "C" {

NKWebKit::WebCoreAbi::Node* APS5_VABI _ZNK7WebCore13ContainerNode17firstElementChildEv(const NKWebKit::WebCoreAbi::ContainerNode* self);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZNK7WebCore13ContainerNode16lastElementChildEv(const NKWebKit::WebCoreAbi::ContainerNode* self);
std::uint32_t APS5_VABI _ZNK7WebCore13ContainerNode17childElementCountEv(const NKWebKit::WebCoreAbi::ContainerNode* self);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZNK7WebCore13ContainerNode17traverseToChildAtEj(const NKWebKit::WebCoreAbi::ContainerNode* self, std::uint32_t index);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZNK7WebCore4Node18nextElementSiblingEv(const NKWebKit::WebCoreAbi::Node* self);
NKWebKit::WebCoreAbi::Node* APS5_VABI _ZNK7WebCore4Node22previousElementSiblingEv(const NKWebKit::WebCoreAbi::Node* self);

}
