#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "LiftedAbi.hpp"

using NKWebKit::Lifted::Read;
using NKWebKit::Lifted::Write;
using NKWebKit::Lifted::ReadBytes;
using NKWebKit::Lifted::WriteBytes;
using NKWebKit::Lifted::Bytes;
using NKWebKit::Lifted::Call;
using NKWebKit::Lifted::CallFp;

extern "C" {

std::uint64_t APS5_VABI WKBundlePageOverlaySetAccessibilityClient(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x20);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x50);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKPageRunBeforeUnloadConfirmPanelResultListenerCall(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x10);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKPageRunJavaScriptAlertResultListenerCall(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x10);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKPageRunJavaScriptConfirmResultListenerCall(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x10);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKUserContentExtensionStoreRemove(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    (void)a0;
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(a3), std::uint64_t{0x0}, std::uint64_t{0x4}, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKWebsiteDataStoreClearAppBoundSession(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0;
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(a2), a1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKWebsiteDataStoreHasAppBoundSession(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0;
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(a2), std::uint64_t{0x0}, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a0;
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(a2), a1, a1, a2, a3, a4, a5);
}

std::uint32_t APS5_VABI _ZN3JSC8Bindings13RuntimeObjectD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x8);
    const std::uint64_t v3 = v1 + std::uint64_t{0x8};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x8);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x60);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff8}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN3JSC8Bindings13RuntimeObjectD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x8);
    const std::uint64_t v3 = v1 + std::uint64_t{0x8};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x8);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x60);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff8}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN3JSC8Debugger23setBreakpointsActivatedEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint16_t v1 = Read<std::uint16_t>(a0 + 0x28);
    const std::uint32_t v2 = static_cast<std::uint32_t>(v1);
    if (static_cast<std::uint8_t>(static_cast<std::uint8_t>((static_cast<std::uint8_t>(static_cast<std::uint8_t>(v2) & std::uint8_t{0x20})) == 0) ^ static_cast<std::uint8_t>(a1)) == 0) {
        const std::uint32_t v8 = static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1)) << 5;
        Write<std::uint16_t>(a0 + 0x28, static_cast<std::uint16_t>(((v2 & std::uint32_t{0xffffffdf}) | v8)));
        const std::uint64_t v10 = Read<std::uint64_t>(a0);
        const std::uint64_t v11 = Read<std::uint64_t>(v10 + 0x28);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v11), a0, a1, a2, static_cast<std::uint64_t>(v8), a4, a5));
    }
    return v2;
}

void APS5_VABI _ZN3WTF21MemoryPressureHandler26endSimulatedMemoryPressureEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    (void)a1;
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x6);
    if (v1 == std::uint8_t{0x0}) {
        return;
    }
    Write<std::uint8_t>(a0 + 0x6, std::uint8_t{0x0});
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x20);
    if (v2 == std::uint64_t{0x0}) {
        return;
    }
    const std::uint8_t v3 = Read<std::uint8_t>(a0 + 0x4);
    const std::uint64_t v4 = Read<std::uint64_t>(a0 + 0x20);
    const std::uint64_t v5 = Read<std::uint64_t>(v4);
    const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x10);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), v4, static_cast<std::uint64_t>(static_cast<std::uint32_t>(v3)), a2, v7, a4, a5);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandler21setCompletionCallbackEONS_8FunctionIFvvEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = a0 + std::uint64_t{0x70};
    if (v1 == a1) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    const std::uint64_t v3 = Read<std::uint64_t>(v1);
    Write<std::uint64_t>(v1, v2);
    if (v3 == 0) {
        return;
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x8);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore10JSDocumentC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore10JSDocumentC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore10JSDocumentD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore10JSDocumentD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint8_t APS5_VABI _ZN7WebCore10ScrollView17setUseFixedLayoutEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x120);
    const std::uint8_t v2 = static_cast<std::uint8_t>(v1 != std::uint8_t{0x0});
    if (v2 == static_cast<std::uint8_t>(a1)) {
        return v2;
    }
    Write<std::uint8_t>(a0 + 0x120, static_cast<std::uint8_t>(a1));
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0xd8);
    if (static_cast<std::int32_t>(v3) <= static_cast<std::int32_t>(std::uint32_t{0x0})) {
        return v2;
    }
    const std::uint32_t v4 = Read<std::uint32_t>(a0 + 0xdc);
    if (static_cast<std::int32_t>(v4) <= static_cast<std::int32_t>(std::uint32_t{0x0})) {
        return v2;
    }
    const std::uint64_t v5 = Read<std::uint64_t>(a0);
    const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x218);
    return static_cast<std::uint8_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v6), a0, std::uint64_t{0x1}, a2, a3, a4, a5));
}

std::uint64_t APS5_VABI _ZN7WebCore10ScrollView18setFixedLayoutSizeERKNS_7IntSizeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xd8);
    const std::uint32_t v2 = Read<std::uint32_t>(a1);
    if (v2 != static_cast<std::uint32_t>(v1)) {
        const std::uint64_t v3 = Read<std::uint64_t>(a1);
        Write<std::uint64_t>(a0 + 0xd8, v3);
        const std::uint8_t v4 = Read<std::uint8_t>(a0 + 0x120);
        if (v4 == std::uint8_t{0x0}) {
            return v3;
        }
        const std::uint64_t v5 = Read<std::uint64_t>(a0);
        const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x218);
        return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v6), a0, std::uint64_t{0x1}, a2, a3, a4, a5);
    }
    const std::uint64_t v7 = v1 >> 32;
    const std::uint32_t v8 = Read<std::uint32_t>(a1 + 0x4);
    if (v8 == static_cast<std::uint32_t>(v7)) {
        return v7;
    }
    const std::uint64_t v9 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0xd8, v9);
    const std::uint8_t v10 = Read<std::uint8_t>(a0 + 0x120);
    if (v10 == std::uint8_t{0x0}) {
        return v9;
    }
    const std::uint64_t v11 = Read<std::uint64_t>(a0);
    const std::uint64_t v12 = Read<std::uint64_t>(v11 + 0x218);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v12), a0, std::uint64_t{0x1}, a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore10ScrollView21setDelegatesScrollingEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x122);
    if (v1 != static_cast<std::uint8_t>(a1)) {
        Write<std::uint8_t>(a0 + 0x122, static_cast<std::uint8_t>(a1));
        const std::uint64_t v2 = Read<std::uint64_t>(a0);
        const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x228);
        Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), a0, a1, a2, a3, a4, a5);
        return;
    }
}

void APS5_VABI _ZN7WebCore11CachedFrame26setCachedFramePlatformDataESt10unique_ptrINS_23CachedFramePlatformDataESt14default_deleteIS2_EE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = a0 + std::uint64_t{0x48};
    if (v1 == a1) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    const std::uint64_t v3 = Read<std::uint64_t>(v1);
    Write<std::uint64_t>(v1, v2);
    if (v3 == 0) {
        return;
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x8);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer10setPreloadENS_16MediaPlayerEnums7PreloadE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint8_t>(a0 + 0xc0, static_cast<std::uint8_t>(a1));
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x248);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore11MediaPlayer11muteChangedEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0xd2);
    if (v1 != static_cast<std::uint8_t>(a1)) {
        Write<std::uint8_t>(a0 + 0xd2, static_cast<std::uint8_t>(a1));
        const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x18);
        const std::uint64_t v3 = Read<std::uint64_t>(v2);
        const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x28);
        Call<std::uint64_t>(reinterpret_cast<void (*)()>(v4), v2, a1, a2, a3, a4, a5);
        return;
    }
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer13volumeChangedEd(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    Write<double>(a0 + 0xc8, x0);
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x20);
    return CallFp<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5, x0);
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer17setPreservesPitchEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint8_t>(a0 + 0xd3, static_cast<std::uint8_t>(a1));
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x158);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer19prepareForRenderingEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint8_t>(a0 + 0xd5, std::uint8_t{0x1});
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x290);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore11MediaPlayer21endSimulatedHDCPErrorEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    if (v1 == 0) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x300);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore11MediaPlayer21setShouldDisableSleepEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    if (v1 == 0) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v4 = Read<std::uint64_t>(v2 + 0x340);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v4), v1, static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1))), a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore11MediaPlayer22setPrivateBrowsingModeEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint8_t>(a0 + 0xd4, static_cast<std::uint8_t>(a1));
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    if (v1 == 0) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v4 = Read<std::uint64_t>(v2 + 0x2c8);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v4), v1, static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::uint8_t>(a1))), a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore11MediaPlayer23beginSimulatedHDCPErrorEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    if (v1 == 0) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x2f8);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer28setPreferredDynamicRangeModeENS_16DynamicRangeModeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint8_t>(a0 + 0xd8, static_cast<std::uint8_t>(a1));
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x368);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer7setSizeERKNS_7IntSizeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0xb8, v1);
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x50);
    const std::uint64_t v3 = Read<std::uint64_t>(v2);
    const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x210);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v4), v2, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer8setMutedEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint8_t>(a0 + 0xd2, static_cast<std::uint8_t>(a1));
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x180);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore11MediaPlayer9setVolumeEd(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, double x0) {
    Write<double>(a0 + 0xc8, x0);
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x178);
    return CallFp<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5, x0);
}

std::uint32_t APS5_VABI _ZN7WebCore12JSAudioTrackD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x10);
    const std::uint64_t v3 = v1 + std::uint64_t{0x10};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x10);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x10);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff0}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore12JSAudioTrackD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x10);
    const std::uint64_t v3 = v1 + std::uint64_t{0x10};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x10);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x10);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff0}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint64_t APS5_VABI _ZN7WebCore13JSHTMLElementC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore13JSHTMLElementC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore13JSHTMLElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore13JSHTMLElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore14JSWebAnimationC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore14JSWebAnimationC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore14JSWebAnimationD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore14JSWebAnimationD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore14ResourceHandle6cancelEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    Write<std::uint8_t>(v1 + 0x110, std::uint8_t{0x1});
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x120);
    if (v3 == 0) {
        return v2;
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x48);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore14ScrollableArea21scrollbarStyleChangedENS_14ScrollbarStyleEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0xb0);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v2), a0, std::uint64_t{0x0}, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore15GraphicsContext21setCompositeOperationENS_17CompositeOperatorENS_9BlendModeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    Write<std::uint8_t>(a0 + 0xc2, static_cast<std::uint8_t>(a1));
    Write<std::uint8_t>(a0 + 0xc3, static_cast<std::uint8_t>(a2));
    const std::uint64_t v2 = Read<std::uint64_t>(a0);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x58);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), a0, (a0 + 0x8), std::uint64_t{0x1000}, v3, a4, a5);
}

std::uint32_t APS5_VABI _ZN7WebCore15JSFetchResponseD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x18);
    const std::uint64_t v3 = v1 + std::uint64_t{0x18};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x18);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x10);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xffffffffffffffe8}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore15JSFetchResponseD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x18);
    const std::uint64_t v3 = v1 + std::uint64_t{0x18};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x18);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x10);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xffffffffffffffe8}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint64_t APS5_VABI _ZN7WebCore15JSSVGSVGElementC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore15JSSVGSVGElementC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore15JSSVGSVGElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore15JSSVGSVGElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore15XPathExpressionD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x8);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore15XPathExpressionD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x8);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore16HTMLMediaElement30setWebkitClosedCaptionsVisibleEb(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint16_t v1 = Read<std::uint16_t>(a0 + 0x425);
    const std::uint32_t v2 = static_cast<std::uint32_t>(v1);
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0x421);
    Write<std::uint16_t>(a0 + 0x425, static_cast<std::uint16_t>(v2));
    const std::uint64_t v8 = (((static_cast<std::uint64_t>(v2) << 32) | static_cast<std::uint64_t>(v3)) & std::uint64_t{0xfffffffffffbffff}) | (a1 << 18);
    Write<std::uint32_t>(a0 + 0x421, static_cast<std::uint32_t>(v8));
    const std::uint64_t v9 = Read<std::uint64_t>(a0);
    const std::uint64_t v10 = Read<std::uint64_t>(v9 + 0x6c0);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v10), a0, a1, a2, v8, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore16JSXMLHttpRequestD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore16JSXMLHttpRequestD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore17JSHTMLLinkElementC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore17JSHTMLLinkElementC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore17JSHTMLLinkElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore17JSHTMLLinkElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18ImageBufferBackend13drawConsumingERNS_15GraphicsContextERKNS_9FloatRectES5_RKNS_20ImagePaintingOptionsE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x38);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v2), a0, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLImageElementC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLImageElementC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLImageElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLImageElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLInputElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLInputElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLMediaElementC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLMediaElementC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLMediaElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLMediaElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLVideoElementC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLVideoElementC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLVideoElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore18JSHTMLVideoElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint32_t APS5_VABI _ZN7WebCore19JSAnimationTimelineD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x8);
    const std::uint64_t v3 = v1 + std::uint64_t{0x8};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x8);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x8);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff8}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore19JSAnimationTimelineD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x8);
    const std::uint64_t v3 = v1 + std::uint64_t{0x8};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x8);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x8);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff8}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint64_t APS5_VABI _ZN7WebCore19JSHTMLSelectElementC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore19JSHTMLSelectElementC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore19JSHTMLSelectElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore19JSHTMLSelectElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore21JSCSSStyleDeclarationC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x10);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore21JSCSSStyleDeclarationC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x10);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore21JSCSSStyleDeclarationD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x18);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore21JSCSSStyleDeclarationD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x18);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore21JSTextTrackCueGenericD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore21JSTextTrackCueGenericD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal15changeSheetRectERKNS_9FloatRectE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x150);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v2), a0, a1, a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore4Page12whenUnnestedEON3WTF8FunctionIFvvEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = a0 + std::uint64_t{0xc8};
    if (v1 == a1) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    const std::uint64_t v3 = Read<std::uint64_t>(v1);
    Write<std::uint64_t>(v1, v2);
    if (v3 == 0) {
        return;
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x8);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore4Page29applicationDidEnterBackgroundEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xa8);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x10);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, std::uint64_t{0x0}, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore4Page30applicationWillEnterForegroundEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xa8);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x10);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, std::uint64_t{0x1}, a2, a3, a4, a5);
}

std::uint32_t APS5_VABI _ZN7WebCore6JSFileD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x10);
    const std::uint64_t v3 = v1 + std::uint64_t{0x10};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x10);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x8);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff0}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore6JSFileD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x10);
    const std::uint64_t v3 = v1 + std::uint64_t{0x10};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x10);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x8);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff0}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint64_t APS5_VABI _ZN7WebCore6JSNodeC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore6JSNodeC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore6JSNodeD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore6JSNodeD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore6Widget16removeFromParentEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x20);
    if (v1 == 0) {
        return v1;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return v1;
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2);
    const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x120);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v4), v2, a0, a2, a3, a4, a5);
}

std::uint32_t APS5_VABI _ZN7WebCore7JSRangeD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x8);
    const std::uint64_t v3 = v1 + std::uint64_t{0x8};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x8);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x8);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff8}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore7JSRangeD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return static_cast<std::uint32_t>(a0);
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x8);
    const std::uint64_t v3 = v1 + std::uint64_t{0x8};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = Read<std::uint64_t>(v3 - 0x8);
        const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x8);
        return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v7), (v3 + std::uint64_t{0xfffffffffffffff8}), a1, a2, a3, a4, a5));
    }
    Write<std::uint32_t>(v3, v4);
    return v4;
}

std::uint64_t APS5_VABI _ZN7WebCore9JSElementC1ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore9JSElementC2ERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x78);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore9JSElementD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

std::uint64_t APS5_VABI _ZN7WebCore9JSElementD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return a0;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), v1, a1, a2, a3, a4, a5);
}

void APS5_VABI _ZN7WebCore9Scrollbar13setProportionEii(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x58);
    if (v1 != static_cast<std::uint32_t>(a1)) {
        Write<std::uint32_t>(a0 + 0x58, static_cast<std::uint32_t>(a1));
        Write<std::uint32_t>(a0 + 0x5c, static_cast<std::uint32_t>(a2));
        const std::uint64_t v2 = Read<std::uint64_t>(a0);
        const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x120);
        Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), a0, a1, a2, a3, a4, a5);
        return;
    }
    const std::uint32_t v4 = Read<std::uint32_t>(a0 + 0x5c);
    if (v4 != static_cast<std::uint32_t>(a2)) {
        Write<std::uint32_t>(a0 + 0x58, static_cast<std::uint32_t>(a1));
        Write<std::uint32_t>(a0 + 0x5c, static_cast<std::uint32_t>(a2));
        const std::uint64_t v5 = Read<std::uint64_t>(a0);
        const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x120);
        Call<std::uint64_t>(reinterpret_cast<void (*)()>(v6), a0, a1, a2, a3, a4, a5);
        return;
    }
}

void APS5_VABI _ZN9Inspector15InspectorTarget17setResumeCallbackEON3WTF8FunctionIFvvEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = a0 + std::uint64_t{0x8};
    if (v1 == a1) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    const std::uint64_t v3 = Read<std::uint64_t>(v1);
    Write<std::uint64_t>(v1, v2);
    if (v3 == 0) {
        return;
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v3);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x8);
    Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v3, a1, a2, v4, a4, a5);
}

std::uint64_t APS5_VABI _ZN9Inspector21InjectedScriptManager10disconnectEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x20);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v2), a0, a1, a2, a3, a4, a5);
}

std::uint8_t APS5_VABI _ZNK7WebCore15ActiveDOMObject18hasPendingActivityEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    if (v1 == std::uint64_t{0x0}) {
        const std::uint64_t v2 = Read<std::uint64_t>(a0);
        const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x38);
        return static_cast<std::uint8_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), a0, a1, a2, a3, a4, a5));
    }
    return std::uint8_t{0x1};
}

std::uint64_t APS5_VABI _ZNK7WebCore16HTMLMediaElement23webkitHasClosedCaptionsEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x6b0);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v2), a0, a1, a2, a3, a4, a5);
}

std::uint32_t APS5_VABI _ZNK7WebCore6Editor7Command11isSupportedEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint32_t v2 = Read<std::uint32_t>(a0 + 0x8);
    const std::uint32_t v3 = static_cast<std::uint32_t>(static_cast<std::uint64_t>(v2) - 0x1);
    if (v3 >= std::uint32_t{0x2}) {
        if (v2 == 0) {
            return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | std::uint64_t{0x1}));
        }
        return std::uint32_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint64_t v5 = Read<std::uint64_t>(v1 + 0x8);
    return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v5), v4, a1, static_cast<std::uint64_t>(v3), static_cast<std::uint64_t>(v2), a4, a5));
}

std::uint32_t APS5_VABI _ZNK9Inspector18InjectedScriptBase31hasAccessToInspectedScriptStateEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x20);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint64_t v3 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint64_t v4 = Read<std::uint64_t>(v2 + 0x18);
    return static_cast<std::uint32_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v4), v1, v3, a2, v2, a4, a5));
}

std::uint64_t APS5_VABI _ZNK9Inspector21InspectorConsoleAgent22developerExtrasEnabledEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x18);
    const std::uint64_t v3 = Read<std::uint64_t>(v2);
    const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x10);
    return Call<std::uint64_t>(reinterpret_cast<void (*)()>(v4), v2, a1, a2, a3, a4, a5);
}

std::uint8_t APS5_VABI _ZNK9Inspector22RemoteInspectionTarget20remoteControlAllowedEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0xc);
    if (v1 == std::uint8_t{0x0}) {
        const std::uint64_t v2 = Read<std::uint64_t>(a0);
        const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x48);
        return static_cast<std::uint8_t>(Call<std::uint64_t>(reinterpret_cast<void (*)()>(v3), a0, a1, a2, a3, a4, a5));
    }
    return std::uint8_t{0x1};
}

}
