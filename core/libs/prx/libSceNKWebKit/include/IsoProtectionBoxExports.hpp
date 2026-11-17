#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/IsoBoxParsing.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::IsoAbi {

struct OriginalFormatBox {
    Box box;
    std::uint32_t dataFormat;
};

struct SchemeTypeBox {
    FullBox fullBox;
    std::uint32_t schemeType;
    std::uint32_t schemeVersion;
};

static_assert(sizeof(OriginalFormatBox) == 48);
static_assert(offsetof(OriginalFormatBox, dataFormat) == 40);
static_assert(sizeof(SchemeTypeBox) == 56);
static_assert(offsetof(SchemeTypeBox, schemeType) == 48);
static_assert(offsetof(SchemeTypeBox, schemeVersion) == 52);

}  // namespace NKWebKit::IsoAbi

namespace NKWebKit::IsoParse {

const Vtable* OriginalFormatBoxVtable();
const Vtable* SchemeTypeBoxVtable();
const Vtable* TrackEncryptionBoxVtable();

}  // namespace NKWebKit::IsoParse

extern "C" {

void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxC1Ev(NKWebKit::IsoAbi::OriginalFormatBox* self);
void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxC2Ev(NKWebKit::IsoAbi::OriginalFormatBox* self);
void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxC1ERKS0_(NKWebKit::IsoAbi::OriginalFormatBox* self, const NKWebKit::IsoAbi::OriginalFormatBox* other);
void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxC2ERKS0_(NKWebKit::IsoAbi::OriginalFormatBox* self, const NKWebKit::IsoAbi::OriginalFormatBox* other);
void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxD0Ev(NKWebKit::IsoAbi::OriginalFormatBox* self);
void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxD1Ev(NKWebKit::IsoAbi::OriginalFormatBox* self);
void APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxD2Ev(NKWebKit::IsoAbi::OriginalFormatBox* self);
NKWebKit::IsoAbi::OriginalFormatBox* APS5_VABI _ZN7WebCore20ISOOriginalFormatBoxaSERKS0_(NKWebKit::IsoAbi::OriginalFormatBox* self, const NKWebKit::IsoAbi::OriginalFormatBox* other);
bool APS5_VABI _ZN7WebCore20ISOOriginalFormatBox5parseERN3JSC8DataViewERj(NKWebKit::IsoAbi::OriginalFormatBox* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);

void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxC1Ev(NKWebKit::IsoAbi::SchemeTypeBox* self);
void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxC2Ev(NKWebKit::IsoAbi::SchemeTypeBox* self);
void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxC1ERKS0_(NKWebKit::IsoAbi::SchemeTypeBox* self, const NKWebKit::IsoAbi::SchemeTypeBox* other);
void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxC2ERKS0_(NKWebKit::IsoAbi::SchemeTypeBox* self, const NKWebKit::IsoAbi::SchemeTypeBox* other);
void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxD0Ev(NKWebKit::IsoAbi::SchemeTypeBox* self);
void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxD1Ev(NKWebKit::IsoAbi::SchemeTypeBox* self);
void APS5_VABI _ZN7WebCore16ISOSchemeTypeBoxD2Ev(NKWebKit::IsoAbi::SchemeTypeBox* self);
bool APS5_VABI _ZN7WebCore16ISOSchemeTypeBox5parseERN3JSC8DataViewERj(NKWebKit::IsoAbi::SchemeTypeBox* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);

void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxC1Ev(NKWebKit::IsoAbi::TrackEncryptionBox* self);
void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxC2Ev(NKWebKit::IsoAbi::TrackEncryptionBox* self);
void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxC1ERKS0_(NKWebKit::IsoAbi::TrackEncryptionBox* self, const NKWebKit::IsoAbi::TrackEncryptionBox* other);
void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxC2ERKS0_(NKWebKit::IsoAbi::TrackEncryptionBox* self, const NKWebKit::IsoAbi::TrackEncryptionBox* other);
void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxD0Ev(NKWebKit::IsoAbi::TrackEncryptionBox* self);
void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxD1Ev(NKWebKit::IsoAbi::TrackEncryptionBox* self);
void APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxD2Ev(NKWebKit::IsoAbi::TrackEncryptionBox* self);
NKWebKit::IsoAbi::TrackEncryptionBox* APS5_VABI _ZN7WebCore21ISOTrackEncryptionBoxaSERKS0_(NKWebKit::IsoAbi::TrackEncryptionBox* self, const NKWebKit::IsoAbi::TrackEncryptionBox* other);
bool APS5_VABI _ZN7WebCore21ISOTrackEncryptionBox5parseERN3JSC8DataViewERj(NKWebKit::IsoAbi::TrackEncryptionBox* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);

}
