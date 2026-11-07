#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"

extern "C" {

const NKWebKit::WtfAbi::URL& APS5_VABI _ZNK7WebCore20ResourceResponseBase3urlEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase8mimeTypeEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
long long APS5_VABI _ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase16textEncodingNameEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase14httpStatusTextEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase11httpVersionEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
const NKWebKit::WebCoreAbi::ResponseHeaderStorage& APS5_VABI _ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);

}
