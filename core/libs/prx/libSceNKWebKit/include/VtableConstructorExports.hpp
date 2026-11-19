#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

const void* const* APS5_VABI NKWebKitVtable_JSC_WeakHandleOwnerTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_JSC_TypedArrayControllerTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WTF_FunctionDispatcherTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_MIMETypeCacheTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_XPathNSResolverTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_BlobRegistryImplTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_VisitedLinkStoreTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_FrameLoaderClientTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_LibWebRTCProviderTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_PluginInfoProviderTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_UserContentProviderTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_EmptyFrameLoaderClientTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_StorageNamespaceProviderTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_ContentSecurityPolicyClientTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_WebCore_InspectorFrontendClientLocal_SettingsTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_Inspector_RemoteInspector_ClientTable_nid_no_patch();
const void* const* APS5_VABI NKWebKitVtable_Inspector_RemoteControllableTargetTable_nid_no_patch();

void APS5_VABI NKWebKitVtableEmpty_nid_no_patch();
std::uint64_t APS5_VABI NKWebKitVtableZero_nid_no_patch();
void* APS5_VABI NKWebKitVtableNullResult_nid_no_patch(void* result);
void* APS5_VABI NKWebKitVtableZeroResult_nid_no_patch(void* result);

void APS5_VABI _ZN3JSC15WeakHandleOwnerC1ERKS0_(void* self, const void* other);
void APS5_VABI _ZN3JSC15WeakHandleOwnerC1Ev(void* self);
void APS5_VABI _ZN3JSC15WeakHandleOwnerC2ERKS0_(void* self, const void* other);
void APS5_VABI _ZN3JSC15WeakHandleOwnerC2Ev(void* self);
void APS5_VABI _ZN3JSC20TypedArrayControllerC2Ev(void* self);
void APS5_VABI _ZN3WTF18FunctionDispatcherC2Ev(void* self);
void APS5_VABI _ZN7WebCore13MIMETypeCacheC1Ev(void* self);
void APS5_VABI _ZN7WebCore13MIMETypeCacheC2Ev(void* self);
void APS5_VABI _ZN7WebCore15XPathNSResolverC2Ev(void* self);
void APS5_VABI _ZN7WebCore16BlobRegistryImplC1Ev(void* self);
void APS5_VABI _ZN7WebCore16BlobRegistryImplC2Ev(void* self);
void APS5_VABI _ZN7WebCore16VisitedLinkStoreC2Ev(void* self);
void APS5_VABI _ZN7WebCore17FrameLoaderClientC2ERKS0_(void* self, const void* other);
void APS5_VABI _ZN7WebCore17FrameLoaderClientC2Ev(void* self);
void APS5_VABI _ZN7WebCore17LibWebRTCProviderC1ERKS0_(void* self, const void* other);
void APS5_VABI _ZN7WebCore17LibWebRTCProviderC1Ev(void* self);
void APS5_VABI _ZN7WebCore17LibWebRTCProviderC2ERKS0_(void* self, const void* other);
void APS5_VABI _ZN7WebCore17LibWebRTCProviderC2Ev(void* self);
void APS5_VABI _ZN7WebCore18PluginInfoProviderC2Ev(void* self);
void APS5_VABI _ZN7WebCore19UserContentProviderC2Ev(void* self);
void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClientC1ERKS0_(void* self, const void* other);
void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClientC1Ev(void* self);
void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClientC2ERKS0_(void* self, const void* other);
void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClientC2Ev(void* self);
void APS5_VABI _ZN7WebCore24StorageNamespaceProviderC2Ev(void* self);
void APS5_VABI _ZN7WebCore27ContentSecurityPolicyClientC2ERKS0_(void* self, const void* other);
void APS5_VABI _ZN7WebCore27ContentSecurityPolicyClientC2Ev(void* self);
void APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8SettingsC1ERKS1_(void* self, const void* other);
void APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8SettingsC1Ev(void* self);
void APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8SettingsC2ERKS1_(void* self, const void* other);
void APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal8SettingsC2Ev(void* self);
void APS5_VABI _ZN9Inspector15RemoteInspector6ClientC2ERKS1_(void* self, const void* other);
void APS5_VABI _ZN9Inspector15RemoteInspector6ClientC2Ev(void* self);
void APS5_VABI _ZN9Inspector24RemoteControllableTargetC2ERKS0_(void* self, const void* other);
void APS5_VABI _ZN9Inspector24RemoteControllableTargetC2Ev(void* self);

}
