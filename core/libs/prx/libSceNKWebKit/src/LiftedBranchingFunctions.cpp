#include "prx/libSceNKWebKit/include/LiftedBranchingFunctionsExports.hpp"
#include "LiftedAbi.hpp"

using NKWebKit::Lifted::Read;
using NKWebKit::Lifted::Write;
using NKWebKit::Lifted::ReadBytes;
using NKWebKit::Lifted::WriteBytes;
using NKWebKit::Lifted::Bytes;

extern "C" {

std::uint64_t APS5_VABI _ZN3JSC17DebuggerCallFrame11callerFrameEv(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x8);
    if (v1 != std::uint64_t{0x0}) {
        const std::uint64_t v2 = Read<std::uint64_t>(a1 + 0x10);
        Write<std::uint64_t>(a0, v2);
        if (v2 == 0) {
            return a0;
        }
        const std::uint32_t v3 = Read<std::uint32_t>(v2);
        Write<std::uint32_t>(v2, (v3 + 1));
        return a0;
    }
    const std::uint8_t v5 = Read<std::uint8_t>(a1 + 0x54);
    if (v5 != std::uint8_t{0x0}) {
        const std::uint64_t v6 = Read<std::uint64_t>(a1 + 0x10);
        Write<std::uint64_t>(a0, v6);
        if (v6 == 0) {
            return a0;
        }
        const std::uint32_t v7 = Read<std::uint32_t>(v6);
        Write<std::uint32_t>(v6, (v7 + 1));
        return a0;
    }
    Write<std::uint64_t>(a0, std::uint64_t{0x0});
    return a0;
}

std::uint64_t APS5_VABI _ZN3JSC4Heap32setGarbageCollectionTimerEnabledEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x2c0);
    if (v1 == 0) {
        const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x2c8);
        if (v2 == 0) {
            return v2;
        }
        Write<std::uint8_t>(v2 + 0x23, static_cast<std::uint8_t>(a1));
        return v2;
    }
    Write<std::uint8_t>(v1 + 0x23, static_cast<std::uint8_t>(a1));
    const std::uint64_t v3 = Read<std::uint64_t>(a0 + 0x2c8);
    if (v3 == 0) {
        return v3;
    }
    Write<std::uint8_t>(v3 + 0x23, static_cast<std::uint8_t>(a1));
    return v3;
}

std::uint64_t APS5_VABI _ZN3JSC4Yarr17RegularExpressionC1ERKS1_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    if (v1 == 0) {
        return v1;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    Write<std::uint32_t>(v1, (v2 + 1));
    return v1;
}

std::uint64_t APS5_VABI _ZN3JSC4Yarr17RegularExpressionC2ERKS1_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    if (v1 == 0) {
        return v1;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    Write<std::uint32_t>(v1, (v2 + 1));
    return v1;
}

std::uint64_t APS5_VABI _ZN3JSC8Bindings13RuntimeObjectC1ERKS1_(std::uint64_t a0, std::uint64_t a1) {
    const Bytes<16> v1 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v1);
    const std::uint64_t v2 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v2);
    if (v2 == 0) {
        return v2;
    }
    const std::uint32_t v3 = Read<std::uint32_t>(v2 + 0x8);
    Write<std::uint32_t>(v2 + 0x8, (v3 + 1));
    return v2;
}

std::uint64_t APS5_VABI _ZN3JSC8Bindings13RuntimeObjectC2ERKS1_(std::uint64_t a0, std::uint64_t a1) {
    const Bytes<16> v1 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v1);
    const std::uint64_t v2 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v2);
    if (v2 == 0) {
        return v2;
    }
    const std::uint32_t v3 = Read<std::uint32_t>(v2 + 0x8);
    Write<std::uint32_t>(v2 + 0x8, (v3 + 1));
    return v2;
}

std::uint8_t APS5_VABI _ZN3WTF15AutomaticThread7tryStopERKNS_14AbstractLockerE(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x29);
    if (v1 == std::uint8_t{0x0}) {
        return std::uint8_t{0x1};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(a0 + 0x2b);
    if (v2 == std::uint8_t{0x0}) {
        Write<std::uint8_t>(a0 + 0x29, std::uint8_t{0x0});
        return std::uint8_t{0x1};
    }
    return std::uint8_t{0x0};
}

void APS5_VABI _ZN3WTF21MemoryPressureHandler15setProcessStateENS_18WebsamProcessStateE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x9);
    if (v1 == static_cast<std::uint8_t>(a1)) {
        return;
    }
    Write<std::uint8_t>(a0 + 0x9, static_cast<std::uint8_t>(a1));
}

std::uint32_t APS5_VABI _ZN7WebCore10ScrollView21setExposedContentRectERKNS_9FloatRectE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0xb4);
    if (v1 != std::uint8_t{0x0}) {
        const std::uint32_t v2 = Read<std::uint32_t>(a1 + 0x10);
        Write<std::uint32_t>(a0 + 0xb0, v2);
        const Bytes<16> v3 = ReadBytes<16>(a1);
        WriteBytes<16>(a0 + 0xa0, v3);
        return v2;
    }
    Write<std::uint16_t>(a0 + 0x94, std::uint16_t{0x0});
    WriteBytes<16>(a0 + 0x96, Bytes<16>{});
    WriteBytes<16>(a0 + 0xa4, Bytes<16>{});
    Write<std::uint8_t>(a0 + 0xb4, std::uint8_t{0x1});
    const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x10);
    Write<std::uint32_t>(a0 + 0xb0, v4);
    const Bytes<16> v5 = ReadBytes<16>(a1);
    WriteBytes<16>(a0 + 0xa0, v5);
    return v4;
}

void APS5_VABI _ZN7WebCore11FrameLoader21clearTestingOverridesEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x149);
    if (v1 == std::uint8_t{0x0}) {
        const std::uint8_t v2 = Read<std::uint8_t>(a0 + 0x14b);
        if (v2 == std::uint8_t{0x0}) {
            Write<std::uint8_t>(a0 + 0x14c, std::uint8_t{0x0});
            return;
        }
        Write<std::uint8_t>(a0 + 0x14b, std::uint8_t{0x0});
        Write<std::uint8_t>(a0 + 0x14c, std::uint8_t{0x0});
        return;
    }
    Write<std::uint8_t>(a0 + 0x149, std::uint8_t{0x0});
    const std::uint8_t v3 = Read<std::uint8_t>(a0 + 0x14b);
    if (v3 == std::uint8_t{0x0}) {
        Write<std::uint8_t>(a0 + 0x14c, std::uint8_t{0x0});
        return;
    }
    Write<std::uint8_t>(a0 + 0x14b, std::uint8_t{0x0});
    Write<std::uint8_t>(a0 + 0x14c, std::uint8_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore11FrameLoader6openerEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x128);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return v2;
}

std::uint64_t APS5_VABI _ZN7WebCore14PluginDocument12pluginWidgetEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x9f0);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x40);
    const std::uint64_t v3 = std::uint64_t{0xffffffffffff} & v2;
    if (v3 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0xf0);
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore15DatabaseManager16hasOpenDatabasesERNS_8DocumentE(std::uint64_t a0, std::uint64_t a1) {
    (void)a0;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x148);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x28);
    return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v2 != std::uint8_t{0x0}))));
}

void APS5_VABI _ZN7WebCore17HTMLCanvasElement25setUsesDisplayListDrawingEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0xe9);
    if (v1 == std::uint8_t{0x0}) {
        Write<std::uint8_t>(a0 + 0xe9, std::uint8_t{0x1});
        Write<std::uint8_t>(a0 + 0xe8, static_cast<std::uint8_t>(a1));
        return;
    }
    Write<std::uint8_t>(a0 + 0xe8, static_cast<std::uint8_t>(a1));
}

void APS5_VABI _ZN7WebCore21NetworkStorageSession38setCacheMaxAgeCapForPrevalentResourcesEN3WTF7SecondsE(std::uint64_t a0, double x0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x70);
    Write<double>(a0 + 0x68, x0);
    if (v1 == std::uint8_t{0x0}) {
        Write<std::uint8_t>(a0 + 0x70, std::uint8_t{0x1});
        return;
    }
}

void APS5_VABI _ZN7WebCore21NetworkStorageSession40resetCacheMaxAgeCapForPrevalentResourcesEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x70);
    if (v1 == std::uint8_t{0x0}) {
        return;
    }
    Write<std::uint8_t>(a0 + 0x70, std::uint8_t{0x0});
}

std::uint32_t APS5_VABI _ZN7WebCore24CoordinatedGraphicsLayer20didUpdateTileBuffersEv(std::uint64_t a0) {
    const std::uint16_t v1 = Read<std::uint16_t>(a0 + 0xb1);
    const std::uint32_t v2 = static_cast<std::uint32_t>(v1);
    if ((v2 & std::uint32_t{0x2000}) == 0) {
        return v2;
    }
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0xb4);
    const std::uint32_t v4 = v3 + 1;
    Write<std::uint32_t>(a0 + 0xb4, v4);
    Write<std::uint32_t>(a0 + 0x844, v4);
    const std::uint8_t v5 = Read<std::uint8_t>(a0 + 0x842);
    Write<std::uint8_t>(a0 + 0x842, static_cast<std::uint8_t>(v5 | std::uint8_t{0x80}));
    return v4;
}

void APS5_VABI _ZN7WebCore27PlatformMediaSessionManager32setIsPlayingToAutomotiveHeadUnitEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x34);
    if (v1 == static_cast<std::uint8_t>(a1)) {
        return;
    }
    Write<std::uint8_t>(a0 + 0x34, static_cast<std::uint8_t>(a1));
}

void APS5_VABI _ZN7WebCore4Page27setVerticalScrollElasticityENS_16ScrollElasticityE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x155);
    if (v1 == static_cast<std::uint8_t>(a1)) {
        return;
    }
    Write<std::uint8_t>(a0 + 0x155, static_cast<std::uint8_t>(a1));
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x68);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xb8);
    if (v3 == 0) {
        return;
    }
    Write<std::uint8_t>(v3 + 0x69, static_cast<std::uint8_t>(a1));
}

void APS5_VABI _ZN7WebCore4Page29setHorizontalScrollElasticityENS_16ScrollElasticityE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x156);
    if (v1 == static_cast<std::uint8_t>(a1)) {
        return;
    }
    Write<std::uint8_t>(a0 + 0x156, static_cast<std::uint8_t>(a1));
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x68);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xb8);
    if (v3 == 0) {
        return;
    }
    Write<std::uint8_t>(v3 + 0x6a, static_cast<std::uint8_t>(a1));
}

std::uint64_t APS5_VABI _ZN7WebCore4PathC1EOS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    Write<std::uint8_t>(a0 + 0x8, std::uint8_t{0x0});
    Write<std::uint8_t>(a0 + 0x18, std::uint8_t{0x0});
    const std::uint8_t v2 = Read<std::uint8_t>(a1 + 0x18);
    if (v2 == std::uint8_t{0x0}) {
        return v1;
    }
    WriteBytes<16>((a0 + 0x8), Bytes<16>{});
    const std::uint64_t v4 = Read<std::uint64_t>(a1 + 0x8);
    Write<std::uint64_t>(a0 + 0x8, v4);
    Write<std::uint64_t>(a1 + 0x8, std::uint64_t{0x0});
    const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0x10);
    const std::uint32_t v6 = Read<std::uint32_t>(a1 + 0x10);
    Write<std::uint32_t>(a0 + 0x10, v6);
    Write<std::uint32_t>(a1 + 0x10, v5);
    const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x14);
    const std::uint32_t v8 = Read<std::uint32_t>(a1 + 0x14);
    Write<std::uint32_t>(a0 + 0x14, v8);
    Write<std::uint32_t>(a1 + 0x14, v7);
    Write<std::uint8_t>(a0 + 0x18, std::uint8_t{0x1});
    return static_cast<std::uint64_t>(v7);
}

std::uint64_t APS5_VABI _ZN7WebCore4PathC2EOS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    Write<std::uint8_t>(a0 + 0x8, std::uint8_t{0x0});
    Write<std::uint8_t>(a0 + 0x18, std::uint8_t{0x0});
    const std::uint8_t v2 = Read<std::uint8_t>(a1 + 0x18);
    if (v2 == std::uint8_t{0x0}) {
        return v1;
    }
    WriteBytes<16>((a0 + 0x8), Bytes<16>{});
    const std::uint64_t v4 = Read<std::uint64_t>(a1 + 0x8);
    Write<std::uint64_t>(a0 + 0x8, v4);
    Write<std::uint64_t>(a1 + 0x8, std::uint64_t{0x0});
    const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0x10);
    const std::uint32_t v6 = Read<std::uint32_t>(a1 + 0x10);
    Write<std::uint32_t>(a0 + 0x10, v6);
    Write<std::uint32_t>(a1 + 0x10, v5);
    const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x14);
    const std::uint32_t v8 = Read<std::uint32_t>(a1 + 0x14);
    Write<std::uint32_t>(a0 + 0x14, v8);
    Write<std::uint32_t>(a1 + 0x14, v7);
    Write<std::uint8_t>(a0 + 0x18, std::uint8_t{0x1});
    return static_cast<std::uint64_t>(v7);
}

std::uint64_t APS5_VABI _ZN7WebCore4toJSERNS_12VoidCallbackE(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint64_t{0x2};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2);
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore4toJSERNS_14StringCallbackE(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint64_t{0x2};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2);
    return v3;
}

std::uint8_t APS5_VABI _ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    Write<std::uint64_t>(a0, a1);
    if (a1 == 0) {
        Write<std::uint32_t>(a0 + 0x8, static_cast<std::uint32_t>(a2));
        const std::uint8_t v2 = Read<std::uint8_t>(a0 + 0xc);
        const std::uint8_t v4 = static_cast<std::uint8_t>(static_cast<std::uint8_t>(v2 & std::uint8_t{0xf0}) | static_cast<std::uint8_t>(static_cast<std::uint8_t>(a3) & std::uint8_t{0x7}));
        Write<std::uint8_t>(a0 + 0xc, v4);
        return v4;
    }
    const std::uint32_t v5 = Read<std::uint32_t>(a1 + 0x18);
    Write<std::uint32_t>(a1 + 0x18, (v5 + std::uint32_t{0x2}));
    Write<std::uint32_t>(a0 + 0x8, static_cast<std::uint32_t>(a2));
    const std::uint8_t v8 = Read<std::uint8_t>(a0 + 0xc);
    const std::uint8_t v10 = static_cast<std::uint8_t>(static_cast<std::uint8_t>(v8 & std::uint8_t{0xf0}) | static_cast<std::uint8_t>(static_cast<std::uint8_t>(a3) & std::uint8_t{0x7}));
    Write<std::uint8_t>(a0 + 0xc, v10);
    return v10;
}

std::uint8_t APS5_VABI _ZN7WebCore8PositionC2EPNS_4NodeEjNS0_10AnchorTypeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    Write<std::uint64_t>(a0, a1);
    if (a1 == 0) {
        Write<std::uint32_t>(a0 + 0x8, static_cast<std::uint32_t>(a2));
        const std::uint8_t v2 = Read<std::uint8_t>(a0 + 0xc);
        const std::uint8_t v4 = static_cast<std::uint8_t>(static_cast<std::uint8_t>(v2 & std::uint8_t{0xf0}) | static_cast<std::uint8_t>(static_cast<std::uint8_t>(a3) & std::uint8_t{0x7}));
        Write<std::uint8_t>(a0 + 0xc, v4);
        return v4;
    }
    const std::uint32_t v5 = Read<std::uint32_t>(a1 + 0x18);
    Write<std::uint32_t>(a1 + 0x18, (v5 + std::uint32_t{0x2}));
    Write<std::uint32_t>(a0 + 0x8, static_cast<std::uint32_t>(a2));
    const std::uint8_t v8 = Read<std::uint8_t>(a0 + 0xc);
    const std::uint8_t v10 = static_cast<std::uint8_t>(static_cast<std::uint8_t>(v8 & std::uint8_t{0xf0}) | static_cast<std::uint8_t>(static_cast<std::uint8_t>(a3) & std::uint8_t{0x7}));
    Write<std::uint8_t>(a0 + 0xc, v10);
    return v10;
}

std::uint32_t APS5_VABI _ZN7WebCore9CSSParserC1ERKNS_16CSSParserContextE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    if (v1 == 0) {
        const Bytes<32> v2 = ReadBytes<32>(a1 + 0x8);
        WriteBytes<32>(a0 + 0x8, v2);
        const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x28);
        Write<std::uint64_t>(a0 + 0x28, v3);
        if (v3 == 0) {
            const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x4f);
            Write<std::uint32_t>(a0 + 0x4f, v4);
            const Bytes<32> v5 = ReadBytes<32>(a1 + 0x30);
            WriteBytes<32>(a0 + 0x30, v5);
            return v4;
        }
        const std::uint32_t v6 = Read<std::uint32_t>(v3);
        Write<std::uint32_t>(v3, (v6 + std::uint32_t{0x2}));
        const std::uint32_t v8 = Read<std::uint32_t>(a1 + 0x4f);
        Write<std::uint32_t>(a0 + 0x4f, v8);
        const Bytes<32> v9 = ReadBytes<32>(a1 + 0x30);
        WriteBytes<32>(a0 + 0x30, v9);
        return v8;
    }
    const std::uint32_t v10 = Read<std::uint32_t>(v1);
    Write<std::uint32_t>(v1, (v10 + std::uint32_t{0x2}));
    const Bytes<32> v12 = ReadBytes<32>(a1 + 0x8);
    WriteBytes<32>(a0 + 0x8, v12);
    const std::uint64_t v13 = Read<std::uint64_t>(a1 + 0x28);
    Write<std::uint64_t>(a0 + 0x28, v13);
    if (v13 == 0) {
        const std::uint32_t v14 = Read<std::uint32_t>(a1 + 0x4f);
        Write<std::uint32_t>(a0 + 0x4f, v14);
        const Bytes<32> v15 = ReadBytes<32>(a1 + 0x30);
        WriteBytes<32>(a0 + 0x30, v15);
        return v14;
    }
    const std::uint32_t v16 = Read<std::uint32_t>(v13);
    Write<std::uint32_t>(v13, (v16 + std::uint32_t{0x2}));
    const std::uint32_t v18 = Read<std::uint32_t>(a1 + 0x4f);
    Write<std::uint32_t>(a0 + 0x4f, v18);
    const Bytes<32> v19 = ReadBytes<32>(a1 + 0x30);
    WriteBytes<32>(a0 + 0x30, v19);
    return v18;
}

std::uint32_t APS5_VABI _ZN7WebCore9CSSParserC2ERKNS_16CSSParserContextE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    if (v1 == 0) {
        const Bytes<32> v2 = ReadBytes<32>(a1 + 0x8);
        WriteBytes<32>(a0 + 0x8, v2);
        const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x28);
        Write<std::uint64_t>(a0 + 0x28, v3);
        if (v3 == 0) {
            const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x4f);
            Write<std::uint32_t>(a0 + 0x4f, v4);
            const Bytes<32> v5 = ReadBytes<32>(a1 + 0x30);
            WriteBytes<32>(a0 + 0x30, v5);
            return v4;
        }
        const std::uint32_t v6 = Read<std::uint32_t>(v3);
        Write<std::uint32_t>(v3, (v6 + std::uint32_t{0x2}));
        const std::uint32_t v8 = Read<std::uint32_t>(a1 + 0x4f);
        Write<std::uint32_t>(a0 + 0x4f, v8);
        const Bytes<32> v9 = ReadBytes<32>(a1 + 0x30);
        WriteBytes<32>(a0 + 0x30, v9);
        return v8;
    }
    const std::uint32_t v10 = Read<std::uint32_t>(v1);
    Write<std::uint32_t>(v1, (v10 + std::uint32_t{0x2}));
    const Bytes<32> v12 = ReadBytes<32>(a1 + 0x8);
    WriteBytes<32>(a0 + 0x8, v12);
    const std::uint64_t v13 = Read<std::uint64_t>(a1 + 0x28);
    Write<std::uint64_t>(a0 + 0x28, v13);
    if (v13 == 0) {
        const std::uint32_t v14 = Read<std::uint32_t>(a1 + 0x4f);
        Write<std::uint32_t>(a0 + 0x4f, v14);
        const Bytes<32> v15 = ReadBytes<32>(a1 + 0x30);
        WriteBytes<32>(a0 + 0x30, v15);
        return v14;
    }
    const std::uint32_t v16 = Read<std::uint32_t>(v13);
    Write<std::uint32_t>(v13, (v16 + std::uint32_t{0x2}));
    const std::uint32_t v18 = Read<std::uint32_t>(a1 + 0x4f);
    Write<std::uint32_t>(a0 + 0x4f, v18);
    const Bytes<32> v19 = ReadBytes<32>(a1 + 0x30);
    WriteBytes<32>(a0 + 0x30, v19);
    return v18;
}

void APS5_VABI _ZN7WebCore9DOMWindow29setCanShowModalDialogOverrideEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x8c);
    if (v1 == std::uint8_t{0x0}) {
        Write<std::uint8_t>(a0 + 0x8c, std::uint8_t{0x1});
        Write<std::uint8_t>(a0 + 0x8b, static_cast<std::uint8_t>(a1));
        return;
    }
    Write<std::uint8_t>(a0 + 0x8b, static_cast<std::uint8_t>(a1));
}

std::uint32_t APS5_VABI _ZN7WebCore9FontCache9fontCountEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x40);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint32_t v3 = Read<std::uint32_t>(v2 - 0xc);
    return v3;
}

void APS5_VABI _ZN9Inspector21InspectorConsoleAgent29willDestroyFrontendAndBackendENS_16DisconnectReasonE(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x60);
    if (v1 == std::uint8_t{0x0}) {
        return;
    }
    Write<std::uint8_t>(a0 + 0x60, std::uint8_t{0x0});
}

std::uint64_t APS5_VABI _ZNK3JSC2VM29deprecatedVMEntryGlobalObjectEPNS_14JSGlobalObjectE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xc3d8);
    if (v1 == 0) {
        return a1;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return v2;
}

std::uint32_t APS5_VABI _ZNK3JSC7ArgList8getSliceEiRS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    if (static_cast<std::int32_t>(static_cast<std::uint32_t>(a1)) <= 0) {
        Write<std::uint64_t>(a2, std::uint64_t{0x0});
        Write<std::uint32_t>(a2 + 0x8, std::uint32_t{0x0});
        return std::uint32_t{0x0};
    }
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x8);
    if (static_cast<std::int32_t>(v1) <= static_cast<std::int32_t>(static_cast<std::uint32_t>(a1))) {
        Write<std::uint64_t>(a2, std::uint64_t{0x0});
        Write<std::uint32_t>(a2 + 0x8, std::uint32_t{0x0});
        return std::uint32_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(a0);
    Write<std::uint64_t>(a2, ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(a1)) << 3) + v4));
    const std::uint32_t v6 = Read<std::uint32_t>(a0 + 0x8);
    const std::uint32_t v7 = v6 - static_cast<std::uint32_t>(a1);
    Write<std::uint32_t>(a2 + 0x8, v7);
    return v7;
}

std::uint64_t APS5_VABI _ZNK3JSC8Bindings10RootObject12globalObjectEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore10RenderText8textNodeEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x31);
    if ((static_cast<std::uint8_t>(v1 & std::uint8_t{0x2})) != 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x10);
    return v2;
}

std::uint32_t APS5_VABI _ZNK7WebCore10RenderView15usesCompositingEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x1c0);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x52);
    return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v2 != std::uint8_t{0x0}))));
}

std::uint8_t APS5_VABI _ZNK7WebCore10ScrollView14scrollbarModesERNS_13ScrollbarModeES2_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    if (v1 == std::uint64_t{0x0}) {
        const std::uint8_t v2 = Read<std::uint8_t>(a0 + 0x90);
        Write<std::uint8_t>(a1, v2);
        const std::uint8_t v3 = Read<std::uint8_t>(a0 + 0x91);
        Write<std::uint8_t>(a2, v3);
        return v3;
    }
    Write<std::uint8_t>(a1, std::uint8_t{0x0});
    Write<std::uint8_t>(a2, std::uint8_t{0x0});
    return std::uint8_t{0x0};
}

std::uint32_t APS5_VABI _ZNK7WebCore10StorageMap6lengthEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint32_t v3 = Read<std::uint32_t>(v2 - 0xc);
    return v3;
}

std::uint64_t APS5_VABI _ZNK7WebCore11PageOverlay10controllerEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x400);
    return v2;
}

std::uint32_t APS5_VABI _ZNK7WebCore11XPathResult20invalidIteratorStateEv(std::uint64_t a0) {
    const std::uint16_t v1 = Read<std::uint16_t>(a0 + 0x40);
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(v1) & std::uint32_t{0xfffffffe})) != std::uint16_t{0x4}) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(a0 + 0x48);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x368);
    const std::uint64_t v6 = Read<std::uint64_t>(a0 + 0x50);
    return static_cast<std::uint32_t>(((v5 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v5 != v6))));
}

std::uint64_t APS5_VABI _ZNK7WebCore13CSSImportRule4hrefEv(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x18);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x20);
    Write<std::uint64_t>(a0, v2);
    if (v2 == 0) {
        return a0;
    }
    const std::uint32_t v3 = Read<std::uint32_t>(v2);
    Write<std::uint32_t>(v2, (v3 + std::uint32_t{0x2}));
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore13HitTestResult21innerNonSharedElementEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x50);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x1c);
    if ((static_cast<std::uint8_t>(v2 & std::uint8_t{0x8})) != 0) {
        return v1;
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v1 + 0x20);
    if (v3 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint8_t v4 = Read<std::uint8_t>(v3 + 0x1c);
    return (((static_cast<std::uint8_t>(v4 & std::uint8_t{0x8})) != 0) ? v3 : std::uint64_t{0x0});
}

std::uint32_t APS5_VABI _ZNK7WebCore13MIMETypeCache7isEmptyEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x10);
    if (v1 == std::uint8_t{0x0}) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x8);
    if (v2 == 0) {
        return static_cast<std::uint32_t>(((v2 & ~std::uint64_t{0xff}) | std::uint64_t{0x1}));
    }
    const std::uint32_t v3 = Read<std::uint32_t>(v2 - 0xc);
    return static_cast<std::uint32_t>(((v2 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v3 == std::uint32_t{0x0}))));
}

std::uint64_t APS5_VABI _ZNK7WebCore14DocumentLoader11frameLoaderEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xa0);
    return v3;
}

std::uint64_t APS5_VABI _ZNK7WebCore14DocumentLoader18mainResourceLoaderEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x38);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x2a8);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore14ResourceLoader11frameLoaderEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x20);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0xa0);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore14StaticNodeList4itemEj(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x24);
    if (v1 <= static_cast<std::uint32_t>(a1)) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint64_t v4 = Read<std::uint64_t>(v2 + static_cast<std::uint64_t>(static_cast<std::uint32_t>(a1)) * 8);
    return v4;
}

std::uint8_t APS5_VABI _ZNK7WebCore15ActiveDOMObject16isContextStoppedEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint8_t{0x1};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0xa9);
    return static_cast<std::uint8_t>(v2 != std::uint8_t{0x0});
}

std::uint32_t APS5_VABI _ZNK7WebCore15ActiveDOMObject20isAllowedToRunScriptEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0xa9);
    if (v2 == std::uint8_t{0x0}) {
        const std::uint8_t v3 = Read<std::uint8_t>(v1 + 0xa8);
        return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v3 == std::uint8_t{0x0}))));
    }
    return std::uint32_t{0x0};
}

std::uint64_t APS5_VABI _ZNK7WebCore15FocusController18focusedOrMainFrameEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    if (v1 == 0) {
        const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x8);
        const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x68);
        return v3;
    }
    return v1;
}

std::uint64_t APS5_VABI _ZNK7WebCore15SecurityContext14securityOriginEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore16DOMGuardedObject12globalObjectEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x20);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x8);
    if ((static_cast<std::uint8_t>(v2 & std::uint8_t{0x3})) == 0) {
        const std::uint64_t v3 = Read<std::uint64_t>(v1);
        return v3;
    }
    return std::uint64_t{0x0};
}

std::uint64_t APS5_VABI _ZNK7WebCore16DOMGuardedObject13guardedObjectEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x8);
    if ((static_cast<std::uint8_t>(v2 & std::uint8_t{0x3})) == 0) {
        const std::uint64_t v3 = Read<std::uint64_t>(v1);
        return v3;
    }
    return std::uint64_t{0x0};
}

std::uint8_t APS5_VABI _ZNK7WebCore16DOMGuardedObject7isEmptyEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint8_t{0x1};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    if (v2 == std::uint64_t{0x0}) {
        return std::uint8_t{0x1};
    }
    const std::uint8_t v3 = Read<std::uint8_t>(v1 + 0x8);
    return static_cast<std::uint8_t>((static_cast<std::uint8_t>(v3 & std::uint8_t{0x3})) != 0);
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLMediaElement12isFullscreenEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x590);
    const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x20);
    if (v4 == 0) {
        const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0x398);
        return static_cast<std::uint8_t>(v5 != std::uint32_t{0x0});
    }
    if (v4 != a0) {
        const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x398);
        return static_cast<std::uint8_t>(v7 != std::uint32_t{0x0});
    }
    return std::uint8_t{0x1};
}

std::uint64_t APS5_VABI _ZNK7WebCore16HTMLMediaElement33lastMediaResourceLoaderForTestingEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x4a0);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return v2;
}

std::uint32_t APS5_VABI _ZNK7WebCore16HTMLStyleElement8disabledEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x98);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x19);
    return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v2 != std::uint8_t{0x0}))));
}

std::uint32_t APS5_VABI _ZNK7WebCore17ActiveDOMCallback17canInvokeCallbackEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0xa8);
    if (v2 == std::uint8_t{0x0}) {
        const std::uint8_t v3 = Read<std::uint8_t>(v1 + 0xa9);
        return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v3 == std::uint8_t{0x0}))));
    }
    return std::uint32_t{0x0};
}

std::uint8_t APS5_VABI _ZNK7WebCore17ActiveDOMCallback25activeDOMObjectAreStoppedEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint8_t{0x1};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0xa9);
    return static_cast<std::uint8_t>(v2 != std::uint8_t{0x0});
}

std::uint32_t APS5_VABI _ZNK7WebCore17ActiveDOMCallback28activeDOMObjectsAreSuspendedEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0xa8);
    return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v2 != std::uint8_t{0x0}))));
}

std::uint64_t APS5_VABI _ZNK7WebCore21FormAssociatedElement4formEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore21HTMLFrameOwnerElement13contentWindowEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x78);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x18);
    return v3;
}

std::uint64_t APS5_VABI _ZNK7WebCore21HTMLFrameOwnerElement15contentDocumentEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x78);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xc0);
    return v3;
}

std::uint8_t APS5_VABI _ZNK7WebCore26NetscapePlugInStreamLoader6isDoneEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x428);
    if (v1 == 0) {
        return std::uint8_t{0x1};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return static_cast<std::uint8_t>(v2 == std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZNK7WebCore28InspectorFrontendClientLocal13inspectedPageEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x118);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore4Node10shadowHostEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    const std::uint8_t v3 = Read<std::uint8_t>(v2 + 0x1d);
    if ((static_cast<std::uint8_t>(v3 & std::uint8_t{0x2})) == 0) {
        return std::uint64_t{0x0};
    }
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(v2 + 0xb8);
    if (v4 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x8);
    return v5;
}

std::uint64_t APS5_VABI _ZNK7WebCore4Node9renderBoxEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x40);
    const std::uint64_t v2 = std::uint64_t{0xffffffffffff} & v1;
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint8_t v3 = Read<std::uint8_t>(v2 + 0x31);
    return (((static_cast<std::uint8_t>(v3 & std::uint8_t{0x8})) != 0) ? v2 : std::uint64_t{0x0});
}

std::uint32_t APS5_VABI _ZNK7WebCore4Page16hasSeenAnyPluginEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x2b0);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 - 0xc);
    return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v2 != std::uint32_t{0x0}))));
}

std::uint32_t APS5_VABI _ZNK7WebCore4Page19disabledAdaptationsEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x68);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0xc0);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint32_t v3 = Read<std::uint32_t>(v2 + 0x674);
    return v3;
}

std::uint64_t APS5_VABI _ZNK7WebCore4Page28remoteInspectionNameOverrideEv(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x2a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x18);
    Write<std::uint64_t>(a0, v2);
    if (v2 == 0) {
        return a0;
    }
    const std::uint32_t v3 = Read<std::uint32_t>(v2);
    Write<std::uint32_t>(v2, (v3 + std::uint32_t{0x2}));
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore5Frame15contentRendererEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xc0);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x578);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore5Frame6windowEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xc0);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x1b8);
    return v2;
}

std::uint32_t APS5_VABI _ZNK7WebCore6Editor7Command15isTextInsertionEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x28);
    return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v2 != std::uint8_t{0x0}))));
}

std::uint64_t APS5_VABI _ZNK7WebCore6Widget6parentEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x20);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore7Element10shadowRootEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x48);
    const std::uint64_t v2 = std::uint64_t{0xffffffffffff} & v1;
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x40);
    return v3;
}

std::uint64_t APS5_VABI _ZNK7WebCore7Element18afterPseudoElementEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x48);
    const std::uint64_t v2 = std::uint64_t{0xffffffffffff} & v1;
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x80);
    return v3;
}

std::uint64_t APS5_VABI _ZNK7WebCore7Element19beforePseudoElementEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x48);
    const std::uint64_t v2 = std::uint64_t{0xffffffffffff} & v1;
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x78);
    return v3;
}

std::uint64_t APS5_VABI _ZNK7WebCore8Document11windowProxyEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x188);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x18);
    return v3;
}

std::uint8_t APS5_VABI _ZNK7WebCore8Document21haveStylesheetsLoadedEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x388);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x80);
    if (v2 == 0) {
        const std::uint64_t v3 = Read<std::uint64_t>(v1 + 0x78);
        if (v3 == 0) {
            const std::uint64_t v4 = Read<std::uint64_t>(v1 + 0x88);
            if (v4 == 0) {
                return std::uint8_t{0x1};
            }
            const std::uint32_t v5 = Read<std::uint32_t>(v4 - 0xc);
            if (v5 == std::uint32_t{0x0}) {
                return std::uint8_t{0x1};
            }
            const std::uint8_t v6 = Read<std::uint8_t>(a0 + 0x8df);
            return static_cast<std::uint8_t>(v6 != std::uint8_t{0x0});
        }
        const std::uint32_t v9 = Read<std::uint32_t>(v3 - 0xc);
        if (v9 != std::uint32_t{0x0}) {
            const std::uint8_t v10 = Read<std::uint8_t>(a0 + 0x8df);
            return static_cast<std::uint8_t>(v10 != std::uint8_t{0x0});
        }
        const std::uint64_t v12 = Read<std::uint64_t>(v1 + 0x88);
        if (v12 == 0) {
            return std::uint8_t{0x1};
        }
        const std::uint32_t v13 = Read<std::uint32_t>(v12 - 0xc);
        if (v13 == std::uint32_t{0x0}) {
            return std::uint8_t{0x1};
        }
        const std::uint8_t v14 = Read<std::uint8_t>(a0 + 0x8df);
        return static_cast<std::uint8_t>(v14 != std::uint8_t{0x0});
    }
    const std::uint32_t v17 = Read<std::uint32_t>(v2 - 0xc);
    if (v17 != std::uint32_t{0x0}) {
        const std::uint8_t v18 = Read<std::uint8_t>(a0 + 0x8df);
        return static_cast<std::uint8_t>(v18 != std::uint8_t{0x0});
    }
    const std::uint64_t v20 = Read<std::uint64_t>(v1 + 0x78);
    if (v20 == 0) {
        const std::uint64_t v21 = Read<std::uint64_t>(v1 + 0x88);
        if (v21 == 0) {
            return std::uint8_t{0x1};
        }
        const std::uint32_t v22 = Read<std::uint32_t>(v21 - 0xc);
        if (v22 == std::uint32_t{0x0}) {
            return std::uint8_t{0x1};
        }
        const std::uint8_t v23 = Read<std::uint8_t>(a0 + 0x8df);
        return static_cast<std::uint8_t>(v23 != std::uint8_t{0x0});
    }
    const std::uint32_t v26 = Read<std::uint32_t>(v20 - 0xc);
    if (v26 != std::uint32_t{0x0}) {
        const std::uint8_t v27 = Read<std::uint8_t>(a0 + 0x8df);
        return static_cast<std::uint8_t>(v27 != std::uint8_t{0x0});
    }
    const std::uint64_t v29 = Read<std::uint64_t>(v1 + 0x88);
    if (v29 == 0) {
        return std::uint8_t{0x1};
    }
    const std::uint32_t v30 = Read<std::uint32_t>(v29 - 0xc);
    if (v30 == std::uint32_t{0x0}) {
        return std::uint8_t{0x1};
    }
    const std::uint8_t v31 = Read<std::uint8_t>(a0 + 0x8df);
    return static_cast<std::uint8_t>(v31 != std::uint8_t{0x0});
}

std::uint64_t APS5_VABI _ZNK7WebCore8Document4viewEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x188);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xb8);
    return v3;
}

std::uint64_t APS5_VABI _ZNK7WebCore8FileList4itemEj(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x1c);
    if (v1 <= static_cast<std::uint32_t>(a1)) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v4 = Read<std::uint64_t>(v2 + static_cast<std::uint64_t>(static_cast<std::uint32_t>(a1)) * 8);
    return v4;
}

std::uint64_t APS5_VABI _ZNK7WebCore9FrameTree6parentEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore9FrameView10renderViewEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x128);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0xc0);
    if (v2 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x578);
    return v3;
}

std::uint32_t APS5_VABI _ZNK9Inspector14ConsoleMessage13argumentCountEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x1c);
    return v2;
}

std::uint64_t APS5_VABI _ZNK9Inspector15ScriptArguments12globalObjectEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    if (v1 == 0) {
        return std::uint64_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    return v2;
}

std::uint8_t APS5_VABI _ZNK9Inspector18InjectedScriptBase10hasNoValueEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    if (v1 == 0) {
        return std::uint8_t{0x1};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    return static_cast<std::uint8_t>(v2 == std::uint64_t{0x0});
}

std::uint32_t APS5_VABI _ZNK9Inspector19InspectorAuditAgent14hasActiveAuditEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x30);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    return static_cast<std::uint32_t>(((v1 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v2 != std::uint64_t{0x0}))));
}

}
