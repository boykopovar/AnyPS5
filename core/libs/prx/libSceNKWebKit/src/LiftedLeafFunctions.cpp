#include "prx/libSceNKWebKit/include/LiftedLeafFunctionsExports.hpp"
#include "LiftedAbi.hpp"

using NKWebKit::Lifted::Read;
using NKWebKit::Lifted::Write;
using NKWebKit::Lifted::ReadBytes;
using NKWebKit::Lifted::WriteBytes;
using NKWebKit::Lifted::Bytes;
using NKWebKit::Lifted::ExtractBits;

extern "C" {

std::uint64_t APS5_VABI _ZN3JSC11FuzzerAgent13getPredictionEPNS_9CodeBlockERKNS_10CodeOriginEm(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) {
    (void)a2;
    (void)a1;
    (void)a0;
    return a3;
}

std::uint8_t APS5_VABI _ZN3JSC11MarkedBlock13areMarksStaleEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x3ed8);
    const std::uint32_t v2 = Read<std::uint32_t>(a0 + 0x3ef0);
    const std::uint32_t v3 = Read<std::uint32_t>(v1 + 0x168);
    return static_cast<std::uint8_t>(v2 != v3);
}

std::uint8_t APS5_VABI _ZN3JSC11MarkedBlock6Handle13areMarksStaleEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x58);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x3ed8);
    const std::uint32_t v3 = Read<std::uint32_t>(v1 + 0x3ef0);
    const std::uint32_t v4 = Read<std::uint32_t>(v2 + 0x168);
    return static_cast<std::uint8_t>(v3 != v4);
}

std::uint64_t APS5_VABI _ZN3JSC13RuntimeMethod15createPrototypeERNS_2VMERNS_14JSGlobalObjectE(std::uint64_t a0, std::uint64_t a1) {
    (void)a0;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x260);
    return v1;
}

std::uint64_t APS5_VABI _ZN3JSC13RuntimeMethod15subspaceForImplERNS_2VME(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x9a68);
    return (std::uint64_t{0x53b8} + v1);
}

std::uint32_t APS5_VABI _ZN3JSC20failNextNewCodeBlockEPNS_14JSGlobalObjectE(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x38);
    Write<std::uint8_t>(v1 + 0xc5d1, std::uint8_t{0x1});
    return std::uint32_t{0xa};
}

double APS5_VABI _ZN3JSC22EdenGCActivityCallback12lastGCLengthERNS_4HeapE(std::uint64_t a0, std::uint64_t a1) {
    (void)a0;
    const double v1 = Read<double>(a1 + 0x2a0);
    return v1;
}

double APS5_VABI _ZN3JSC22FullGCActivityCallback12lastGCLengthERNS_4HeapE(std::uint64_t a0, std::uint64_t a1) {
    (void)a0;
    const double v1 = Read<double>(a1 + 0x298);
    return v1;
}

std::uint64_t APS5_VABI _ZN3JSC6JSCell9getObjectEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x5);
    return ((v1 >= std::uint8_t{0x17}) ? a0 : std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN3JSC8Bindings13RuntimeObject15createPrototypeERNS_2VMERNS_14JSGlobalObjectE(std::uint64_t a0, std::uint64_t a1) {
    (void)a0;
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x258);
    return v1;
}

std::uint64_t APS5_VABI _ZN3JSC8Bindings13RuntimeObject15subspaceForImplERNS_2VME(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x9a68);
    return (std::uint64_t{0x54f8} + v1);
}

std::uint8_t APS5_VABI _ZN3JSC8Debugger10isAttachedEPNS_14JSGlobalObjectE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x6f8);
    return static_cast<std::uint8_t>(v1 == a0);
}

std::uint64_t APS5_VABI _ZN3WTF10StringView16GraphemeClusters8IteratorC1EOS2_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    return v1;
}

std::uint64_t APS5_VABI _ZN3WTF10StringView16GraphemeClusters8IteratorC2EOS2_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    return v1;
}

std::uint8_t APS5_VABI _ZN3WTF15AutomaticThread9isWaitingERKNS_14AbstractLockerE(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x2b);
    const std::uint8_t v3 = Read<std::uint8_t>(a0 + 0x2a);
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v3 != std::uint8_t{0x0}) & static_cast<std::uint8_t>(v1 != std::uint8_t{0x0}));
}

void APS5_VABI _ZN3WTF24AutomaticThreadConditionC1Ev(std::uint64_t a0) {
    Write<std::uint32_t>(a0, std::uint32_t{0x1});
    Write<std::uint8_t>(a0 + 0x4, std::uint8_t{0x0});
    WriteBytes<16>(a0 + 0x8, Bytes<16>{});
}

void APS5_VABI _ZN3WTF24AutomaticThreadConditionC2Ev(std::uint64_t a0) {
    Write<std::uint32_t>(a0, std::uint32_t{0x1});
    Write<std::uint8_t>(a0 + 0x4, std::uint8_t{0x0});
    WriteBytes<16>(a0 + 0x8, Bytes<16>{});
}

std::uint64_t APS5_VABI _ZN7WebCore10JSLocationC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    const std::uint64_t v6 = Read<std::uint64_t>(a1 + 0x20);
    Write<std::uint64_t>(a0 + 0x20, v6);
    return v6;
}

std::uint64_t APS5_VABI _ZN7WebCore10JSLocationC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    const std::uint64_t v6 = Read<std::uint64_t>(a1 + 0x20);
    Write<std::uint64_t>(a0 + 0x20, v6);
    return v6;
}

std::uint32_t APS5_VABI _ZN7WebCore11DisplayList20FillRectWithGradientC1ERKNS_9FloatRectERNS_8GradientE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x10);
    Write<std::uint32_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    Write<std::uint64_t>(a0 + 0x18, a2);
    const std::uint32_t v3 = Read<std::uint32_t>(a2);
    Write<std::uint32_t>(a2, (v3 + 1));
    return v1;
}

std::uint32_t APS5_VABI _ZN7WebCore11DisplayList20FillRectWithGradientC2ERKNS_9FloatRectERNS_8GradientE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x10);
    Write<std::uint32_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    Write<std::uint64_t>(a0 + 0x18, a2);
    const std::uint32_t v3 = Read<std::uint32_t>(a2);
    Write<std::uint32_t>(a2, (v3 + 1));
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCore11JSDOMMatrixC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore11JSDOMMatrixC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore11JSImageDataC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore11JSImageDataC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + 1));
    return v3;
}

std::uint32_t APS5_VABI _ZN7WebCore11SimpleRangeC1EONS_13BoundaryPointES2_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    const std::uint32_t v2 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x8, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a2);
    Write<std::uint64_t>(a2, std::uint64_t{0x0});
    Write<std::uint64_t>(a0 + 0x10, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(a2 + 0x8);
    Write<std::uint32_t>(a0 + 0x18, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore11SimpleRangeC1ERKNS_13BoundaryPointES3_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x18);
    Write<std::uint32_t>(v1 + 0x18, (v2 + std::uint32_t{0x2}));
    const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x8, v4);
    const std::uint64_t v5 = Read<std::uint64_t>(a2);
    Write<std::uint64_t>(a0 + 0x10, v5);
    const std::uint32_t v6 = Read<std::uint32_t>(v5 + 0x18);
    Write<std::uint32_t>(v5 + 0x18, (v6 + std::uint32_t{0x2}));
    const std::uint32_t v8 = Read<std::uint32_t>(a2 + 0x8);
    Write<std::uint32_t>(a0 + 0x18, v8);
    return v8;
}

std::uint32_t APS5_VABI _ZN7WebCore11SimpleRangeC2EONS_13BoundaryPointES2_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    const std::uint32_t v2 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x8, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a2);
    Write<std::uint64_t>(a2, std::uint64_t{0x0});
    Write<std::uint64_t>(a0 + 0x10, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(a2 + 0x8);
    Write<std::uint32_t>(a0 + 0x18, v4);
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore11SimpleRangeC2ERKNS_13BoundaryPointES3_(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x18);
    Write<std::uint32_t>(v1 + 0x18, (v2 + std::uint32_t{0x2}));
    const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x8, v4);
    const std::uint64_t v5 = Read<std::uint64_t>(a2);
    Write<std::uint64_t>(a0 + 0x10, v5);
    const std::uint32_t v6 = Read<std::uint32_t>(v5 + 0x18);
    Write<std::uint32_t>(v5 + 0x18, (v6 + std::uint32_t{0x2}));
    const std::uint32_t v8 = Read<std::uint32_t>(a2 + 0x8);
    Write<std::uint32_t>(a0 + 0x18, v8);
    return v8;
}

std::uint64_t APS5_VABI _ZN7WebCore12JSTimeRangesC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore12JSTimeRangesC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore12NamedNodeMap3refEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x18);
    Write<std::uint32_t>(v1 + 0x18, (v2 + std::uint32_t{0x2}));
    return v1;
}

void APS5_VABI _ZN7WebCore13GraphicsLayer12setZPositionEf(std::uint64_t a0, float x0) {
    Write<float>(a0 + 0x84, x0);
}

std::uint64_t APS5_VABI _ZN7WebCore13HitTestResultC1ERKNS_15HitTestLocationE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1 + 0x8);
    WriteBytes<16>(a0 + 0x8, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const Bytes<32> v4 = ReadBytes<32>(a1 + 0x20);
    WriteBytes<32>(a0 + 0x20, v4);
    const std::uint8_t v5 = Read<std::uint8_t>(a1 + 0x40);
    Write<std::uint8_t>(a0 + 0x40, v5);
    const std::uint8_t v6 = Read<std::uint8_t>(a1 + 0x41);
    Write<std::uint8_t>(a0 + 0x41, v6);
    WriteBytes<16>(a0 + 0x48, Bytes<16>{});
    Write<std::uint64_t>(a0 + 0x58, v1);
    Write<std::uint64_t>(a0 + 0x80, std::uint64_t{0x0});
    WriteBytes<16>(a0 + 0x60, Bytes<16>{});
    WriteBytes<16>(a0 + 0x69, Bytes<16>{});
    return v1;
}

void APS5_VABI _ZN7WebCore13HitTestResultC1Ev(std::uint64_t a0) {
    WriteBytes<32>(a0 + 0x20, Bytes<32>{});
    WriteBytes<32>(a0, Bytes<32>{});
    Write<std::uint16_t>(a0 + 0x40, std::uint16_t{0x100});
    Write<std::uint64_t>(a0 + 0x80, std::uint64_t{0x0});
    WriteBytes<32>(a0 + 0x48, Bytes<32>{});
    WriteBytes<32>(a0 + 0x59, Bytes<32>{});
}

std::uint64_t APS5_VABI _ZN7WebCore13HitTestResultC2ERKNS_15HitTestLocationE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1 + 0x8);
    WriteBytes<16>(a0 + 0x8, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const Bytes<32> v4 = ReadBytes<32>(a1 + 0x20);
    WriteBytes<32>(a0 + 0x20, v4);
    const std::uint8_t v5 = Read<std::uint8_t>(a1 + 0x40);
    Write<std::uint8_t>(a0 + 0x40, v5);
    const std::uint8_t v6 = Read<std::uint8_t>(a1 + 0x41);
    Write<std::uint8_t>(a0 + 0x41, v6);
    WriteBytes<16>(a0 + 0x48, Bytes<16>{});
    Write<std::uint64_t>(a0 + 0x58, v1);
    Write<std::uint64_t>(a0 + 0x80, std::uint64_t{0x0});
    WriteBytes<16>(a0 + 0x60, Bytes<16>{});
    WriteBytes<16>(a0 + 0x69, Bytes<16>{});
    return v1;
}

void APS5_VABI _ZN7WebCore13HitTestResultC2Ev(std::uint64_t a0) {
    WriteBytes<32>(a0 + 0x20, Bytes<32>{});
    WriteBytes<32>(a0, Bytes<32>{});
    Write<std::uint16_t>(a0 + 0x40, std::uint16_t{0x100});
    Write<std::uint64_t>(a0 + 0x80, std::uint64_t{0x0});
    WriteBytes<32>(a0 + 0x48, Bytes<32>{});
    WriteBytes<32>(a0 + 0x59, Bytes<32>{});
}

std::uint64_t APS5_VABI _ZN7WebCore13JSDOMRectListC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore13JSDOMRectListC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3);
    Write<std::uint32_t>(v3, (v4 + 1));
    return v3;
}

std::uint32_t APS5_VABI _ZN7WebCore14DocumentLoader21setCustomHeaderFieldsEON3WTF6VectorINS_18CustomHeaderFieldsELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xa10);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0xa10, v2);
    Write<std::uint64_t>(a1, v1);
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0xa18);
    const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0xa18, v4);
    Write<std::uint32_t>(a1 + 0x8, v3);
    const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0xa1c);
    const std::uint32_t v6 = Read<std::uint32_t>(a1 + 0xc);
    Write<std::uint32_t>(a0 + 0xa1c, v6);
    Write<std::uint32_t>(a1 + 0xc, v5);
    return v5;
}

std::uint64_t APS5_VABI _ZN7WebCore14ResourceHandle11clearClientEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    Write<std::uint64_t>(v1 + 0x8, std::uint64_t{0x0});
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCore14ResourceHandle12firstRequestEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    return (v1 + std::uint64_t{0x10});
}

void APS5_VABI _ZN7WebCore15GraphicsContext20endTransparencyLayerEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x198);
    Write<std::uint32_t>(a0 + 0x198, (v1 - 1));
}

void APS5_VABI _ZN7WebCore15GraphicsContext22beginTransparencyLayerEf(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x198);
    Write<std::uint32_t>(a0 + 0x198, (v1 + 1));
}

std::uint8_t APS5_VABI _ZN7WebCore15HitTestLocationC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1 + 0x8);
    WriteBytes<16>(a0 + 0x8, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const Bytes<32> v4 = ReadBytes<32>(a1 + 0x20);
    WriteBytes<32>(a0 + 0x20, v4);
    const std::uint8_t v5 = Read<std::uint8_t>(a1 + 0x40);
    Write<std::uint8_t>(a0 + 0x40, v5);
    const std::uint8_t v6 = Read<std::uint8_t>(a1 + 0x41);
    Write<std::uint8_t>(a0 + 0x41, v6);
    return v6;
}

void APS5_VABI _ZN7WebCore15HitTestLocationC1Ev(std::uint64_t a0) {
    WriteBytes<32>(a0 + 0x20, Bytes<32>{});
    WriteBytes<32>(a0, Bytes<32>{});
    Write<std::uint16_t>(a0 + 0x40, std::uint16_t{0x100});
}

std::uint8_t APS5_VABI _ZN7WebCore15HitTestLocationC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1 + 0x8);
    WriteBytes<16>(a0 + 0x8, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const Bytes<32> v4 = ReadBytes<32>(a1 + 0x20);
    WriteBytes<32>(a0 + 0x20, v4);
    const std::uint8_t v5 = Read<std::uint8_t>(a1 + 0x40);
    Write<std::uint8_t>(a0 + 0x40, v5);
    const std::uint8_t v6 = Read<std::uint8_t>(a1 + 0x41);
    Write<std::uint8_t>(a0 + 0x41, v6);
    return v6;
}

void APS5_VABI _ZN7WebCore15HitTestLocationC2Ev(std::uint64_t a0) {
    WriteBytes<32>(a0 + 0x20, Bytes<32>{});
    WriteBytes<32>(a0, Bytes<32>{});
    Write<std::uint16_t>(a0 + 0x40, std::uint16_t{0x100});
}

std::uint64_t APS5_VABI _ZN7WebCore15JSFetchResponseC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x18);
    Write<std::uint32_t>(v3 + 0x18, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore15JSFetchResponseC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x18);
    Write<std::uint32_t>(v3 + 0x18, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore16ScriptController11windowProxyEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x8);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x18);
    return v2;
}

std::uint8_t APS5_VABI _ZN7WebCore16VisibleSelectionC1Ev(std::uint64_t a0) {
    Write<std::uint64_t>(a0, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x8, std::uint32_t{0x0});
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0xc);
    Write<std::uint8_t>(a0 + 0xc, static_cast<std::uint8_t>(v1 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x18, std::uint32_t{0x0});
    const std::uint8_t v3 = Read<std::uint8_t>(a0 + 0x1c);
    Write<std::uint8_t>(a0 + 0x1c, static_cast<std::uint8_t>(v3 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x28, std::uint32_t{0x0});
    const std::uint8_t v5 = Read<std::uint8_t>(a0 + 0x2c);
    Write<std::uint8_t>(a0 + 0x2c, static_cast<std::uint8_t>(v5 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x38, std::uint32_t{0x0});
    const std::uint8_t v7 = Read<std::uint8_t>(a0 + 0x3c);
    Write<std::uint8_t>(a0 + 0x3c, static_cast<std::uint8_t>(v7 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x40, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x48, std::uint32_t{0x0});
    const std::uint8_t v9 = Read<std::uint8_t>(a0 + 0x4c);
    Write<std::uint8_t>(a0 + 0x4c, static_cast<std::uint8_t>(v9 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x50, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x58, std::uint32_t{0x0});
    const std::uint8_t v11 = Read<std::uint8_t>(a0 + 0x5c);
    Write<std::uint8_t>(a0 + 0x5c, static_cast<std::uint8_t>(v11 & std::uint8_t{0xf0}));
    Write<std::uint16_t>(a0 + 0x60, std::uint16_t{0x1});
    const std::uint8_t v13 = Read<std::uint8_t>(a0 + 0x62);
    const std::uint8_t v15 = static_cast<std::uint8_t>(static_cast<std::uint8_t>(v13 & std::uint8_t{0xfc}) | std::uint8_t{0x1});
    Write<std::uint8_t>(a0 + 0x62, v15);
    return v15;
}

std::uint8_t APS5_VABI _ZN7WebCore16VisibleSelectionC2Ev(std::uint64_t a0) {
    Write<std::uint64_t>(a0, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x8, std::uint32_t{0x0});
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0xc);
    Write<std::uint8_t>(a0 + 0xc, static_cast<std::uint8_t>(v1 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x18, std::uint32_t{0x0});
    const std::uint8_t v3 = Read<std::uint8_t>(a0 + 0x1c);
    Write<std::uint8_t>(a0 + 0x1c, static_cast<std::uint8_t>(v3 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x20, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x28, std::uint32_t{0x0});
    const std::uint8_t v5 = Read<std::uint8_t>(a0 + 0x2c);
    Write<std::uint8_t>(a0 + 0x2c, static_cast<std::uint8_t>(v5 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x30, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x38, std::uint32_t{0x0});
    const std::uint8_t v7 = Read<std::uint8_t>(a0 + 0x3c);
    Write<std::uint8_t>(a0 + 0x3c, static_cast<std::uint8_t>(v7 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x40, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x48, std::uint32_t{0x0});
    const std::uint8_t v9 = Read<std::uint8_t>(a0 + 0x4c);
    Write<std::uint8_t>(a0 + 0x4c, static_cast<std::uint8_t>(v9 & std::uint8_t{0xf0}));
    Write<std::uint64_t>(a0 + 0x50, std::uint64_t{0x0});
    Write<std::uint32_t>(a0 + 0x58, std::uint32_t{0x0});
    const std::uint8_t v11 = Read<std::uint8_t>(a0 + 0x5c);
    Write<std::uint8_t>(a0 + 0x5c, static_cast<std::uint8_t>(v11 & std::uint8_t{0xf0}));
    Write<std::uint16_t>(a0 + 0x60, std::uint16_t{0x1});
    const std::uint8_t v13 = Read<std::uint8_t>(a0 + 0x62);
    const std::uint8_t v15 = static_cast<std::uint8_t>(static_cast<std::uint8_t>(v13 & std::uint8_t{0xfc}) | std::uint8_t{0x1});
    Write<std::uint8_t>(a0 + 0x62, v15);
    return v15;
}

std::uint32_t APS5_VABI _ZN7WebCore17FrameLoaderClient11allowScriptEb(std::uint64_t a0, std::uint64_t a1) {
    (void)a0;
    return static_cast<std::uint32_t>(a1);
}

std::uint64_t APS5_VABI _ZN7WebCore17JSDOMRectReadOnlyC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore17JSDOMRectReadOnlyC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore17LibWebRTCProvideraSERKS0_(std::uint64_t a0) {
    return a0;
}

void APS5_VABI _ZN7WebCore18TextureMapperLayer10setOpacityEf(std::uint64_t a0, float x0) {
    Write<float>(a0 + 0x190, x0);
}

std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayer11setPositionERKNS_10FloatPointE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x60, v1);
    return v1;
}

void APS5_VABI _ZN7WebCore18TextureMapperLayer12setTransformERKNS_20TransformationMatrixE(std::uint64_t a0, std::uint64_t a1) {
    const Bytes<32> v1 = ReadBytes<32>(a1);
    const Bytes<32> v2 = ReadBytes<32>(a1 + 0x20);
    const Bytes<32> v3 = ReadBytes<32>(a1 + 0x40);
    const Bytes<32> v4 = ReadBytes<32>(a1 + 0x60);
    WriteBytes<32>(a0 + 0xf0, v4);
    WriteBytes<32>(a0 + 0xd0, v3);
    WriteBytes<32>(a0 + 0xb0, v2);
    WriteBytes<32>(a0 + 0x90, v1);
}

std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayer14setAnchorPointERKNS_12FloatPoint3DE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x70, v1);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x68, v2);
    return v2;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer14setPreserves3DEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x298);
    const std::uint32_t v3 = (v1 & std::uint32_t{0xfffffffe}) | static_cast<std::uint32_t>(a1);
    Write<std::uint16_t>(a0 + 0x298, static_cast<std::uint16_t>(v3));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayer15setBoundsOriginERKNS_10FloatPointE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x74, v1);
    return v1;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer15setContentsRectERKNS_9FloatRectE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x10);
    Write<std::uint32_t>(a0 + 0x1a4, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0 + 0x194, v2);
    return v1;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer15setDrawsContentEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x298);
    const std::uint32_t v3 = static_cast<std::uint32_t>(static_cast<std::uint64_t>((v1 & std::uint32_t{0xfffffffb})) + a1 * 4);
    Write<std::uint16_t>(a0 + 0x298, static_cast<std::uint16_t>(v3));
    return v3;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer16setMasksToBoundsEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x298);
    const std::uint32_t v3 = static_cast<std::uint32_t>(static_cast<std::uint64_t>((v1 & std::uint32_t{0xfffffffd})) + a1 * 2);
    Write<std::uint16_t>(a0 + 0x298, static_cast<std::uint16_t>(v3));
    return v3;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer17setContentsOpaqueEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x298);
    const std::uint32_t v4 = (v1 & std::uint32_t{0xffffffef}) | (static_cast<std::uint32_t>(a1) << 4);
    Write<std::uint16_t>(a0 + 0x298, static_cast<std::uint16_t>(v4));
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer17setRepaintCounterEbi(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x298);
    const std::uint32_t v4 = (std::uint32_t{0xfffffeff} & v1) | (static_cast<std::uint32_t>(a1) << 8);
    Write<std::uint16_t>(a0 + 0x298, static_cast<std::uint16_t>(v4));
    Write<std::uint32_t>(a0 + 0x294, static_cast<std::uint32_t>(a2));
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer18setContentsVisibleEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x298);
    const std::uint32_t v3 = static_cast<std::uint32_t>(static_cast<std::uint64_t>((v1 & std::uint32_t{0xfffffff7})) + a1 * 8);
    Write<std::uint16_t>(a0 + 0x298, static_cast<std::uint16_t>(v3));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayer19setContentsTileSizeERKNS_9FloatSizeE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x1b0, v1);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x1a8, v2);
    return v2;
}

void APS5_VABI _ZN7WebCore18TextureMapperLayer20setChildrenTransformERKNS_20TransformationMatrixE(std::uint64_t a0, std::uint64_t a1) {
    const Bytes<32> v1 = ReadBytes<32>(a1);
    const Bytes<32> v2 = ReadBytes<32>(a1 + 0x20);
    const Bytes<32> v3 = ReadBytes<32>(a1 + 0x40);
    const Bytes<32> v4 = ReadBytes<32>(a1 + 0x60);
    WriteBytes<32>(a0 + 0x170, v4);
    WriteBytes<32>(a0 + 0x150, v3);
    WriteBytes<32>(a0 + 0x130, v2);
    WriteBytes<32>(a0 + 0x110, v1);
}

std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayer20setContentsTilePhaseERKNS_9FloatSizeE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x1bc, v1);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x1b4, v2);
    return v2;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer21setBackfaceVisibilityEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x298);
    const std::uint32_t v4 = (v1 & std::uint32_t{0xffffffdf}) | (static_cast<std::uint32_t>(a1) << 5);
    Write<std::uint16_t>(a0 + 0x298, static_cast<std::uint16_t>(v4));
    return v4;
}

std::uint32_t APS5_VABI _ZN7WebCore18TextureMapperLayer23setContentsClippingRectERKNS_16FloatRoundedRectE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x40);
    Write<std::uint32_t>(a0 + 0x200, v1);
    const Bytes<32> v2 = ReadBytes<32>(a1);
    const Bytes<32> v3 = ReadBytes<32>(a1 + 0x20);
    WriteBytes<32>(a0 + 0x1e0, v3);
    WriteBytes<32>(a0 + 0x1c0, v2);
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCore18TextureMapperLayer7setSizeERKNS_9FloatSizeE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x84, v1);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x7c, v2);
    return v2;
}

std::uint64_t APS5_VABI _ZN7WebCore19JSAnimationTimelineC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore19JSAnimationTimelineC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore19JSDOMMatrixReadOnlyC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore19JSDOMMatrixReadOnlyC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC1Ev(std::uint64_t a0) {
    WriteBytes<16>(a0 + 0xc, Bytes<16>{});
    WriteBytes<16>(a0, Bytes<16>{});
}

void APS5_VABI _ZN7WebCore20PasteboardCustomData5EntryC2Ev(std::uint64_t a0) {
    WriteBytes<16>(a0 + 0xc, Bytes<16>{});
    WriteBytes<16>(a0, Bytes<16>{});
}

std::uint32_t APS5_VABI _ZN7WebCore20PasteboardCustomDataC1EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    WriteBytes<16>(a0 + 0x8, Bytes<16>{});
    const std::uint64_t v2 = Read<std::uint64_t>(a2);
    Write<std::uint64_t>(a0 + 0x8, v2);
    Write<std::uint64_t>(a2, std::uint64_t{0x0});
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0x10);
    const std::uint32_t v4 = Read<std::uint32_t>(a2 + 0x8);
    Write<std::uint32_t>(a0 + 0x10, v4);
    Write<std::uint32_t>(a2 + 0x8, v3);
    const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0x14);
    const std::uint32_t v6 = Read<std::uint32_t>(a2 + 0xc);
    Write<std::uint32_t>(a0 + 0x14, v6);
    Write<std::uint32_t>(a2 + 0xc, v5);
    return v5;
}

std::uint32_t APS5_VABI _ZN7WebCore20PasteboardCustomDataC1EOS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    WriteBytes<16>(a0 + 0x8, Bytes<16>{});
    const std::uint64_t v2 = Read<std::uint64_t>(a1 + 0x8);
    Write<std::uint64_t>(a0 + 0x8, v2);
    Write<std::uint64_t>(a1 + 0x8, std::uint64_t{0x0});
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0x10);
    const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x10);
    Write<std::uint32_t>(a0 + 0x10, v4);
    Write<std::uint32_t>(a1 + 0x10, v3);
    const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0x14);
    const std::uint32_t v6 = Read<std::uint32_t>(a1 + 0x14);
    Write<std::uint32_t>(a0 + 0x14, v6);
    Write<std::uint32_t>(a1 + 0x14, v5);
    return v5;
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC1Ev(std::uint64_t a0) {
    WriteBytes<16>(a0, Bytes<16>{});
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
}

std::uint32_t APS5_VABI _ZN7WebCore20PasteboardCustomDataC2EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    WriteBytes<16>(a0 + 0x8, Bytes<16>{});
    const std::uint64_t v2 = Read<std::uint64_t>(a2);
    Write<std::uint64_t>(a0 + 0x8, v2);
    Write<std::uint64_t>(a2, std::uint64_t{0x0});
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0x10);
    const std::uint32_t v4 = Read<std::uint32_t>(a2 + 0x8);
    Write<std::uint32_t>(a0 + 0x10, v4);
    Write<std::uint32_t>(a2 + 0x8, v3);
    const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0x14);
    const std::uint32_t v6 = Read<std::uint32_t>(a2 + 0xc);
    Write<std::uint32_t>(a0 + 0x14, v6);
    Write<std::uint32_t>(a2 + 0xc, v5);
    return v5;
}

std::uint32_t APS5_VABI _ZN7WebCore20PasteboardCustomDataC2EOS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0, v1);
    WriteBytes<16>(a0 + 0x8, Bytes<16>{});
    const std::uint64_t v2 = Read<std::uint64_t>(a1 + 0x8);
    Write<std::uint64_t>(a0 + 0x8, v2);
    Write<std::uint64_t>(a1 + 0x8, std::uint64_t{0x0});
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0x10);
    const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x10);
    Write<std::uint32_t>(a0 + 0x10, v4);
    Write<std::uint32_t>(a1 + 0x10, v3);
    const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0x14);
    const std::uint32_t v6 = Read<std::uint32_t>(a1 + 0x14);
    Write<std::uint32_t>(a0 + 0x14, v6);
    Write<std::uint32_t>(a1 + 0x14, v5);
    return v5;
}

void APS5_VABI _ZN7WebCore20PasteboardCustomDataC2Ev(std::uint64_t a0) {
    WriteBytes<16>(a0, Bytes<16>{});
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
}

void APS5_VABI _ZN7WebCore20ScrollingCoordinator13pageDestroyedEv(std::uint64_t a0) {
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
}

void APS5_VABI _ZN7WebCore21InspectorFrontendHost16disconnectClientEv(std::uint64_t a0) {
    WriteBytes<16>(a0 + 0x8, Bytes<16>{});
}

void APS5_VABI _ZN7WebCore21InspectorFrontendHostC1EPNS_23InspectorFrontendClientEPNS_4PageE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    Write<std::uint32_t>(a0, std::uint32_t{0x1});
    Write<std::uint64_t>(a0 + 0x8, a1);
    Write<std::uint64_t>(a0 + 0x10, a2);
}

void APS5_VABI _ZN7WebCore21InspectorFrontendHostC2EPNS_23InspectorFrontendClientEPNS_4PageE(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    Write<std::uint32_t>(a0, std::uint32_t{0x1});
    Write<std::uint64_t>(a0 + 0x8, a1);
    Write<std::uint64_t>(a0 + 0x10, a2);
}

void APS5_VABI _ZN7WebCore24CoordinatedGraphicsLayer21invalidateCoordinatorEv(std::uint64_t a0) {
    Write<std::uint64_t>(a0 + 0x520, std::uint64_t{0x0});
}

void APS5_VABI _ZN7WebCore26ContextDestructionObserver16contextDestroyedEv(std::uint64_t a0) {
    Write<std::uint64_t>(a0 + 0x8, std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore26MessagePortChannelRegistryC1EON3WTF8FunctionIFvRKNS_21MessagePortIdentifierENS1_16ObjectIdentifierINS_21ProcessIdentifierTypeEEEONS1_17CompletionHandlerIFvNS_26MessagePortChannelProvider11HasActivityEEEEEEE(std::uint64_t a0, std::uint64_t a1) {
    Write<std::uint64_t>(a0, std::uint64_t{0x0});
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0 + 0x8, v1);
    return v1;
}

std::uint64_t APS5_VABI _ZN7WebCore26MessagePortChannelRegistryC2EON3WTF8FunctionIFvRKNS_21MessagePortIdentifierENS1_16ObjectIdentifierINS_21ProcessIdentifierTypeEEEONS1_17CompletionHandlerIFvNS_26MessagePortChannelProvider11HasActivityEEEEEEE(std::uint64_t a0, std::uint64_t a1) {
    Write<std::uint64_t>(a0, std::uint64_t{0x0});
    const std::uint64_t v1 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a1, std::uint64_t{0x0});
    Write<std::uint64_t>(a0 + 0x8, v1);
    return v1;
}

void APS5_VABI _ZN7WebCore27CSSComputedStyleDeclaration3refEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x1c);
    Write<std::uint32_t>(a0 + 0x1c, (v1 + 1));
}

std::uint32_t APS5_VABI _ZN7WebCore27PlatformMediaSessionManager12restrictionsENS_20PlatformMediaSession9MediaTypeE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v2 = Read<std::uint32_t>(a0 + static_cast<std::uint64_t>(static_cast<std::uint32_t>(a1)) * 4 + 0x8);
    return v2;
}

std::uint32_t APS5_VABI _ZN7WebCore27PlatformMediaSessionManager14addRestrictionENS_20PlatformMediaSession9MediaTypeEj(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2) {
    const std::uint32_t v1 = static_cast<std::uint32_t>(a1);
    const std::uint32_t v2 = Read<std::uint32_t>(a0 + static_cast<std::uint64_t>(v1) * 4 + 0x8);
    Write<std::uint32_t>(a0 + static_cast<std::uint64_t>(v1) * 4 + 0x8, (v2 | static_cast<std::uint32_t>(a2)));
    return v1;
}

std::uint8_t APS5_VABI _ZN7WebCore28InspectorFrontendClientLocal11isUnderTestEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x148);
    return v2;
}

void APS5_VABI _ZN7WebCore4Page19addLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x218);
    Write<std::uint32_t>(a0 + 0x218, (v1 | static_cast<std::uint32_t>(a1)));
}

void APS5_VABI _ZN7WebCore4Page22removeLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v2 = Read<std::uint32_t>(a0 + 0x218);
    Write<std::uint32_t>(a0 + 0x218, (v2 & (~static_cast<std::uint32_t>(a1))));
}

std::uint32_t APS5_VABI _ZN7WebCore4Page24setCORSDisablingPatternsEON3WTF6VectorINS_21UserContentURLPatternELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x448);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    Write<std::uint64_t>(a0 + 0x448, v2);
    Write<std::uint64_t>(a1, v1);
    const std::uint32_t v3 = Read<std::uint32_t>(a0 + 0x450);
    const std::uint32_t v4 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x450, v4);
    Write<std::uint32_t>(a1 + 0x8, v3);
    const std::uint32_t v5 = Read<std::uint32_t>(a0 + 0x454);
    const std::uint32_t v6 = Read<std::uint32_t>(a1 + 0xc);
    Write<std::uint32_t>(a0 + 0x454, v6);
    Write<std::uint32_t>(a1 + 0xc, v5);
    return v5;
}

void APS5_VABI _ZN7WebCore4Page29startTrackingRenderingUpdatesEv(std::uint64_t a0) {
    Write<std::uint8_t>(a0 + 0x34c, std::uint8_t{0x1});
    Write<std::uint32_t>(a0 + 0x348, std::uint32_t{0x0});
}

void APS5_VABI _ZN7WebCore4Page34setInitialScaleIgnoringContentSizeEf(std::uint64_t a0, float x0) {
    Write<float>(a0 + 0x150, x0);
}

std::uint8_t APS5_VABI _ZN7WebCore4Page45setOutsideViewportThrottlingEnabledForTestingEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x3c9);
    const std::uint8_t v2 = static_cast<std::uint8_t>(static_cast<std::uint8_t>(a1) ^ std::uint8_t{0x1});
    const std::uint8_t v5 = static_cast<std::uint8_t>(v1 & std::uint8_t{0xfd});
    Write<std::uint8_t>(a0 + 0x3c9, static_cast<std::uint8_t>(static_cast<std::uint8_t>(v2 + v2) | v5));
    const std::uint8_t v8 = Read<std::uint8_t>(a0 + 0x3c8);
    Write<std::uint8_t>(a0 + 0x3c8, static_cast<std::uint8_t>(v8 & std::uint8_t{0xfd}));
    return v5;
}

void APS5_VABI _ZN7WebCore4PathC1Ev(std::uint64_t a0) {
    WriteBytes<16>(a0, Bytes<16>{});
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
    Write<std::uint8_t>(a0 + 0x18, std::uint8_t{0x1});
}

void APS5_VABI _ZN7WebCore4PathC2Ev(std::uint64_t a0) {
    WriteBytes<16>(a0, Bytes<16>{});
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
    Write<std::uint8_t>(a0 + 0x18, std::uint8_t{0x1});
}

void APS5_VABI _ZN7WebCore6Editor7CommandC1Ev(std::uint64_t a0) {
    Write<std::uint64_t>(a0, std::uint64_t{0x0});
    WriteBytes<16>(a0 + 0x10, Bytes<16>{});
}

void APS5_VABI _ZN7WebCore6Editor7CommandC2Ev(std::uint64_t a0) {
    Write<std::uint64_t>(a0, std::uint64_t{0x0});
    WriteBytes<16>(a0 + 0x10, Bytes<16>{});
}

std::uint64_t APS5_VABI _ZN7WebCore6JSFileC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x10);
    Write<std::uint32_t>(v3 + 0x10, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore6JSFileC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x10);
    Write<std::uint32_t>(v3 + 0x10, (v4 + 1));
    return v3;
}

void APS5_VABI _ZN7WebCore6Widget12setFrameRectERKNS_7IntRectE(std::uint64_t a0, std::uint64_t a1) {
    const Bytes<16> v1 = ReadBytes<16>(a1);
    WriteBytes<16>(a0 + 0x30, v1);
}

std::uint64_t APS5_VABI _ZN7WebCore7CSSRule10setCssTextERKN3WTF6StringE(std::uint64_t a0) {
    WriteBytes<16>(a0, Bytes<16>{});
    Write<std::uint64_t>(a0 + 0x10, std::uint64_t{0x0});
    Write<std::uint8_t>(a0 + 0x10, std::uint8_t{0x1});
    return a0;
}

std::uint64_t APS5_VABI _ZN7WebCore7JSRangeC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore7JSRangeC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint8_t APS5_VABI _ZN7WebCore8Document16setXMLStandaloneEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint8_t v1 = static_cast<std::uint8_t>(std::uint8_t{0x2} - static_cast<std::uint8_t>(a1));
    Write<std::uint8_t>(a0 + 0x4b8, v1);
    return v1;
}

void APS5_VABI _ZN7WebCore8Document25startTrackingStyleRecalcsEv(std::uint64_t a0) {
    Write<std::uint32_t>(a0 + 0x898, std::uint32_t{0x0});
}

std::uint64_t APS5_VABI _ZN7WebCore8JSDOMURLC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    const std::uint64_t v6 = Read<std::uint64_t>(a1 + 0x20);
    Write<std::uint64_t>(a0 + 0x20, v6);
    return v6;
}

std::uint64_t APS5_VABI _ZN7WebCore8JSDOMURLC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    const std::uint64_t v6 = Read<std::uint64_t>(a1 + 0x20);
    Write<std::uint64_t>(a0 + 0x20, v6);
    return v6;
}

std::uint64_t APS5_VABI _ZN7WebCore8JSPath2DC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x28);
    Write<std::uint32_t>(v3 + 0x28, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore8JSPath2DC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x28);
    Write<std::uint32_t>(v3 + 0x28, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore9JSDOMRectC1ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

std::uint64_t APS5_VABI _ZN7WebCore9JSDOMRectC2ERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x10);
    Write<std::uint64_t>(a0 + 0x10, v1);
    const Bytes<16> v2 = ReadBytes<16>(a1);
    WriteBytes<16>(a0, v2);
    const std::uint64_t v3 = Read<std::uint64_t>(a1 + 0x18);
    Write<std::uint64_t>(a0 + 0x18, v3);
    const std::uint32_t v4 = Read<std::uint32_t>(v3 + 0x8);
    Write<std::uint32_t>(v3 + 0x8, (v4 + 1));
    return v3;
}

void APS5_VABI _ZN9Inspector14ConsoleMessage14incrementCountEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x48);
    Write<std::uint32_t>(a0 + 0x48, (v1 + 1));
}

std::uint64_t APS5_VABI _ZN9Inspector22InspectorDebuggerAgent20setSuppressAllPausesEb(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x38);
    const std::uint32_t v3 = Read<std::uint32_t>(v1 + 0x28);
    Write<std::uint16_t>(v1 + 0x28, static_cast<std::uint16_t>(((std::uint32_t{0xffffff7f} & v3) | (static_cast<std::uint32_t>(a1) << 7))));
    return v1;
}

std::uint64_t APS5_VABI _ZN9Inspector22RemoteInspectionTargetaSERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x8, v1);
    const std::uint8_t v2 = Read<std::uint8_t>(a1 + 0xc);
    Write<std::uint8_t>(a0 + 0xc, v2);
    return a0;
}

std::uint64_t APS5_VABI _ZN9Inspector24RemoteControllableTargetaSERKS0_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint32_t v1 = Read<std::uint32_t>(a1 + 0x8);
    Write<std::uint32_t>(a0 + 0x8, v1);
    return a0;
}

std::uint32_t APS5_VABI _ZNK3JSC4Yarr17RegularExpression13matchedLengthEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x10);
    return v2;
}

std::uint8_t APS5_VABI _ZNK3WTF10StringView16GraphemeClusters8IteratoreqERKS2_(std::uint64_t a0, std::uint64_t a1) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(a1);
    const std::uint32_t v3 = Read<std::uint32_t>(v1 + 0x20);
    const std::uint32_t v4 = Read<std::uint32_t>(v2 + 0x20);
    return static_cast<std::uint8_t>(v3 == v4);
}

std::uint64_t APS5_VABI _ZNK7WebCore11FrameLoader20activeDocumentLoaderEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x50);
    const std::uint64_t v3 = Read<std::uint64_t>(a0 + static_cast<std::uint64_t>(static_cast<std::uint8_t>(v1 == std::uint8_t{0x0})) * 8 + 0x58);
    return v3;
}

double APS5_VABI _ZNK7WebCore11MediaPlayer6volumeEv(std::uint64_t a0) {
    const double v1 = Read<double>(a0 + 0xc8);
    return v1;
}

std::uint8_t APS5_VABI _ZNK7WebCore11MessagePort18isLocallyReachableEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x73);
    return static_cast<std::uint8_t>(v1 == std::uint8_t{0x0});
}

std::uint64_t APS5_VABI _ZNK7WebCore11RenderStyle10lineHeightEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x38);
    return (v1 + std::uint64_t{0xc});
}

std::uint64_t APS5_VABI _ZNK7WebCore11RenderStyle11fontCascadeEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x38);
    return (v1 + std::uint64_t{0x18});
}

std::uint64_t APS5_VABI _ZNK7WebCore11RenderStyle15fontDescriptionEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x38);
    return (v1 + std::uint64_t{0x20});
}

std::uint64_t APS5_VABI _ZNK7WebCore14ResourceHandle6clientEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore16HTMLImageElement11cachedImageEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x80);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x18);
    return v2;
}

std::uint32_t APS5_VABI _ZNK7WebCore16HTMLImageElement36pendingDecodePromisesCountForTestingEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x80);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0x6c);
    return v2;
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLImageElement8completeEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x80);
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x70);
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v2 & std::uint8_t{0x8}) >> 3);
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement10isURLFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x80000});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement11isDateFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x8});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement11isTextFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>((v2 & std::uint32_t{0x2a8c20}) != 0);
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement11isTimeFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x40000});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement11isWeekFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x100000});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement12isEmailFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x20});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement12isFileUploadEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x40});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement12isMonthFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x200});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement13isNumberFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x400});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement13isSearchFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x8000});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement15isPasswordFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x800});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement16isTelephoneFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x20000});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement20isDateTimeLocalFieldEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>(v2 == std::uint32_t{0x10});
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLInputElement6isTextEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0xf8);
    const std::uint32_t v2 = Read<std::uint32_t>(v1 + 0xc);
    return static_cast<std::uint8_t>((v2 & std::uint32_t{0x2a8820}) != 0);
}

double APS5_VABI _ZNK7WebCore16HTMLMediaElement12playbackRateEv(std::uint64_t a0) {
    const double v1 = Read<double>(a0 + 0x2c8);
    return v1;
}

std::uint8_t APS5_VABI _ZNK7WebCore16HTMLMediaElement27webkitClosedCaptionsVisibleEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x421);
    return static_cast<std::uint8_t>(((~v1) & std::uint32_t{0x60000}) == 0);
}

std::uint32_t APS5_VABI _ZNK7WebCore16HTMLMediaElement6pausedEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x421);
    return ExtractBits(v1, std::uint32_t{0x10a});
}

double APS5_VABI _ZNK7WebCore16HTMLMediaElement6volumeEv(std::uint64_t a0) {
    const double v1 = Read<double>(a0 + 0x338);
    return v1;
}

std::uint32_t APS5_VABI _ZNK7WebCore16HTMLMediaElement7seekingEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x421);
    return ExtractBits(v1, std::uint32_t{0x10b});
}

std::uint64_t APS5_VABI _ZNK7WebCore16JSXMLHttpRequest7wrappedEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    return (v1 + std::uint64_t{0xffffffffffffffd8});
}

std::uint8_t APS5_VABI _ZNK7WebCore17JSDOMGlobalObject13worldIsNormalEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x930);
    return static_cast<std::uint8_t>(v1 != std::uint8_t{0x0});
}

std::uint32_t APS5_VABI _ZNK7WebCore17MouseRelatedEvent1xEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x60);
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(((static_cast<std::int32_t>(v1) >= 0) ? v1 : static_cast<std::uint32_t>(static_cast<std::uint64_t>(v1) + 0x3f))) >> 6);
}

std::uint32_t APS5_VABI _ZNK7WebCore17MouseRelatedEvent1yEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x64);
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(((static_cast<std::int32_t>(v1) >= 0) ? v1 : static_cast<std::uint32_t>(static_cast<std::uint64_t>(v1) + 0x3f))) >> 6);
}

std::uint8_t APS5_VABI _ZNK7WebCore18TextureMapperLayer10hasFiltersEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x54);
    return static_cast<std::uint8_t>(v1 != std::uint32_t{0x0});
}

std::uint8_t APS5_VABI _ZNK7WebCore18TextureMapperLayer12drawsContentEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x298);
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v1 & std::uint8_t{0x4}) >> 2);
}

std::uint8_t APS5_VABI _ZNK7WebCore18TextureMapperLayer18contentsAreVisibleEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x298);
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v1 & std::uint8_t{0x8}) >> 3);
}

float APS5_VABI _ZNK7WebCore18TextureMapperLayer7opacityEv(std::uint64_t a0) {
    const float v1 = Read<float>(a0 + 0x190);
    return v1;
}

std::uint64_t APS5_VABI _ZNK7WebCore18TextureMapperLayer9layerRectEv(std::uint64_t a0, std::uint64_t a1) {
    Write<std::uint64_t>(a0, std::uint64_t{0x0});
    const std::uint64_t v1 = Read<std::uint64_t>(a1 + 0x7c);
    Write<std::uint64_t>(a0 + 0x8, v1);
    const std::uint32_t v2 = Read<std::uint32_t>(a1 + 0x84);
    Write<std::uint32_t>(a0 + 0x10, v2);
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore18TextureMapperLayer9transformEv(std::uint64_t a0, std::uint64_t a1) {
    const Bytes<32> v1 = ReadBytes<32>(a1 + 0x90);
    const Bytes<32> v2 = ReadBytes<32>(a1 + 0xb0);
    const Bytes<32> v3 = ReadBytes<32>(a1 + 0xd0);
    const Bytes<32> v4 = ReadBytes<32>(a1 + 0xf0);
    WriteBytes<32>(a0 + 0x60, v4);
    WriteBytes<32>(a0 + 0x40, v3);
    WriteBytes<32>(a0 + 0x20, v2);
    WriteBytes<32>(a0, v1);
    return a0;
}

std::uint64_t APS5_VABI _ZNK7WebCore21RenderLayerCompositor15rootRenderLayerEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x10);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0xa0);
    return v2;
}

std::uint8_t APS5_VABI _ZNK7WebCore23FrameLoaderStateMachine15firstLayoutDoneEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0);
    return static_cast<std::uint8_t>(v1 == std::uint32_t{0x4});
}

std::uint8_t APS5_VABI _ZNK7WebCore23FrameLoaderStateMachine23committingFirstRealLoadEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0);
    return static_cast<std::uint8_t>(v1 == std::uint32_t{0x1});
}

std::uint8_t APS5_VABI _ZNK7WebCore23FrameLoaderStateMachine28creatingInitialEmptyDocumentEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0);
    return static_cast<std::uint8_t>(v1 == std::uint32_t{0x0});
}

std::uint8_t APS5_VABI _ZNK7WebCore23FrameLoaderStateMachine32isDisplayingInitialEmptyDocumentEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0);
    return static_cast<std::uint8_t>((v1 - 1) < std::uint32_t{0x2});
}

std::uint64_t APS5_VABI _ZNK7WebCore24CachedResourceHandleBase3getEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    return v1;
}

std::uint64_t APS5_VABI _ZNK7WebCore24CachedResourceHandleBasecvMS0_PNS_14CachedResourceEEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    return ((v1 < std::uint64_t{0x1}) ? ~std::uint64_t{0} : std::uint64_t{0});
}

std::uint8_t APS5_VABI _ZNK7WebCore24CachedResourceHandleBasentEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    return static_cast<std::uint8_t>(v1 == std::uint64_t{0x0});
}

std::uint8_t APS5_VABI _ZNK7WebCore24CoordinatedGraphicsLayer17usesContentsLayerEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x870);
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x7c0);
    return static_cast<std::uint8_t>((v1 | v2) != 0);
}

std::uint8_t APS5_VABI _ZNK7WebCore25BackForwardItemIdentifier7isValidEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    const std::uint64_t v2 = Read<std::uint64_t>(a0 + 0x8);
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v1 != std::uint64_t{0xffffffffffffffff}) & static_cast<std::uint8_t>((v2 | v1) != 0));
}

std::uint8_t APS5_VABI _ZNK7WebCore26HTMLTextFormControlElement26isInnerTextElementEditableEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0xad);
    return static_cast<std::uint8_t>((static_cast<std::uint8_t>(v1 & std::uint8_t{0x13})) == 0);
}

std::uint8_t APS5_VABI _ZNK7WebCore28InspectorFrontendClientLocal28userInterfaceLayoutDirectionEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x18);
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x3b2);
    return v2;
}

std::uint64_t APS5_VABI _ZNK7WebCore4Node13ownerDocumentEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    const std::uint64_t v2 = Read<std::uint64_t>(v1 + 0x8);
    return ((v2 != a0) ? v2 : std::uint64_t{0x0});
}

std::uint32_t APS5_VABI _ZNK7WebCore4Page15visibilityStateEv(std::uint64_t a0) {
    const std::uint32_t v1 = Read<std::uint32_t>(a0 + 0x214);
    return ExtractBits(v1, std::uint32_t{0x102});
}

std::uint8_t APS5_VABI _ZNK7WebCore4Page23isMonitoringWheelEventsEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x328);
    return static_cast<std::uint8_t>(v1 != std::uint64_t{0x0});
}

std::uint8_t APS5_VABI _ZNK7WebCore4Page23remoteInspectionAllowedEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x2a0);
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0xc);
    return v2;
}

std::uint8_t APS5_VABI _ZNK7WebCore4Path6isNullEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0);
    return static_cast<std::uint8_t>(v1 == std::uint64_t{0x0});
}

std::uint64_t APS5_VABI _ZNK7WebCore6JSNode8nodeTypeERN3JSC14JSGlobalObjectE(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x5);
    return (std::uint64_t{0xfffe000000000000} | static_cast<std::uint64_t>((static_cast<std::uint32_t>(v1) & std::uint32_t{0xf})));
}

std::uint64_t APS5_VABI _ZNK7WebCore9DOMWindow8documentEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x78);
    return ((v1 == 0) ? v1 : (v1 - 0xb0));
}

std::uint8_t APS5_VABI _ZNK7WebCore9FrameView14didFirstLayoutEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0 + 0x1a1);
    return static_cast<std::uint8_t>(v1 == std::uint8_t{0x0});
}

std::uint8_t APS5_VABI _ZNK7WebCore9SampleMap5emptyEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x28);
    return static_cast<std::uint8_t>(v1 == std::uint64_t{0x0});
}

std::uint8_t APS5_VABI _ZNK9Inspector14ConsoleMessage6sourceEv(std::uint64_t a0) {
    const std::uint8_t v1 = Read<std::uint8_t>(a0);
    return v1;
}

std::uint8_t APS5_VABI _ZNK9Inspector22InspectorDebuggerAgent17breakpointsActiveEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x38);
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x28);
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v2 & std::uint8_t{0x20}) >> 5);
}

std::uint8_t APS5_VABI _ZNK9Inspector22InspectorDebuggerAgent8isPausedEv(std::uint64_t a0) {
    const std::uint64_t v1 = Read<std::uint64_t>(a0 + 0x38);
    const std::uint8_t v2 = Read<std::uint8_t>(v1 + 0x28);
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(v2 & std::uint8_t{0x10}) >> 4);
}

}
