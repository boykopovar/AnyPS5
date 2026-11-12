#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ISOBoxAbi.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {

std::size_t APS5_VABI _ZN7WebCore6ISOBox14minimumBoxSizeEv();
std::uint64_t APS5_VABI _ZNK7WebCore6ISOBox4sizeEv(const NKWebKit::WebCoreAbi::ISOBox* self);
const NKWebKit::WebCoreAbi::FourCC& APS5_VABI _ZNK7WebCore6ISOBox7boxTypeEv(const NKWebKit::WebCoreAbi::ISOBox* self);
const NKWebKit::WtfAbi::Vector<std::uint8_t>& APS5_VABI _ZNK7WebCore6ISOBox12extendedTypeEv(const NKWebKit::WebCoreAbi::ISOBox* self);
std::uint8_t APS5_VABI _ZNK7WebCore10ISOFullBox7versionEv(const NKWebKit::WebCoreAbi::ISOFullBox* self);
std::uint32_t APS5_VABI _ZNK7WebCore10ISOFullBox5flagsEv(const NKWebKit::WebCoreAbi::ISOFullBox* self);
NKWebKit::WebCoreAbi::FourCC APS5_VABI _ZN7WebCore12ISOWebVTTCue11boxTypeNameEv();
const NKWebKit::MediaAbi::MediaTime& APS5_VABI _ZNK7WebCore12ISOWebVTTCue16presentationTimeEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self);
const NKWebKit::MediaAbi::MediaTime& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8durationEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8sourceIDEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue2idEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue17originalStartTimeEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8settingsEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self);
const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue7cueTextEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self);

}
