#include "prx/libSceNKWebKit/include/ISOBoxExports.hpp"

extern "C" {

std::size_t APS5_VABI _ZN7WebCore6ISOBox14minimumBoxSizeEv() {
    return 2 * sizeof(std::uint32_t);
}

std::uint64_t APS5_VABI _ZNK7WebCore6ISOBox4sizeEv(const NKWebKit::WebCoreAbi::ISOBox* self) {
    return self->m_size;
}

const NKWebKit::WebCoreAbi::FourCC& APS5_VABI _ZNK7WebCore6ISOBox7boxTypeEv(const NKWebKit::WebCoreAbi::ISOBox* self) {
    return self->m_boxType;
}

const NKWebKit::WtfAbi::Vector<std::uint8_t>& APS5_VABI _ZNK7WebCore6ISOBox12extendedTypeEv(const NKWebKit::WebCoreAbi::ISOBox* self) {
    return self->m_extendedType;
}

std::uint8_t APS5_VABI _ZNK7WebCore10ISOFullBox7versionEv(const NKWebKit::WebCoreAbi::ISOFullBox* self) {
    return self->m_version;
}

std::uint32_t APS5_VABI _ZNK7WebCore10ISOFullBox5flagsEv(const NKWebKit::WebCoreAbi::ISOFullBox* self) {
    return self->m_flags;
}

NKWebKit::WebCoreAbi::FourCC APS5_VABI _ZN7WebCore12ISOWebVTTCue11boxTypeNameEv() {
    return {std::uint32_t{'v'} << 24 | std::uint32_t{'t'} << 16 | std::uint32_t{'t'} << 8 | std::uint32_t{'c'}};
}

const NKWebKit::MediaAbi::MediaTime& APS5_VABI _ZNK7WebCore12ISOWebVTTCue16presentationTimeEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self) {
    return self->m_presentationTime;
}

const NKWebKit::MediaAbi::MediaTime& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8durationEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self) {
    return self->m_duration;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8sourceIDEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self) {
    return self->m_sourceID;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue2idEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self) {
    return self->m_identifier;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue17originalStartTimeEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self) {
    return self->m_originalStartTime;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue8settingsEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self) {
    return self->m_settings;
}

const NKWebKit::WtfAbi::String& APS5_VABI _ZNK7WebCore12ISOWebVTTCue7cueTextEv(const NKWebKit::WebCoreAbi::ISOWebVTTCue* self) {
    return self->m_cueText;
}

}
