#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

constexpr int kCodecvtOk = 0;
constexpr int kCodecvtNoconv = 3;

const GuestLocale::CodecvtVtable* VtableOf(const GuestLocale::Facet* self) {
    return reinterpret_cast<const GuestLocale::CodecvtVtable*>(self->vtable);
}

[[noreturn]] void Unwritable(const char* function) {
    throw std::invalid_argument(std::string(function) + ": unwritable next pointer");
}

template<typename T>
void StoreNext(const char* function, T** next, T* value) {
    if (!GuestCopyOut_nid_no_patch(next, &value, sizeof(value))) Unwritable(function);
}

int Noconv(const char* function, const char** next1, const char* first1, char** next2, char* first2) {
    if (!GuestWritable_nid_no_patch(next1, sizeof(*next1)) || !GuestWritable_nid_no_patch(next2, sizeof(*next2))) Unwritable(function);
    StoreNext(function, next1, first1);
    StoreNext(function, next2, first2);
    return kCodecvtNoconv;
}

void ConstructCodecvtChar(GuestLocale::Facet* self, std::size_t refs, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
    self->vtable = reinterpret_cast<const GuestLocale::FacetVtable*>(&_ZTVSt7codecvtIcc9_MbstatetE_nid_postfix[2]);
    self->references = static_cast<std::uint32_t>(refs);
    self->reserved = static_cast<std::uint32_t>(static_cast<std::uint64_t>(refs) >> 32);
}

void ConstructCodecvtCharFromLocinfo(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs, const char* function) {
    if (info == nullptr) throw std::invalid_argument(std::string(function) + ": null _Locinfo");
    ConstructCodecvtChar(self, refs, function);
}

}

extern "C" {

bool APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix(const GuestLocale::Facet*) {
    return true;
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE5do_inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first2, char*, char** next2) {
    return Noconv(__func__, next1, first1, next2, first2);
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE6do_outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(GuestLocale::Facet*, std::uint64_t*, const char* first1, const char*, const char** next1, char* first2, char*, char** next2) {
    return Noconv(__func__, next1, first1, next2, first2);
}

int APS5_VABI _ZNKSt7codecvtIcc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(GuestLocale::Facet*, std::uint64_t*, char* first2, char*, char** next2) {
    StoreNext(__func__, next2, first2);
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

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetEC1Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    ConstructCodecvtChar(self, refs, "codecvt<char, char>::codecvt");
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetEC2Em_nid_postfix(GuestLocale::Facet* self, std::size_t refs) {
    ConstructCodecvtChar(self, refs, "codecvt<char, char>::codecvt");
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetEC1ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs) {
    ConstructCodecvtCharFromLocinfo(self, info, refs, "codecvt<char, char>::codecvt");
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetEC2ERKSt8_Locinfom_nid_postfix(GuestLocale::Facet* self, const GuestLocale::LocinfoStorage* info, std::size_t refs) {
    ConstructCodecvtCharFromLocinfo(self, info, refs, "codecvt<char, char>::codecvt");
}

void APS5_VABI _ZNSt7codecvtIcc9_MbstatetED0Ev_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("codecvt<char, char>::~codecvt: null object");
    _ZdlPv_nid_postfix(self);
}

}
