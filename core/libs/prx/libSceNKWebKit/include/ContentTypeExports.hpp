#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ContentTypeAbi.hpp"

extern "C" {

const NKWebKit::WtfAbi::String& APS5_VABI _ZN7WebCore11ContentType15codecsParameterEv();
void APS5_VABI _ZN7WebCore11ContentTypeC1EON3WTF6StringE(NKWebKit::WebCoreAbi::ContentType* self, NKWebKit::WtfAbi::String* type);
void APS5_VABI _ZN7WebCore11ContentTypeC1ERKN3WTF6StringE(NKWebKit::WebCoreAbi::ContentType* self, const NKWebKit::WtfAbi::String* type);
void APS5_VABI _ZN7WebCore11ContentTypeC2EON3WTF6StringE(NKWebKit::WebCoreAbi::ContentType* self, NKWebKit::WtfAbi::String* type);
void APS5_VABI _ZN7WebCore11ContentTypeC2ERKN3WTF6StringE(NKWebKit::WebCoreAbi::ContentType* self, const NKWebKit::WtfAbi::String* type);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore11ContentType13containerTypeEv(const NKWebKit::WebCoreAbi::ContentType* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(const NKWebKit::WebCoreAbi::ContentType* self, const NKWebKit::WtfAbi::String* parameterName);

}
