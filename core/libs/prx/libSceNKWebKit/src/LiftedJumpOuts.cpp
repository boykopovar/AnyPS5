#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "LiftedAbi.hpp"

using NKWebKit::Lifted::Read;
using NKWebKit::Lifted::Write;
using NKWebKit::Lifted::ReadBytes;
using NKWebKit::Lifted::WriteBytes;
using NKWebKit::Lifted::Bytes;
using NKWebKit::Lifted::Call;

extern "C" {

std::uint32_t APS5_VABI WKAXObjectPage(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x10);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x8);
    return static_cast<std::uint32_t>(((v3 == 0) ? v3 : (v3 - 0x18)));
}

std::uint32_t APS5_VABI WKBundleFrameGetJavaScriptContext(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xc8);
    const std::uint64_t v4 = Call<std::uint64_t>(_ZN7WebCore21mainThreadNormalWorldEv, a0, a1, a2, a3, a4, a5);
    const std::uint64_t v5 = Call<std::uint64_t>(_ZN7WebCore16ScriptController13jsWindowProxyERNS_15DOMWrapperWorldE, v3, v4, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x10);
    return static_cast<std::uint32_t>(v6);
}

std::uint32_t APS5_VABI WKBundleFrameGetPendingUnloadCount(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xc0);
    const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x1b8);
    return static_cast<std::uint32_t>(Call<std::uint64_t>(_ZNK7WebCore9DOMWindow27pendingUnloadEventListenersEv, v4, a1, a2, a3, a4, a5));
}

std::uint64_t APS5_VABI WKBundleFrameStopLoading(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return v1;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return v2;
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xa0);
    return Call<std::uint64_t>(_ZN7WebCore11FrameLoader17stopForUserCancelEb, v3, std::uint64_t{0x0}, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKBundleGarbageCollectJavaScriptObjects(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN7WebCore12GCController9singletonEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore12GCController17garbageCollectNowEv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN7WebCore12GCController9singletonEv, a0, a1, a2, a3, a4, a5);
    return Call<std::uint64_t>(_ZN7WebCore12GCController43garbageCollectOnAlternateThreadForDebuggingEb, v2, static_cast<std::uint64_t>(static_cast<std::uint32_t>(a1)), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI WKBundlePageClearMainFrameName(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x30);
    if (v1 == 0) {
        return v1;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x68);
    if (v2 == 0) {
        return v1;
    }
    return Call<std::uint64_t>(_ZN7WebCore9FrameTree9clearNameEv, (v2 + std::uint64_t{0x50}), a1, a2, a3, a4, a5);
}

std::uint32_t APS5_VABI WKBundlePageIsTrackingRepaints(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x30);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x68);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xb8);
    if (v3 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v4 = Read<std::uint8_t>(v3 + 0x415);
    return static_cast<std::uint32_t>(((v3 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v4 != std::uint8_t{0x0}))));
}

void APS5_VABI WKBundlePageListenForLayoutMilestones(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x30);
    if (v2 == 0) {
        return;
    }
    Call<std::uint64_t>(_ZN7WebCore4Page19addLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE, v2, static_cast<std::uint64_t>((static_cast<std::uint32_t>(a1) & std::uint32_t{0x1bf})), a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKBundlePageOverlayClear(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    return Call<std::uint64_t>(_ZN7WebCore11PageOverlay5clearEv, v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKBundlePageResetTrackedRepaints(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x30);
    if (v1 == 0) {
        return v1;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x68);
    if (v2 == 0) {
        return v2;
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xb8);
    if (v3 == 0) {
        return v2;
    }
    return Call<std::uint64_t>(_ZN7WebCore9FrameView20resetTrackedRepaintsEv, v3, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKBundlePageSetAppCacheMaximumSize(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x30);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x2e0);
    return Call<std::uint64_t>(_ZN7WebCore23ApplicationCacheStorage14setMaximumSizeEl, v2, a1, a2, a3, a4, a5);
}

void APS5_VABI WKBundlePageSetPolicyClient(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = a0 + std::uint64_t{0x100};
    if (a1 == 0) {
        WriteBytes<32>(v1 + 0x10, Bytes<32>{});
        WriteBytes<32>(v1, Bytes<32>{});
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(a1);
    if (v2 == std::uint32_t{0x0}) {
        const Bytes<32> v3 = ReadBytes<32>(a1);
        const Bytes<32> v4 = ReadBytes<32>(a1 + 0x10);
        WriteBytes<32>(v1 + 0x10, v4);
        WriteBytes<32>(v1, v3);
        return;
    }
    WriteBytes<32>(v1 + 0x10, Bytes<32>{});
    WriteBytes<32>(v1, Bytes<32>{});
    const std::uint32_t v5 = Read<std::uint32_t>(a1);
    if (static_cast<std::int32_t>(v5 - std::uint32_t{0x0}) < 0) {
        const Bytes<32> v6 = ReadBytes<32>(a1);
        const Bytes<32> v7 = ReadBytes<32>(a1 + 0x10);
        WriteBytes<32>(v1 + 0x10, v7);
        WriteBytes<32>(v1, v6);
        return;
    }
}

std::uint64_t APS5_VABI WKBundlePageSetTracksRepaints(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x30);
    if (v1 == 0) {
        return v1;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x68);
    if (v2 == 0) {
        return v2;
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xb8);
    if (v3 == 0) {
        return v2;
    }
    return Call<std::uint64_t>(_ZN7WebCore9FrameView17setTracksRepaintsEb, v3, static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1))), a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKBundleSetTabKeyCyclesThroughElements(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    (void)a0;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x30);
    Write<std::uint8_t>(v1 + 0xda, static_cast<std::uint8_t>(a2));
    return v1;
}

void APS5_VABI WKContextSetConnectionClient(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = a0 + std::uint64_t{0xb0};
    if (a1 == 0) {
        WriteBytes<16>(v1, Bytes<16>{});
        Write<std::uint64_t>(v1 + 0x10, std::uint64_t{0x0});
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(a1);
    if (v2 == std::uint32_t{0x0}) {
        const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x10);
        Write<std::uint64_t>(v1 + 0x10, v3);
        const Bytes<16> v4 = ReadBytes<16>(a1);
        WriteBytes<16>(v1, v4);
        return;
    }
    WriteBytes<16>(v1, Bytes<16>{});
    Write<std::uint64_t>(v1 + 0x10, std::uint64_t{0x0});
    const std::uint32_t v5 = Read<std::uint32_t>(a1);
    if (static_cast<std::int32_t>(v5 - std::uint32_t{0x0}) < 0) {
        const std::uint64_t v6 = Read<std::uint64_t>(a1 + 0x10);
        Write<std::uint64_t>(v1 + 0x10, v6);
        const Bytes<16> v7 = ReadBytes<16>(a1);
        WriteBytes<16>(v1, v7);
        return;
    }
}

std::uint32_t APS5_VABI WKPageGetContext(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xd8);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x120);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x8);
    return static_cast<std::uint32_t>(((v3 == 0) ? v3 : (v3 - 0x10)));
}

void APS5_VABI WKViewSetViewAccessibilityClient(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = a0 + std::uint64_t{0x40};
    if (a1 == 0) {
        WriteBytes<32>(v1 + 0x20, Bytes<32>{});
        WriteBytes<32>(v1, Bytes<32>{});
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(a1);
    if (v2 == std::uint32_t{0x0}) {
        const Bytes<32> v3 = ReadBytes<32>(a1);
        const Bytes<32> v4 = ReadBytes<32>(a1 + 0x20);
        WriteBytes<32>(v1 + 0x20, v4);
        WriteBytes<32>(v1, v3);
        return;
    }
    WriteBytes<32>(v1 + 0x20, Bytes<32>{});
    WriteBytes<32>(v1, Bytes<32>{});
    const std::uint32_t v5 = Read<std::uint32_t>(a1);
    if (static_cast<std::int32_t>(v5 - std::uint32_t{0x0}) < 0) {
        const Bytes<32> v6 = ReadBytes<32>(a1);
        const Bytes<32> v7 = ReadBytes<32>(a1 + 0x20);
        WriteBytes<32>(v1 + 0x20, v7);
        WriteBytes<32>(v1, v6);
        return;
    }
}

std::uint8_t APS5_VABI WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x49);
    return v1;
}

std::uint64_t APS5_VABI _ZN3JSC21MarkedJSValueRefArrayD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x58);
        if (v2 == 0) {
            return v1;
        }
        return Call<std::uint64_t>(_ZN8Gigacage4freeENS_4KindEPv, std::uint64_t{0x1}, v2, a2, a3, a4, a5);
    }
    const std::uint64_t v3 = Read<std::uint64_t>(a0);
    Write<std::uint64_t>(v1, v3);
    Write<std::uint64_t>(v3 + 0x8, v1);
    WriteBytes<16>(a0, Bytes<16>{});
    const std::uint64_t v4 = Read<std::uint64_t>(a0 + 0x58);
    if (v4 == 0) {
        return v1;
    }
    return Call<std::uint64_t>(_ZN8Gigacage4freeENS_4KindEPv, std::uint64_t{0x1}, v4, a2, v3, a4, a5);
}

std::uint64_t APS5_VABI _ZN3WTF24AutomaticThreadConditionD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return a0;
    }
    Write<std::uint64_t>(a0 + 0x8, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x10, std::uint32_t{0x0});
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN3WTF24AutomaticThreadConditionD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return a0;
    }
    Write<std::uint64_t>(a0 + 0x8, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x10, std::uint32_t{0x0});
    return Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore13GraphicsLayer18setBackgroundColorERKNS_5ColorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Call<std::uint64_t>(_ZN7WebCore5ColoraSERKS0_, (a0 + std::uint64_t{0x78}), a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayer13setSolidColorERKNS_5ColorE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    return Call<std::uint64_t>(_ZN7WebCore5ColoraSERKS0_, (a0 + std::uint64_t{0x268}), a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore4Page31setRemoteInspectionNameOverrideERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x2a0);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    if (v2 == 0) {
        const std::uint64_t v3 = Read<std::uint64_t>(v1 + 0x18);
        Write<std::uint64_t>(v1 + 0x18, v2);
        if (v3 == 0) {
            return Call<std::uint64_t>(_ZN9Inspector24RemoteControllableTarget6updateEv, v1, a1, a2, a3, a4, a5);
        }
        const std::uint32_t v4 = Read<std::uint32_t>(v3);
        const std::uint32_t v5 = v4 + std::uint32_t{0xfffffffe};
        if (v5 == 0) {
            Call<std::uint64_t>(_ZN3WTF10StringImpl7destroyEPS0_, v3, a1, a2, a3, a4, a5);
            return Call<std::uint64_t>(_ZN9Inspector24RemoteControllableTarget6updateEv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        }
        Write<std::uint32_t>(v3, v5);
        return Call<std::uint64_t>(_ZN9Inspector24RemoteControllableTarget6updateEv, v1, a1, a2, a3, a4, a5);
    }
    const std::uint32_t v7 = Read<std::uint32_t>(v2);
    Write<std::uint32_t>(v2, (v7 + std::uint32_t{0x2}));
    const std::uint64_t v9 = Read<std::uint64_t>(v1 + 0x18);
    Write<std::uint64_t>(v1 + 0x18, v2);
    if (v9 == 0) {
        return Call<std::uint64_t>(_ZN9Inspector24RemoteControllableTarget6updateEv, v1, a1, a2, a3, a4, a5);
    }
    const std::uint32_t v10 = Read<std::uint32_t>(v9);
    const std::uint32_t v11 = v10 + std::uint32_t{0xfffffffe};
    if (v11 == 0) {
        Call<std::uint64_t>(_ZN3WTF10StringImpl7destroyEPS0_, v9, a1, a2, a3, a4, a5);
        return Call<std::uint64_t>(_ZN9Inspector24RemoteControllableTarget6updateEv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    }
    Write<std::uint32_t>(v9, v11);
    return Call<std::uint64_t>(_ZN9Inspector24RemoteControllableTarget6updateEv, v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore8Document56hasRequestedPageSpecificStorageAccessWithUserInteractionERKNS_17RegistrableDomainE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x960);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    return Call<std::uint64_t>(_ZN3WTF5equalEPKNS_10StringImplES2_, v1, v2, a2, a3, a4, a5);
}

std::uint32_t APS5_VABI _ZN7WebCore8Settings23setUsesBackForwardCacheEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x1ec);
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0x1e8);
    const std::uint64_t v5 = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(v1)) << 32) | static_cast<std::uint64_t>(v3);
    if (static_cast<std::uint8_t>(static_cast<std::uint8_t>((static_cast<std::uint8_t>(static_cast<std::uint8_t>(v5) & std::uint8_t{0x20})) == 0) ^ static_cast<std::uint8_t>(a1)) == 0) {
        const std::uint64_t v10 = Read<std::uint64_t>(a0 + 0x1e0);
        const std::uint64_t v11 = Read<std::uint64_t>(a0 + 0x1d8);
        const std::uint64_t v12 = Read<std::uint64_t>(a0 + 0x1c8);
        const std::uint64_t v13 = Read<std::uint64_t>(a0 + 0x1d0);
        const std::uint64_t v15 = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1))) << 5;
        Write<std::uint8_t>(a0 + 0x1ec, static_cast<std::uint8_t>((v5 >> 32)));
        const std::uint64_t v17 = (v5 & std::uint64_t{0xffffffffffffffdf}) | v15;
        Write<std::uint64_t>(a0 + 0x1c8, v12);
        Write<std::uint64_t>(a0 + 0x1d0, v13);
        Write<std::uint64_t>(a0 + 0x1d8, v11);
        Write<std::uint64_t>(a0 + 0x1e0, v10);
        Write<std::uint32_t>(a0 + 0x1e8, static_cast<std::uint32_t>(v17));
        const std::uint64_t v18 = Read<std::uint64_t>(a0 + 0x8);
        if (v18 == 0) {
            return static_cast<std::uint32_t>(v18);
        }
        const std::uint64_t v19 = Read<std::uint64_t>(v18 + 0x50);
        const std::uint32_t v20 = Read<std::uint32_t>(v19 + 0x1e8);
        if ((static_cast<std::uint8_t>(static_cast<std::uint8_t>(v20) & std::uint8_t{0x20})) != 0) {
            return v20;
        }
        const std::uint64_t v21 = Call<std::uint64_t>(_ZN7WebCore16BackForwardCache9singletonEv, a0, v15, v13, v17, v10, v11);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(_ZN7WebCore16BackForwardCache14pruneToSizeNowEjNS_13PruningReasonE, v21, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}));
    }
    return static_cast<std::uint32_t>(v5);
}

std::uint64_t APS5_VABI _ZN7WebCore8Settings24setVisualViewportEnabledEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x1ec);
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0x1e8);
    const std::uint64_t v5 = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(v1)) << 32) | static_cast<std::uint64_t>(v3);
    if (static_cast<std::uint8_t>(static_cast<std::uint8_t>((static_cast<std::uint32_t>(v5) & std::uint32_t{0x400}) == 0) ^ static_cast<std::uint8_t>(a1)) == 0) {
        const std::uint64_t v10 = Read<std::uint64_t>(a0 + 0x1e0);
        const std::uint64_t v11 = Read<std::uint64_t>(a0 + 0x1d8);
        const std::uint64_t v12 = Read<std::uint64_t>(a0 + 0x1c8);
        const std::uint64_t v13 = Read<std::uint64_t>(a0 + 0x1d0);
        const std::uint64_t v14 = v5 >> 32;
        const std::uint64_t v15 = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1))) << 10;
        Write<std::uint8_t>(a0 + 0x1ec, static_cast<std::uint8_t>(v14));
        const std::uint64_t v17 = (v5 & std::uint64_t{0xfffffffffffffbff}) | v15;
        Write<std::uint64_t>(a0 + 0x1c8, v12);
        Write<std::uint64_t>(a0 + 0x1d0, v13);
        Write<std::uint64_t>(a0 + 0x1d8, v11);
        Write<std::uint64_t>(a0 + 0x1e0, v10);
        Write<std::uint32_t>(a0 + 0x1e8, static_cast<std::uint32_t>(v17));
        const std::uint64_t v18 = Read<std::uint64_t>(a0 + 0x8);
        if (v18 == 0) {
            return v14;
        }
        return Call<std::uint64_t>(_ZN7WebCore4Page30setNeedsRecalcStyleInAllFramesEv, v18, v15, v13, v17, v10, v11);
    }
    return v5;
}

std::uint64_t APS5_VABI _ZN7WebCore8Settings27setLoadsImagesAutomaticallyEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x1d8);
    if (static_cast<std::uint8_t>(static_cast<std::uint8_t>((static_cast<std::uint32_t>(v1) & std::uint32_t{0x200000}) == 0) ^ static_cast<std::uint8_t>(a1)) == 0) {
        const std::uint8_t v5 = Read<std::uint8_t>(a0 + 0x1ec);
        const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x1e8);
        const std::uint64_t v9 = Read<std::uint64_t>(a0 + 0x1e0);
        const std::uint64_t v10 = Read<std::uint64_t>(a0 + 0x1c8);
        const std::uint64_t v11 = Read<std::uint64_t>(a0 + 0x1d0);
        const std::uint64_t v13 = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1))) << 21;
        Write<std::uint64_t>(a0 + 0x1c8, v10);
        Write<std::uint64_t>(a0 + 0x1d0, v11);
        Write<std::uint64_t>(a0 + 0x1e0, v9);
        Write<std::uint64_t>(a0 + 0x1d8, ((v1 & std::uint64_t{0xffffffffffdfffff}) | v13));
        const std::uint64_t v16 = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(v5)) << 32) | static_cast<std::uint64_t>(v7);
        Write<std::uint32_t>(a0 + 0x1e8, static_cast<std::uint32_t>(v16));
        const std::uint64_t v17 = v16 >> 32;
        Write<std::uint8_t>(a0 + 0x1ec, static_cast<std::uint8_t>(v17));
        return Call<std::uint64_t>(_ZN7WebCore9TimerBase5startEN3WTF7SecondsES2_, (a0 + std::uint64_t{0x18}), v13, v17, static_cast<std::uint64_t>(v7), v9, v11);
    }
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCore8Settings32setAcceleratedCompositingEnabledEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x1c8);
    if (static_cast<std::uint8_t>(static_cast<std::uint8_t>((static_cast<std::uint8_t>(static_cast<std::uint8_t>(v1) & std::uint8_t{0x8})) == 0) ^ static_cast<std::uint8_t>(a1)) == 0) {
        const std::uint8_t v5 = Read<std::uint8_t>(a0 + 0x1ec);
        const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x1e8);
        const std::uint64_t v8 = Read<std::uint64_t>(a0 + 0x1e0);
        const std::uint64_t v9 = Read<std::uint64_t>(a0 + 0x1d8);
        const std::uint64_t v10 = Read<std::uint64_t>(a0 + 0x1d0);
        const std::uint32_t v11 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1));
        const std::uint64_t v13 = (v1 & std::uint64_t{0xfffffffffffffff7}) + static_cast<std::uint64_t>(v11) * 8;
        Write<std::uint64_t>(a0 + 0x1d0, v10);
        Write<std::uint64_t>(a0 + 0x1d8, v9);
        Write<std::uint64_t>(a0 + 0x1e0, v8);
        Write<std::uint64_t>(a0 + 0x1c8, v13);
        const std::uint64_t v15 = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(v5)) << 32) | static_cast<std::uint64_t>(v7);
        Write<std::uint32_t>(a0 + 0x1e8, static_cast<std::uint32_t>(v15));
        const std::uint64_t v16 = v15 >> 32;
        Write<std::uint8_t>(a0 + 0x1ec, static_cast<std::uint8_t>(v16));
        const std::uint64_t v17 = Read<std::uint64_t>(a0 + 0x8);
        if (v17 == 0) {
            return v13;
        }
        return Call<std::uint64_t>(_ZN7WebCore4Page30setNeedsRecalcStyleInAllFramesEv, v17, static_cast<std::uint64_t>(v11), v16, static_cast<std::uint64_t>(v7), v8, v9);
    }
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCore8Settings32setShouldAllowUserInstalledFontsEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x1e0);
    if (static_cast<std::uint8_t>(static_cast<std::uint8_t>((static_cast<std::uint32_t>(v1) & std::uint32_t{0x20000}) == 0) ^ static_cast<std::uint8_t>(a1)) == 0) {
        const std::uint8_t v5 = Read<std::uint8_t>(a0 + 0x1ec);
        const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x1e8);
        const std::uint64_t v9 = Read<std::uint64_t>(a0 + 0x1d8);
        const std::uint64_t v10 = Read<std::uint64_t>(a0 + 0x1c8);
        const std::uint64_t v11 = Read<std::uint64_t>(a0 + 0x1d0);
        const std::uint64_t v13 = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1))) << 17;
        const std::uint64_t v14 = (v1 & std::uint64_t{0xfffffffffffdffff}) | v13;
        Write<std::uint64_t>(a0 + 0x1c8, v10);
        Write<std::uint64_t>(a0 + 0x1d0, v11);
        Write<std::uint64_t>(a0 + 0x1d8, v9);
        Write<std::uint64_t>(a0 + 0x1e0, v14);
        const std::uint64_t v16 = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(v5)) << 32) | static_cast<std::uint64_t>(v7);
        Write<std::uint32_t>(a0 + 0x1e8, static_cast<std::uint32_t>(v16));
        const std::uint64_t v17 = v16 >> 32;
        Write<std::uint8_t>(a0 + 0x1ec, static_cast<std::uint8_t>(v17));
        const std::uint64_t v18 = Read<std::uint64_t>(a0 + 0x8);
        if (v18 == 0) {
            return v14;
        }
        return Call<std::uint64_t>(_ZN7WebCore4Page30setNeedsRecalcStyleInAllFramesEv, v18, v13, v17, static_cast<std::uint64_t>(v7), v9, v11);
    }
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCore8Settings33setFontFallbackPrefersPictographsEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x1d0);
    if (static_cast<std::uint8_t>(static_cast<std::uint8_t>((static_cast<std::uint32_t>(v1) & std::uint32_t{0x40000000}) == 0) ^ static_cast<std::uint8_t>(a1)) == 0) {
        const std::uint8_t v5 = Read<std::uint8_t>(a0 + 0x1ec);
        const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x1e8);
        const std::uint64_t v9 = Read<std::uint64_t>(a0 + 0x1e0);
        const std::uint64_t v10 = Read<std::uint64_t>(a0 + 0x1d8);
        const std::uint64_t v11 = Read<std::uint64_t>(a0 + 0x1c8);
        const std::uint64_t v13 = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1))) << 30;
        const std::uint64_t v14 = (v1 & std::uint64_t{0xffffffffbfffffff}) | v13;
        Write<std::uint64_t>(a0 + 0x1c8, v11);
        Write<std::uint64_t>(a0 + 0x1d8, v10);
        Write<std::uint64_t>(a0 + 0x1e0, v9);
        Write<std::uint64_t>(a0 + 0x1d0, v14);
        const std::uint64_t v16 = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(v5)) << 32) | static_cast<std::uint64_t>(v7);
        Write<std::uint32_t>(a0 + 0x1e8, static_cast<std::uint32_t>(v16));
        const std::uint64_t v17 = v16 >> 32;
        Write<std::uint8_t>(a0 + 0x1ec, static_cast<std::uint8_t>(v17));
        const std::uint64_t v18 = Read<std::uint64_t>(a0 + 0x8);
        if (v18 == 0) {
            return v14;
        }
        return Call<std::uint64_t>(_ZN7WebCore4Page30setNeedsRecalcStyleInAllFramesEv, v18, v13, v17, static_cast<std::uint64_t>(v7), v9, v10);
    }
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCore8Settings41setAcceleratedCompositedAnimationsEnabledEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x1c8);
    if (static_cast<std::uint8_t>(static_cast<std::uint8_t>((static_cast<std::uint8_t>(static_cast<std::uint8_t>(v1) & std::uint8_t{0x4})) == 0) ^ static_cast<std::uint8_t>(a1)) == 0) {
        const std::uint8_t v5 = Read<std::uint8_t>(a0 + 0x1ec);
        const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x1e8);
        const std::uint64_t v8 = Read<std::uint64_t>(a0 + 0x1e0);
        const std::uint64_t v9 = Read<std::uint64_t>(a0 + 0x1d8);
        const std::uint64_t v10 = Read<std::uint64_t>(a0 + 0x1d0);
        const std::uint32_t v11 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1));
        const std::uint64_t v13 = (v1 & std::uint64_t{0xfffffffffffffffb}) + static_cast<std::uint64_t>(v11) * 4;
        Write<std::uint64_t>(a0 + 0x1d0, v10);
        Write<std::uint64_t>(a0 + 0x1d8, v9);
        Write<std::uint64_t>(a0 + 0x1e0, v8);
        Write<std::uint64_t>(a0 + 0x1c8, v13);
        const std::uint64_t v15 = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(v5)) << 32) | static_cast<std::uint64_t>(v7);
        Write<std::uint32_t>(a0 + 0x1e8, static_cast<std::uint32_t>(v15));
        const std::uint64_t v16 = v15 >> 32;
        Write<std::uint8_t>(a0 + 0x1ec, static_cast<std::uint8_t>(v16));
        const std::uint64_t v17 = Read<std::uint64_t>(a0 + 0x8);
        if (v17 == 0) {
            return v13;
        }
        return Call<std::uint64_t>(_ZN7WebCore4Page30setNeedsRecalcStyleInAllFramesEv, v17, static_cast<std::uint64_t>(v11), v16, static_cast<std::uint64_t>(v7), v8, v9);
    }
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    return Call<std::uint64_t>(_ZN3WTF10TextStreamlsEPKv, a0, v1, a2, a3, a4, a5);
}

std::uint32_t APS5_VABI _ZNK7WebCore13HitTestResult14innerNodeFrameEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    if (v1 != 0) {
        const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x28);
        const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x8);
        const std::uint64_t v5 = Read<std::uint64_t>((std::uint64_t{0x180} + v3) + 0x8);
        if (v5 == 0) {
            return std::uint32_t{0x0};
        }
        const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x8);
        return static_cast<std::uint32_t>(v6);
    }
    const std::uint64_t v7 = Read<std::uint64_t>(a0 + 0x48);
    if (v7 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v8 = Read<std::uint64_t>(v7 + 0x28);
    const std::uint64_t v9 = Read<std::uint64_t>(v8 + 0x8);
    const std::uint64_t v11 = Read<std::uint64_t>((std::uint64_t{0x180} + v9) + 0x8);
    if (v11 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v12 = Read<std::uint64_t>(v11 + 0x8);
    return static_cast<std::uint32_t>(v12);
}

std::uint32_t APS5_VABI _ZNK7WebCore18DOMWindowExtension5frameEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x78);
    if (v3 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v5 = Read<std::uint64_t>((v3 + std::uint64_t{0xd0}) + 0x8);
    if (v5 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x8);
    return static_cast<std::uint32_t>(v6);
}

std::uint32_t APS5_VABI _ZNK7WebCore20ScrollingCoordinator36coordinatesScrollingForOverflowLayerERKNS_11RenderLayerE(std::uint64_t a0, std::uint64_t a1) {
    (void)a0;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0xf0);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x35);
    return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v2 != std::uint8_t{0x0}))));
}

std::uint64_t APS5_VABI _ZNK7WebCore4Node7baseURIEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x208);
    if (v3 == std::uint64_t{0x0}) {
        return Call<std::uint64_t>(_ZN3WTF13aboutBlankURLEv, a0, a1, a2, a3, a4, a5);
    }
    return (v2 + std::uint64_t{0x208});
}

std::uint32_t APS5_VABI _ZNK7WebCore8Document4pageEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x188);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x40);
    if (v3 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x8);
    return static_cast<std::uint32_t>(v4);
}

std::uint32_t APS5_VABI _ZNK7WebCore9DOMWindow5frameEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x78);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>((v1 + std::uint64_t{0xd0}) + 0x8);
    if (v3 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x8);
    return static_cast<std::uint32_t>(v4);
}

}
