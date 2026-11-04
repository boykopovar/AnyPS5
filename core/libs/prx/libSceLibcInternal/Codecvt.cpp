#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

const GuestLocale::CodecvtVtable& Conversion(const GuestLocale::Facet* self, const char* function) {
    if (self == nullptr || self->vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return *reinterpret_cast<const GuestLocale::CodecvtVtable*>(self->vtable);
}

}

extern "C" {

bool APS5_VABI sceLibcInternalCodecvtDoAlwaysNoconv_nid_no_patch(const GuestLocale::Facet* self) {
    return LibcCodecvtMethods_nid_no_patch()->alwaysNoconv(self);
}
APS5_EXPORT("7tIwDZyyKHo", sceLibcInternalCodecvtDoAlwaysNoconv_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtDoIn_nid_no_patch(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first3, char* last3, char** next3) {
    return LibcCodecvtMethods_nid_no_patch()->in(self, state, first1, last1, next1, first3, last3, next3);
}
APS5_EXPORT("P+q1OLiErP0", sceLibcInternalCodecvtDoIn_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtDoOut_nid_no_patch(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first3, char* last3, char** next3) {
    return LibcCodecvtMethods_nid_no_patch()->out(self, state, first1, last1, next1, first3, last3, next3);
}
APS5_EXPORT("Uc+-Sx0UZ3U", sceLibcInternalCodecvtDoOut_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtDoUnshift_nid_no_patch(GuestLocale::Facet* self, std::uint64_t* state, char* first3, char* last3, char** next3) {
    return LibcCodecvtMethods_nid_no_patch()->unshift(self, state, first3, last3, next3);
}
APS5_EXPORT("iCWgjeqMHvg", sceLibcInternalCodecvtDoUnshift_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtDoLength_nid_no_patch(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, std::size_t count) {
    return static_cast<int>(LibcCodecvtMethods_nid_no_patch()->length(self, state, first1, last1, count));
}
APS5_EXPORT("Xk7IZcfHDD8", sceLibcInternalCodecvtDoLength_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtIn_nid_no_patch(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first3, char* last3, char** next3) {
    return Conversion(self, "codecvt::in").in(self, state, first1, last1, next1, first3, last3, next3);
}
APS5_EXPORT("TNexGlwiVEQ", sceLibcInternalCodecvtIn_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtOut_nid_no_patch(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first3, char* last3, char** next3) {
    return Conversion(self, "codecvt::out").out(self, state, first1, last1, next1, first3, last3, next3);
}
APS5_EXPORT("14xKj+SV72o", sceLibcInternalCodecvtOut_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtUnshift_nid_no_patch(GuestLocale::Facet* self, std::uint64_t* state, char* first3, char* last3, char** next3) {
    return Conversion(self, "codecvt::unshift").unshift(self, state, first3, last3, next3);
}
APS5_EXPORT("N7z+Dnk1cS0", sceLibcInternalCodecvtUnshift_nid_no_patch);

int APS5_VABI sceLibcInternalCodecvtLength_nid_no_patch(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, std::size_t count) {
    return static_cast<int>(Conversion(self, "codecvt::length").length(self, state, first1, last1, count));
}
APS5_EXPORT("ikBt0lqkxPc", sceLibcInternalCodecvtLength_nid_no_patch);

}
