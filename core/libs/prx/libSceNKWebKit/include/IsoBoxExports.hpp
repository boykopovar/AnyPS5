#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/IsoBoxAbi.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {

void APS5_VABI _ZN7WebCore12ISOWebVTTCueC1ERKS0_(NKWebKit::IsoAbi::WebVTTCue* self, const NKWebKit::IsoAbi::WebVTTCue* source);
void APS5_VABI _ZN7WebCore12ISOWebVTTCueC2ERKS0_(NKWebKit::IsoAbi::WebVTTCue* self, const NKWebKit::IsoAbi::WebVTTCue* source);
void APS5_VABI _ZN7WebCore12ISOWebVTTCueC1ERKN3WTF9MediaTimeES4_(NKWebKit::IsoAbi::WebVTTCue* self, const NKWebKit::MediaAbi::MediaTime* presentationTime, const NKWebKit::MediaAbi::MediaTime* duration);
void APS5_VABI _ZN7WebCore12ISOWebVTTCueC2ERKN3WTF9MediaTimeES4_(NKWebKit::IsoAbi::WebVTTCue* self, const NKWebKit::MediaAbi::MediaTime* presentationTime, const NKWebKit::MediaAbi::MediaTime* duration);
void APS5_VABI _ZN7WebCore12ISOWebVTTCueC1EON3WTF9MediaTimeES3_ONS1_6StringES5_S5_S5_S5_(NKWebKit::IsoAbi::WebVTTCue* self,
    NKWebKit::MediaAbi::MediaTime* presentationTime, NKWebKit::MediaAbi::MediaTime* duration,
    NKWebKit::WtfAbi::String* sourceID, NKWebKit::WtfAbi::String* identifier,
    NKWebKit::WtfAbi::String* originalStartTime, NKWebKit::WtfAbi::String* settings, NKWebKit::WtfAbi::String* cueText);
void APS5_VABI _ZN7WebCore12ISOWebVTTCueC2EON3WTF9MediaTimeES3_ONS1_6StringES5_S5_S5_S5_(NKWebKit::IsoAbi::WebVTTCue* self,
    NKWebKit::MediaAbi::MediaTime* presentationTime, NKWebKit::MediaAbi::MediaTime* duration,
    NKWebKit::WtfAbi::String* sourceID, NKWebKit::WtfAbi::String* identifier,
    NKWebKit::WtfAbi::String* originalStartTime, NKWebKit::WtfAbi::String* settings, NKWebKit::WtfAbi::String* cueText);
NKWebKit::IsoAbi::WebVTTCue* APS5_VABI _ZN7WebCore12ISOWebVTTCueaSERKS0_(NKWebKit::IsoAbi::WebVTTCue* self, const NKWebKit::IsoAbi::WebVTTCue* source);
void APS5_VABI _ZN7WebCore12ISOWebVTTCueD1Ev(NKWebKit::IsoAbi::WebVTTCue* self);
void APS5_VABI _ZN7WebCore12ISOWebVTTCueD2Ev(NKWebKit::IsoAbi::WebVTTCue* self);

std::uint64_t APS5_VABI _ZNK7WebCore6ISOBox4sizeEv(const NKWebKit::IsoAbi::Box* self);
const std::uint32_t* APS5_VABI _ZNK7WebCore6ISOBox7boxTypeEv(const NKWebKit::IsoAbi::Box* self);
const NKWebKit::IsoAbi::ByteVector& APS5_VABI _ZNK7WebCore6ISOBox12extendedTypeEv(const NKWebKit::IsoAbi::Box* self);
std::uint8_t APS5_VABI _ZNK7WebCore10ISOFullBox7versionEv(const NKWebKit::IsoAbi::FullBox* self);
std::uint32_t APS5_VABI _ZNK7WebCore10ISOFullBox5flagsEv(const NKWebKit::IsoAbi::FullBox* self);
NKWebKit::IsoAbi::LegacyOptionalInt8* APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox21defaultCryptByteBlockEv(NKWebKit::IsoAbi::LegacyOptionalInt8* result, const NKWebKit::IsoAbi::TrackEncryptionBox* self);
NKWebKit::IsoAbi::LegacyOptionalInt8* APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox20defaultSkipByteBlockEv(NKWebKit::IsoAbi::LegacyOptionalInt8* result, const NKWebKit::IsoAbi::TrackEncryptionBox* self);
std::int8_t APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox18defaultIsProtectedEv(const NKWebKit::IsoAbi::TrackEncryptionBox* self);
std::int8_t APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox22defaultPerSampleIVSizeEv(const NKWebKit::IsoAbi::TrackEncryptionBox* self);
NKWebKit::IsoAbi::ByteVector APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox10defaultKIDEv(const NKWebKit::IsoAbi::TrackEncryptionBox* self);
NKWebKit::IsoAbi::ByteVector APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox17defaultConstantIVEv(const NKWebKit::IsoAbi::TrackEncryptionBox* self);
NKWebKit::IsoAbi::ByteVector APS5_VABI _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox8systemIDEv(const NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self);
NKWebKit::WtfAbi::Vector<NKWebKit::IsoAbi::ByteVector> APS5_VABI _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox6keyIDsEv(const NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self);
NKWebKit::IsoAbi::ByteVector APS5_VABI _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox4dataEv(const NKWebKit::IsoAbi::ProtectionSystemSpecificHeaderBox* self);
const NKWebKit::MediaAbi::MediaTime& APS5_VABI _ZNK7WebCore12ISOWebVTTCue16presentationTimeEv(const NKWebKit::IsoAbi::WebVTTCue* self);
const NKWebKit::MediaAbi::MediaTime& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8durationEv(const NKWebKit::IsoAbi::WebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8sourceIDEv(const NKWebKit::IsoAbi::WebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue2idEv(const NKWebKit::IsoAbi::WebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue17originalStartTimeEv(const NKWebKit::IsoAbi::WebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8settingsEv(const NKWebKit::IsoAbi::WebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue7cueTextEv(const NKWebKit::IsoAbi::WebVTTCue* self);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(const NKWebKit::IsoAbi::WebVTTCue* self);

}
