#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/MimeAbi.hpp"

extern "C" {

bool APS5_VABI _ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry24registerURLSchemeAsLocalERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry25registerURLSchemeAsSecureERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry27registerURLSchemeAsNoAccessERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry30registerURLSchemeAsCORSEnabledERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry32registerURLSchemeAsEmptyDocumentERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
bool APS5_VABI _ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry34registerURLSchemeAsDisplayIsolatedERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry35registerURLSchemeAsCachePartitionedERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZN7WebCore20LegacySchemeRegistry36allURLSchemesRegisteredAsCORSEnabledEv();
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry36registerAsCanDisplayOnlyIfCanRequestERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry36registerURLSchemeAsAlwaysRevalidatedERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry40setDomainRelaxationForbiddenForURLSchemeEbRKN3WTF6StringE(bool forbidden, const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry49registerURLSchemeAsBypassingContentSecurityPolicyERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry57removeURLSchemeRegisteredAsBypassingContentSecurityPolicyERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);
void APS5_VABI _ZN7WebCore20LegacySchemeRegistry58registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsingERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme);

NKWebKit::WtfAbi::FixedVector<NKWebKit::WtfAbi::ASCIILiteral> APS5_VABI _ZN7WebCore16MIMETypeRegistry12pdfMIMETypesEv();
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);
NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZN7WebCore16MIMETypeRegistry16allowedMIMETypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& mimeTypes, const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& extensions);
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(NKWebKit::WtfAbi::StringView mimeType);
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);
NKWebKit::WtfAbi::FixedVector<NKWebKit::WtfAbi::ASCIILiteral> APS5_VABI _ZN7WebCore16MIMETypeRegistry23supportedImageMIMETypesEv();
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);
NKWebKit::WtfAbi::FixedVector<NKWebKit::WtfAbi::ASCIILiteral> APS5_VABI _ZN7WebCore16MIMETypeRegistry24unsupportedTextMIMETypesEv();
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry28isSupportedTextTrackMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& type);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(const NKWebKit::WtfAbi::String& filename, const NKWebKit::WtfAbi::String& mimeType);
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry32containsImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& mimeTypes, const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& extensions);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry33preferredImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& mimeTypes, const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& extensions);
bool APS5_VABI _ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType);

}