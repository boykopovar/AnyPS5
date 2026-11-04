#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdint>

namespace {

short WideCharacterMask(char16_t character) {
    return character < 256 ? _Getpctype_nid_postfix()[character] : 0;
}

char16_t WideUpper(char16_t character) {
    return static_cast<char16_t>(towupper_nid_postfix(character));
}

}

extern "C" {

bool APS5_VABI sceLibcInternalWideCtypeDoIs_nid_no_patch(const GuestLocale::Facet*, short mask, char16_t character) {
    return (WideCharacterMask(character) & mask) != 0;
}
APS5_EXPORT("rh7L-TliPoc", sceLibcInternalWideCtypeDoIs_nid_no_patch);

const char16_t* APS5_VABI sceLibcInternalWideCtypeDoIsRange_nid_no_patch(const GuestLocale::Facet*, const char16_t* first, const char16_t* last, short* destination) {
    for (; first != last; ++first, ++destination) *destination = WideCharacterMask(*first);
    return first;
}
APS5_EXPORT("D0lUMKU+ELI", sceLibcInternalWideCtypeDoIsRange_nid_no_patch);

const char16_t* APS5_VABI sceLibcInternalWideCtypeDoScanNot_nid_no_patch(const GuestLocale::Facet*, short mask, const char16_t* first, const char16_t* last) {
    while (first != last && (WideCharacterMask(*first) & mask) != 0) ++first;
    return first;
}
APS5_EXPORT("Kbe+LHOer9o", sceLibcInternalWideCtypeDoScanNot_nid_no_patch);

char16_t APS5_VABI sceLibcInternalWideCtypeDoToupper_nid_no_patch(const GuestLocale::Facet*, char16_t character) {
    return WideUpper(character);
}
APS5_EXPORT("JsZjB3TnZ8A", sceLibcInternalWideCtypeDoToupper_nid_no_patch);

const char16_t* APS5_VABI sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(const GuestLocale::Facet*, char16_t* first, const char16_t* last) {
    for (; first != last; ++first) *first = WideUpper(*first);
    return first;
}
APS5_EXPORT("r1W-NtOi6E8", sceLibcInternalWideCtypeDoToupperRange_nid_no_patch);

}
