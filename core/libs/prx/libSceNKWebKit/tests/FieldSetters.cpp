#include "prx/libSceNKWebKit/include/FieldSettersExports.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool condition) {
    if (!condition) std::abort();
}

#define CHECK64(name, offset) \
    do { \
        std::array<std::uint8_t, 128> object; \
        object.fill(0xa5); \
        const auto before = object; \
        const std::uint64_t value = 0x1122334455667788ull; \
        name(object.data(), value); \
        for (std::size_t i = 0; i < object.size(); ++i) { \
            if (i >= (offset) && i < (offset) + sizeof(value)) continue; \
            Require(object[i] == before[i]); \
        } \
        std::uint64_t stored; \
        std::memcpy(&stored, object.data() + (offset), sizeof(stored)); \
        Require(stored == value); \
    } while (false)

#define CHECK32(name, offset) \
    do { \
        std::array<std::uint8_t, 128> object; \
        object.fill(0xa5); \
        const auto before = object; \
        const std::uint32_t value = 0x89abcdefu; \
        name(object.data(), value); \
        for (std::size_t i = 0; i < object.size(); ++i) { \
            if (i >= (offset) && i < (offset) + sizeof(value)) continue; \
            Require(object[i] == before[i]); \
        } \
        std::uint32_t stored; \
        std::memcpy(&stored, object.data() + (offset), sizeof(stored)); \
        Require(stored == value); \
    } while (false)

int main() {
    CHECK64(_ZN3JSC14ProtoCallFrame12setCodeBlockEPNS_9CodeBlockE, 0);
    CHECK64(_ZN3JSC14ProtoCallFrame12setThisValueENS_7JSValueE, 24);
    CHECK64(_ZN3JSC14ProtoCallFrame15setGlobalObjectEPNS_14JSGlobalObjectE, 48);
    CHECK32(_ZN3JSC14ProtoCallFrame17setPaddedArgCountEj, 32);
    CHECK32(_ZN3JSC14ProtoCallFrame29setArgumentCountIncludingThisEi, 16);
    CHECK64(_ZN3JSC14ProtoCallFrame9setCalleeEPNS_8JSObjectE, 8);
    CHECK64(_ZN7WebCore13GraphicsLayer9setClientERNS_19GraphicsLayerClientE, 16);
    CHECK64(_ZN7WebCore18TextureMapperLayer15setBackingStoreEPNS_25TextureMapperBackingStoreE, 48);
    CHECK64(_ZN7WebCore18TextureMapperLayer16setContentsLayerEPNS_26TextureMapperPlatformLayerE, 56);
    CHECK64(_ZN7WebCore23ApplicationCacheStorage14setMaximumSizeEl, 32);
    CHECK64(_ZN7WebCore23ApplicationCacheStorage21setDefaultOriginQuotaEl, 48);
    CHECK64(_ZN7WebCore29SQLiteStatementAutoResetScopeC1EPNS_15SQLiteStatementE, 0);
    CHECK64(_ZN7WebCore29SQLiteStatementAutoResetScopeC2EPNS_15SQLiteStatementE, 0);
    CHECK64(_ZN9Inspector21CSSFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector21CSSFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector21DOMFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector21DOMFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector21InspectorConsoleAgent12setHeapAgentEPNS_18InspectorHeapAgentE, 48);
    CHECK64(_ZN9Inspector22HeapFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector22HeapFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector22PageFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector22PageFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector24CanvasFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector24CanvasFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK32(_ZN9Inspector24RemoteControllableTarget19setTargetIdentifierEj, 8);
    CHECK64(_ZN9Inspector24TargetFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector24TargetFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector24WorkerFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector24WorkerFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector25BrowserFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector25BrowserFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector25ConsoleFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector25ConsoleFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector25NetworkFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector25NetworkFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector25RuntimeFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector25RuntimeFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector26DatabaseFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector26DatabaseFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector26DebuggerFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector26DebuggerFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector26TimelineFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector26TimelineFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector27AnimationFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector27AnimationFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector27InspectorFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector27InspectorFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector27LayerTreeFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector27LayerTreeFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector28DOMStorageFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector28DOMStorageFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector32ScriptProfilerFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector32ScriptProfilerFrontendDispatcherC2ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector34ApplicationCacheFrontendDispatcherC1ERNS_14FrontendRouterE, 0);
    CHECK64(_ZN9Inspector34ApplicationCacheFrontendDispatcherC2ERNS_14FrontendRouterE, 0);

    bool threw = false;
    try {
        _ZN3JSC14ProtoCallFrame12setCodeBlockEPNS_9CodeBlockE(nullptr, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    return 0;
}
