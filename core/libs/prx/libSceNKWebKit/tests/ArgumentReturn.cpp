#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit argument check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

int main() {
    int first = 0;
    int second = 0;
    int third = 0;

    Require(_ZN3JSC12HeapAnalyzeraSERKS0_(&first) == &first);
    Require(_ZN3JSC9CodeBlock19baselineAlternativeEv(&first) == &first);
    Require(_ZN3WTF11PrintStream5beginEv(&first) == &first);
    Require(_ZN7WebCore17JSDOMGlobalObject12globalObjectEv(&first) == &first);
    Require(_ZN7WebCore21MediaRecorderProvideraSERKS0_(&first) == &first);
    Require(_ZN7WebCore22EmptyFrameLoaderClientaSERKS0_(&first) == &first);
    Require(_ZN7WebCore24CachedResourceHandleBaseaSERKS0_(&first) == &first);
    Require(_ZN7WebCore27ContentSecurityPolicyClientaSERKS0_(&first) == &first);
    Require(_ZN7WebCore28InspectorFrontendClientLocal8SettingsaSERKS1_(&first) == &first);
    Require(_ZN9Inspector31NetworkBackendDispatcherHandleraSERKS0_(&first) == &first);
    Require(_ZN9Inspector32DatabaseBackendDispatcherHandleraSERKS0_(&first) == &first);
    Require(static_cast<const void*>(&_ZNK7WebCore14CredentialBase4userEv(
                reinterpret_cast<const NKWebKit::WebCoreAbi::CredentialBase*>(&first))) == &first);
    Require(static_cast<const void*>(&_ZNK7WebCore15HTTPHeaderField4nameEv(
                reinterpret_cast<const NKWebKit::WebCoreAbi::HTTPHeaderField*>(&first))) == &first);
    Require(_ZNK9Inspector15ScriptCallFrame12functionNameEv(&first) == &first);
    Require(_ZN3JSC19HeapSnapshotBuildernaEmPv(&first, &second) == &second);
    Require(_ZN3JSC19HeapSnapshotBuildernwEmPv(&first, &second) == &second);
    Require(_ZN3JSC4Yarr17RegularExpressionnaEmPv(&first, &second) == &second);
    Require(_ZN3JSC4Yarr17RegularExpressionnwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore11MediaPlayernaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore11MediaPlayernwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore13MIMETypeCachenaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore13MIMETypeCachenwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore14StaticNodeListnaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore14StaticNodeListnwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore15AsyncFileStreamnaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore15AsyncFileStreamnwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore16BlobRegistryImplnaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore16BlobRegistryImplnwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore16TrackPrivateBasenaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore16TrackPrivateBasenwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore17NowPlayingManagernaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore17NowPlayingManagernwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore17PageConsoleClientnaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore17PageConsoleClientnwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore18PrewarmInformationnwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore18TextureMapperLayernaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore18TextureMapperLayernwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore21MediaRecorderProvidernaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore21MediaRecorderProvidernwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore28InspectorFrontendClientLocal8SettingsnaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore28InspectorFrontendClientLocal8SettingsnwEmPv(&first, &second) == &second);
    Require(_ZN7WebCore6Path2DnaEmPv(&first, &second) == &second);
    Require(_ZN7WebCore6Path2DnwEmPv(&first, &second) == &second);
    Require(_ZN7bmalloc11IsoTLSEntrynaEmPv(&first, &second) == &second);
    Require(_ZN7bmalloc11IsoTLSEntrynwEmPv(&first, &second) == &second);
    Require(_ZN7bmalloc15IsoHeapImplBasenaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector14ConsoleMessagenaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector14ConsoleMessagenwEmPv(&first, &second) == &second);
    Require(_ZN9Inspector14InspectorAgentnaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector14InspectorAgentnwEmPv(&first, &second) == &second);
    Require(_ZN9Inspector18InspectorHeapAgentnaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector18InspectorHeapAgentnwEmPv(&first, &second) == &second);
    Require(_ZN9Inspector19InspectorAuditAgentnaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector19InspectorAuditAgentnwEmPv(&first, &second) == &second);
    Require(_ZN9Inspector20InspectorTargetAgentnaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector20InspectorTargetAgentnwEmPv(&first, &second) == &second);
    Require(_ZN9Inspector21InjectedScriptManagernaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector21InjectedScriptManagernwEmPv(&first, &second) == &second);
    Require(_ZN9Inspector21InspectorConsoleAgentnaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector21InspectorConsoleAgentnwEmPv(&first, &second) == &second);
    Require(_ZN9Inspector21InspectorRuntimeAgentnaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector21InspectorRuntimeAgentnwEmPv(&first, &second) == &second);
    Require(_ZN9Inspector22InspectorDebuggerAgentnaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector28InspectorScriptProfilerAgentnaEmPv(&first, &second) == &second);
    Require(_ZN9Inspector28InspectorScriptProfilerAgentnwEmPv(&first, &second) == &second);
    Require(_ZN3JSC19HeapSnapshotBuildernwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN3JSC4Yarr17RegularExpressionnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN7WebCore11MediaPlayernwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN7WebCore16BlobRegistryImplnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN7WebCore16TrackPrivateBasenwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN7WebCore17PageConsoleClientnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN7WebCore18TextureMapperLayernwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN7WebCore6Path2DnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN9Inspector14InspectorAgentnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN9Inspector18InspectorHeapAgentnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN9Inspector21InjectedScriptManagernwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN9Inspector21InspectorConsoleAgentnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN9Inspector21InspectorRuntimeAgentnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN9Inspector22InspectorDebuggerAgentnwEm10NotNullTagPv(&first, &second, &third) == &third);
    Require(_ZN9Inspector28InspectorScriptProfilerAgentnwEm10NotNullTagPv(&first, &second, &third) == &third);

    return 0;
}
