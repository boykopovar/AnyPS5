#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/IsoBoxParsing.hpp"

#include <cstddef>

extern "C" {

void* APS5_VABI _ZN7WebCore6ISOBoxnwEm10NotNullTagPv(std::size_t size, int tag, void* location);
void APS5_VABI _ZN7WebCore6ISOBoxC1Ev(NKWebKit::IsoAbi::Box* self);
void APS5_VABI _ZN7WebCore6ISOBoxC2Ev(NKWebKit::IsoAbi::Box* self);
void APS5_VABI _ZN7WebCore6ISOBoxC1ERKS0_(NKWebKit::IsoAbi::Box* self, const NKWebKit::IsoAbi::Box* other);
void APS5_VABI _ZN7WebCore6ISOBoxC2ERKS0_(NKWebKit::IsoAbi::Box* self, const NKWebKit::IsoAbi::Box* other);
void APS5_VABI _ZN7WebCore6ISOBoxD0Ev(NKWebKit::IsoAbi::Box* self);
void APS5_VABI _ZN7WebCore6ISOBoxD1Ev(NKWebKit::IsoAbi::Box* self);
void APS5_VABI _ZN7WebCore6ISOBoxD2Ev(NKWebKit::IsoAbi::Box* self);
bool APS5_VABI _ZN7WebCore6ISOBox4readERN3JSC8DataViewE(NKWebKit::IsoAbi::Box* self, NKWebKit::IsoParse::DataView* view);
bool APS5_VABI _ZN7WebCore6ISOBox4readERN3JSC8DataViewERj(NKWebKit::IsoAbi::Box* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);
bool APS5_VABI _ZN7WebCore6ISOBox5parseERN3JSC8DataViewERj(NKWebKit::IsoAbi::Box* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);
NKWebKit::IsoParse::PeekResult APS5_VABI _ZN7WebCore6ISOBox7peekBoxERN3JSC8DataViewEj(NKWebKit::IsoParse::DataView* view, unsigned offset);

void APS5_VABI _ZN7WebCore10ISOFullBoxC1Ev(NKWebKit::IsoAbi::FullBox* self);
void APS5_VABI _ZN7WebCore10ISOFullBoxC2Ev(NKWebKit::IsoAbi::FullBox* self);
void APS5_VABI _ZN7WebCore10ISOFullBoxC1ERKS0_(NKWebKit::IsoAbi::FullBox* self, const NKWebKit::IsoAbi::FullBox* other);
void APS5_VABI _ZN7WebCore10ISOFullBoxC2ERKS0_(NKWebKit::IsoAbi::FullBox* self, const NKWebKit::IsoAbi::FullBox* other);
void APS5_VABI _ZN7WebCore10ISOFullBoxD0Ev(NKWebKit::IsoAbi::FullBox* self);
void APS5_VABI _ZN7WebCore10ISOFullBoxD1Ev(NKWebKit::IsoAbi::FullBox* self);
void APS5_VABI _ZN7WebCore10ISOFullBoxD2Ev(NKWebKit::IsoAbi::FullBox* self);
NKWebKit::IsoAbi::FullBox* APS5_VABI _ZN7WebCore10ISOFullBoxaSERKS0_(NKWebKit::IsoAbi::FullBox* self, const NKWebKit::IsoAbi::FullBox* other);
bool APS5_VABI _ZN7WebCore10ISOFullBox5parseERN3JSC8DataViewERj(NKWebKit::IsoAbi::FullBox* self, NKWebKit::IsoParse::DataView* view, unsigned* offset);

}
