#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <algorithm>

namespace {

constexpr int kCodecvtOk = 0;
constexpr int kCodecvtNoconv = 3;

const GuestLocale::CodecvtVtable* VtableOf(const GuestLocale::Facet* self) {
    return reinterpret_cast<const GuestLocale::CodecvtVtable*>(self->vtable);
}

}

extern "C" {

bool APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix(const GuestLocale::Facet*) {
    return true;
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE5do_inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first2, char*, char** next2) {
    *next1 = first1;
    *next2 = first2;
    return kCodecvtNoconv;
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE6do_outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first2, char*, char** next2) {
    *next1 = first1;
    *next2 = first2;
    return kCodecvtNoconv;
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(GuestLocale::Facet*, std::uint64_t*, char* first2, char*, char** next2) {
    *next2 = first2;
    return kCodecvtOk;
}

std::size_t APS5_VABI sceLibcInternalCodecvtCharDoLength_nid_no_patch(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char* last1, std::size_t count) {
    return std::min(static_cast<std::size_t>(last1 - first1), count);
}
APS5_EXPORT("Xk7IZcfHDD8", sceLibcInternalCodecvtCharDoLength_nid_no_patch);

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE2inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first2, char* last2, char** next2) {
    return VtableOf(self)->in(self, state, first1, last1, next1, first2, last2, next2);
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE3outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, const char** next1, char* first2, char* last2, char** next2) {
    return VtableOf(self)->out(self, state, first1, last1, next1, first2, last2, next2);
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE7unshiftERS0_PcS3_RS3__nid_postfix(GuestLocale::Facet* self, std::uint64_t* state, char* first2, char* last2, char** next2) {
    return VtableOf(self)->unshift(self, state, first2, last2, next2);
}

std::size_t APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE6lengthERS0_PKcS4_m_nid_postfix(GuestLocale::Facet* self, std::uint64_t* state, const char* first1, const char* last1, std::size_t count) {
    return VtableOf(self)->length(self, state, first1, last1, count);
}

}
