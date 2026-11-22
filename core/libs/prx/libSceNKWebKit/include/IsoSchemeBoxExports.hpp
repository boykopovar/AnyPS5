#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/IsoProtectionBoxExports.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::IsoAbi {

struct SchemeInformationBox {
    Box box;
    Box* schemeSpecificData;
};

struct ProtectionSchemeInfoBox {
    Box box;
    OriginalFormatBox originalFormatBox;
    SchemeTypeBox* schemeTypeBox;
    SchemeInformationBox* schemeInformationBox;
};

struct OptionalByteVector {
    ByteVector value;
    bool engaged;
};

static_assert(sizeof(SchemeInformationBox) == 48);
static_assert(offsetof(SchemeInformationBox, schemeSpecificData) == 40);
static_assert(sizeof(ProtectionSchemeInfoBox) == 104);
static_assert(offsetof(ProtectionSchemeInfoBox, originalFormatBox) == 40);
static_assert(offsetof(ProtectionSchemeInfoBox, schemeTypeBox) == 88);
static_assert(offsetof(ProtectionSchemeInfoBox, schemeInformationBox) == 96);
static_assert(sizeof(OptionalByteVector) == 24);
static_assert(offsetof(OptionalByteVector, engaged) == 16);

}  // namespace NKWebKit::IsoAbi

extern "C" {

void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxC1Ev(NKWebKit::IsoAbi::SchemeInformationBox* self);
void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxC2Ev(NKWebKit::IsoAbi::SchemeInformationBox* self);
void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxD0Ev(NKWebKit::IsoAbi::SchemeInformationBox* self);
void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxD1Ev(NKWebKit::IsoAbi::SchemeInformationBox* self);
void APS5_VABI _ZN7WebCore23ISOSchemeInformationBoxD2Ev(NKWebKit::IsoAbi::SchemeInformationBox* self);
bool APS5_VABI _ZN7WebCore23ISOSchemeInformationBox5parseERN3JSC8DataViewERj(NKWebKit::IsoAbi::SchemeInformationBox* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);

void APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBoxC1Ev(NKWebKit::IsoAbi::ProtectionSchemeInfoBox* self);
void APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBoxC2Ev(NKWebKit::IsoAbi::ProtectionSchemeInfoBox* self);
void APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBoxD0Ev(NKWebKit::IsoAbi::ProtectionSchemeInfoBox* self);
void APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBoxD2Ev(NKWebKit::IsoAbi::ProtectionSchemeInfoBox* self);
bool APS5_VABI _ZN7WebCore26ISOProtectionSchemeInfoBox5parseERN3JSC8DataViewERj(NKWebKit::IsoAbi::ProtectionSchemeInfoBox* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);

void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC1Ev(NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self);
void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC2Ev(NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self);
void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC2ERKS0_(NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self, const NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* other);
void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD0Ev(NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self);
void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD1Ev(NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self);
void APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxD2Ev(NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self);
bool APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBox5parseERN3JSC8DataViewERj(NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);
NKWebKit::IsoAbi::OptionalByteVector APS5_VABI _ZN7WebCore36ISOProtectionSystemSpecificHeaderBox12peekSystemIDERN3JSC8DataViewEj(NKWebKit::IsoParse::DataView* view, unsigned offset);

}
