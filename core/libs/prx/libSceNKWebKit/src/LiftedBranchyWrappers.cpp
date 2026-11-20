#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "LiftedAbi.hpp"

using NKWebKit::Lifted::Read;
using NKWebKit::Lifted::Write;
using NKWebKit::Lifted::ReadBytes;
using NKWebKit::Lifted::WriteBytes;
using NKWebKit::Lifted::Bytes;
using NKWebKit::Lifted::Call;

extern "C" {

std::uint32_t APS5_VABI WKAccessibilityRootObject(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (a0 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x30);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x68);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0xc0);
    if (v3 == std::uint64_t{0x0}) {
        return std::uint32_t{0x0};
    }
    Call<std::uint64_t>(_ZN7WebCore13AXObjectCache19enableAccessibilityEv, a0, a1, a2, a3, a4, a5);
    const std::uint64_t v5 = Read<std::uint64_t>(v2 + 0xc0);
    const std::uint64_t v6 = Call<std::uint64_t>(_ZNK7WebCore8Document13axObjectCacheEv, v5, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    const std::uint64_t v7 = Call<std::uint64_t>(_ZN7WebCore13AXObjectCache10rootObjectEv, v6, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    if (v7 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v8 = Read<std::uint64_t>(v7 + 0x10);
    return static_cast<std::uint32_t>(v8);
}

std::uint32_t APS5_VABI _ZN3JSC12StackVisitor5Frame8sourceIDEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x40);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x28);
    if (v3 == 0) {
        return std::uint32_t{0x1};
    }
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x60);
    if (v4 == 0) {
        Call<std::uint64_t>(_ZN3JSC14SourceProvider5getIDEv, v3, a1, a2, a3, a4, a5);
        const std::uint32_t v6 = Read<std::uint32_t>(v3 + 0x60);
        return v6;
    }
    return v4;
}

std::uint32_t APS5_VABI _ZN3JSC17DebuggerCallFrame20sourceIDForCallFrameEPNS_9CallFrameE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    if (a0 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x40);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x28);
    if (v3 == 0) {
        return std::uint32_t{0x1};
    }
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x60);
    if (v4 == 0) {
        Call<std::uint64_t>(_ZN3JSC14SourceProvider5getIDEv, v3, a1, a2, v1, a4, a5);
        const std::uint32_t v6 = Read<std::uint32_t>(v3 + 0x60);
        return v6;
    }
    return v4;
}

std::uint8_t APS5_VABI _ZN3JSC9Structure16isValidPrototypeENS_7JSValueE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (a0 != std::uint64_t{0x2}) {
        if ((a0 & std::uint64_t{0xfffe000000000002}) != 0) {
            return std::uint8_t{0x0};
        }
        const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x5);
        if (v1 < std::uint8_t{0x17}) {
            return std::uint8_t{0x0};
        }
        const std::uint64_t v2 = Call<std::uint64_t>(_ZN3JSC6JSCell9getObjectEv, a0, a1, a2, a3, a4, a5);
        const std::uint8_t v3 = Read<std::uint8_t>(v2 + 0x6);
        return static_cast<std::uint8_t>(v3 >> 7);
    }
    return std::uint8_t{0x1};
}

void APS5_VABI _ZN3WTF10StringView16GraphemeClusters8IteratorD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    if (v1 == 0) {
        return;
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x18);
    if (v2 == std::uint8_t{0x0}) {
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, a1, a2, a3, a4, a5);
        return;
    }
    Call<std::uint64_t>(_ZN3WTF31NonSharedCharacterBreakIteratorD1Ev, (v1 + 0x10), a1, a2, a3, a4, a5);
    Write<std::uint8_t>(v1 + 0x18, std::uint8_t{0x0});
    Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

void APS5_VABI _ZN3WTF10StringView16GraphemeClusters8IteratorD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    if (v1 == 0) {
        return;
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x18);
    if (v2 == std::uint8_t{0x0}) {
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, a1, a2, a3, a4, a5);
        return;
    }
    Call<std::uint64_t>(_ZN3WTF31NonSharedCharacterBreakIteratorD1Ev, (v1 + 0x10), a1, a2, a3, a4, a5);
    Write<std::uint8_t>(v1 + 0x18, std::uint8_t{0x0});
    Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
}

void APS5_VABI _ZN7WebCore11MemoryCache14evictResourcesEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint8_t v1 = Read<std::uint8_t>(a0);
    if (v1 == std::uint8_t{0x0}) {
        Call<std::uint64_t>(_ZN7WebCore11MemoryCache11setDisabledEb, a0, std::uint64_t{0x1}, a2, a3, a4, a5);
        Write<std::uint8_t>(a0, std::uint8_t{0x0});
        return;
    }
}

void APS5_VABI _ZN7WebCore12EventHandler30setCapturingMouseEventsElementEPNS_7ElementE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    if (a1 == 0) {
        const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x68);
        Write<std::uint64_t>(a0 + 0x68, a1);
        if (v1 == 0) {
            Write<std::uint8_t>(a0 + 0x64, std::uint8_t{0x0});
            return;
        }
        const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x18);
        const std::uint32_t v3 = v2 + std::uint32_t{0xfffffffe};
        if (v3 == 0) {
            Call<std::uint64_t>(_ZN7WebCore4Node14removedLastRefEv, v1, a1, a2, a3, a4, a5);
            Write<std::uint8_t>(a0 + 0x64, std::uint8_t{0x0});
            return;
        }
        Write<std::uint32_t>(v1 + 0x18, v3);
        Write<std::uint8_t>(a0 + 0x64, std::uint8_t{0x0});
        return;
    }
    const std::uint32_t v5 = Read<std::uint32_t>(a1 + 0x18);
    Write<std::uint32_t>(a1 + 0x18, (v5 + std::uint32_t{0x2}));
    const std::uint64_t v7 = Read<std::uint64_t>(a0 + 0x68);
    Write<std::uint64_t>(a0 + 0x68, a1);
    if (v7 == 0) {
        Write<std::uint8_t>(a0 + 0x64, std::uint8_t{0x0});
        return;
    }
    const std::uint32_t v8 = Read<std::uint32_t>(v7 + 0x18);
    const std::uint32_t v9 = v8 + std::uint32_t{0xfffffffe};
    if (v9 == 0) {
        Call<std::uint64_t>(_ZN7WebCore4Node14removedLastRefEv, v7, a1, a2, a3, a4, a5);
        Write<std::uint8_t>(a0 + 0x64, std::uint8_t{0x0});
        return;
    }
    Write<std::uint32_t>(v7 + 0x18, v9);
    Write<std::uint8_t>(a0 + 0x64, std::uint8_t{0x0});
}

void APS5_VABI _ZN7WebCore12JSTimeRangesD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    const std::uint32_t v3 = v2 - 1;
    if (v3 == 0) {
        const std::uint64_t v4 = Read<std::uint64_t>(v1 + 0x8);
        if (v4 == 0) {
            Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, a1, a2, a3, a4, a5);
            return;
        }
        Write<std::uint64_t>(v1 + 0x8, std::uint64_t{0x0});
        Write<std::uint32_t>(v1 + 0x10, std::uint32_t{0x0});
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v4, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v1, v3);
}

void APS5_VABI _ZN7WebCore12JSTimeRangesD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    const std::uint32_t v3 = v2 - 1;
    if (v3 == 0) {
        const std::uint64_t v4 = Read<std::uint64_t>(v1 + 0x8);
        if (v4 == 0) {
            Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, a1, a2, a3, a4, a5);
            return;
        }
        Write<std::uint64_t>(v1 + 0x8, std::uint64_t{0x0});
        Write<std::uint32_t>(v1 + 0x10, std::uint32_t{0x0});
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v4, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v1, v3);
}

std::uint64_t APS5_VABI _ZN7WebCore12JSTimeRangesaSERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + 1));
    const std::uint64_t v6 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    if (v6 == 0) {
        return a0;
    }
    const std::uint32_t v7 = Read<std::uint32_t>(v6);
    const std::uint32_t v8 = v7 - 1;
    if (v8 == 0) {
        const std::uint64_t v9 = Read<std::uint64_t>(v6 + 0x8);
        if (v9 == 0) {
            Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v6, a1, a2, a3, a4, a5);
            return a0;
        }
        Write<std::uint64_t>(v6 + 0x8, std::uint64_t{0x0});
        Write<std::uint32_t>(v6 + 0x10, std::uint32_t{0x0});
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v9, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v6, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return a0;
    }
    Write<std::uint32_t>(v6, v8);
    return a0;
}

void APS5_VABI _ZN7WebCore13JSDOMRectListD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    const std::uint32_t v3 = v2 - 1;
    if (v3 == 0) {
        Call<std::uint64_t>(_ZN7WebCore11DOMRectListD1Ev, v1, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v1, v3);
}

void APS5_VABI _ZN7WebCore13JSDOMRectListD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    const std::uint32_t v3 = v2 - 1;
    if (v3 == 0) {
        Call<std::uint64_t>(_ZN7WebCore11DOMRectListD1Ev, v1, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v1, v3);
}

std::uint64_t APS5_VABI _ZN7WebCore13JSDOMRectListaSERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + 1));
    const std::uint64_t v6 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    if (v6 == 0) {
        return a0;
    }
    const std::uint32_t v7 = Read<std::uint32_t>(v6);
    const std::uint32_t v8 = v7 - 1;
    if (v8 == 0) {
        Call<std::uint64_t>(_ZN7WebCore11DOMRectListD1Ev, v6, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v6, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return a0;
    }
    Write<std::uint32_t>(v6, v8);
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore13displayStringERKN3WTF6StringEPKNS_4NodeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    if (a2 == 0) {
        const std::uint64_t v1 = Read<std::uint64_t>(a1);
        Write<std::uint64_t>(a0, v1);
        if (v1 == 0) {
            return a0;
        }
        const std::uint32_t v2 = Read<std::uint32_t>(v1);
        Write<std::uint32_t>(v1, (v2 + std::uint32_t{0x2}));
        return a0;
    }
    const std::uint64_t v4 = Read<std::uint64_t>(a2 + 0x28);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x8);
    Call<std::uint64_t>(_ZNK7WebCore8Document31displayStringModifiedByEncodingERKN3WTF6StringE, a0, v5, a1, v4, a4, a5);
    return a0;
}

std::uint32_t APS5_VABI _ZN7WebCore20HTMLFrameElementBase5widthEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    Call<std::uint64_t>(_ZN7WebCore8Document36updateLayoutIgnorePendingStylesheetsENS0_18RunPostLayoutTasksE, v2, std::uint64_t{0x0}, a2, a3, a4, a5);
    const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore4Node9renderBoxEv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    if (v4 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v5 = Call<std::uint64_t>(_ZNK7WebCore4Node9renderBoxEv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    const std::uint32_t v6 = Read<std::uint32_t>(v5 + 0xb0);
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(((static_cast<std::int32_t>(v6) >= 0) ? v6 : static_cast<std::uint32_t>(static_cast<std::uint64_t>(v6) + 0x3f))) >> 6);
}

std::uint32_t APS5_VABI _ZN7WebCore20HTMLFrameElementBase6heightEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a1;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    Call<std::uint64_t>(_ZN7WebCore8Document36updateLayoutIgnorePendingStylesheetsENS0_18RunPostLayoutTasksE, v2, std::uint64_t{0x0}, a2, a3, a4, a5);
    const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore4Node9renderBoxEv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    if (v4 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v5 = Call<std::uint64_t>(_ZNK7WebCore4Node9renderBoxEv, a0, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    const std::uint32_t v6 = Read<std::uint32_t>(v5 + 0xb4);
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(((static_cast<std::int32_t>(v6) >= 0) ? v6 : static_cast<std::uint32_t>(static_cast<std::uint64_t>(v6) + 0x3f))) >> 6);
}

std::uint64_t APS5_VABI _ZN7WebCore22makeRangeSelectingNodeERNS_4NodeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x20);
    if (v1 == 0) {
        WriteBytes<32>(a0, Bytes<32>{});
        Write<std::uint8_t>(a0 + 0x20, std::uint8_t{0x0});
        return a0;
    }
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK7WebCore4Node16computeNodeIndexEv, a1, a1, a2, a3, a4, a5);
    const std::uint32_t v3 = Read<std::uint32_t>(v1 + 0x18);
    Write<std::uint32_t>(v1 + 0x18, (v3 + std::uint32_t{0x4}));
    Write<std::uint64_t>(a0, v1);
    Write<std::uint32_t>(a0 + 0x8, static_cast<std::uint32_t>(v2));
    Write<std::uint64_t>(a0 + 0x10, v1);
    Write<std::uint32_t>(a0 + 0x18, (static_cast<std::uint32_t>(v2) + 1));
    Write<std::uint8_t>(a0 + 0x20, std::uint8_t{0x1});
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore23ApplicationCacheStorageC1ERKN3WTF6StringES4_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint32_t>(a0, std::uint32_t{0x1});
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x8, v1);
    if (v1 == 0) {
        const std::uint64_t v2 = Read<std::uint64_t>(a2);
        Write<std::uint64_t>(a0 + 0x10, v2);
        if (v2 == 0) {
            Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
            Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x7fffffffffffffff});
            Write<std::uint8_t>(a0 + 0x28, std::uint8_t{0x0});
            Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x7fffffffffffffff});
            const std::uint64_t v4 = Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (a0 + 0x38), a1, a2, a3, a4, a5);
            WriteBytes<16>(a0 + 0x78, Bytes<16>{});
            return v4;
        }
        const std::uint32_t v5 = Read<std::uint32_t>(v2);
        Write<std::uint32_t>(v2, (v5 + std::uint32_t{0x2}));
        Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
        Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x7fffffffffffffff});
        Write<std::uint8_t>(a0 + 0x28, std::uint8_t{0x0});
        Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x7fffffffffffffff});
        const std::uint64_t v8 = Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (a0 + 0x38), a1, a2, a3, a4, a5);
        WriteBytes<16>(a0 + 0x78, Bytes<16>{});
        return v8;
    }
    const std::uint32_t v9 = Read<std::uint32_t>(v1);
    Write<std::uint32_t>(v1, (v9 + std::uint32_t{0x2}));
    const std::uint64_t v11 = Read<std::uint64_t>(a2);
    Write<std::uint64_t>(a0 + 0x10, v11);
    if (v11 == 0) {
        Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
        Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x7fffffffffffffff});
        Write<std::uint8_t>(a0 + 0x28, std::uint8_t{0x0});
        Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x7fffffffffffffff});
        const std::uint64_t v13 = Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (a0 + 0x38), a1, a2, a3, a4, a5);
        WriteBytes<16>(a0 + 0x78, Bytes<16>{});
        return v13;
    }
    const std::uint32_t v14 = Read<std::uint32_t>(v11);
    Write<std::uint32_t>(v11, (v14 + std::uint32_t{0x2}));
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x7fffffffffffffff});
    Write<std::uint8_t>(a0 + 0x28, std::uint8_t{0x0});
    Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x7fffffffffffffff});
    const std::uint64_t v17 = Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (a0 + 0x38), a1, a2, a3, a4, a5);
    WriteBytes<16>(a0 + 0x78, Bytes<16>{});
    return v17;
}

std::uint64_t APS5_VABI _ZN7WebCore23ApplicationCacheStorageC2ERKN3WTF6StringES4_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint32_t>(a0, std::uint32_t{0x1});
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x8, v1);
    if (v1 == 0) {
        const std::uint64_t v2 = Read<std::uint64_t>(a2);
        Write<std::uint64_t>(a0 + 0x10, v2);
        if (v2 == 0) {
            Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
            Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x7fffffffffffffff});
            Write<std::uint8_t>(a0 + 0x28, std::uint8_t{0x0});
            Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x7fffffffffffffff});
            const std::uint64_t v4 = Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (a0 + 0x38), a1, a2, a3, a4, a5);
            WriteBytes<16>(a0 + 0x78, Bytes<16>{});
            return v4;
        }
        const std::uint32_t v5 = Read<std::uint32_t>(v2);
        Write<std::uint32_t>(v2, (v5 + std::uint32_t{0x2}));
        Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
        Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x7fffffffffffffff});
        Write<std::uint8_t>(a0 + 0x28, std::uint8_t{0x0});
        Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x7fffffffffffffff});
        const std::uint64_t v8 = Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (a0 + 0x38), a1, a2, a3, a4, a5);
        WriteBytes<16>(a0 + 0x78, Bytes<16>{});
        return v8;
    }
    const std::uint32_t v9 = Read<std::uint32_t>(v1);
    Write<std::uint32_t>(v1, (v9 + std::uint32_t{0x2}));
    const std::uint64_t v11 = Read<std::uint64_t>(a2);
    Write<std::uint64_t>(a0 + 0x10, v11);
    if (v11 == 0) {
        Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
        Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x7fffffffffffffff});
        Write<std::uint8_t>(a0 + 0x28, std::uint8_t{0x0});
        Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x7fffffffffffffff});
        const std::uint64_t v13 = Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (a0 + 0x38), a1, a2, a3, a4, a5);
        WriteBytes<16>(a0 + 0x78, Bytes<16>{});
        return v13;
    }
    const std::uint32_t v14 = Read<std::uint32_t>(v11);
    Write<std::uint32_t>(v11, (v14 + std::uint32_t{0x2}));
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x7fffffffffffffff});
    Write<std::uint8_t>(a0 + 0x28, std::uint8_t{0x0});
    Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x7fffffffffffffff});
    const std::uint64_t v17 = Call<std::uint64_t>(_ZN7WebCore14SQLiteDatabaseC1Ev, (a0 + 0x38), a1, a2, a3, a4, a5);
    WriteBytes<16>(a0 + 0x78, Bytes<16>{});
    return v17;
}

std::uint32_t APS5_VABI _ZN7WebCore23MediaQueryParserContextC1ERKNS_8DocumentE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint8_t>(a0, std::uint8_t{0x0});
    Write<std::uint32_t>(a0 + 0x4, std::uint32_t{0x0});
    const std::uint64_t v1 = Call<std::uint64_t>(_ZNK7WebCore8Document4pageEv, a1, a1, a2, a3, a4, a5);
    if (v1 == 0) {
        Write<std::uint8_t>(a0, std::uint8_t{0x0});
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK7WebCore8Document4pageEv, a1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    const std::uint8_t v3 = Read<std::uint8_t>(v2 + 0x148);
    Write<std::uint8_t>(a0, v3);
    return static_cast<std::uint32_t>(((v2 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(v3)));
}

std::uint32_t APS5_VABI _ZN7WebCore23MediaQueryParserContextC2ERKNS_8DocumentE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    Write<std::uint8_t>(a0, std::uint8_t{0x0});
    Write<std::uint32_t>(a0 + 0x4, std::uint32_t{0x0});
    const std::uint64_t v1 = Call<std::uint64_t>(_ZNK7WebCore8Document4pageEv, a1, a1, a2, a3, a4, a5);
    if (v1 == 0) {
        Write<std::uint8_t>(a0, std::uint8_t{0x0});
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK7WebCore8Document4pageEv, a1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    const std::uint8_t v3 = Read<std::uint8_t>(v2 + 0x148);
    Write<std::uint8_t>(a0, v3);
    return static_cast<std::uint32_t>(((v2 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(v3)));
}

std::uint64_t APS5_VABI _ZN7WebCore26makeBoundaryPointAfterNodeERNS_4NodeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x20);
    if (v1 == 0) {
        WriteBytes<16>(a0, Bytes<16>{});
        Write<std::uint8_t>(a0 + 0x10, std::uint8_t{0x0});
        return a0;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x18);
    Write<std::uint32_t>(v1 + 0x18, (v2 + std::uint32_t{0x2}));
    const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore4Node16computeNodeIndexEv, a1, a1, a2, a3, a4, a5);
    Write<std::uint64_t>(a0, v1);
    Write<std::uint32_t>(a0 + 0x8, (static_cast<std::uint32_t>(v4) + 1));
    Write<std::uint8_t>(a0 + 0x10, std::uint8_t{0x1});
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore27makeBoundaryPointBeforeNodeERNS_4NodeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x20);
    if (v1 == 0) {
        WriteBytes<16>(a0, Bytes<16>{});
        Write<std::uint8_t>(a0 + 0x10, std::uint8_t{0x0});
        return a0;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x18);
    Write<std::uint32_t>(v1 + 0x18, (v2 + std::uint32_t{0x2}));
    const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore4Node16computeNodeIndexEv, a1, a1, a2, a3, a4, a5);
    Write<std::uint64_t>(a0, v1);
    Write<std::uint32_t>(a0 + 0x8, static_cast<std::uint32_t>(v4));
    Write<std::uint8_t>(a0 + 0x10, std::uint8_t{0x1});
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore30makeRangeSelectingNodeContentsERNS_4NodeE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x18);
    Write<std::uint32_t>(a1 + 0x18, (v1 + std::uint32_t{0x4}));
    const std::uint32_t v3 = Read<std::uint32_t>(a1 + 0x1c);
    if ((static_cast<std::uint8_t>(static_cast<std::uint8_t>(v3) & std::uint8_t{0x1})) != 0) {
        const std::uint64_t v4 = Read<std::uint64_t>(a1 + 0x50);
        if (v4 == 0) {
            Write<std::uint64_t>(a0, a1);
            Write<std::uint32_t>(a0 + 0x8, std::uint32_t{0x0});
            Write<std::uint64_t>(a0 + 0x10, a1);
            Write<std::uint32_t>(a0 + 0x18, std::uint32_t{0x0});
            return a0;
        }
        const std::uint32_t v5 = Read<std::uint32_t>(v4 + 0x4);
        Write<std::uint64_t>(a0, a1);
        Write<std::uint32_t>(a0 + 0x8, std::uint32_t{0x0});
        Write<std::uint64_t>(a0 + 0x10, a1);
        Write<std::uint32_t>(a0 + 0x18, v5);
        return a0;
    }
    if ((static_cast<std::uint8_t>(static_cast<std::uint8_t>(v3) & std::uint8_t{0x4})) == 0) {
        Write<std::uint64_t>(a0, a1);
        Write<std::uint32_t>(a0 + 0x8, std::uint32_t{0x0});
        Write<std::uint64_t>(a0 + 0x10, a1);
        Write<std::uint32_t>(a0 + 0x18, std::uint32_t{0x0});
        return a0;
    }
    const std::uint64_t v6 = Call<std::uint64_t>(_ZNK7WebCore13ContainerNode15countChildNodesEv, a1, a1, a2, static_cast<std::uint64_t>(v3), a4, a5);
    Write<std::uint64_t>(a0, a1);
    Write<std::uint32_t>(a0 + 0x8, std::uint32_t{0x0});
    Write<std::uint64_t>(a0 + 0x10, a1);
    Write<std::uint32_t>(a0 + 0x18, static_cast<std::uint32_t>(v6));
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore4Node11appendChildERS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a1 + 0x1c);
    if ((static_cast<std::uint8_t>(v1 & std::uint8_t{0x4})) != 0) {
        Call<std::uint64_t>(_ZN7WebCore13ContainerNode11appendChildERNS_4NodeE, a0, a1, a2, a3, a4, a5);
        return a0;
    }
    Write<std::uint32_t>(a0, std::uint32_t{0x1});
    Write<std::uint64_t>(a0 + 0x8, std::uint64_t{0x0});
    Write<std::uint8_t>(a0 + 0x10, std::uint8_t{0x0});
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore4Node11removeChildERS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a1 + 0x1c);
    if ((static_cast<std::uint8_t>(v1 & std::uint8_t{0x4})) != 0) {
        Call<std::uint64_t>(_ZN7WebCore13ContainerNode11removeChildERNS_4NodeE, a0, a1, a2, a3, a4, a5);
        return a0;
    }
    Write<std::uint32_t>(a0, std::uint32_t{0x5});
    Write<std::uint64_t>(a0 + 0x8, std::uint64_t{0x0});
    Write<std::uint8_t>(a0 + 0x10, std::uint8_t{0x0});
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore4Node6removeEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x20);
    if (v1 == 0) {
        WriteBytes<16>(a0, Bytes<16>{});
        Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
        Write<std::uint8_t>(a0 + 0x10, std::uint8_t{0x1});
        return a0;
    }
    Call<std::uint64_t>(_ZN7WebCore13ContainerNode11removeChildERNS_4NodeE, a0, v1, a1, a3, a4, a5);
    return a0;
}

std::uint32_t APS5_VABI _ZN7WebCore4toJSEPN3JSC14JSGlobalObjectERNS_9DOMWindowE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZNK7WebCore9DOMWindow5frameEv, a1, a1, a2, a3, a4, a5);
    if (v1 == 0) {
        return std::uint32_t{0x2};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x18);
    return static_cast<std::uint32_t>(Call<std::uint64_t>(_ZN7WebCore4toJSEPN3JSC14JSGlobalObjectERNS_11WindowProxyE, a0, v2, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}));
}

void APS5_VABI _ZN7WebCore8JSPath2DD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x28);
    const std::uint64_t v3 = v1 + std::uint64_t{0x28};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = v3 + std::uint64_t{0xffffffffffffffd8};
        Call<std::uint64_t>(_ZN7WebCore6Path2DD1Ev, v5, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v5, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v3, v4);
}

void APS5_VABI _ZN7WebCore8JSPath2DD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x28);
    const std::uint64_t v3 = v1 + std::uint64_t{0x28};
    const std::uint32_t v4 = v2 - 1;
    if (v4 == 0) {
        const std::uint64_t v5 = v3 + std::uint64_t{0xffffffffffffffd8};
        Call<std::uint64_t>(_ZN7WebCore6Path2DD1Ev, v5, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v5, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v3, v4);
}

std::uint64_t APS5_VABI _ZN7WebCore8JSPath2DaSERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x28);
    Write<std::uint32_t>(v3 + 0x28, (v4 + 1));
    const std::uint64_t v6 = Read<std::uint64_t>(a0 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    if (v6 == 0) {
        return a0;
    }
    const std::uint32_t v7 = Read<std::uint32_t>(v6 + 0x28);
    const std::uint64_t v8 = v6 + std::uint64_t{0x28};
    const std::uint32_t v9 = v7 - 1;
    if (v9 == 0) {
        const std::uint64_t v10 = v8 + std::uint64_t{0xffffffffffffffd8};
        Call<std::uint64_t>(_ZN7WebCore6Path2DD1Ev, v10, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v10, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return a0;
    }
    Write<std::uint32_t>(v8, v9);
    return a0;
}

std::uint32_t APS5_VABI _ZN7bmalloc13IsoSharedPage9tryCreateEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Call<std::uint64_t>(_ZN7bmalloc11IsoPageBase18allocatePageMemoryEv, a0, a1, a2, a3, a4, a5);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v2 = Read<std::uint8_t>(v1);
    Write<std::uint8_t>(v1, static_cast<std::uint8_t>(static_cast<std::uint8_t>(v2 & std::uint8_t{0xf8}) | std::uint8_t{0x3}));
    return static_cast<std::uint32_t>(v1);
}

void APS5_VABI _ZN9Inspector22InspectorDebuggerAgent19clearExceptionValueEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0xe5);
    if (v1 == std::uint8_t{0x0}) {
        return;
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x40);
    Call<std::uint64_t>(_ZN9Inspector21InjectedScriptManager19clearExceptionValueEv, v2, a1, a2, a3, a4, a5);
    Write<std::uint8_t>(a0 + 0xe5, std::uint8_t{0x0});
}

void APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandler20LoadResourceCallbackD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    Write<std::uint64_t>(a0 + 0x8, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    const std::uint32_t v3 = v2 - 1;
    if (v3 == 0) {
        Call<std::uint64_t>(_ZN9Inspector17BackendDispatcherD2Ev, v1, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v1, v3);
}

void APS5_VABI _ZN9Inspector32DatabaseBackendDispatcherHandler18ExecuteSQLCallbackD1Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    Write<std::uint64_t>(a0 + 0x8, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    const std::uint32_t v3 = v2 - 1;
    if (v3 == 0) {
        Call<std::uint64_t>(_ZN9Inspector17BackendDispatcherD2Ev, v1, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v1, v3);
}

void APS5_VABI _ZN9Inspector32DatabaseBackendDispatcherHandler18ExecuteSQLCallbackD2Ev(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    Write<std::uint64_t>(a0 + 0x8, std::uint64_t{0x0});
    if (v1 == 0) {
        return;
    }
    const std::uint32_t v2 = Read<std::uint32_t>(v1);
    const std::uint32_t v3 = v2 - 1;
    if (v3 == 0) {
        Call<std::uint64_t>(_ZN9Inspector17BackendDispatcherD2Ev, v1, a1, a2, a3, a4, a5);
        Call<std::uint64_t>(_ZN3WTF8fastFreeEPv, v1, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
        return;
    }
    Write<std::uint32_t>(v1, v3);
}

std::uint32_t APS5_VABI _ZNK7WebCore12NamedNodeMap6lengthEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK7WebCore7Element13hasAttributesEv, v1, a1, a2, a3, a4, a5);
    if (static_cast<std::uint8_t>(static_cast<std::uint32_t>(v2)) == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(a0 + 0x8);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x68);
    const std::uint32_t v6 = Read<std::uint32_t>(v5 + 0x4);
    if ((static_cast<std::uint8_t>(static_cast<std::uint8_t>(v6) & std::uint8_t{0x1})) != 0) {
        const std::uint32_t v7 = Read<std::uint32_t>(v5 + 0x34);
        return v7;
    }
    return (v6 >> 5);
}

std::uint64_t APS5_VABI _ZNK7WebCore13HitTestResult11textContentEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a2;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x68);
    if (v1 == 0) {
        Write<std::uint64_t>(a0, std::uint64_t{0x0});
        return a0;
    }
    Call<std::uint64_t>(_ZNK7WebCore4Node11textContentEb, a0, v1, std::uint64_t{0x0}, a3, a4, a5);
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore13HitTestResult15absoluteLinkURLEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x68);
    if (v1 == 0) {
        Write<std::uint64_t>(a0, std::uint64_t{0x0});
        Call<std::uint64_t>(_ZN3WTF3URL10invalidateEv, a0, v1, a2, a3, a4, a5);
        return a0;
    }
    Call<std::uint64_t>(_ZNK7WebCore7Element15absoluteLinkURLEv, a0, v1, a2, a3, a4, a5);
    return a0;
}

std::uint32_t APS5_VABI _ZNK7WebCore14DocumentLoader21archiveResourceForURLERKN3WTF3URLE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x970);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Call<std::uint64_t>(_ZN7WebCore25ArchiveResourceCollection21archiveResourceForURLERKN3WTF3URLE, v1, a1, a2, a3, a4, a5);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint8_t v3 = Read<std::uint8_t>(v2 + 0x168);
    return static_cast<std::uint32_t>(((v3 == std::uint8_t{0x0}) ? v2 : std::uint64_t{0x0}));
}

std::uint32_t APS5_VABI _ZNK7WebCore14FrameSelection18isFocusedAndActiveEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x14c);
    if ((static_cast<std::uint8_t>(v1 & std::uint8_t{0x10})) == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x8);
    const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore8Document4pageEv, v3, a1, a2, a3, a4, a5);
    if (v4 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v5 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x8);
    const std::uint64_t v7 = Call<std::uint64_t>(_ZNK7WebCore8Document4pageEv, v6, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    const std::uint64_t v8 = Read<std::uint64_t>(v7 + 0x20);
    const std::uint8_t v9 = Read<std::uint8_t>(v8 + 0x1c);
    return static_cast<std::uint32_t>(((((v8 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(v9)) & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v9 & std::uint8_t{0x1}))));
}

std::uint64_t APS5_VABI _ZNK7WebCore14FrameSelection36rootEditableElementOrDocumentElementEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK7WebCore16VisibleSelection19rootEditableElementEv, (a0 + std::uint64_t{0x30}), a1, a2, a3, a4, a5);
    if (v2 == 0) {
        const std::uint64_t v3 = Read<std::uint64_t>(a0 + 0x18);
        const std::uint64_t v4 = Read<std::uint64_t>(v3 + 0x8);
        const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x358);
        return v5;
    }
    return v2;
}

std::uint8_t APS5_VABI _ZNK7WebCore16DOMGuardedObject11isSuspendedEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    (void)a3;
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        return std::uint8_t{0x1};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1);
    if (v2 == std::uint64_t{0x0}) {
        return std::uint8_t{0x1};
    }
    const std::uint8_t v3 = Read<std::uint8_t>(v1 + 0x8);
    if ((static_cast<std::uint8_t>(v3 & std::uint8_t{0x3})) == 0) {
        const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore17ActiveDOMCallback17canInvokeCallbackEv, a0, a1, a2, v1, a4, a5);
        return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v4) ^ std::uint8_t{0x1});
    }
    return std::uint8_t{0x1};
}

std::uint32_t APS5_VABI _ZNK7WebCore20ScrollingCoordinator32coordinatesScrollingForFrameViewERKNS_9FrameViewE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x128);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x38);
    if (v2 == v1) {
        const std::uint64_t v3 = Call<std::uint64_t>(_ZNK7WebCore5Frame15contentRendererEv, v1, a1, a2, a3, a4, a5);
        if (v3 == 0) {
            return std::uint32_t{0x0};
        }
        return static_cast<std::uint32_t>(Call<std::uint64_t>(_ZNK7WebCore10RenderView15usesCompositingEv, v3, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}));
    }
    const std::uint64_t v4 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v5 = Read<std::uint64_t>(v4 + 0x50);
    const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x1c8);
    const std::uint64_t v7 = Read<std::uint64_t>(v5 + 0x1e0);
    const std::uint64_t v10 = static_cast<std::uint64_t>((static_cast<std::uint32_t>(v6) & std::uint32_t{0x40000000})) | static_cast<std::uint64_t>((static_cast<std::uint32_t>(v7) & std::uint32_t{0x1000}));
    if (v10 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v11 = Call<std::uint64_t>(_ZNK7WebCore5Frame15contentRendererEv, v1, a1, a2, v10, a4, a5);
    if (v11 == 0) {
        return std::uint32_t{0x0};
    }
    return static_cast<std::uint32_t>(Call<std::uint64_t>(_ZNK7WebCore10RenderView15usesCompositingEv, v11, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}));
}

std::uint8_t APS5_VABI _ZNK7WebCore27ScriptedAnimationController17throttlingReasonsEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    if (v1 == 0) {
        const std::uint8_t v2 = Read<std::uint8_t>(a0 + 0x30);
        return v2;
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v1 + 0x8);
    if (v3 == 0) {
        const std::uint8_t v4 = Read<std::uint8_t>(a0 + 0x30);
        return v4;
    }
    const std::uint64_t v5 = Call<std::uint64_t>(_ZNK7WebCore8Document4pageEv, v3, a1, a2, a3, a4, a5);
    if (v5 == 0) {
        const std::uint8_t v6 = Read<std::uint8_t>(a0 + 0x30);
        return v6;
    }
    const std::uint8_t v7 = Read<std::uint8_t>(a0 + 0x30);
    const std::uint8_t v8 = Read<std::uint8_t>(v5 + 0x3c8);
    return static_cast<std::uint8_t>(v7 | v8);
}

std::uint64_t APS5_VABI _ZNK7WebCore4Page27pageExtendedBackgroundColorEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x68);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0xb8);
    if (v2 == 0) {
        Write<std::uint64_t>(a0, std::uint64_t{0x0});
        return a0;
    }
    const std::uint64_t v3 = Call<std::uint64_t>(_ZNK7WebCore9FrameView10renderViewEv, v2, a1, a2, a3, a4, a5);
    if (v3 == 0) {
        Write<std::uint64_t>(a0, std::uint64_t{0x0});
        return a0;
    }
    const std::uint64_t v4 = Call<std::uint64_t>(_ZN7WebCore10RenderView10compositorEv, v3, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    Call<std::uint64_t>(_ZN7WebCore5ColorC1ERKS0_, a0, (v4 + 0xc8), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore5Frame31displayStringModifiedByEncodingERKN3WTF6StringE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0xc0);
    if (v1 == 0) {
        const std::uint64_t v2 = Read<std::uint64_t>(a2);
        Write<std::uint64_t>(a0, v2);
        if (v2 == 0) {
            return a0;
        }
        const std::uint32_t v3 = Read<std::uint32_t>(v2);
        Write<std::uint32_t>(v2, (v3 + std::uint32_t{0x2}));
        return a0;
    }
    Call<std::uint64_t>(_ZNK7WebCore8Document31displayStringModifiedByEncodingERKN3WTF6StringE, a0, v1, a2, a3, a4, a5);
    return a0;
}

std::uint32_t APS5_VABI _ZNK7WebCore6Editor6clientEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK7WebCore8Document4pageEv, v1, a1, a2, a3, a4, a5);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x78);
    return static_cast<std::uint32_t>(v3);
}

std::uint32_t APS5_VABI _ZNK7WebCore6Editor9canDeleteEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x9a8);
    const std::uint8_t v3 = Read<std::uint8_t>(v2 + 0x91);
    if (v3 != std::uint8_t{0x2}) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v5 = Call<std::uint64_t>(_ZNK7WebCore16VisibleSelection19rootEditableElementEv, (v2 + std::uint64_t{0x30}), a1, a2, a3, a4, a5);
    return static_cast<std::uint32_t>(((v5 & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v5 != 0))));
}

std::uint32_t APS5_VABI _ZNK7WebCore6Quirks70shouldSuppressAutocorrectionAndAutocaptializationInHiddenEditableAreasEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    if (v1 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    if (v2 == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x1a8);
    const std::uint64_t v4 = Call<std::uint64_t>(_ZNK7WebCore8Settings23needsSiteSpecificQuirksEv, v3, a1, a2, a3, a4, a5);
    if (static_cast<std::uint8_t>(v4) == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v5 = Read<std::uint64_t>(a0);
    const std::uint64_t v6 = Read<std::uint64_t>(v5 + 0x8);
    const std::uint64_t v7 = Call<std::uint64_t>(_ZNK7WebCore8Document11topDocumentEv, v6, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    Call<std::uint64_t>(_ZNK3WTF3URL4hostEv, (v7 + 0x1e0), std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    return std::uint32_t{0x0};
}

std::uint64_t APS5_VABI _ZNK7WebCore8Document6domainEv(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v2 = Call<std::uint64_t>(_ZNK7WebCore15SecurityContext14securityOriginEv, (a1 + 0xb0), a1, a2, a3, a4, a5);
    const std::uint64_t v3 = Read<std::uint64_t>(v2 + 0x20);
    Write<std::uint64_t>(a0, v3);
    if (v3 == 0) {
        return a0;
    }
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + std::uint32_t{0x2}));
    return a0;
}

std::uint32_t APS5_VABI _ZNK9Inspector15ScriptCallFrame7isEqualERKS0_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    const std::uint64_t v3 = Call<std::uint64_t>(_ZN3WTF5equalEPKNS_10StringImplES2_, v1, v2, a2, a3, a4, a5);
    if (static_cast<std::uint8_t>(v3) == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint64_t v4 = Read<std::uint64_t>(a0 + 0x8);
    const std::uint64_t v5 = Read<std::uint64_t>(a1 + 0x8);
    const std::uint64_t v6 = Call<std::uint64_t>(_ZN3WTF5equalEPKNS_10StringImplES2_, v4, v5, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0}, std::uint64_t{0x0});
    if (static_cast<std::uint8_t>(v6) == 0) {
        return std::uint32_t{0x0};
    }
    const std::uint32_t v7 = Read<std::uint32_t>(a0 + 0x14);
    const std::uint32_t v8 = Read<std::uint32_t>(a1 + 0x14);
    if (v7 != v8) {
        return std::uint32_t{0x0};
    }
    const std::uint32_t v9 = Read<std::uint32_t>(a0 + 0x18);
    const std::uint32_t v10 = Read<std::uint32_t>(a1 + 0x18);
    return static_cast<std::uint32_t>(((static_cast<std::uint64_t>(v9) & ~std::uint64_t{0xff}) | static_cast<std::uint64_t>(static_cast<std::uint8_t>(v9 == v10))));
}

}
