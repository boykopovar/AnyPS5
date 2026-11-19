#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/IosBase.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::int32_t FormatMask = 0xffff;
constexpr std::int32_t BaseDec = 0x10;
constexpr std::int32_t BaseOct = 0x20;
constexpr std::int32_t BaseHex = 0x40;
constexpr std::int32_t BaseField = 0x70;

GuestLocale::IosBase* APS5_VABI SetStreamFormatFlags(GuestLocale::IosBase* stream, std::int32_t mask) {
    if (stream == nullptr) throw std::invalid_argument("setiosflags: null stream");
    stream->flags = (stream->flags & ~mask) | (mask & FormatMask);
    return stream;
}

GuestLocale::IosBase* APS5_VABI ResetStreamFormatFlags(GuestLocale::IosBase* stream, std::int32_t mask) {
    if (stream == nullptr) throw std::invalid_argument("resetiosflags: null stream");
    stream->flags &= ~mask;
    return stream;
}

GuestLocale::IosBase* APS5_VABI SetStreamBase(GuestLocale::IosBase* stream, std::int32_t base) {
    if (stream == nullptr) throw std::invalid_argument("setbase: null stream");
    const std::int32_t field = base == 8 ? BaseOct : base == 10 ? BaseDec : base == 16 ? BaseHex : 0;
    stream->flags = (stream->flags & ~BaseField) | field;
    return stream;
}

}

extern "C" {

StreamManipulator APS5_VABI _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask) {
    return {&SetStreamFormatFlags, mask};
}

StreamManipulator APS5_VABI sceLibcInternalResetiosflags_nid_no_patch(std::int32_t mask) {
    return {&ResetStreamFormatFlags, mask};
}
APS5_EXPORT("qMXslRdZVj4", sceLibcInternalResetiosflags_nid_no_patch);

StreamManipulator APS5_VABI sceLibcInternalSetbase_nid_no_patch(std::int32_t base) {
    return {&SetStreamBase, base};
}
APS5_EXPORT("4rrOHCHAV1w", sceLibcInternalSetbase_nid_no_patch);

void APS5_VABI sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
}
APS5_EXPORT("X9D8WWSG3As", sceLibcInternalIosBaseCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(void* self) {
    _ZNSt8ios_baseD2Ev_nid_postfix(static_cast<GuestLocale::IosBase*>(self));
    _ZdlPv_nid_postfix(self);
}
APS5_EXPORT("I5jcbATyIWo", sceLibcInternalIosBaseDeletingDestructor_nid_no_patch);

}
