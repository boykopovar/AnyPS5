#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/HistoryItemAbi.hpp"
#include <cstdint>

extern "C" {

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore11HistoryItem9urlStringEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore11HistoryItem17originalURLStringEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore11HistoryItem5titleEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore11HistoryItem14alternateTitleEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore11HistoryItem8referrerEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore11HistoryItem6targetEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
const WebCore::IntPoint& APS5_VABI _ZNK7WebCore11HistoryItem14scrollPositionEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
float APS5_VABI _ZNK7WebCore11HistoryItem15pageScaleFactorEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
bool APS5_VABI _ZNK7WebCore11HistoryItem12isTargetItemEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
bool APS5_VABI _ZNK7WebCore11HistoryItem27shouldRestoreScrollPositionEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
std::uint8_t APS5_VABI _ZNK7WebCore11HistoryItem28shouldOpenExternalURLsPolicyEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
const NKWebKit::WebCoreAbi::HistoryItemVectorStorage& APS5_VABI _ZNK7WebCore11HistoryItem13documentStateEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
const NKWebKit::WebCoreAbi::HistoryItemVectorStorage& APS5_VABI _ZNK7WebCore11HistoryItem8childrenEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore11HistoryItem15formContentTypeEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
bool APS5_VABI _ZNK7WebCore11HistoryItem11hasChildrenEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
bool APS5_VABI _ZNK7WebCore11HistoryItem20hasCachedPageExpiredEv(const NKWebKit::WebCoreAbi::HistoryItem* self);
void* APS5_VABI _ZN7WebCore11HistoryItem8formDataEv(NKWebKit::WebCoreAbi::HistoryItem* self);
void APS5_VABI _ZN7WebCore11HistoryItem17setAlternateTitleERKN3WTF6StringE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore11HistoryItem20setOriginalURLStringERKN3WTF6StringE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore11HistoryItem8setTitleERKN3WTF6StringE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore11HistoryItem18setFormContentTypeERKN3WTF6StringE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN7WebCore11HistoryItem17setScrollPositionERKNS_8IntPointE(NKWebKit::WebCoreAbi::HistoryItem* self, const WebCore::IntPoint* value);
void APS5_VABI _ZN7WebCore11HistoryItem18setPageScaleFactorEf(NKWebKit::WebCoreAbi::HistoryItem* self, float value);
void APS5_VABI _ZN7WebCore11HistoryItem15setIsTargetItemEb(NKWebKit::WebCoreAbi::HistoryItem* self, bool value);
void APS5_VABI _ZN7WebCore11HistoryItem30setShouldRestoreScrollPositionEb(NKWebKit::WebCoreAbi::HistoryItem* self, bool value);
void APS5_VABI _ZN7WebCore11HistoryItem31setShouldOpenExternalURLsPolicyENS_28ShouldOpenExternalURLsPolicyE(NKWebKit::WebCoreAbi::HistoryItem* self, std::uint8_t value);
void APS5_VABI _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* value);
NKWebKit::WebCoreAbi::HistoryItem* APS5_VABI _ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* target);
void APS5_VABI _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* urlString, const NKWebKit::WtfAbi::String* title, const NKWebKit::WtfAbi::String* alternateTitle, NKWebKit::WebCoreAbi::BackForwardItemIdentifier identifier);
void APS5_VABI _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WtfAbi::String* urlString, const NKWebKit::WtfAbi::String* title, const NKWebKit::WtfAbi::String* alternateTitle, NKWebKit::WebCoreAbi::BackForwardItemIdentifier identifier);
void APS5_VABI _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(NKWebKit::WebCoreAbi::HistoryItem* self, const NKWebKit::WebCoreAbi::HistoryItemVectorStorage* state);
void APS5_VABI _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(NKWebKit::WebCoreAbi::HistoryItem* self, NKWebKit::WebCoreAbi::HistoryItem** child);
void APS5_VABI _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(NKWebKit::WebCoreAbi::HistoryItem* self, void** object);
void APS5_VABI _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(NKWebKit::WebCoreAbi::HistoryItem* self, void** formData);

}
