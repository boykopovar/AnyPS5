#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore8Settings16setImagesEnabledEb(void* self, bool imagesEnabled);
void APS5_VABI _ZN7WebCore8Settings19setShowDebugBordersEb(void* self, bool showDebugBorders);
void APS5_VABI _ZN7WebCore8Settings21setShowRepaintCounterEb(void* self, bool showRepaintCounter);
void APS5_VABI _ZN7WebCore8Settings29setAuthorAndUserStylesEnabledEb(void* self, bool authorAndUserStylesEnabled);
void APS5_VABI _ZN7WebCore8Settings31setICECandidateFilteringEnabledEb(void* self, bool iceCandidateFilteringEnabled);
void APS5_VABI _ZN7WebCore8Settings33setImagesEnabledInspectorOverrideEN3WTF8OptionalIbEE(void* self, NKWebKit::WebCoreAbi::OptionalBool imagesEnabledInspectorOverride);
void APS5_VABI _ZN7WebCore8Settings36setShowDebugBordersInspectorOverrideEN3WTF8OptionalIbEE(void* self, NKWebKit::WebCoreAbi::OptionalBool showDebugBordersInspectorOverride);
void APS5_VABI _ZN7WebCore8Settings38setShowRepaintCounterInspectorOverrideEN3WTF8OptionalIbEE(void* self, NKWebKit::WebCoreAbi::OptionalBool showRepaintCounterInspectorOverride);
void APS5_VABI _ZN7WebCore8Settings46setAuthorAndUserStylesEnabledInspectorOverrideEN3WTF8OptionalIbEE(void* self, NKWebKit::WebCoreAbi::OptionalBool authorAndUserStylesEnabledInspectorOverride);
void APS5_VABI _ZN7WebCore8Settings48setICECandidateFilteringEnabledInspectorOverrideEN3WTF8OptionalIbEE(void* self, NKWebKit::WebCoreAbi::OptionalBool iceCandidateFilteringEnabledInspectorOverride);
bool APS5_VABI _ZNK7WebCore8Settings15isScriptEnabledEv(const void* self);
bool APS5_VABI _ZNK7WebCore8Settings16areImagesEnabledEv(const void* self);
bool APS5_VABI _ZNK7WebCore8Settings16showDebugBordersEv(const void* self);
bool APS5_VABI _ZNK7WebCore8Settings18showRepaintCounterEv(const void* self);
bool APS5_VABI _ZNK7WebCore8Settings18webSecurityEnabledEv(const void* self);
bool APS5_VABI _ZNK7WebCore8Settings23needsSiteSpecificQuirksEv(const void* self);
bool APS5_VABI _ZNK7WebCore8Settings23webRTCEncryptionEnabledEv(const void* self);
bool APS5_VABI _ZNK7WebCore8Settings26authorAndUserStylesEnabledEv(const void* self);
bool APS5_VABI _ZNK7WebCore8Settings28iceCandidateFilteringEnabledEv(const void* self);

}
