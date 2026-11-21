#include "prx/libSceNKWebKit/include/FieldGettersExports.hpp"
#include "prx/libSceNKWebKit/include/FieldSettersExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ModuleFieldAccessors check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

alignas(16) static unsigned char object[0x1000];

template<typename T> static T Pattern() {
    T value;
    const unsigned char bytes[8] = {0x91, 0x82, 0x73, 0x64, 0x55, 0x46, 0x37, 0x28};
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

template<typename T, typename F> static void CheckGet(F get, std::size_t offset) {
    std::memset(object, 0xa5, sizeof(object));
    const T value = Pattern<T>();
    std::memcpy(object + offset, &value, sizeof(value));
    const T read = get(object);
    Require(std::memcmp(&read, &value, sizeof(value)) == 0);
}

template<typename T, typename F> static void CheckSet(F set, std::size_t offset, T value) {
    std::memset(object, 0xa5, sizeof(object));
    set(object, value);
    for (std::size_t i = 0; i < sizeof(object); ++i) {
        if (i >= offset && i < offset + sizeof(T)) continue;
        Require(object[i] == 0xa5);
    }
    Require(std::memcmp(object + offset, &value, sizeof(T)) == 0);
}

template<typename F> static bool Throws(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    CheckSet<std::uint8_t>([](void* p, std::uint8_t) { _ZN3JSC17DeferredWorkTimer16stopRunningTasksEv(p); }, 0x24, 0);
    Require(Throws([] { _ZN3JSC17DeferredWorkTimer16stopRunningTasksEv(nullptr); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN3JSC18GCActivityCallback10setEnabledEb(p, v); }, 0x23, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN3JSC18GCActivityCallback10setEnabledEb(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t) { _ZN3JSC22FullGCActivityCallback16setDidGCRecentlyEv(p); }, 0x30, 1);
    Require(Throws([] { _ZN3JSC22FullGCActivityCallback16setDidGCRecentlyEv(nullptr); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore10ScrollView23setPaintsEntireContentsEb(p, v); }, 0x121, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore10ScrollView23setPaintsEntireContentsEb(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore12EventHandler23setImmediateActionStageENS_20ImmediateActionStageE(p, v); }, 0x66, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore12EventHandler23setImmediateActionStageENS_20ImmediateActionStageE(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore15DatabaseManager14setIsAvailableEb(p, v); }, 0x8, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore15DatabaseManager14setIsAvailableEb(nullptr, 0); }));
    CheckSet<std::uint32_t>([](void* p, std::uint32_t v) { _ZN7WebCore18TextureMapperLayer5setIDEj(p, v); }, 0x2b0, Pattern<std::uint32_t>());
    Require(Throws([] { _ZN7WebCore18TextureMapperLayer5setIDEj(nullptr, 0); }));
    CheckSet<std::uintptr_t>([](void* p, std::uintptr_t v) { _ZN7WebCore19InspectorController26setInspectorFrontendClientEPNS_23InspectorFrontendClientE(p, v); }, 0x128, Pattern<std::uintptr_t>());
    Require(Throws([] { _ZN7WebCore19InspectorController26setInspectorFrontendClientEPNS_23InspectorFrontendClientE(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore21NetworkStorageSession31setThirdPartyCookieBlockingModeENS_28ThirdPartyCookieBlockingModeE(p, v); }, 0xa1, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore21NetworkStorageSession31setThirdPartyCookieBlockingModeENS_28ThirdPartyCookieBlockingModeE(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore21NetworkStorageSession32setResourceLoadStatisticsEnabledEb(p, v); }, 0x30, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore21NetworkStorageSession32setResourceLoadStatisticsEnabledEb(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore21NetworkStorageSession44setResourceLoadStatisticsDebugLoggingEnabledEb(p, v); }, 0x31, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore21NetworkStorageSession44setResourceLoadStatisticsDebugLoggingEnabledEb(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore4Page19setDelegatesScalingEb(p, v); }, 0x392, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore4Page19setDelegatesScalingEb(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t) { _ZN7WebCore4Page27enableICECandidateFilteringEv(p); }, 0x38e, 1);
    Require(Throws([] { _ZN7WebCore4Page27enableICECandidateFilteringEv(nullptr); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t) { _ZN7WebCore4Page28disableICECandidateFilteringEv(p); }, 0x38e, 0);
    Require(Throws([] { _ZN7WebCore4Page28disableICECandidateFilteringEv(nullptr); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore4Page37setInLowQualityImageInterpolationModeEb(p, v); }, 0xe0, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore4Page37setInLowQualityImageInterpolationModeEb(nullptr, 0); }));
    CheckGet<std::uint64_t>([](const void* p) { return _ZN7WebCore8Document13svgExtensionsEv(p); }, 0x508);
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore8Document24setShouldCreateRenderersEb(p, v); }, 0x8dc, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore8Document24setShouldCreateRenderersEb(nullptr, 0); }));
    CheckSet<std::uint16_t>([](void* p, std::uint16_t v) { _ZN7WebCore9FrameView16setPaintBehaviorEN3WTF9OptionSetINS_13PaintBehaviorEEE(p, v); }, 0x390, Pattern<std::uint16_t>());
    Require(Throws([] { _ZN7WebCore9FrameView16setPaintBehaviorEN3WTF9OptionSetINS_13PaintBehaviorEEE(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t v) { _ZN7WebCore9FrameView29setShouldUpdateWhileOffscreenEb(p, v); }, 0x40b, Pattern<std::uint8_t>());
    Require(Throws([] { _ZN7WebCore9FrameView29setShouldUpdateWhileOffscreenEb(nullptr, 0); }));
    CheckSet<std::uint8_t>([](void* p, std::uint8_t) { _ZN9Inspector15InspectorTarget5pauseEv(p); }, 0x10, 1);
    Require(Throws([] { _ZN9Inspector15InspectorTarget5pauseEv(nullptr); }));
    CheckGet<std::uint8_t>([](const void* p) { return _ZN9Inspector15RemoteInspector29waitingForAutomaticInspectionEj(p); }, 0x82);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK3JSC18GCActivityCallback9isEnabledEv(p); }, 0x23);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK3JSC22FullGCActivityCallback13didGCRecentlyEv(p); }, 0x30);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK3WTF15AutomaticThread19hasUnderlyingThreadERKNS_14AbstractLockerE(p); }, 0x2b);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore10ScrollView14useFixedLayoutEv(p); }, 0x120);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore11FrameLoader10isCompleteEv(p); }, 0xb0);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore11FrameLoader8loadTypeEv(p); }, 0x51);
    CheckGet<std::uint64_t>([](const void* p) { return _ZNK7WebCore11JSDOMWindow4selfERN3JSC14JSGlobalObjectE(p); }, 0x68);
    CheckGet<std::uint64_t>([](const void* p) { return _ZNK7WebCore11JSDOMWindow6framesERN3JSC14JSGlobalObjectE(p); }, 0x68);
    CheckGet<std::uint64_t>([](const void* p) { return _ZNK7WebCore11JSDOMWindow6windowERN3JSC14JSGlobalObjectE(p); }, 0x68);
    CheckGet<std::uint64_t>([](const void* p) { return _ZNK7WebCore11JSDOMWindow7wrappedEv(p); }, 0xeb8);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore11MediaPlayer14preservesPitchEv(p); }, 0xd3);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore11MediaPlayer25preferredDynamicRangeModeEv(p); }, 0xd8);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore11MediaPlayer39contentMIMETypeWasInferredFromExtensionEv(p); }, 0xd6);
    CheckGet<std::uint64_t>([](const void* p) { return _ZNK7WebCore11MediaPlayer4sizeEv(p); }, 0xb8);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore11MediaPlayer5mutedEv(p); }, 0xd2);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore11MediaPlayer7preloadEv(p); }, 0xc0);
    CheckGet<std::uint16_t>([](const void* p) { return _ZNK7WebCore11XPathResult10resultTypeEv(p); }, 0x40);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore14DocumentLoader16mouseEventPolicyEv(p); }, 0xa6f);
    Require(_ZNK7WebCore15HTMLFormElement24unsafeAssociatedElementsEv(object) == object + 0xc0);
    CheckGet<double>([](const void* p) { return _ZNK7WebCore15ProgressTracker17estimatedProgressEv(p); }, 0x68);
    CheckGet<std::uint32_t>([](const void* p) { return _ZNK7WebCore16HTMLInputElement4sizeEv(p); }, 0xe8);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore16HTMLMediaElement15bufferingPolicyEv(p); }, 0x420);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore16HTMLMediaElement20webkitPreservesPitchEv(p); }, 0x2e0);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore17FullscreenManager21isAnimatingFullscreenEv(p); }, 0x99);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore17FullscreenManager27areFullscreenControlsHiddenEv(p); }, 0x9a);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore21NetworkStorageSession29resourceLoadStatisticsEnabledEv(p); }, 0x30);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore21NetworkStorageSession41resourceLoadStatisticsDebugLoggingEnabledEv(p); }, 0x31);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore27DocumentTimelinesController22animationsAreSuspendedEv(p); }, 0x28);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore4Page34inLowQualityImageInterpolationModeEv(p); }, 0xe0);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore9FrameView13isTransparentEv(p); }, 0x414);
    CheckGet<std::uint16_t>([](const void* p) { return _ZNK7WebCore9FrameView13paintBehaviorEv(p); }, 0x390);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK7WebCore9FrameView17wasScrolledByUserEv(p); }, 0x416);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector14ConsoleMessage4typeEv(p); }, 0x1);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector14ConsoleMessage5levelEv(p); }, 0x2);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector15InspectorTarget8isPausedEv(p); }, 0x10);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector15RemoteInspector21hasActiveDebugSessionEv(p); }, 0x79);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector15RemoteInspector7enabledEv(p); }, 0x78);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector21InspectorConsoleAgent7enabledEv(p); }, 0x60);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector22InspectorDebuggerAgent27pauseOnNextStatementEnabledEv(p); }, 0xe3);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector22InspectorDebuggerAgent7enabledEv(p); }, 0xe0);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector22RemoteAutomationTarget8isPairedEv(p); }, 0xc);
    CheckGet<std::uint8_t>([](const void* p) { return _ZNK9Inspector28InspectorScriptProfilerAgent18isAlreadyProfilingEv(p); }, 0x3a);
    CheckGet<std::uint8_t>([](const void* p) { return _ZThn24_NK9Inspector28InspectorScriptProfilerAgent18isAlreadyProfilingEv(p); }, 0x22);
    return 0;
}
