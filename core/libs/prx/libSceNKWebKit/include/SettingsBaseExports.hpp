#pragma once

#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceNKWebKit/src/ContentTypeAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const NKWebKit::WtfAbi::AtomString* family, int script);
void APS5_VABI _ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const NKWebKit::WtfAbi::AtomString* family, int script);
void APS5_VABI _ZN7WebCore12SettingsBase20setCursiveFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const NKWebKit::WtfAbi::AtomString* family, int script);
void APS5_VABI _ZN7WebCore12SettingsBase20setFantasyFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const NKWebKit::WtfAbi::AtomString* family, int script);
void APS5_VABI _ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const NKWebKit::WtfAbi::AtomString* family, int script);
void APS5_VABI _ZN7WebCore12SettingsBase22setSansSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const NKWebKit::WtfAbi::AtomString* family, int script);
void APS5_VABI _ZN7WebCore12SettingsBase23setPictographFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const NKWebKit::WtfAbi::AtomString* family, int script);
void APS5_VABI _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(void* self, WTF::Seconds interval);
void APS5_VABI _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(void* self, const NKWebKit::WtfAbi::String* contentTypes);
void APS5_VABI _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(void* self, const NKWebKit::WtfAbi::Vector<NKWebKit::WebCoreAbi::ContentType>* contentTypes);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(const void* self, int script);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode(const void* self, int script);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12SettingsBase17cursiveFontFamilyE11UScriptCode(const void* self, int script);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12SettingsBase17fantasyFontFamilyE11UScriptCode(const void* self, int script);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(const void* self, int script);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12SettingsBase19sansSerifFontFamilyE11UScriptCode(const void* self, int script);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode(const void* self, int script);

}
