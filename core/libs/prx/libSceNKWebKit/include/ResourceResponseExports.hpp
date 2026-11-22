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
void APS5_VABI _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceResponseBase* self, NKWebKit::HttpAbi::HTTPHeaderName name, const NKWebKit::WtfAbi::String& value);
void APS5_VABI _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldERKN3WTF6StringES4_(NKWebKit::WebCoreAbi::ResourceResponseBase* self, const NKWebKit::WtfAbi::String& name, const NKWebKit::WtfAbi::String& value);
void APS5_VABI _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceResponseBase* self, NKWebKit::HttpAbi::HTTPHeaderName name, const NKWebKit::WtfAbi::String& value);
void APS5_VABI _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldERKN3WTF6StringES4_(NKWebKit::WebCoreAbi::ResourceResponseBase* self, const NKWebKit::WtfAbi::String& name, const NKWebKit::WtfAbi::String& value);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(const NKWebKit::WebCoreAbi::ResourceResponseBase* self, NKWebKit::HttpAbi::HTTPHeaderName name);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase8isHTTP09Ev(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase12isAttachmentEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase24isAttachmentWithFilenameEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20ResourceResponseBase25sanitizeSuggestedFilenameERKN3WTF6StringE(const NKWebKit::WtfAbi::String& filename);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase23hasCacheValidatorFieldsEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoCacheEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoStoreEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase29cacheControlContainsImmutableEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
bool APS5_VABI _ZNK7WebCore20ResourceResponseBase34cacheControlContainsMustRevalidateEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
NKWebKit::WebCoreAbi::ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase18cacheControlMaxAgeEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
NKWebKit::WebCoreAbi::ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase32cacheControlStaleWhileRevalidateEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
NKWebKit::WebCoreAbi::ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase4dateEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
NKWebKit::WebCoreAbi::ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase7expiresEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
NKWebKit::WebCoreAbi::ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase12lastModifiedEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
NKWebKit::WebCoreAbi::ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase3ageEv(const NKWebKit::WebCoreAbi::ResourceResponseBase* self);
void APS5_VABI _ZN7WebCore20ResourceResponseBase11setMimeTypeERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceResponseBase* self, const NKWebKit::WtfAbi::String& mimeType);
void APS5_VABI _ZN7WebCore20ResourceResponseBase19setTextEncodingNameERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceResponseBase* self, const NKWebKit::WtfAbi::String& textEncodingName);
void APS5_VABI _ZN7WebCore20ResourceResponseBase17setHTTPStatusTextERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceResponseBase* self, const NKWebKit::WtfAbi::String& statusText);
void APS5_VABI _ZN7WebCore20ResourceResponseBase14setHTTPVersionERKN3WTF6StringE(NKWebKit::WebCoreAbi::ResourceResponseBase* self, const NKWebKit::WtfAbi::String& httpVersion);
void APS5_VABI _ZN7WebCore20ResourceResponseBaseC1ERKN3WTF3URLERKNS1_6StringExS7_(NKWebKit::WebCoreAbi::ResourceResponseBase* self, const NKWebKit::WtfAbi::URL& url, const NKWebKit::WtfAbi::String& mimeType, long long expectedContentLength, const NKWebKit::WtfAbi::String& textEncodingName);
void APS5_VABI _ZN7WebCore20ResourceResponseBaseC2ERKN3WTF3URLERKNS1_6StringExS7_(NKWebKit::WebCoreAbi::ResourceResponseBase* self, const NKWebKit::WtfAbi::URL& url, const NKWebKit::WtfAbi::String& mimeType, long long expectedContentLength, const NKWebKit::WtfAbi::String& textEncodingName);
void APS5_VABI _ZN7WebCore16ResourceResponseC1Ev(NKWebKit::WebCoreAbi::ResourceResponse* self);
void APS5_VABI _ZN7WebCore16ResourceResponseC2Ev(NKWebKit::WebCoreAbi::ResourceResponse* self);
void APS5_VABI _ZN7WebCore16ResourceResponseC1ERKN3WTF3URLERKNS1_6StringExS7_(NKWebKit::WebCoreAbi::ResourceResponse* self, const NKWebKit::WtfAbi::URL& url, const NKWebKit::WtfAbi::String& mimeType, long long expectedContentLength, const NKWebKit::WtfAbi::String& textEncodingName);
void APS5_VABI _ZN7WebCore16ResourceResponseC2ERKN3WTF3URLERKNS1_6StringExS7_(NKWebKit::WebCoreAbi::ResourceResponse* self, const NKWebKit::WtfAbi::URL& url, const NKWebKit::WtfAbi::String& mimeType, long long expectedContentLength, const NKWebKit::WtfAbi::String& textEncodingName);
void APS5_VABI _ZN7WebCore16ResourceResponseC1ERKS0_(NKWebKit::WebCoreAbi::ResourceResponse* self, const NKWebKit::WebCoreAbi::ResourceResponse* other);
void APS5_VABI _ZN7WebCore16ResourceResponseC2ERKS0_(NKWebKit::WebCoreAbi::ResourceResponse* self, const NKWebKit::WebCoreAbi::ResourceResponse* other);
NKWebKit::WebCoreAbi::ResourceResponse& APS5_VABI _ZN7WebCore16ResourceResponseaSERKS0_(NKWebKit::WebCoreAbi::ResourceResponse* self, const NKWebKit::WebCoreAbi::ResourceResponse* other);
void APS5_VABI _ZN7WebCore16ResourceResponseD1Ev(NKWebKit::WebCoreAbi::ResourceResponse* self);
void APS5_VABI _ZN7WebCore16ResourceResponseD2Ev(NKWebKit::WebCoreAbi::ResourceResponse* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(const NKWebKit::WebCoreAbi::ResourceResponse* self);

}
