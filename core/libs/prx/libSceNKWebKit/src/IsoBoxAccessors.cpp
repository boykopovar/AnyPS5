#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/IsoBoxExports.hpp"

#include <stdexcept>
#include <string>

namespace {

using namespace NKWebKit::IsoAbi;

template <typename T>
const T& Checked(const T* self, const char* name) {
    if (!self) throw std::invalid_argument(std::string(name) + ": null this");
    return *self;
}

}  // namespace

extern "C" {

std::uint64_t APS5_VABI _ZNK7WebCore6ISOBox4sizeEv(const Box* self) {
    return Checked(self, "ISOBox::size").size;
}

std::uint32_t APS5_VABI _ZNK7WebCore6ISOBox7boxTypeEv(const Box* self) {
    return Checked(self, "ISOBox::boxType").boxType;
}

const ByteVector& APS5_VABI _ZNK7WebCore6ISOBox12extendedTypeEv(const Box* self) {
    return Checked(self, "ISOBox::extendedType").extendedType;
}

std::uint8_t APS5_VABI _ZNK7WebCore10ISOFullBox7versionEv(const FullBox* self) {
    return Checked(self, "ISOFullBox::version").version;
}

std::uint32_t APS5_VABI _ZNK7WebCore10ISOFullBox5flagsEv(const FullBox* self) {
    return Checked(self, "ISOFullBox::flags").flags;
}

OptionalInt8 APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox21defaultCryptByteBlockEv(const TrackEncryptionBox* self) {
    return Checked(self, "ISOTrackEncryptionBox::defaultCryptByteBlock").defaultCryptByteBlock;
}

OptionalInt8 APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox20defaultSkipByteBlockEv(const TrackEncryptionBox* self) {
    return Checked(self, "ISOTrackEncryptionBox::defaultSkipByteBlock").defaultSkipByteBlock;
}

std::int8_t APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox18defaultIsProtectedEv(const TrackEncryptionBox* self) {
    return Checked(self, "ISOTrackEncryptionBox::defaultIsProtected").defaultIsProtected;
}

std::int8_t APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox22defaultPerSampleIVSizeEv(const TrackEncryptionBox* self) {
    return Checked(self, "ISOTrackEncryptionBox::defaultPerSampleIVSize").defaultPerSampleIVSize;
}

ByteVector APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox10defaultKIDEv(const TrackEncryptionBox* self) {
    return Checked(self, "ISOTrackEncryptionBox::defaultKID").defaultKID;
}

ByteVector APS5_VABI _ZNK7WebCore21ISOTrackEncryptionBox17defaultConstantIVEv(const TrackEncryptionBox* self) {
    return Checked(self, "ISOTrackEncryptionBox::defaultConstantIV").defaultConstantIV;
}

ByteVector APS5_VABI _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox8systemIDEv(const ProtectionSystemSpecificHeaderBox* self) {
    return Checked(self, "ISOProtectionSystemSpecificHeaderBox::systemID").systemID;
}

NKWebKit::WtfAbi::Vector<ByteVector> APS5_VABI _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox6keyIDsEv(const ProtectionSystemSpecificHeaderBox* self) {
    return Checked(self, "ISOProtectionSystemSpecificHeaderBox::keyIDs").keyIDs;
}

ByteVector APS5_VABI _ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox4dataEv(const ProtectionSystemSpecificHeaderBox* self) {
    return Checked(self, "ISOProtectionSystemSpecificHeaderBox::data").data;
}

const NKWebKit::MediaAbi::MediaTime& APS5_VABI _ZNK7WebCore12ISOWebVTTCue16presentationTimeEv(const WebVTTCue* self) {
    return Checked(self, "ISOWebVTTCue::presentationTime").presentationTime;
}

const NKWebKit::MediaAbi::MediaTime& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8durationEv(const WebVTTCue* self) {
    return Checked(self, "ISOWebVTTCue::duration").duration;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8sourceIDEv(const WebVTTCue* self) {
    return Checked(self, "ISOWebVTTCue::sourceID").sourceID;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue2idEv(const WebVTTCue* self) {
    return Checked(self, "ISOWebVTTCue::id").identifier;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue17originalStartTimeEv(const WebVTTCue* self) {
    return Checked(self, "ISOWebVTTCue::originalStartTime").originalStartTime;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8settingsEv(const WebVTTCue* self) {
    return Checked(self, "ISOWebVTTCue::settings").settings;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue7cueTextEv(const WebVTTCue* self) {
    return Checked(self, "ISOWebVTTCue::cueText").cueText;
}

}
